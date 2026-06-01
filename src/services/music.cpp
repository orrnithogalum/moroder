#include "../../include/services/music.hpp"

#include "../../include/ipc/playlist/library_playlists_response.hpp"
#include "../../include/ipc/playlist/library_playlists_request.hpp"
#include "../../include/ipc/playlist/playlist_response.hpp"
#include "../../include/ipc/playlist/playlist_request.hpp"

#include "../../include/ipc/search/search_response.hpp"
#include "../../include/ipc/search/search_request.hpp"

#include "../../include/ipc/radio/radio_next_response.hpp"
#include "../../include/ipc/radio/radio_next_request.hpp"
#include "../../include/ipc/radio/radio_response.hpp"
#include "../../include/ipc/radio/radio_request.hpp"

#include "../../include/ipc/album/library_albums_response.hpp"
#include "../../include/ipc/album/library_albums_request.hpp"
#include "../../include/ipc/album/album_response.hpp"
#include "../../include/ipc/album/album_request.hpp"

#include "../../include/ipc/artist/library_artists_response.hpp"
#include "../../include/ipc/artist/library_artists_request.hpp"

#include "../../include/ipc/song/library_songs_response.hpp"
#include "../../include/ipc/song/library_songs_request.hpp"

#include "../../include/ipc/podcast/library_podcasts_response.hpp"
#include "../../include/ipc/podcast/library_podcasts_request.hpp"

#include "../../include/ipc/browse/is_logged_in_response.hpp"
#include "../../include/ipc/browse/is_logged_in_request.hpp"
#include "../../include/ipc/browse/song_response.hpp"
#include "../../include/ipc/browse/song_request.hpp"
#include "../../include/ipc/browse/home_response.hpp"
#include "../../include/ipc/browse/home_request.hpp"

#include "../../include/config/config.hpp"
#include "../../include/models/radio.hpp"
#include "../../include/utils/utils.hpp"
#include "../../include/ipc/request.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <memory>

namespace fs = std::filesystem;

services::Music::Music(const std::string_view& app_name) {

    const Config& cfg = Config::get();

    if (!cfg.LASTFM_API_KEY.empty()) {
        if (setenv("LASTFM_API_KEY", cfg.LASTFM_API_KEY.c_str(), 1) != 0) {
            spdlog::error("PYTHON: setenv failed");

        } else {
            std::string masked = cfg.LASTFM_API_KEY.substr(0, 4) + "...";
            spdlog::info("PYTHON: setenv LASTFM_API_KEY to " + masked);
        }
    }

    spdlog::info("PYTHON: server starting...");

    fs::path python_script_path = utils::resolve_path(MORODER_PYTHON_PATH).string() + "/main.py";

    if (fs::exists(python_script_path)) {
    	this->python_server_path = python_script_path.string();
    } else {
    	throw std::runtime_error("PYTHON: script wasn't found at " + python_script_path.string());
    }

    spdlog::info("PYTHON: path, " + this->python_server_path);

    if (pipe(this->pipe_stdin) == -1 || pipe(this->pipe_stdout) == -1) {
        throw std::runtime_error("PYTHON: could not create pipe");
    }

    this->python_pid = fork();
    if (this->python_pid < 0) {
        throw std::runtime_error("PYTHON: could not fork process");
    }

    if (this->python_pid == 0) {
        close(pipe_stdin[1]);
        dup2(pipe_stdin[0], STDIN_FILENO);
        close(pipe_stdin[0]);

        close(pipe_stdout[0]);
        dup2(pipe_stdout[1], STDOUT_FILENO);
        close(pipe_stdout[1]);

        std::string app_name_str(app_name);

        auto cookies_path = (cfg.YTM_COOKIES_PATH) / "browser.json";

        if(std::filesystem::exists(cookies_path)) {
            spdlog::info("PYTHON: found cookies at " + cookies_path.string());
        } else {
            spdlog::warn("PYTHON: couldn't find cookies at " + cookies_path.string());
            cookies_path = "";
        }

        bool use_default = false;

        if (cfg.PYTHON_PATH.string().empty()) {
            spdlog::warn("PYTHON: python path empty, defaulting to /usr/bin/python");
            use_default = true;

        } else if (!fs::exists(cfg.PYTHON_PATH) || !fs::is_regular_file(cfg.PYTHON_PATH)) {
            spdlog::warn("PYTHON: python path not found at {}, defaulting to /usr/bin/python", cfg.PYTHON_PATH.string());
            use_default = true;
        }

        execl(
            use_default ? "/usr/bin/python" : cfg.PYTHON_PATH.c_str(),
            use_default ? "/usr/bin/python" : cfg.PYTHON_PATH.c_str(),
            "-u",
            this->python_server_path.c_str(),
            app_name_str.c_str(),
            cookies_path.c_str(),
            (char*) nullptr
        );

        throw std::runtime_error("PYTHON: could not run");
    }

    close(pipe_stdin[0]);
    close(pipe_stdout[1]);
}

