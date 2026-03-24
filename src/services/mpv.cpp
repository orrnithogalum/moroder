#include "../../include/services/mpv.hpp"

#include "../../include/utils/utils.hpp"

#include <spdlog/spdlog.h>
#include <stdexcept>

services::MPV::MPV() {
    mpv = mpv_create();

    mpv_set_option_string(mpv, "video", "no");
    mpv_set_option_string(mpv, "no-config", "yes");
    mpv_set_option_string(mpv, "idle", "yes");

    mpv_set_option_string(mpv, "cache", "yes");

    mpv_set_option_string(mpv, "prefetch-playlist", "yes");
    mpv_set_option_string(mpv, "playlist-start", "0");

    mpv_set_option_string(mpv, "ytdl", "yes");
    mpv_set_option_string(mpv, "ytdl-format", "bestaudio");

    mpv_set_option_string(mpv, "log-file", std::string(utils::resolve_path(MORODER_LOG_PATH).string() + "/mpv.log").c_str());

    if (!mpv) {
        throw std::runtime_error("Failed to create mpv instance");
    }

    if (mpv_initialize(mpv) < 0) {
        throw std::runtime_error("Failed to initialize mpv");
    }

    event_thread = std::thread(&MPV::eventLoop, this);
}

services::MPV::~MPV() {
    running = false;
    if (event_thread.joinable()) {
          event_thread.join();
    }

    if (mpv) {
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
    }
}

void services::MPV::eventLoop() {
    while (running) {
        mpv_event* event = mpv_wait_event(mpv, 0.01);

        if (!event) continue;

        switch (event->event_id) {

        case MPV_EVENT_START_FILE: {
            spdlog::info("MPV: starting to load file");

            if (on_stream_load) {
                on_stream_load();
            }
            break;
        }

        case MPV_EVENT_FILE_LOADED: {
            spdlog::info("MPV: file loaded, playback started");

            if (on_stream_start) {
                on_stream_start();
            }
            break;
        }

        case MPV_EVENT_END_FILE: {
            auto* ev = (mpv_event_end_file*)event->data;

            if (ev->reason != MPV_END_FILE_REASON_EOF) { break; }

            spdlog::info("MPV: song ended");

            if (on_stream_end) {
                on_stream_end();
            }

            break;
        }

        case MPV_EVENT_SHUTDOWN:
            spdlog::info("MPV: shutdown event");
            return;

        default:
            break;
        }
    }
}

void services::MPV::setOnStreamStart(std::function<void()> cb) {
    on_stream_start = std::move(cb);
}

void services::MPV::setOnStreamLoad(std::function<void()> cb) {
    on_stream_load = std::move(cb);
}

void services::MPV::setOnStreamEnd(std::function<void()> cb) {
    on_stream_end = std::move(cb);
}

void services::MPV::command(const char** args) {
    std::lock_guard<std::mutex> lock(mtx);

    if (mpv_command(mpv, args) < 0) {
        spdlog::error("MPV: command failed");
    }
}

void services::MPV::load(const std::string& url) {
    const char* args[] = {"loadfile", url.c_str(), "append-play", nullptr};
    command(args);
}

void services::MPV::stop() {
    const char* args[] = {"stop", nullptr};
    command(args);
}

void services::MPV::pause() {
    std::lock_guard<std::mutex> lock(mtx);
    int pause = 1;
    mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
}

void services::MPV::resume() {
    std::lock_guard<std::mutex> lock(mtx);
    int pause = 0;
    mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
}

void services::MPV::seekForward(uint64_t microseconds) {
    double seconds = static_cast<double>(microseconds) / 1000000.0;

    const std::string val = std::to_string(seconds);

    const char* args[] = {
        "seek",
        val.c_str(),
        "relative",
        nullptr
    };

    command(args);
}

void services::MPV::seekBackward(uint64_t microseconds) {
    double seconds = -static_cast<double>(microseconds) / 1000000.0;

    const std::string val = std::to_string(seconds);

    const char* args[] = {
        "seek",
        val.c_str(),
        "relative",
        nullptr
    };

    command(args);
}

void services::MPV::setPosition(uint64_t microseconds) {
    double seconds = static_cast<double>(microseconds) / 1000000.0;

    const std::string val = std::to_string(seconds);

    const char* args[] = {
        "seek",
        val.c_str(),
        "absolute",
        nullptr
    };

    command(args);
}

void services::MPV::skipForward() {
    const char* args[] = {"playlist-next", "force", nullptr};
    command(args);
}

void services::MPV::skipBackward() {
    const char* args[] = {"playlist-prev", "force", nullptr};
    command(args);
}

uint64_t services::MPV::getStreamPosition() {
    std::lock_guard<std::mutex> lock(mtx);

    double position = 0.0;

    if (mpv_get_property(mpv, "playback-time", MPV_FORMAT_DOUBLE, &position) < 0) {
        spdlog::warn("MPV: get property playback-time failed");
        return 0;
    }

    return static_cast<uint64_t>(position * 1000000.0);
}

uint64_t services::MPV::getStreamDuration() {
    std::lock_guard<std::mutex> lock(mtx);

    double duration = 0.0;

    if (mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &duration) < 0) {
        spdlog::warn("MPV: get property duration failed");
        return 0;
    }

    return static_cast<uint64_t>(duration * 1000000.0);
}
