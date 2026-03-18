/* SOCIAL
- Handles updating Discord Rich Presence with song metadata (title, author, album, cover, duration)
- Runs in its own dedicated thread to continuously process Discord callbacks
- Provides methods to pause/resume playback, seek, and update presence
*/

#pragma once

#include <thread>
#include <mutex>

#include "discordpp.h"

namespace services {

class Social {
public:
    /* Constructor
    - Initializes the Discord client with the given application ID
    - Starts the worker thread to run the Discord callback loop
    */
    Social(const uint64_t application_id);

    /* Destructor
    - Stops the worker thread and cleans up the Discord client
    */
    ~Social();

    /* Rich presence controls
    - Basic playback operations
    */
    void pause();
    void resume();
    void setPosition(const uint64_t position);

    void setStatus(const std::string& title, const std::string& author, const std::string& album, const std::string& cover_url, uint64_t duration);

    /* removeStatus
    - Removes the current rich presence entirely
    */
    void removeStatus();

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

    void threadLoop();
    void updatePresence();
};

}
