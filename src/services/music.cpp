#include "../../include/services/music.hpp"

#include "../../include/ipc/playlist/playlist_request.hpp"
#include "../../include/ipc/search/search_request.hpp"
#include "../../include/ipc/radio/radio_response.hpp"
#include "../../include/ipc/album/album_response.hpp"
#include "../../include/ipc/radio/radio_request.hpp"
#include "../../include/ipc/browse/song_request.hpp"
#include "../../include/ipc/album/album_request.hpp"
#include "../../include/config/config.hpp"
#include "../../include/utils/utils.hpp"
#include "../../include/ipc/request.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>
#include <string>

namespace fs = std::filesystem;

services::Music::Music(const std::string_view& app_name) {

    const Config& cfg = Config::get();

    if (!cfg.LASTFM_API_KEY.empty()) {
        if (setenv("LASTFM_API_KEY", cfg.LASTFM_API_KEY.c_str(), 1) != 0) {
            spdlog::error("PYTHON: setenv failed");

        } else {
            std::string masked = cfg.LASTFM_API_KEY.substr(0, 4) + "...";
            spdlog::info("PYTHON: setenv LASTFM_API_KEY to " + masked);
        }
    }

    spdlog::info("PYTHON: server starting...");

    fs::path python_script_path = utils::resolve_path(MORODER_PYTHON_PATH).string() + "/main.py";

    if (fs::exists(python_script_path)) {
    	this->python_server_path = python_script_path.string();
    } else {
    	throw std::runtime_error("PYTHON: script wasn't found at " + python_script_path.string());
    }

    spdlog::info("PYTHON: path, " + this->python_server_path);

    if (pipe(this->pipe_stdin) == -1 || pipe(this->pipe_stdout) == -1) {
        throw std::runtime_error("PYTHON: could not create pipe");
    }

    this->python_pid = fork();
    if (this->python_pid < 0) {
        throw std::runtime_error("PYTHON: could not fork process");
    }

    if (this->python_pid == 0) {
        close(pipe_stdin[1]);
        dup2(pipe_stdin[0], STDIN_FILENO);
        close(pipe_stdin[0]);

        close(pipe_stdout[0]);
        dup2(pipe_stdout[1], STDOUT_FILENO);
        close(pipe_stdout[1]);

        std::string app_name_str(app_name);

        auto cookies_path = (cfg.COOKIES_PATH) / "browser.json";

        if(std::filesystem::exists(cookies_path)) {
            spdlog::info("PYTHON: found cookies at " + cookies_path.string());
        } else {
            spdlog::warn("PYTHON: couldn't find cookies at " + cookies_path.string());
            cookies_path = "";
        }

        execl(
            "/usr/bin/python",
            "/usr/bin/python",
            "-u",
            this->python_server_path.c_str(),
            app_name_str.c_str(),
            cookies_path.c_str(),
            (char*) nullptr
        );

        throw std::runtime_error("PYTHON: could not run");
    }

    close(pipe_stdin[0]);
    close(pipe_stdout[1]);
}

services::Music::~Music() {
    if (python_pid <= 0) {
        return;
    }

    spdlog::info("PYTHON: stopping...");

    close(pipe_stdin[1]);
    close(pipe_stdout[0]);

    if (kill(python_pid, SIGTERM) == -1) {
        spdlog::info("PYTHON: failed to send SIGTERM to python process");
    }

    int status = 0;
    pid_t result = waitpid(python_pid, &status, 0);

    if (result == -1) {
        spdlog::error("PYTHON: failed on waitpid");
    } else {
        if (WIFEXITED(status)) {
            spdlog::info("PYTHON: exited with code {}", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            spdlog::warn("PYTHON: killed by signal {}", WTERMSIG(status));
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

    auto j = nlohmann::json::parse(buffer, nullptr, false);
    if (j.is_discarded()) {
        spdlog::warn(log + " (invalid JSON): {}", buffer);
        return ResponseType();
    }

    buffer[n] = '\0';
    return ResponseType(buffer);
}

template <typename Request, typename Response> Response services::Music::sendStreamed(
    const Request& request,
    const std::function<void(nlohmann::json&, Response&)>& handleResponse,
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

                spdlog::info(j.dump(4));

                std::string type = j["type"].get<std::string>();

                if (type == doneType) {
                    stop = true;
                    break;
                } else if (type == "error") {
                    spdlog::error("PYTHON: stream error, {}", j.dump());
                    stop = true;
                    break;
                } else {
                    handleResponse(j, response);
                    count++;
                }

            } catch (const std::exception& e) {
                spdlog::warn("PYTHON: failed to parse stream line, {}", e.what());
            }
        }
    }

    spdlog::info("PYTHON: received " + std::to_string(count) + " entries from stream");
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

ipc::SongResponse services::Music::getSong(const music::SongRef& song) {
    ipc::SongRequest request(song);

    ipc::SongResponse response = send<ipc::SongResponse>(
        request,
        "PYTHON: returned empty on song request, " + song.id
    );

    response.song.ref = song;
    return response;
}

ipc::AlbumResponse services::Music::getAlbum(const music::AlbumRef& album) {
    ipc::AlbumRequest request(album);

    return sendStreamed<ipc::AlbumRequest, ipc::AlbumResponse>(
        request,
        [album](nlohmann::json& j, ipc::AlbumResponse& response) {
            if (j["type"] == "album-track") {
                response.addItem(j["data"], album);
            }
        },
        "album-done"
    );
}

ipc::PlaylistResponse services::Music::getPlaylist(const music::PlaylistRef& playlist) {
    ipc::PlaylistRequest request(playlist);

    return sendStreamed<ipc::PlaylistRequest, ipc::PlaylistResponse>(
        request,
        [](nlohmann::json& j, ipc::PlaylistResponse& response) {
            if (j["type"] == "playlist-track") {
                response.addItem(j["data"]);
            }
        },
        "playlist-done"
    );
}
