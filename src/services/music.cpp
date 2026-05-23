#include "../../include/services/music.hpp"

#include "../../include/ytm/metadata.hpp"

#include "../../include/ipc/playlist/library_playlists_response.hpp"
#include "../../include/ipc/playlist/playlist_response.hpp"
#include "../../include/ipc/search/search_response.hpp"
#include "../../include/ipc/radio/radio_next_response.hpp"
#include "../../include/ipc/radio/radio_response.hpp"
#include "../../include/ipc/album/album_response.hpp"
#include "../../include/ipc/browse/song_response.hpp"
#include "../../include/ipc/browse/home_response.hpp"

#include "../../include/config/config.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace {

// Tunables that used to live in the Python request objects.
constexpr int SEARCH_LIMIT   = 20;
constexpr int RADIO_LIMIT    = 50;
constexpr int HOME_LIMIT     = 10;
constexpr int LIBRARY_LIMIT  = 25;

bool startsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

std::string userMessage(ytm::Error::Kind kind) {
    switch (kind) {
        case ytm::Error::Kind::Network:
            return "Couldn't reach YouTube Music. Check your connection.";
        case ytm::Error::Kind::Auth:
            return "Your YouTube Music session has expired. Re-run the cookie setup to sign in again.";
        case ytm::Error::Kind::RateLimit:
            return "YouTube Music is rate limiting requests. Give it a moment.";
        case ytm::Error::Kind::Server:
            return "YouTube Music is having trouble right now.";
        case ytm::Error::Kind::Request:
            return "YouTube Music rejected the request. This is probably a bug in moroder.";
        case ytm::Error::Kind::Parse:
            return "Couldn't make sense of YouTube Music's response. The API may have changed.";
        case ytm::Error::Kind::NotFound:
            return "That doesn't seem to exist anymore.";
        default:
            return "Something went wrong.";
    }
}

/* tryAdd
- The parsers emit JSON null for absent fields, matching ytmusicapi exactly.
- Several *Ref::from_json helpers read those with j.value(key, ""), which throws type_error.302 on null rather than falling back.
- sendStreamed used to swallow that in a catch(...) and abandon the rest of the stream. This keeps the rest of the results and logs what was dropped.
*/
template <typename Fn> bool tryAdd(const nlohmann::json& item, const char* what, Fn&& add) {
    if (item.is_null()) return false;

    try {
        add();
        return true;

    } catch (const std::exception& e) {
        spdlog::warn("MUSIC: skipped a malformed {} item, {}", what, e.what());
        return false;
    }
}

}

services::Music::Music(const std::string_view& app_name) {
    const Config& cfg = Config::get();

    lastfm_api_key = cfg.LASTFM_API_KEY;

    if (!lastfm_api_key.empty()) {
        spdlog::info("MUSIC: last.fm key loaded ({}...)", lastfm_api_key.substr(0, 4));
    }

    fs::path cookies_path = cfg.YTM_COOKIES_PATH / "browser.json";

    if (fs::exists(cookies_path)) {
        spdlog::info("MUSIC: found cookies at {}", cookies_path.string());
    } else {
        spdlog::warn("MUSIC: no cookies at {}, running unauthenticated", cookies_path.string());
        cookies_path.clear();
    }

    ytm = std::make_unique<ytm::YTMusic>(cookies_path);

    if (!ytm->lastError().empty()) {
        spdlog::warn("MUSIC: auth setup, {}", ytm->lastError());
    }

    spdlog::info("MUSIC: ready ({})", ytm->isAuthenticated() ? "authenticated" : "anonymous");
}

services::Music::~Music() = default;

void services::Music::captureError(const char* what) {
    const ytm::Error& err = ytm->lastErrorInfo();

    last_error = RequestError{};

    if (err.ok()) return;

    last_error.failed = true;
    last_error.cancelled = (err.kind == ytm::Error::Kind::Cancelled);
    last_error.retryable = err.retryable();
    last_error.detail = err.detail;
    last_error.message = userMessage(err.kind);

    if (last_error.cancelled) {
        spdlog::info("MUSIC: {} cancelled", what);
        return;
    }

    spdlog::warn("MUSIC: {} failed, {}", what, err.detail);
}