services::Music::~Music() {
    if (python_pid <= 0) {
        return;
    }

    spdlog::info("PYTHON: stopping...");

    close(pipe_stdin[1]);
    close(pipe_stdout[0]);

    if (kill(python_pid, SIGTERM) == -1) {
        spdlog::info("PYTHON: failed to send SIGTERM to python process");
    }

    int status = 0;
    pid_t result = waitpid(python_pid, &status, 0);

    if (result == -1) {
        spdlog::error("PYTHON: failed on waitpid");
    } else {
        if (WIFEXITED(status)) {
            spdlog::info("PYTHON: exited with code {}", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            spdlog::info("PYTHON: killed by signal {}", WTERMSIG(status));
        }
    }

    python_pid = -1;
}

template<typename ResponseType> ResponseType services::Music::send(const ipc::Request& request, const std::string& log) {
    std::string request_string = request.serialize();

    write(pipe_stdin[1], request_string.c_str(), request_string.size());
    ssize_t n = read(pipe_stdout[0], this->buffer, sizeof(this->buffer)-1);

    auto j = nlohmann::json::parse(std::string(this->buffer, n), nullptr, false);
    if (j.is_discarded()) {
        spdlog::warn(log + " (invalid JSON): {}", buffer);
        return ResponseType();
    }

    if (n <= 0) {
        spdlog::warn(log);
        return ResponseType();
    }

    buffer[n] = '\0';

    try {
        return ResponseType(buffer);

    } catch(...) {
        return ResponseType();
    }
}

template <typename Request, typename Response> Response services::Music::sendStreamed(
    const Request& request,
    const std::function<void(nlohmann::json&, Response&)>& handleResponse,
    const std::string& doneType
) {
    std::string request_string = request.serialize();
    write(pipe_stdin[1], request_string.c_str(), request_string.size());

    std::string buffer;
    char temp[4096];

    Response response;

    bool stop = false;
    int count = 0;
    while (!stop) {
        ssize_t n = read(pipe_stdout[0], temp, sizeof(temp));
        if (n <= 0) {
            break;
        }

        buffer.append(temp, n);

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            if (line.empty()) continue;

            try {
                auto j = nlohmann::json::parse(line);

                // Useful for debug but pollutes log
                // spdlog::info(j.dump(4));

                if (j.contains("data") && j["data"].is_null()) {
                    break;
                }

                std::string type = j["type"].get<std::string>();

                if (type == doneType) {
                    handleResponse(j, response);
                    stop = true;
                    break;

                } else if (type == "error") {
                    spdlog::error("PYTHON: stream error, {}", j.dump());
                    stop = true;
                    break;

                } else {
                    handleResponse(j, response);
                    count++;
                }

            } catch (...) {
                spdlog::error("PYTHON: failed to parse stream");
                stop = true;
                break;
            }
        }
    }

    spdlog::info("PYTHON: received " + std::to_string(count) + " entries from stream");
    return response;
}

std::vector<music::ApiResult> services::Music::getSearch(const std::string& query) {
    ipc::SearchRequest request(query);

    ipc::SearchResponse response = this->sendStreamed<ipc::SearchRequest, ipc::SearchResponse>(
        request,
        [](const nlohmann::json& j, ipc::SearchResponse& response) {
            if (j["type"] == "search-result") {
                response.addItem(j["data"]);
            }
        },
        "search-done"
    );

    return response.results;
}

music::Radio services::Music::getRadio(const std::string id, const std::string type) {
    ipc::RadioRequest request(id, type);

    ipc::RadioResponse response = this->sendStreamed<ipc::RadioRequest, ipc::RadioResponse>(
        request,
        [](const nlohmann::json& j, ipc::RadioResponse& response) {
            if (j["type"] == "radio-track") {
                response.addItem(j["data"]);

            } else if(j["type"] == "radio-done") {
                response.seed_id = j["id"];
                response.continuation = j["continuation"];
            }
        },
        "radio-done"
    );

    music::Radio r;
    r.type = type;
    r.seed_id = response.seed_id;
    r.continuation = response.continuation;

    for (const auto& s : response.results) {
        r.addStreamable(s);
    }

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
    ipc::RadioNextRequest request(radio);

    ipc::RadioNextResponse response = this->sendStreamed<ipc::RadioNextRequest, ipc::RadioNextResponse>(
        request,
        [](const nlohmann::json& j, ipc::RadioNextResponse& response) {
            if (j["type"] == "radio-track") {
                response.addItem(j["data"]);

            } else if(j["type"] == "radio-done") {
                response.seed_id = j["id"];
                response.continuation = j["continuation"];
            }
        },
        "radio-done"
    );

    music::Radio r;
    r.seed_id = response.seed_id;
    r.continuation = response.continuation;

    for (const auto& s : response.results) {
        r.addStreamable(s);
    }

    return r;
}

music::Song services::Music::getSong(const music::SongRef& song) {
    ipc::SongRequest request(song);

    ipc::SongResponse response = send<ipc::SongResponse>(
        request,
        "PYTHON: returned empty on song request, " + song.id
    );

    response.song.setRef(song);
    return response.song;
}

