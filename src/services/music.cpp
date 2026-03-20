#include "../../include/services/music.hpp"

#include "../../include/ipc/control/control_request.hpp"
#include "../../include/ipc/stream/stream_request.hpp"
#include "../../include/ipc/search/search_request.hpp"
#include "../../include/ipc/event/event_response.hpp"
#include "../../include/ipc/radio/radio_response.hpp"
#include "../../include/ipc/radio/radio_request.hpp"
#include "../../include/ipc/browse/song_request.hpp"
#include "../../include/config/config.hpp"
#include "../../include/ipc/request.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>
#include <string>

namespace fs = std::filesystem;

void services::Music::event_worker(int fd) {
    char local_buffer[8192];

    while (true) {
        ssize_t n = read(fd, local_buffer, sizeof(local_buffer) - 1);
        if (n <= 0) {
            spdlog::warn("Event pipe closed or read error");
            break;
        }

        local_buffer[n] = '\0';
        std::string raw(local_buffer);

        // In case multiple events are sent in one read, split by newline
        size_t start = 0;
        while (start < raw.size()) {
            size_t end = raw.find('\n', start);
            if (end == std::string::npos) end = raw.size();

            std::string line = raw.substr(start, end - start);
            start = end + 1;

            if (line.empty()) {
                continue;
            }

            ipc::EventResponse event(line);
            spdlog::info("Received event: {}", event.name);

            if (event.name == "stop") {
                return;
            }

            if (event.name == "song-end") {
                notifyStreamDone();
                continue;
            }

            spdlog::info("Python server unknown event received: " + event.name);
        }
    }
}
services::Music::Music(const std::string_view& app_name) {

    const Config& cfg = Config::get();

    if (!cfg.LASTFM_API_KEY.empty()) {
        if (setenv("LASTFM_API_KEY", cfg.LASTFM_API_KEY.c_str(), 1) != 0) {
            spdlog::error("setenv failed");

        } else {
            std::string masked = cfg.LASTFM_API_KEY.substr(0, 4) + "...";
            spdlog::info("setenv LASTFM_API_KEY to " + masked);
        }
    }

    spdlog::info("Python server starting...");

    fs::path python_script_path = std::string(MORODER_PYTHON_PATH) + "/main.py";

    if (fs::exists(python_script_path)) {
    	this->python_server_path = python_script_path.string();
    } else {
    	throw std::runtime_error("Python script wasn't found at " + python_script_path.string());
    }

    spdlog::info("Python path: " + this->python_server_path);

    if (pipe(this->pipe_stdin) == -1 || pipe(this->pipe_stdout) == -1 || pipe(this->pipe_event) == -1) {
        throw std::runtime_error("Python server could not create pipe");
    }

    this->python_pid = fork();
    if (this->python_pid < 0) {
        throw std::runtime_error("Python server could not fork process");
    }

    if (this->python_pid == 0) {
        // redirect stdin
        close(pipe_stdin[1]);
        dup2(pipe_stdin[0], STDIN_FILENO);
        close(pipe_stdin[0]);

        // redirect stdout
        close(pipe_stdout[0]);
        dup2(pipe_stdout[1], STDOUT_FILENO);
        close(pipe_stdout[1]);

        close(pipe_event[0]);
        std::string fd_str = std::to_string(pipe_event[1]);
        std::string app_name_str(app_name);

        auto cookies_path = (cfg.COOKIES_PATH) / "browser.json";

        if(std::filesystem::exists(cookies_path)) {
            spdlog::info("Python server found cookies at " + cookies_path.string());
        } else {
            spdlog::warn("Python server couldn't find cookies at " + cookies_path.string());
            cookies_path = "";
        }

        execl(
            "/usr/bin/python",
            "/usr/bin/python",
            "-u",
            this->python_server_path.c_str(),
            fd_str.c_str(),
            app_name_str.c_str(),
            cookies_path.c_str(),
            (char*) nullptr
        );

        // If exec fails
        throw std::runtime_error("Python server could not run");
    }

    close(pipe_stdin[0]);
    close(pipe_stdout[1]);
    close(pipe_event[1]);

    this->event_thread = std::thread([this]() {
        this->event_worker(pipe_event[0]);
    });
}