std::vector<music::ApiResult> services::Music::getSearch(const std::string& query) {
    ipc::SearchResponse response;

    nlohmann::json results = ytm->search(query, SEARCH_LIMIT);
    captureError("search");

    for (const nlohmann::json& item : results) {
        tryAdd(item, "search", [&] { response.addItem(item); });
    }

    spdlog::info("MUSIC: search returned {} results", results.size());

    return response.results;
}

music::Radio services::Music::getRadio(const std::string id, const std::string type) {
    music::Radio r;
    r.type = type;

    last_error = RequestError{};

    if (id.empty()) return r;

    ytm::WatchPlaylist wp;

    if (type == "song") {
        wp = ytm->getWatchPlaylist(id, "", -1);

    } else {
        std::string pid = id;

        /* An album browseId can't seed a radio; resolve it to the album's audio
        playlist first. The Python side never did this, which is why album
        radio only worked when the caller happened to pass an OLAK id.
        */
        if (startsWith(pid, "MPRE")) {
            nlohmann::json album = ytm->getAlbum(pid);
            std::string    audio = album.value("audioPlaylistId", std::string());

            if (audio.empty()) {
                captureError("radio");

                if (!last_error.failed) {
                    last_error.failed = true;
                    last_error.retryable = false;
                    last_error.detail = "album " + pid + " has no audioPlaylistId";
                    last_error.message = "That album can't be used to start a radio.";
                }

                spdlog::warn("MUSIC: could not resolve audio playlist for album {}", pid);
                return r;
            }

            pid = audio;
        }

        /* Strip VL before prepending PL. Doing it the other way round, as the
        Python did, turned "VLPLxxx" into the unusable "PLVLPLxxx".
        */
        if (startsWith(pid, "VL")) pid = pid.substr(2);
        if (!startsWith(pid, "PL")) pid = "PL" + pid;

        wp = ytm->getWatchPlaylist("", "RDAM" + pid, -1);
    }

    captureError("radio");

    ipc::RadioResponse response;

    for (const nlohmann::json& track : wp.tracks) {
        tryAdd(track, "radio", [&] { response.addItem(track); });
    }

    r.seed_id      = wp.seedId;
    r.continuation = wp.continuation;

    for (const auto& s : response.results) {
        r.addStreamable(s);
    }

    spdlog::info("MUSIC: radio returned {} tracks", wp.tracks.size());

    return r;
}

music::Radio services::Music::getRadio(const music::SongRef& song) {
    return this->getRadio(song.id, "song");
}

music::Radio services::Music::getRadio(const music::AlbumRef& album) {
    return this->getRadio(album.id, "playlist");
}

music::Radio services::Music::getRadio(const music::EpisodeRef& episode) {
    return this->getRadio(episode.id, "song");
}

music::Radio services::Music::getRadio(const music::PlaylistRef& playlist) {
    return this->getRadio(playlist.id, "playlist");
}

music::Radio services::Music::getRadioNext(const music::Radio& radio) {
    music::Radio r;

    // The Python version dropped the type here, so a second continuation always
    // took the song-radio path regardless of what the queue actually was.
    r.type = radio.type;

    last_error = RequestError{};

    if (radio.continuation.empty()) return r;

    ytm::WatchPlaylist wp = (radio.type == "song")
        ? ytm->getWatchPlaylistNext(radio.seed_id, "", radio.continuation, RADIO_LIMIT)
        : ytm->getWatchPlaylistNext("", radio.seed_id, radio.continuation, RADIO_LIMIT);

    captureError("radio continuation");

    ipc::RadioNextResponse response;

    for (const nlohmann::json& track : wp.tracks) {
        tryAdd(track, "radio", [&] { response.addItem(track); });
    }

    r.seed_id      = wp.seedId.empty() ? radio.seed_id : wp.seedId;
    r.continuation = wp.continuation;

    for (const auto& s : response.results) {
        r.addStreamable(s);
    }

    spdlog::info("MUSIC: radio continuation returned {} tracks", wp.tracks.size());

    return r;
}

