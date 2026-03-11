/* YTMUSIC
- Service responsible for all communications between cpp and the python server
- Uses all requests and reponse classes as communication objects
- Everything is logged
*/

#pragma once

#include "../ipc/control/control_response.hpp"
#include "../ipc/search/search_response.hpp"
#include "../ipc/stream/stream_response.hpp"
#include "../ipc/browse/song_response.hpp"
#include "../models/song.hpp"
#include "../ipc/request.hpp"

#include <sys/types.h>
#include <string_view>
#include <string>

namespace services {

class YTMusic {
public:
    YTMusic(const std::string_view& app_name);

    ipc::SearchResponse search(const std::string& query);
    ipc::StreamResponse stream(const music::SongRef& song);

    /* player controls
    - basic playback operations
    */
    ipc::ControlResponse resume();
    ipc::ControlResponse pause();
    ipc::ControlResponse setPosition(const uint64_t position);
    ipc::ControlResponse backward(const uint64_t duration);
    ipc::ControlResponse forward(const uint64_t duration);

    /* getSong
    - Fetches extra song details for a given SongRef.
    - SongResponse contains a Song object
    */
    ipc::SongResponse getSong(const music::SongRef& ref);
    
    /* stop
    - Ends the mpv process
    */
    ipc::ControlResponse stop();

    /* end
    - Ends the mpv process and the python process
    */ 
    void quit();

private:
    /* python_server_path
    - Path to the python script
    - Defined by cmake, will vary depending on if user builds with --install or not
    - will be overridable
    */
    std::string python_server_path;

    /* pipes
    - Used to send / receive data from python
    */
    int pipe_event[2];
    int pipe_stdin[2];
    int pipe_stdout[2];

    /* buffer
    - temporary storage for the python json response
    */
    char buffer[16384];

    /* python_pid
    - PID of the python server (ran in a separate process)
    */
    pid_t python_pid;

    /* event_thread
    - Polls events in the dedicated event_pipe
    */
    std::thread event_thread;

    /* send
    - serialized any request, send it, return the response
    */
    template<typename ResponseType> ResponseType send(const ipc::Request& request, const std::string& log);

    /* event_worker
    - Event polling logic
    */
    void event_worker(int fd);
};

}