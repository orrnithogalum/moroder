#include "../../include/services/yt_music.hpp"

#include "../../include/ipc/stream_request.hpp"
#include "../../include/ipc/search_request.hpp"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>

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

void services::YTMusic::stop() {
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

ipc::SearchResponse services::YTMusic::search(const std::string& query) {
    auto request = ipc::SearchRequest(query);

    spdlog::info("Python server searching for query: " + query);
    
    std::string request_string = request.serialize();
    write(pipe_stdin[1], request_string.c_str(), request_string.size());
    ssize_t n = read(pipe_stdout[0], this->buffer, sizeof(this->buffer)-1);

    ipc::SearchResponse response;
    if (n <= 0) {
        spdlog::warn("Python server returned empty on query: " + query);
        return response;
    }

    buffer[n] = '\0';
    response = ipc::SearchResponse(buffer);
    
    return response;
}

ipc::StreamResponse services::YTMusic::stream(const music::SongRef& song) {
    auto request = ipc::StreamRequest(song);

    spdlog::info("Python server streaming for song: " + song.id);

    std::string request_string = request.serialize();
    write(pipe_stdin[1], request_string.c_str(), request_string.size());
    ssize_t n = read(pipe_stdout[0], this->buffer, sizeof(this->buffer)-1);

    ipc::StreamResponse response;
    if (n <= 0) {
        spdlog::warn("Python server returned empty on song: " + song.id);
        return response;
    }

    buffer[n] = '\0';
    response = ipc::StreamResponse(buffer);

    return response;
}

ipc::StreamResponse services::YTMusic::stream(const music::VideoRef& video) {
    auto request = ipc::StreamRequest(video);

    spdlog::info("Python server streaming for video: " + video.id);

    std::string request_string = request.serialize();
    write(pipe_stdin[1], request_string.c_str(), request_string.size());
    ssize_t n = read(pipe_stdout[0], this->buffer, sizeof(this->buffer)-1);

    ipc::StreamResponse response;
    if (n <= 0) {
        spdlog::warn("Python server returned empty on video: " + video.id);
        return response;
    }

    buffer[n] = '\0';
    response = ipc::StreamResponse(buffer);

    return response;
}