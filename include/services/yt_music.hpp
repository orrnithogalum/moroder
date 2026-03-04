#pragma once

#include <sys/types.h>
#include <string_view>
#include <string>

#include "../ipc/search_response.hpp"

namespace services {

class YTMusic {
public:
    YTMusic(const std::string_view& app_name);

    ipc::SearchResponse search(const std::string& query);
    void stop();

private:
    std::string python_server_path;

    int pipe_stdin[2];
    int pipe_stdout[2];

    char buffer[16384];

    pid_t python_pid;
};

}