#include <algorithm>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <iostream>

#include "../include/ipc/control/control_response.hpp"
#include "../include/ipc/search/search_response.hpp"
#include "../include/services/mpris.hpp"
#include "../include/services/music.hpp"

#define APP_NAME_HUMAN "Moroder"
#define APP_NAME "moroder"

int main(int argc, char* argv[]) {
    // --------------------------------------
    //             LOGGER SETUP
    // --------------------------------------
    auto logger = spdlog::basic_logger_mt(APP_NAME, std::string("logs/") + APP_NAME + ".log", true);
    spdlog::set_default_logger(logger);


    // --------------------------------------
    //              FETCH SONG
    // --------------------------------------
    auto ytmusic_service = services::YTMusic(APP_NAME);
    ipc::SearchResponse response = ytmusic_service.search("iNjGNNoUjkk"); // Daft punk within, random access memories edition

    if (response.results.empty()) {
        std::cout << "Exiting, no results" << std::endl;
        return 0;
    }

    auto* song_ref = std::get_if<music::SongRef>(&response.results[0].data);
    if (!song_ref) {
        std::cout << "Exiting, results found but the selected one wasn't a video." << std::endl;
        return 0;
    }

    music::Song video = ytmusic_service.getSong(*song_ref).song;


    // --------------------------------------
    //              START MPRIS
    // --------------------------------------
    int i = 0;
    int64_t pos = 0;
    bool playing = false;

    auto opt = services::Mpris::make(APP_NAME);
    if (!opt) {
        fprintf(stderr, "can't connect: someone already there.\n");
        return 1;
    }

    auto &mpris_service = *opt;

    mpris_service.setHumanName(APP_NAME_HUMAN);
    mpris_service.setMetadata({
        { services::Field::TrackId, sdbus::Variant(services::OBJECT_PATH + "/track/" + video.ref.id) },
        { services::Field::Album,   sdbus::Variant("an album") },
        { services::Field::Title,   sdbus::Variant(video.ref.title) },
        { services::Field::Artist,  sdbus::Variant(video.ref.artists[0].name) },
        { services::Field::Length,  sdbus::Variant(video.duration_seconds * 1000 * 1000) },
        { services::Field::ArtUrl,  sdbus::Variant(video.ref.thumbnail) }
    });

    mpris_service.onQuit([&] { 
        ytmusic_service.end();
        std::exit(0); 
    });

    mpris_service.onNext([&] { i++; });
    mpris_service.onPrevious([&] { i--; });
    
    mpris_service.onPause([&] {
        playing = false;
        
        ipc::ControlResponse response = ytmusic_service.pause();

        // Convert to milliseconds
        mpris_service.setPosition(static_cast<uint64_t>(response.position * 1000 * 1000));

        mpris_service.setPlaybackStatus(services::PlaybackStatus::Paused);
    });
    
    mpris_service.onToggle([&] {
        playing = !playing;

        if(playing) {
            ytmusic_service.pause();
        } else {
            ytmusic_service.resume();
        }

        mpris_service.setPlaybackStatus(playing ? services::PlaybackStatus::Playing : services::PlaybackStatus::Paused);
    });
    
    mpris_service.onStop([&] {
        playing = false;
        ytmusic_service.stop();

        mpris_service.setPlaybackStatus(services::PlaybackStatus::Stopped);
    });
    
    mpris_service.onPlay([&] {
        playing = true;
        ytmusic_service.resume();

        mpris_service.setPlaybackStatus(services::PlaybackStatus::Playing);
    });
    
    mpris_service.onSeek([&] (int64_t p) {
        pos += p;
        
        // Convert to seconds
        if(p < 0) {
            ytmusic_service.backward(p / 1000000.0);    
        } else {
            ytmusic_service.forward(p / 1000000.0);
        }

        mpris_service.setPosition(pos);
    });

    mpris_service.onSetPosition([&] (int64_t p) {
        pos  = p;

        // Convert to seconds
        ytmusic_service.setPosition(p / 1000000.0);    

        mpris_service.setPosition(pos);
    });

    mpris_service.onLoopStatusChanged([&] (services::LoopStatus status) { });
    mpris_service.onShuffleChanged([&] (bool shuffle) { });

    mpris_service.startLoopAsync();

    ytmusic_service.stream(video.ref);
    mpris_service.setPlaybackStatus(services::PlaybackStatus::Playing);
    
    std::this_thread::sleep_for(std::chrono::seconds(std::min(video.duration_seconds, 20)));

    return 0;
}