#include <ostream>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <iostream>

#include "../include/ipc/search_response.hpp"
#include "../include/services/yt_music.hpp"

#define APP_NAME_SMALL "ytmusic"

int main(int argc, char* argv[]) {
    auto logger = spdlog::basic_logger_mt(APP_NAME_SMALL, std::string("logs/") + APP_NAME_SMALL + ".txt", true);
    spdlog::set_default_logger(logger);

    auto ytmusic_service = services::YTMusic(APP_NAME_SMALL);
    
    ipc::SearchResponse response = ytmusic_service.search("daft punk within");
    std::cout << response.results.size() << std::endl;
    std::cout << response.results[0].category << ": " << response.results[0].resultType << std::endl;
    std::cout << response.results[1].category << ": " << response.results[1].resultType << std::endl;
    std::cout << response.results[2].category << ": " << response.results[2].resultType << std::endl;
    std::cout << response.results[3].category << ": " << response.results[3].resultType << std::endl;

    ytmusic_service.stop();

    return 0;
}