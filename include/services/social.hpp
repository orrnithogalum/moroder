#pragma once

#include <thread>
#include <mutex>

#include "discordpp.h"

namespace services {

class Social {
public:
    Social(uint64_t application_id);
    ~Social();

    void pause();
    void resume();
    void setPosition(uint64_t position);

    void setStatus(const std::string& title, const std::string& author, const std::string& album, const std::string& cover_url, uint64_t duration);

    void removeStatus();

private:
    void threadLoop();
    void updatePresence();

private:
    uint64_t app_id;

    std::shared_ptr<discordpp::Client> client;
    std::atomic<bool> running = true;
    std::thread worker;

    std::mutex mutex;

    bool paused = false;
    bool has_status = false;
    int duration = 0;

    std::string title;
    std::string author;
    std::string album;
    std::string cover_url;

    std::time_t start_time = 0;
};

}