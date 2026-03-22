/* MPV
- Service responsible for interacting with the MPV media player.
- Provides methods to control playback, load files, and handle events.
- Uses a separate thread to listen for MPV events.
- Thread-safe operations are ensured using a mutex.
*/

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

    /* load
    - Loads a media file into MPV and starts playback.
    - If no song is playing it starts automatically
    */
    void load(const std::string& url);
    void stop();

    /* Player controls
    - Basic playback operations
    */
    void pause();
    void resume();

    void seekForward(uint64_t microseconds);
    void seekBackward(uint64_t microseconds);

    void setPosition(uint64_t microseconds);

    void skipForward();
    void skipBackward();

    uint64_t getPosition();

    /* getStreamDuration
    - Retrieves the duration of the currently loaded stream in microseconds.
    - Returns 0 if the duration property cannot be retrieved from MPV.
    */
    uint64_t getStreamDuration();

    /* getStreamPosition
    - Retrieves the position of the currently loaded stream in microseconds.
    - Returns 0 if the position property cannot be retrieved from MPV.
    */
    uint64_t getStreamPosition();

    void setOnStreamStart(std::function<void()> cb);
    void setOnStreamLoad(std::function<void()> cb);
    void setOnStreamEnd(std::function<void()> cb);

private:
    /* mpv
    - Handle to the MPV player instance.
    */
    mpv_handle* mpv = nullptr;
    std::mutex mtx;

    std::thread event_thread;
    std::atomic<bool> running{true};

    std::function<void()> on_stream_start;
    std::function<void()> on_stream_load;
    std::function<void()> on_stream_end;

    /* command
    - Sends a command to MPV.
    */
    void command(const char** args);

    /* eventLoop
    - The main loop that listens for and processes MPV events.
    */
    void eventLoop();
};

}
