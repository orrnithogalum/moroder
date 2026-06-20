#include "../../include/ytm/metadata.hpp"

#include <spdlog/spdlog.h>

namespace ytm {

nlohmann::json lookupAlbum(Http& http, const std::string& lastfmApiKey, const std::string& title, const std::string& artist) {
    const std::string agent = "user-agent: moroder/1.0 (orrnithogalum@github.com)";

    if (!lastfmApiKey.empty()) {
        std::string url = "https://ws.audioscrobbler.com/2.0/"
                          "?method=track.getInfo&format=json"
                          "&api_key=" + Http::urlEncode(lastfmApiKey) +
                          "&artist="  + Http::urlEncode(artist) +
                          "&track="   + Http::urlEncode(title);

        Http::Response r = http.get(url, {agent});

        if (!r.ok()) {
            // Not fatal, iTunes is tried next, but a persistently failing key
            // is otherwise invisible.
            spdlog::warn("METADATA: last.fm lookup failed, HTTP {} {}", r.status, r.error);
        }

        if (r.ok()) {
            nlohmann::json data = nlohmann::json::parse(r.body, nullptr, false);

            if (!data.is_discarded() && !data.contains("error")) {
                std::string album;

                if (data.contains("track") && data["track"].is_object() &&
                    data["track"].contains("album") && data["track"]["album"].is_object()) {
                    album = data["track"]["album"].value("title", std::string());
                }

                if (!album.empty()) {
                    spdlog::debug("METADATA: '{}' resolved to album '{}' via last.fm", title, album);
                    return {{"status", "ok"}, {"song", {{"album", album}}}, {"source", "lastfm"}};
                }
            }
        }
    }

    std::string url = "https://itunes.apple.com/search"
                      "?entity=song&country=US&limit=10"
                      "&term=" + Http::urlEncode(title + " " + artist);

    Http::Response r = http.get(url, {agent});

    if (!r.ok()) {
        std::string why = !r.error.empty()
            ? r.error
            : "iTunes returned HTTP " + std::to_string(r.status);

        return {{"status", "error"}, {"message", why}};
    }

    nlohmann::json data = nlohmann::json::parse(r.body, nullptr, false);

    if (data.is_discarded() || !data.contains("results") || !data["results"].is_array() ||
        data["results"].empty()) {
        return {{"status", "error"}, {"message", "Song not found"}};
    }

    std::string album = data["results"][0].value("collectionName", std::string());

    if (album.empty()) {
        return {{"status", "error"}, {"message", "No valid album found"}};
    }

    spdlog::debug("METADATA: '{}' resolved to album '{}' via iTunes", title, album);

    return {{"status", "ok"}, {"song", {{"album", album}}}, {"source", "itunes"}};
}

}