music::Episode services::Music::getEpisode(const music::EpisodeRef& episode) {
    // No need to make requests, we have the data we need
    music::Episode e;
    e.setRef(episode);
    return e;
}

music::Album services::Music::getAlbum(const music::AlbumRef& album) {
    ipc::AlbumRequest request(album);

    ipc::AlbumResponse response = this->sendStreamed<ipc::AlbumRequest, ipc::AlbumResponse>(
        request,
        [album](nlohmann::json& j, ipc::AlbumResponse& response) {
            if (j["type"] == "album-track") {
                response.addItem(j["data"], album);
            }
        },
        "album-done"
    );

    music::Album a;
    a.setRef(album);

    for (const auto& s : response.results) {
        a.addStreamable(s);
    }

    return a;
}

music::Playlist services::Music::getPlaylist(const music::PlaylistRef& playlist) {
    ipc::PlaylistRequest request(playlist);

    ipc::PlaylistResponse response = this->sendStreamed<ipc::PlaylistRequest, ipc::PlaylistResponse>(
        request,
        [](nlohmann::json& j, ipc::PlaylistResponse& response) {
            if (j["type"] == "playlist-track") {
                response.addItem(j["data"]);
            }
        },
        "playlist-done"
    );

    music::Playlist p;
    p.setRef(playlist);

    for (const auto& s : response.results) {
        p.addStreamable(s);
    }

    return p;
}

std::unordered_map<std::string, std::vector<music::ApiResult>> services::Music::getHome() {
    ipc::HomeRequest request;

    ipc::HomeResponse response = this->sendStreamed<ipc::HomeRequest, ipc::HomeResponse>(
        request,
        [](nlohmann::json& j, ipc::HomeResponse& response) {
            if (j["type"] == "home-item") {
                response.addItem(j["category"], j["data"]);
            }
        },
        "home-done"
    );

    return response.results;
}

std::vector<music::Playlist> services::Music::getLibraryPlaylists() {
    ipc::LibraryPlaylistsRequest request;

    ipc::LibraryPlaylistsResponse response = this->sendStreamed<ipc::LibraryPlaylistsRequest, ipc::LibraryPlaylistsResponse>(
        request,
        [](nlohmann::json& j, ipc::LibraryPlaylistsResponse& response) {
            if (j["type"] == "library-playlists-item") {
                response.addItem(j["data"]);
            }
        },
        "library-playlists-done"
    );

    return response.results;
}

std::vector<music::Album> services::Music::getLibraryAlbums() {
    ipc::LibraryAlbumsRequest request;

    ipc::LibraryAlbumsResponse response = this->sendStreamed<ipc::LibraryAlbumsRequest, ipc::LibraryAlbumsResponse>(
        request,
        [](nlohmann::json& j, ipc::LibraryAlbumsResponse& response) {
            if (j["type"] == "library-albums-item") {
                response.addItem(j["data"]);
            }
        },
        "library-albums-done"
    );

    return response.results;
}

std::vector<std::shared_ptr<music::IStreamable>> services::Music::getLibrarySongs() {
    ipc::LibrarySongsRequest request;

    ipc::LibrarySongsResponse response = this->sendStreamed<ipc::LibrarySongsRequest, ipc::LibrarySongsResponse>(
        request,
        [](nlohmann::json& j, ipc::LibrarySongsResponse& response) {
            if (j["type"] == "library-songs-item") {
                response.addItem(j["data"]);
            }
        },
        "library-songs-done"
    );

    return response.results;
}

std::vector<music::ArtistRef> services::Music::getLibraryArtists() {
    ipc::LibraryArtistsRequest request;

    ipc::LibraryArtistsResponse response = this->sendStreamed<ipc::LibraryArtistsRequest, ipc::LibraryArtistsResponse>(
        request,
        [](nlohmann::json& j, ipc::LibraryArtistsResponse& response) {
            if (j["type"] == "library-artists-item") {
                response.addItem(j["data"]);
            }
        },
        "library-artists-done"
    );

    return response.results;
}

std::vector<music::PodcastRef> services::Music::getLibraryPodcasts() {
    ipc::LibraryPodcastsRequest request;

    ipc::LibraryPodcastsResponse response = this->sendStreamed<ipc::LibraryPodcastsRequest, ipc::LibraryPodcastsResponse>(
        request,
        [](nlohmann::json& j, ipc::LibraryPodcastsResponse& response) {
            if (j["type"] == "library-podcasts-item") {
                response.addItem(j["data"]);
            }
        },
        "library-podcasts-done"
    );

    return response.results;
}

bool services::Music::isLoggedIn() {
    ipc::IsLoggedInRequest request;

    ipc::IsLoggedInResponse response = send<ipc::IsLoggedInResponse>(
        request,
        "PYTHON: returned empty on is logged in request"
    );

    return response.is_logged_in;
}
