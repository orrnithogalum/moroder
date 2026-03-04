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

    ipc::SearchResponse response = ytmusic_service.search("behind blue eyes limp bizkit");
    std::cout << "Searched: " << response.results.size() << " results." << std::endl;
    
    if (auto* song = std::get_if<music::Song>(&response.results[0].data)) {
        std::cout << "Song: " << song->title << "\n";
        std::cout << "Artist: " << song->artists[0].name << "\n";
        std::cout << "Views: " << song->views << "\n";
        std::cout << "Id: " << song->id << "\n";
    }
    else if (auto* album = std::get_if<music::Album>(&response.results[0].data)) {
        std::cout << "Album: " << album->title << "\n";
    }

    ytmusic_service.stop();

    return 0;
}