services::Music::~Music() {
    this->stop();
    this->event_thread.join();

    if (python_pid <= 0) {
        return;
    }

    spdlog::info("Python server stopping...");

    close(pipe_stdin[1]);
    close(pipe_stdout[0]);

    if (kill(python_pid, SIGTERM) == -1) {
        spdlog::info("Python server failed to send SIGTERM to python process");
    }

    int status = 0;
    pid_t result = waitpid(python_pid, &status, 0);

    if (result == -1) {
        spdlog::error("Python server failed on waitpid");
    } else {
        if (WIFEXITED(status)) {
            spdlog::info("Python server exited with code {}", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            spdlog::warn("Python server killed by signal {}", WTERMSIG(status));
        }
    }

    python_pid = -1;
}

template<typename ResponseType> ResponseType services::Music::send(const ipc::Request& request, const std::string& log) {
    std::string request_string = request.serialize();

    write(pipe_stdin[1], request_string.c_str(), request_string.size());
    ssize_t n = read(pipe_stdout[0], this->buffer, sizeof(this->buffer)-1);
    if (n <= 0) {
        spdlog::warn(log);
        return ResponseType();
    }

    buffer[n] = '\0';
    return ResponseType(buffer);
}

template <typename Request, typename Response> Response services::Music::sendStreamed(
    const Request& request,
    const std::function<void(const nlohmann::json&, Response&)>& handleResponse,
    const std::string& doneType
) {
    std::string request_string = request.serialize();
    write(pipe_stdin[1], request_string.c_str(), request_string.size());

    std::string buffer;
    char temp[4096];

    Response response;

    bool stop = false;
    int count = 0;
    while (!stop) {
        ssize_t n = read(pipe_stdout[0], temp, sizeof(temp));
        if (n <= 0) {
            break;
        }

        buffer.append(temp, n);

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            if (line.empty()) continue;

            try {
                auto j = nlohmann::json::parse(line);

                std::string type = j["type"].get<std::string>();

                if (type == doneType) {
                    stop = true;
                    break;
                } else if (type == "error") {
                    spdlog::error("Stream error: {}", j.dump());
                    stop = true;
                    break;
                } else {
                    handleResponse(j, response);
                }

            } catch (const std::exception& e) {
                spdlog::warn("Failed to parse stream line: {}", e.what());
            }
        }
        count++;
    }

    spdlog::info("Received " + std::to_string(count) + " entries from stream.");
    return response;
}

ipc::SearchResponse services::Music::search(const std::string& query) {
    ipc::SearchRequest request(query);

    return sendStreamed<ipc::SearchRequest, ipc::SearchResponse>(
        request,
        [](const nlohmann::json& j, ipc::SearchResponse& response) {
            if (j["type"] == "search-result") {
                response.addItem(j["data"]);
            }
        },
        "search-done"
    );
}

ipc::RadioResponse services::Music::radio(const music::SongRef& song) {
    ipc::RadioRequest request(song);

    return sendStreamed<ipc::RadioRequest, ipc::RadioResponse>(
        request,
        [](const nlohmann::json& j, ipc::RadioResponse& response) {
            if (j["type"] == "radio-track") {
                response.addItem(j["data"]);
            }
        },
        "radio-done"
    );
}

ipc::StreamResponse services::Music::stream(const music::SongRef& song) {
    ipc::StreamRequest request(song);

    return send<ipc::StreamResponse>(
        request,
        "Python server returned empty on song: " + song.id
    );
}

ipc::ControlResponse services::Music::resume() {
    ipc::ControlRequest request("resume");

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: resume"
    );
}

ipc::ControlResponse services::Music::pause() {
    ipc::ControlRequest request("pause");

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: pause"
    );
}

ipc::ControlResponse services::Music::setPosition(const uint64_t position) {
    ipc::ControlRequest request("setpos", position);

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: setpos " + std::to_string(position)
    );
}

ipc::ControlResponse services::Music::seekBackward(const uint64_t duration) {
    ipc::ControlRequest request("seek-backward", duration);

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: backward " + std::to_string(duration)
    );
}

ipc::ControlResponse services::Music::seekForward(const uint64_t duration) {
    ipc::ControlRequest request("seek-forward", duration);

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: forward " + std::to_string(duration)
    );
}

ipc::ControlResponse services::Music::skipBackward() {
    ipc::ControlRequest request("skip-backward");

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: skip-backward"
    );
}

ipc::ControlResponse services::Music::skipForward() {
    ipc::ControlRequest request("skip-forward");

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: forward"
    );
}

ipc::ControlResponse services::Music::stop() {
    ipc::ControlRequest request("stop");

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: stop"
    );
}

ipc::SongResponse services::Music::getSong(const music::SongRef& ref) {
    ipc::SongRequest request(ref);

    ipc::SongResponse response = send<ipc::SongResponse>(
        request,
        "Python server returned empty on song request: " + ref.id
    );

    response.song.ref = ref;
    return response;
}
