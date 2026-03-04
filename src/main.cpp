#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "../include/services/yt_music.hpp"

#define APP_NAME_SMALL "ytmusic"

int main(int argc, char* argv[]) {
    auto logger = spdlog::basic_logger_mt(APP_NAME_SMALL, std::string("logs/") + APP_NAME_SMALL + ".txt");
    spdlog::set_default_logger(logger);

    auto ytmusic_service = services::YTMusic(APP_NAME_SMALL);

    return 0;
}