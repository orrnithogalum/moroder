#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <iostream>

#include "../include/ipc/control/control_response.hpp"
#include "../include/ipc/stream/stream_response.hpp"
#include "../include/ipc/search/search_response.hpp"
#include "../include/services/social.hpp"
#include "../include/services/mpris.hpp"
#include "../include/services/music.hpp"

#define APP_NAME_HUMAN "Moroder"
#define APP_NAME "moroder"

int main(int argc, char* argv[]) {
    // --------------------------------------
    //             LOGGER SETUP
    // --------------------------------------
    auto logger = spdlog::basic_logger_mt(APP_NAME, std::string("logs/") + APP_NAME + ".log", true);
    logger->flush_on(spdlog::level::info); // flush on every info or higher
    spdlog::set_default_logger(logger);


    // --------------------------------------
    //              FETCH SONG
    // --------------------------------------
    auto music_service = services::Music(APP_NAME);
    // ipc::SearchResponse search_response = music_service.search("iNjGNNoUjkk"); // Within, Daft Punk, Random Access Memories
    // ipc::SearchResponse search_response = music_service.search("1LrHumAQBso"); // Sing for absolution, Muse, Absolution
    ipc::SearchResponse search_response = music_service.search(argv[1]);

    if (search_response.results.empty()) {
        std::cout << "Exiting, no results" << std::endl;
        exit(0);
    }

    auto* song_ref = std::get_if<music::SongRef>(&search_response.results[0].data);
    if (!song_ref) {
        std::cout << "Exiting, results found but the selected one wasn't a video." << std::endl;
        exit(0);
    }

    music::Song video = music_service.getSong(*song_ref).song;


    // --------------------------------------
    //      START MPRIS, MUSIC & SOCIAL
    // --------------------------------------
    int i = 0;
    int64_t pos = 0;
    bool playing = false;

    auto opt = services::Mpris::make(APP_NAME);
    if (!opt) {
        fprintf(stderr, "Can't connect: someone already there.\n");
        return 1;
    }

    auto &mpris_service = *opt;

    auto social_service = services::Social(1481401025964540125);

    ipc::StreamResponse stream_response = music_service.stream(video.ref);
    mpris_service.setPlaybackStatus(services::PlaybackStatus::Playing);

    mpris_service.setHumanName(APP_NAME_HUMAN);
    mpris_service.setMetadata({
        { services::Field::TrackId, sdbus::Variant(services::OBJECT_PATH + "/track/" + video.ref.id) },
        { services::Field::Album,   sdbus::Variant(video.album_title) },
        { services::Field::Title,   sdbus::Variant(video.ref.title) },
        { services::Field::Artist,  sdbus::Variant(video.ref.artists[0].name) },
        { services::Field::Length,  sdbus::Variant(stream_response.duration) },
        { services::Field::ArtUrl,  sdbus::Variant(video.ref.thumbnail) }
    });

    social_service.setStatus(
        video.ref.title,
        video.ref.artists[0].name,
        video.album_title,
        video.ref.thumbnail,
        stream_response.duration
    );

    mpris_service.onQuit([&] {});

    // mpris_service.onNext([&] { i++; });
    // mpris_service.onPrevious([&] { i--; });
    
    mpris_service.onPause([&] {
        playing = false;
        
        ipc::ControlResponse control_response = music_service.pause();
        social_service.pause();
        mpris_service.setPosition(static_cast<uint64_t>(control_response.position));

        mpris_service.setPlaybackStatus(services::PlaybackStatus::Paused);
    });
    
    mpris_service.onToggle([&] {
        playing = !playing;

        if(playing) {
            music_service.pause();
            social_service.pause();
        } else {
            music_service.resume();
            social_service.resume();
        }

        mpris_service.setPlaybackStatus(playing ? services::PlaybackStatus::Playing : services::PlaybackStatus::Paused);
    });
    
    mpris_service.onStop([&] {
        playing = false;
        music_service.stop();
        social_service.removeStatus();

        mpris_service.setPlaybackStatus(services::PlaybackStatus::Stopped);
    });
    
    mpris_service.onPlay([&] {
        playing = true;
        music_service.resume();
        social_service.resume();
        social_service.setPosition(pos);

        mpris_service.setPlaybackStatus(services::PlaybackStatus::Playing);
    });
    
    mpris_service.onSeek([&] (int64_t p) {
        pos += p;
        
        if(p < 0) {
            music_service.backward(p);
        } else {
            music_service.forward(p);
        }

        social_service.setPosition(pos);  
        mpris_service.setPosition(pos);
    });

    mpris_service.onSetPosition([&] (int64_t p) {
        pos = p;

        music_service.setPosition(pos);
        social_service.setPosition(pos);
        mpris_service.setPosition(pos);
    });

    mpris_service.onLoopStatusChanged([&] (services::LoopStatus status) { });
    mpris_service.onShuffleChanged([&] (bool shuffle) { });
    mpris_service.startLoopAsync();

    music_service.waitUntilStreamEnds();

    pos = 0;
    mpris_service.setPosition(pos);
    mpris_service.sendSeekedSignal(pos);

    mpris_service.setPlaybackStatus(services::PlaybackStatus::Stopped);
    stream_response = music_service.stream(video.ref);
    mpris_service.setPlaybackStatus(services::PlaybackStatus::Playing);
    
    // reset metadata for another supposed song
    mpris_service.setMetadata({
        { services::Field::TrackId, sdbus::Variant(services::OBJECT_PATH + "/track/" + video.ref.id) },
        { services::Field::Album,   sdbus::Variant(video.album_title) },
        { services::Field::Title,   sdbus::Variant(video.ref.title) },
        { services::Field::Artist,  sdbus::Variant(video.ref.artists[0].name) },
        { services::Field::Length,  sdbus::Variant(stream_response.duration) },
        { services::Field::ArtUrl,  sdbus::Variant(video.ref.thumbnail) }
    });

    // reset discord status for another supposed song
    social_service.setStatus(
        video.ref.title,
        video.ref.artists[0].name,
        video.album_title,
        video.ref.thumbnail,
        stream_response.duration
    );

    music_service.waitUntilStreamEnds();

    return 0;
}