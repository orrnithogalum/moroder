/* MUSIC
- Service responsible for all communications with the YouTube Music API
- Talks to InnerTube directly; no Python process, no pipes, no IPC framing
- Still builds models through the existing ipc::*Response classes
- Everything is logged
*/

#pragma once

#include "../models/api_result.hpp"
#include "../models/radio.hpp"

#include "../ytm/ytmusic.hpp"
#include "../ytm/http.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

namespace services {

struct RequestError {
    bool failed = false;
    bool retryable = false;
    bool cancelled = false;
    std::string message;
    std::string detail;
};

class Music {
public:
    /* app_name
    - Retained for source compatibility only
    - It used to name the Python IPC socket, which no longer exists
    */
    Music(const std::string_view& app_name = {});
    ~Music();

    bool isLoggedIn();

    const RequestError& lastError() const { return last_error; }

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
    /* ytm
    - The InnerTube client: transport, auth, endpoints, continuations
    - Reads the same browser.json ytmusicapi's setup writes
    */
    std::unique_ptr<ytm::YTMusic> ytm;

    /* metadata_http
    - last.fm / iTunes only, for the album-name enrichment in getSong
    - This never went through ytmusicapi
    */
    ytm::Http   metadata_http;
    std::string lastfm_api_key;

    RequestError last_error;
    void captureError(const char* what);

    music::Radio getRadio(const std::string id, const std::string type);
};

}
