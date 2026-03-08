#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <iostream>

#include "../include/ipc/search/search_response.hpp"
#include "../include/services/music.hpp"

#define APP_NAME_SMALL "ytmusic"

int main(int argc, char* argv[]) {
    auto logger = spdlog::basic_logger_mt(APP_NAME_SMALL, std::string("logs/") + APP_NAME_SMALL + ".txt", true);
    spdlog::set_default_logger(logger);

    auto ytmusic_service = services::YTMusic(APP_NAME_SMALL);

    ipc::SearchResponse response = ytmusic_service.search("prmvo0uprc0"); // Daft punk within drumless edition

    if (response.results.empty()) {
        std::cout << "Exiting, no results" << std::endl;
        return 0;
    }

    auto* video_ref = std::get_if<music::SongRef>(&response.results[0].data);
    if (!video_ref) {
        std::cout << "Exiting, results found but the first one wasn't a video." << std::endl;
        return 0;
    }

    music::Song video = ytmusic_service.getSong(*video_ref).song;


    std::cout << "Video: " << video.ref.title << "\n";
    if (!video.ref.artists.empty()) {
        std::cout << "Artist: " << video.ref.artists[0].name << "\n";
    }
    std::cout << "Id: " << video.ref.id << "\n";


    std::cout << "Start stream..." << std::endl;
    ytmusic_service.stream(video.ref);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "pause" << std::endl;
    ytmusic_service.pause();

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "resume" << std::endl;
    ytmusic_service.resume();

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "forward" << std::endl;
    ytmusic_service.forward(30);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "backward" << std::endl;
    ytmusic_service.backward(10);

    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "end" << std::endl;
    ytmusic_service.end();

    return 0;
}