music::Album services::Music::getAlbum(const music::AlbumRef& album) {
    nlohmann::json data = ytm->getAlbum(album.id);
    captureError("album");

    ipc::AlbumResponse response;

    if (data.contains("tracks")) {
        for (nlohmann::json& track : data["tracks"]) {
            tryAdd(track, "album track", [&] { response.addItem(track, album); });
        }
    }

    music::Album a;
    a.setRef(album);

    for (const auto& s : response.results) {
        a.addStreamable(s);
    }

    return a;
}

music::Playlist services::Music::getPlaylist(const music::PlaylistRef& playlist) {
    nlohmann::json data = ytm->getPlaylist(playlist.id, -1);
    captureError("playlist");

    ipc::PlaylistResponse response;

    if (data.contains("tracks")) {
        for (nlohmann::json& track : data["tracks"]) {
            tryAdd(track, "playlist track", [&] { response.addItem(track); });
        }
    }

    music::Playlist p;
    p.setRef(playlist);

    for (const auto& s : response.results) {
        p.addStreamable(s);
    }

    return p;
}

std::unordered_map<std::string, std::vector<music::ApiResult>> services::Music::getHome() {
    nlohmann::json sections = ytm->getHome(HOME_LIMIT);
    captureError("home");

    ipc::HomeResponse response;

    for (const nlohmann::json& section : sections) {
        std::string category = section.value("title", std::string());

        if (!section.contains("contents")) continue;

        for (const nlohmann::json& item : section["contents"]) {
            /* parse_mixed_content yields null for renderer types it doesn't
            recognise; the Python protocol used that as a stream terminator,
            which silently truncated the page. Skip them instead.
            */
            tryAdd(item, "home", [&] { response.addItem(category, item); });
        }
    }

    return response.results;
}

std::vector<music::Playlist> services::Music::getLibraryPlaylists() {
    if (!ytm->isAuthenticated()) {
        spdlog::info("MUSIC: not logged in, no library playlists");

        last_error = RequestError{};
        return {};
    }

    nlohmann::json playlists = ytm->getLibraryPlaylists(LIBRARY_LIMIT);
    captureError("library playlists");

    ipc::LibraryPlaylistsResponse response;

    for (const nlohmann::json& p : playlists) {
        tryAdd(p, "library playlist", [&] { response.addItem(p); });
    }

    return response.results;
}

bool services::Music::isLoggedIn() {
    last_error = RequestError{};

    return ytm->isAuthenticated();
}

music::Song services::Music::getSong(const music::SongRef& song) {
    // ipc::SongRequest read artists[0].name unguarded, which was undefined
    // behaviour for a result with no artists.
    std::string artist = song.artists.empty() ? std::string() : song.artists.front().name;

    nlohmann::json payload = ytm::lookupAlbum(metadata_http, lastfm_api_key, song.title, artist);

    last_error = RequestError{};

    if (payload.value("status", std::string()) != "ok") {
        last_error.failed = true;
        last_error.retryable = true;
        last_error.detail = payload.value("message", std::string());
        last_error.message = "Couldn't look up album details for this track.";

        spdlog::warn("MUSIC: album lookup failed for '{}', {}", song.title, last_error.detail);
    }

    ipc::SongResponse response(payload.dump());

    response.song.setRef(song);
    return response.song;
}

music::Episode services::Music::getEpisode(const music::EpisodeRef& episode) {
    last_error = RequestError{};

    // No request needed, we already have the data.
    music::Episode e;
    e.setRef(episode);

    return e;
}
