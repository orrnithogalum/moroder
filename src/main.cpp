#include <ostream>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <iostream>

#include "../include/ipc/search/search_response.hpp"
#include "../include/services/yt_music.hpp"

#define APP_NAME_SMALL "ytmusic"

int main(int argc, char* argv[]) {
    auto logger = spdlog::basic_logger_mt(APP_NAME_SMALL, std::string("logs/") + APP_NAME_SMALL + ".txt", true);
    spdlog::set_default_logger(logger);

    auto ytmusic_service = services::YTMusic(APP_NAME_SMALL);

    ipc::SearchResponse response = ytmusic_service.search("daft punk within drumless edition");

    if (response.results.empty()) {
        std::cout << "Exiting, no results" << std::endl;
        return 0;
    }

    auto* video = std::get_if<music::VideoRef>(&response.results[0].data);
    if (!video) {
        std::cout << "Exiting, results found but the first one wasn't a video." << std::endl;
        return 0;
    }

    std::cout << "Video: " << video->title << "\n";
    if (!video->artists.empty()) {
        std::cout << "Artist: " << video->artists[0].name << "\n";
    }
    std::cout << "Id: " << video->id << "\n";

    std::cout << "Start stream..." << std::endl;
    ytmusic_service.stream(*video);
    std::cout << "stream started?" << std::endl;

    std::cout << "Start stream2..." << std::endl;
    ytmusic_service.stream(*video);
    std::cout << "stream2 started?" << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "pause" << std::endl;
    ytmusic_service.pause();

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "pause2" << std::endl;
    ytmusic_service.pause();

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "resume" << std::endl;
    ytmusic_service.resume();

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "resume2" << std::endl;
    ytmusic_service.resume();

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "forward" << std::endl;
    ytmusic_service.forward(10);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "forward2" << std::endl;
    ytmusic_service.forward(10);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "backward" << std::endl;
    ytmusic_service.backward(10);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "backward2" << std::endl;
    ytmusic_service.backward(10);

    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "end" << std::endl;
    ytmusic_service.end();

    return 0;
}