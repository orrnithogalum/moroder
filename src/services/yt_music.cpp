#include "../../include/services/yt_music.hpp"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>

namespace fs = std::filesystem;

services::YTMusic::YTMusic(const std::string_view& app_name) {

    spdlog::info("Starting python server...");

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
        throw std::runtime_error("Could not create pipe for python server");
    }

    this->python_pid = fork();
    if (this->python_pid < 0) {
        throw std::runtime_error("Could not fork process for python server");
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
        throw std::runtime_error("Could not run python server");
    }

    close(pipe_stdin[0]);
    close(pipe_stdout[1]);
}

void services::YTMusic::stop() {
    if (python_pid <= 0) {
        return;
    }

    spdlog::info("Stopping python server...");

    close(pipe_stdin[1]);
    close(pipe_stdout[0]);

    if (kill(python_pid, SIGTERM) == -1) {
        spdlog::warn("Failed to send SIGTERM to python process");
    }

    int status = 0;
    pid_t result = waitpid(python_pid, &status, 0);

    if (result == -1) {
        spdlog::error("waitpid failed for python process");
    } else {
        if (WIFEXITED(status)) {
            spdlog::info("Python server exited with code {}", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            spdlog::warn("Python server killed by signal {}", WTERMSIG(status));
        }
    }

    python_pid = -1;
}

void services::YTMusic::search(const std::string& query) {
    std::string request = "{\"action\":\"search\", \"query\":\"daft punk\"}\n";
    write(pipe_stdin[1], request.c_str(), request.size()); // send JSON request
    // optional: close(pipe_stdin[1]); // if you don't plan to send more requests

    // Read response
    ssize_t n = read(pipe_stdout[0], this->buffer, sizeof(this->buffer)-1);
    if (n > 0) {
        buffer[n] = '\0';
        std::cout << "Response: " << buffer << std::endl;
    }

}