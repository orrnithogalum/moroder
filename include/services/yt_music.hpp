#pragma once

#include "../ipc/search_response.hpp"
#include "../ipc/stream_response.hpp"
#include "../models/video.hpp"
#include "../models/song.hpp"

#include <sys/types.h>
#include <string_view>
#include <string>

namespace services {

class YTMusic {
public:
    YTMusic(const std::string_view& app_name);

    ipc::SearchResponse search(const std::string& query);
    ipc::StreamResponse stream(const music::SongRef& song);
    ipc::StreamResponse stream(const music::VideoRef& video);

    void stop();

private:
    std::string python_server_path;

    int pipe_stdin[2];
    int pipe_stdout[2];

    char buffer[16384];

    pid_t python_pid;
};

}