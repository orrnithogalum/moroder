#include "../../include/services/music.hpp"

#include "../../include/ipc/control/control_request.hpp"
#include "../../include/ipc/stream/stream_request.hpp"
#include "../../include/ipc/search/search_request.hpp"
#include "../../include/ipc/browse/song_request.hpp"
#include "../../include/ipc/request.hpp"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>
#include <string>

namespace fs = std::filesystem;

services::YTMusic::YTMusic(const std::string_view& app_name) {

    spdlog::info("Python server starting...");

    fs::path installed = "/usr/share/ytmusic/python/main.py";
    fs::path dev = YTM_DEV_PYTHON_PATH;

    if (fs::exists(installed)) {
        this->python_server_path = installed.string();

    } else if (fs::exists(dev)) {
        this->python_server_path = dev.string();
    
    } else {
        throw std::runtime_error("Python server not found");
    }

    spdlog::info("Python path: " + this->python_server_path);

    if (pipe(this->pipe_stdin) == -1 || pipe(this->pipe_stdout) == -1) {
        throw std::runtime_error("Python server could not create pipe");
    }

    this->python_pid = fork();
    if (this->python_pid < 0) {
        throw std::runtime_error("Python server could not fork process");
    }

    if (this->python_pid == 0) {
        close(pipe_stdin[1]);
        dup2(pipe_stdin[0], STDIN_FILENO);   // redirect stdin
        close(pipe_stdin[0]);

        close(pipe_stdout[0]);
        dup2(pipe_stdout[1], STDOUT_FILENO); // redirect stdout
        close(pipe_stdout[1]);

        execl("/usr/bin/python", "/usr/bin/python", "-u", this->python_server_path.c_str(), (char*) nullptr);

        // If exec fails
        throw std::runtime_error("Python server could not run");
    }

    close(pipe_stdin[0]);
    close(pipe_stdout[1]);
}

void services::YTMusic::end() {
    this->stop();

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

template<typename ResponseType> ResponseType services::YTMusic::send(const ipc::Request& request, const std::string& log) {
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

ipc::SearchResponse services::YTMusic::search(const std::string& query) {
    ipc::SearchRequest request(query);

    return send<ipc::SearchResponse>(
        request,
        "Python server returned empty on query: " + query
    );
}

ipc::StreamResponse services::YTMusic::stream(const music::SongRef& song) {
    ipc::StreamRequest request(song);

    return send<ipc::StreamResponse>(
        request,
        "Python server returned empty on song: " + song.id
    );
}

ipc::ControlResponse services::YTMusic::resume() {
    ipc::ControlRequest request("resume");

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: resume"
    );
}

ipc::ControlResponse services::YTMusic::pause() {
    ipc::ControlRequest request("pause");

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: pause"
    );
}

ipc::ControlResponse services::YTMusic::setPosition(const float position) {
    ipc::ControlRequest request("setpos", position);

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: setpos " + std::to_string(position)
    );
}

ipc::ControlResponse services::YTMusic::backward(const float duration) {
    ipc::ControlRequest request("backward", duration);

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: backward " + std::to_string(duration)
    );
}

ipc::ControlResponse services::YTMusic::forward(const float duration) {
    ipc::ControlRequest request("forward", duration);

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: forward " + std::to_string(duration)
    );
}

ipc::ControlResponse services::YTMusic::stop() {
    ipc::ControlRequest request("stop");

    return send<ipc::ControlResponse>(
        request,
        "Python server returned empty on command: stop"
    );
}

ipc::SongResponse services::YTMusic::getSong(const music::SongRef& ref) {
    ipc::SongRequest request(ref);

    ipc::SongResponse response = send<ipc::SongResponse>(
        request,
        "Python server returned empty on song request: " + ref.id
    );

    response.song.ref = ref;
    return response;
}