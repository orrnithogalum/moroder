/* MUSIC
- Service responsible for all communications between cpp and the python server
- Uses all requests and reponse classes as communication objects
- Everything is logged
*/

#pragma once

#include "../ipc/search/search_response.hpp"
#include "../ipc/browse/song_response.hpp"
#include "../ipc/album/album_response.hpp"
#include "../ipc/radio/radio_response.hpp"
#include "../models/song.hpp"
#include "../ipc/request.hpp"

#include <sys/types.h>
#include <string_view>
#include <string>

namespace services {

class Music {
public:
    Music(const std::string_view& app_name);
    ~Music();

    ipc::SearchResponse search(const std::string& query);
    ipc::RadioResponse radio(const music::SongRef& song);

    /* getSong
    - Fetches extra song details for a given SongRef.
    - SongResponse contains a Song object
    */
    ipc::SongResponse getSong(const music::SongRef& ref);
    ipc::AlbumResponse getAlbum(const music::AlbumRef& album);

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
    int pipe_stdin[2];
    int pipe_stdout[2];

    /* buffer
    - temporary storage for the python json response
    */
    char buffer[8192];

    /* python_pid
    - PID of the python server (ran in a separate process)
    */
    pid_t python_pid;

    bool song_finished = false;

    /* send
    - serialized any request, send it, return the response
    */
    template<typename ResponseType> ResponseType send(const ipc::Request& request, const std::string& log);
    template <typename Request, typename Response> Response sendStreamed(
        const Request& request,
        const std::function<void(nlohmann::json&, Response&)>& handleResponse,
        const std::string& doneType
    );
};

}
