/* MUSIC
- Service responsible for all communications between cpp and the python server
- Uses all requests and reponse classes as communication objects
- Everything is logged
*/

#pragma once

#include "../models/search_result.hpp"
#include "../models/radio.hpp"
#include "../ipc/request.hpp"

#include <sys/types.h>
#include <string_view>
#include <string>

namespace services {

class Music {
public:
    Music(const std::string_view& app_name);
    ~Music();

    std::vector<music::SearchResult> getSearch(const std::string& query);

    music::Song     getSong(const music::SongRef& ref);
    music::Radio    getRadio(const music::SongRef& song);
    music::Album    getAlbum(const music::AlbumRef& album);
    music::Playlist getPlaylist(const music::PlaylistRef& playlist);

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
