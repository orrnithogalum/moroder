/* MUSIC
- Service responsible for all communications between cpp and the python server
- Uses all requests and reponse classes as communication objects
- Everything is logged
*/

#pragma once

#include "../models/api_result.hpp"
#include "../models/radio.hpp"
#include "../ipc/request.hpp"

#include <unordered_map>
#include <sys/types.h>
#include <string_view>
#include <string>
#include <vector>

namespace services {

class Music {
public:
    Music(const std::string_view& app_name);
    ~Music();

    bool isLoggedIn();

    std::vector<music::Playlist>  getLibraryPlaylists();
    std::vector<music::ApiResult> getSearch(const std::string& query);

    std::unordered_map<std::string, std::vector<music::ApiResult>> getHome();

    music::Song     getSong(const music::SongRef& song);
    music::Episode  getEpisode(const music::EpisodeRef& episode);

    music::Radio    getRadio(const music::SongRef& song);
    music::Radio    getRadio(const music::AlbumRef& album);
    music::Radio    getRadio(const music::EpisodeRef& episode);
    music::Radio    getRadio(const music::PlaylistRef& playlist);

    music::Radio    getRadioNext(const music::Radio& radio);

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

    music::Radio getRadio(const std::string id, const std::string type);
};

}
