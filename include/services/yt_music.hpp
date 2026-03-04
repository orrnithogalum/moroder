#pragma once

#include <string_view>
#include <sys/types.h>

namespace services {

class YTMusic {
public:
    YTMusic(const std::string_view& app_name);
    ~YTMusic();

    void start();

private:
    const char* python_path;

    int pipe_stdin[2];
    int pipe_stdout[2];

    char buffer[8192];

    pid_t python_pid;
};

}