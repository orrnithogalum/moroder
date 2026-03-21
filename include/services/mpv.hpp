#pragma once

#include <mpv/client.h>
#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

namespace services {

class MPV {
public:
    MPV();
    ~MPV();

    void loadFile(const std::string& url);
    void appendFile(const std::string& url);
    void stop();

    void pause();
    void resume();

    void seekForward(uint64_t microseconds);
    void seekBackward(uint64_t microseconds);

    void setPosition(uint64_t microseconds);

    void skipForward();
    void skipBackward();

    uint64_t getPosition();

    void setOnSongEnd(std::function<void()> cb);

private:
    mpv_handle* mpv = nullptr;
    std::mutex mtx;

    std::thread event_thread;
    std::atomic<bool> running{true};

    std::function<void()> on_song_end;

    void command(const char** args);

    void eventLoop();
};

}
