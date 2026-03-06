#pragma once

#include "../ipc/control/control_response.hpp"
#include "../ipc/search/search_response.hpp"
#include "../ipc/stream/stream_response.hpp"
#include "../models/video.hpp"
#include "../models/song.hpp"
#include "../ipc/request.hpp"

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

    ipc::ControlResponse resume();
    ipc::ControlResponse pause();
    ipc::ControlResponse backward(const std::uint8_t duration);
    ipc::ControlResponse forward(const std::uint8_t duration);
    
    // Ends the mpv process
    ipc::ControlResponse stop();

    // Ends the mpv process and the python process
    void end();

private:
    std::string python_server_path;

    int pipe_stdin[2];
    int pipe_stdout[2];

    char buffer[16384];

    pid_t python_pid;

    template<typename ResponseType> ResponseType send(const ipc::Request& request, const std::string& log);
};

}