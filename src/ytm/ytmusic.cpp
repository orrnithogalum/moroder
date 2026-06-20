#include "../../include/ytm/ytmusic.hpp"

#include "../../include/ytm/parsers.hpp"
#include "../../include/ytm/sha1.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <ctime>

namespace ytm {

using namespace ytm::path;

namespace {

const std::string YTM_DOMAIN     = "https://music.youtube.com";
const std::string YTM_BASE_API   = YTM_DOMAIN + "/youtubei/v1/";
const std::string YTM_PARAMS     = "?alt=json";
const std::string YTM_PARAMS_KEY = "&key=AIzaSyC9XL3ZjWddXya6X74dJoCTL-WEYFDNX30";
const std::string USER_AGENT     ="Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:88.0) Gecko/20100101 Firefox/88.0";

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

/* clientVersion
- Is a date stamp
- ytmusicapi regenerates it per process
*/
std::string clientVersion() {
    std::time_t now = std::time(nullptr);
    std::tm     utc{};
    gmtime_r(&now, &utc);

    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y%m%d", &utc);

    return std::string("1.") + buf + ".01.00";
}

/* authorizationHeader
- Reverse-engineered SAPISIDHASH scheme Google uses for cookie-authed calls.
*/
std::string authorizationHeader(const std::string& sapisid, const std::string& origin) {
    std::string ts = std::to_string(static_cast<long long>(std::time(nullptr)));
    return "SAPISIDHASH " + ts + "_" + sha1Hex(ts + " " + sapisid + " " + origin);
}

/* cookieValue
- Pulls a named cookie out of a raw Cookie header.
*/
std::string cookieValue(const std::string& raw, const std::string& name) {
    size_t pos = 0;

    while ((pos = raw.find(name + "=", pos)) != std::string::npos) {
        // Must be at the start or preceded by a separator, or PAPISID would
        // match inside __Secure-3PAPISID.
        if (pos != 0) {
            char prev = raw[pos - 1];
            if (prev != ' ' && prev != ';') {
                pos += name.size();
                continue;
            }
        }

        size_t start = pos + name.size() + 1;
        size_t end   = raw.find(';', start);
        std::string value = raw.substr(start, end == std::string::npos ? end : end - start);

        // Strip quotes and whitespace.
        value.erase(std::remove(value.begin(), value.end(), '"'), value.end());
        size_t b = value.find_first_not_of(" \t");
        size_t e = value.find_last_not_of(" \t");
        if (b == std::string::npos) return "";

        return value.substr(b, e - b + 1);
    }

    return "";
}

std::string continuationString(const std::string& ctoken) {
    return "&ctoken=" + ctoken + "&continuation=" + ctoken;
}

// continuations.py get_continuation_token
std::string continuationToken(const json& items) {
    if (!items.is_array() || items.empty()) return "";

    const json& last = items.back();

    std::string token = str(nav(last, CONTINUATION_TOKEN));
    if (!token.empty()) return token;

    const json* commands = nav(last, COMMAND_EXECUTOR_COMMANDS);
    if (commands && commands->is_array()) {
        for (const json& c : *commands) {
            if (str(nav(c, {"continuationCommand", "request"})) == "CONTINUATION_REQUEST_TYPE_BROWSE") {
                return str(nav(c, {"continuationCommand", "token"}));
            }
        }
    }

    return "";
}

/* libraryContents
- library.py get_library_contents.
- "renderer" is GRID for the tiled pages (playlists, albums, podcasts) or
  MUSIC_SHELF for the list pages (songs, artists).
*/
const json* libraryContents(const json& response, const Path& renderer) {
    const json* section = nav(response, SINGLE_COLUMN_TAB + SECTION_LIST);

    if (!section) {
        // Empty library, or a non-premium account with no downloads tab.
        const json* tabs = nav(response, SINGLE_COLUMN + Path{"tabs"});
        size_t n = (tabs && tabs->is_array()) ? tabs->size() : 0;

        const Path& libraryTab = (n < 3) ? TAB_1_CONTENT : TAB_2_CONTENT;
        return nav(response, SINGLE_COLUMN + libraryTab + SECTION_LIST_ITEM + renderer);
    }

    const json* results = findObjectByKey(section, "itemSectionRenderer");
    if (!results) return nav(response, SINGLE_COLUMN_TAB + SECTION_LIST_ITEM + renderer);

    return nav(results, ITEM_SECTION + renderer);
}

/* kindName
- Logging only, so an unknown kind degrades to a label rather than breaking
  the build when the enum grows
*/
const char* kindName(Error::Kind kind) {
    switch (kind) {
        case Error::Kind::Network:   return "network";
        case Error::Kind::Auth:      return "auth";
        case Error::Kind::RateLimit: return "rate limit";
        case Error::Kind::Server:    return "server";
        case Error::Kind::Request:   return "request";
        case Error::Kind::Parse:     return "parse";
        case Error::Kind::NotFound:  return "not found";
        case Error::Kind::Cancelled: return "cancelled";
        default:                     return "unknown";
    }
}

}



void YTMusic::setError(Error::Kind kind, std::string detail) {
    last_error.kind = kind;
    last_error.detail = std::move(detail);

    /* Every failure in this class goes through here, so this one line covers
    auth, transport, HTTP status and parse errors alike. A cancel is an ordinary
    outcome rather than a fault, so it stays at info.
    */
    if (kind == Error::Kind::Cancelled) {
        spdlog::info("YTM: cancelled, {}", last_error.detail);
    } else {
        spdlog::warn("YTM: {} error, {}", kindName(kind), last_error.detail);
    }
}

YTMusic::YTMusic(const std::filesystem::path& browserJson, std::string lang, std::string loc)
    : language(std::move(lang)), location(std::move(loc)) {

    params = YTM_PARAMS;

    if (!browserJson.empty() && std::filesystem::exists(browserJson)) {
        loadAuth(browserJson);
    }

    if (authenticated) params += YTM_PARAMS_KEY;
}

void YTMusic::loadAuth(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        setError(Error::Kind::Auth, "could not open " + path.string());
        return;
    }

    json raw;
    try {
        in >> raw;
    } catch (const std::exception& e) {
        setError(Error::Kind::Auth, std::string("invalid auth JSON: ") + e.what());
        return;
    }

    if (!raw.is_object()) {
        setError(Error::Kind::Auth, "auth file is not a header object");
        return;
    }

    for (auto& [k, v] : raw.items()) {
        if (v.is_string()) auth_headers[lower(k)] = v;
    }

    std::string cookie = auth_headers.value("cookie", std::string());
    if (cookie.empty()) {
        setError(Error::Kind::Auth, "auth file has no cookie header");
        auth_headers = json::object();
        return;
    }

    sapisid = cookieValue(cookie, "__Secure-3PAPISID");
    if (sapisid.empty()) sapisid = cookieValue(cookie, "SAPISID");

    if (sapisid.empty()) {
        setError(Error::Kind::Auth, "cookie has no SAPISID");
        auth_headers = json::object();
        return;
    }

    origin = auth_headers.value("origin", auth_headers.value("x-origin", YTM_DOMAIN));

    // If the exported headers already carry a visitor id, reuse it rather than
    // scraping the homepage for a fresh one.
    visitor_id = auth_headers.value("x-goog-visitor-id", std::string());

    authenticated = true;

    spdlog::info("YTM: auth loaded from {}, visitor id {}", path.string(), visitor_id.empty() ? "not in headers" : "reused from headers");
}

void YTMusic::ensureVisitorId() {
    if (!visitor_id.empty()) return;

    Http::Response r = http.get(YTM_DOMAIN, {"user-agent: " + USER_AGENT, "accept: */*"});
    if (!r.ok()) {
        spdlog::warn("YTM: visitor id scrape failed, HTTP {} {}", r.status, r.error);
        return;
    }

    // The page embeds ytcfg as one long line; pulling the field out directly is
    // more robust than brace-matching a megabyte of JS.
    static const std::string needle = "\"VISITOR_DATA\":\"";

    size_t pos = r.body.find(needle);
    if (pos == std::string::npos) {
        spdlog::warn("YTM: visitor id not found in homepage, the page layout may have changed");
        return;
    }

    size_t start = pos + needle.size();
    size_t end = r.body.find('"', start);
    if (end == std::string::npos) {
        spdlog::warn("YTM: visitor id field was truncated");
        return;
    }

    visitor_id = r.body.substr(start, end - start);

    spdlog::info("YTM: visitor id acquired");
}

std::vector<std::string> YTMusic::buildHeaders() {
    ensureVisitorId();

    std::vector<std::string> out;
    std::vector<std::string> emitted;

    /* Just to be extra safe:
    - Even if the skip list accidentally fails to exclude something, we have another safety check that makes sure we never send the same header name twice.
    - YouTube returns a 400 error if we do.
    */
    auto add = [&](const std::string& name, const std::string& value) {
        if (value.empty()) return;

        std::string key = lower(name);
        if (std::find(emitted.begin(), emitted.end(), key) != emitted.end()) return;

        emitted.push_back(key);
        out.push_back(name + ": " + value);
    };

    if (authenticated) {
        add("authorization", authorizationHeader(sapisid, origin));
        add("X-Origin", origin);
    }

    add("X-Goog-Visitor-Id", visitor_id);

    /* Curl manages accept-encoding itself (it also has to decode the response), and content-length/host are transport-level.
    - Content-encoding is dropped because ytmusicapi sends "gzip" without actually gzipping the body,
      YouTube ignores it, but there's no reason to repeat the lie.
    */
    static const std::vector<std::string> skip = {
        "accept-encoding", "content-encoding", "content-length", "host",
    };

    for (auto& [k, v] : auth_headers.items()) {
        if (std::find(skip.begin(), skip.end(), k) != skip.end()) continue;
        if (!v.is_string()) continue;
        add(k, v.get<std::string>());
    }

    add("user-agent", USER_AGENT);
    add("accept", "*/*");
    add("content-type", "application/json");
    add("origin", YTM_DOMAIN);

    return out;
}

std::vector<std::string> YTMusic::debugHeaderNames() {
    std::vector<std::string> names;

    for (const std::string& h : buildHeaders()) {
        names.push_back(h.substr(0, h.find(':')));
    }

    return names;
}



json YTMusic::sendRequest(const std::string& endpoint, const json& body, const std::string& additionalParams) {
    std::lock_guard lock(mtx);

    json client = json::object();
    client["clientName"] = "WEB_REMIX";
    client["clientVersion"] = clientVersion();
    if (!language.empty()) client["hl"] = language;
    if (!location.empty()) client["gl"] = location;

    json payload = body;
    payload["context"] = {{"client", client}, {"user", json::object()}};

    std::string url = YTM_BASE_API + endpoint + params + additionalParams;

    spdlog::debug("YTM: {} request ({})", endpoint, authenticated ? "authenticated" : "anonymous");

    Http::Response r = http.post(url, payload.dump(), buildHeaders());

    if (!r.error.empty()) {
        setError(r.error == "aborted" ? Error::Kind::Cancelled : Error::Kind::Network, "transport: " + r.error);
        return json::object();
    }

    if (r.status < 200 || r.status >= 300) {
        std::string detail = "HTTP " + std::to_string(r.status) + " from " + endpoint;

        json err = json::parse(r.body, nullptr, false);
        if (!err.is_discarded()) {
            std::string message = str(nav(err, {"error", "message"}));
            if (!message.empty()) detail += ": " + message;
        }

        Error::Kind kind;

        if (r.status == 401 || r.status == 403)  kind = Error::Kind::Auth;
        else if (r.status == 404)                kind = Error::Kind::NotFound;
        else if (r.status == 429)                kind = Error::Kind::RateLimit;
        else if (r.status >= 500)                kind = Error::Kind::Server;
        else                                     kind = Error::Kind::Request;

        setError(kind, detail);

        return json::object();
    }

    json parsed = json::parse(r.body, nullptr, false);
    if (parsed.is_discarded()) {
        setError(Error::Kind::Parse, "invalid JSON from " + endpoint);
        return json::object();
    }

    last_error = Error{};

    spdlog::debug("YTM: {} ok, {} bytes", endpoint, r.body.size());

    return parsed;
}



json YTMusic::getContinuations(json results, const std::string& continuationType, int limit, const std::string& endpoint, const json& body, const std::function<json(const json&)>& parseFunc) {
    json items = json::array();

    while (results.contains("continuations") && (limit < 0 || static_cast<int>(items.size()) < limit)) {
        std::string ctoken = str(nav(results, {"continuations", 0, "next" + std::string("ContinuationData"), "continuation"}));
        if (ctoken.empty()) break;

        json response = sendRequest(endpoint, body, continuationString(ctoken));

        const json* next = nav(response, {"continuationContents", continuationType});
        if (!next) break;

        results = *next;

        json contents;
        if (results.contains("contents"))   contents = parseFunc(results["contents"]);
        else if (results.contains("items")) contents = parseFunc(results["items"]);
        else                                break;

        if (contents.empty()) break;

        for (json& c : contents) items.push_back(std::move(c));
    }

    return items;
}

json YTMusic::getContinuations2025(const json& results, int limit, const std::string& endpoint, const json& baseBody, const std::function<json(const json&)>& parseFunc) {
    json items = json::array();

    if (!results.contains("contents")) return items;

    std::string token = continuationToken(results["contents"]);

    while (!token.empty() && (limit < 0 || static_cast<int>(items.size()) < limit)) {
        json body = baseBody;
        body["continuation"] = token;

        json response = sendRequest(endpoint, body);

        const json* continuationItems = nav(response, CONTINUATION_ITEMS);
        if (!continuationItems) break;

        json contents = parseFunc(*continuationItems);
        if (contents.empty()) break;

        for (json& c : contents) items.push_back(std::move(c));

        token = continuationToken(*continuationItems);
    }

    return items;
}



json YTMusic::search(const std::string& query, int limit, const std::string& filter) {
    json results = json::array();

    json body = json::object();
    body["query"] = query;

    std::string p = parse::searchParams(filter, false);
    if (!p.empty()) body["params"] = p;

    json response = sendRequest("search", body);
    if (!response.contains("contents")) return results;

    const json* content = nullptr;
    if (response["contents"].contains("tabbedSearchResultsRenderer")) {
        content = nav(response, {"contents", "tabbedSearchResultsRenderer", "tabs", 0, "tabRenderer", "content"});
    } else {
        content = &response["contents"];
    }

    const json* sectionList = nav(content, SECTION_LIST);
    if (!sectionList || !sectionList->is_array()) return results;

    // A single itemSectionRenderer means "no results".
    if (sectionList->size() == 1 && (*sectionList)[0].contains("itemSectionRenderer")) return results;

    std::string internalFilter = filter;
    if (!internalFilter.empty() && internalFilter.find("playlists") != std::string::npos) {
        internalFilter = "playlists";
    }

    for (const json& res : *sectionList) {
        json category = nullptr;
        json resultType = nullptr;

        const json* shelfContents = nullptr;
        json cardContents;

        if (res.contains("musicCardShelfRenderer")) {
            results.push_back(parse::topResult(res["musicCardShelfRenderer"]));

            const json* raw = nav(res, {"musicCardShelfRenderer", "contents"});
            if (!raw) continue;

            cardContents = *raw;

            // "More from YouTube" isn't parseable; drop it and take its label.
            if (!cardContents.empty() && cardContents[0].contains("messageRenderer")) {
                category = navJson(cardContents[0], Path{"messageRenderer"} + TEXT_RUN_TEXT);
                cardContents.erase(cardContents.begin());
            }

            shelfContents = &cardContents;

        } else if (res.contains("musicShelfRenderer")) {
            shelfContents = nav(res, {"musicShelfRenderer", "contents"});
            category = navJson(&res, MUSIC_SHELF + TITLE_TEXT);

        } else if (res.contains("itemSectionRenderer")) {
            shelfContents = nav(res, {"itemSectionRenderer", "contents"});
            if (!shelfContents || shelfContents->empty() || !(*shelfContents)[0].contains(MRLIR)) continue;

        } else {
            continue;
        }

        if (!shelfContents) continue;

        bool isShelf = res.contains("musicShelfRenderer") || res.contains("itemSectionRenderer");

        if (isShelf && !internalFilter.empty()) {
            // YTM sometimes pads results with a differently-categorised shelf.
            std::string singular = internalFilter.substr(0, internalFilter.size() - 1);
            singular = lower(singular);

            if (category.is_string()) {
                std::string cat = lower(category.get<std::string>());
                if (cat.find(singular) == std::string::npos) continue;
            }

            resultType = singular;
        }

        json parsed = parse::searchResults(*shelfContents, resultType, category);
        for (json& item : parsed) results.push_back(std::move(item));

        if (!internalFilter.empty() && res.contains("musicShelfRenderer")) {
            int remaining = limit - static_cast<int>(results.size());

            json more = getContinuations(
                res["musicShelfRenderer"], "musicShelfContinuation", remaining, "search", body,
                [&](const json& contents) { return parse::searchResults(contents, resultType, category); });

            for (json& item : more) results.push_back(std::move(item));
        }
    }

    // ytmusicapi's limit is a floor per shelf; the Python side truncated after
    // the fact, so do the same here to keep behaviour identical.
    if (limit >= 0 && static_cast<int>(results.size()) > limit) {
        json trimmed = json::array();
        for (int i = 0; i < limit; ++i) trimmed.push_back(results[static_cast<size_t>(i)]);
        return trimmed;
    }

    return results;
}



json YTMusic::getHome(int limit) {
    json body = json::object();
    body["browseId"] = "FEmusic_home";

    json response = sendRequest("browse", body);

    const json* results = nav(response, SINGLE_COLUMN_TAB + SECTION_LIST);
    if (!results) return json::array();

    json home = parse::mixedContent(*results);

    const json* sectionList = nav(response, SINGLE_COLUMN_TAB + SECTION);
    if (sectionList && sectionList->contains("continuations")) {
        int remaining = limit < 0 ? -1 : limit - static_cast<int>(home.size());

        json more = getContinuations(*sectionList, "sectionListContinuation", remaining, "browse", body, [](const json& rows) { return parse::mixedContent(rows); });

        for (json& section : more) home.push_back(std::move(section));
    }

    return home;
}



json YTMusic::getAlbum(const std::string& browseId) {
    if (browseId.empty() || !startsWith(browseId, "MPRE")) {
        setError(Error::Kind::Request, "invalid album browseId, must start with MPRE: " + browseId);
        return json::object();
    }

    json body = json::object();
    body["browseId"] = browseId;

    json response = sendRequest("browse", body);

    json album = parse::albumHeader2024(response);

    const json* shelf = nav(response, TWO_COLUMN_RENDERER + Path{"secondaryContents"} + SECTION_LIST_ITEM + MUSIC_SHELF);

    album["tracks"] = json::array();
    if (shelf && shelf->contains("contents")) {
        album["tracks"] = parse::playlistItems((*shelf)["contents"], true);
    }

    long total = 0;
    for (json& track : album["tracks"]) {
        if (track.contains("duration_seconds") && track["duration_seconds"].is_number()) {
            total += track["duration_seconds"].get<long>();
        }

        track["album"] = album.value("title", json(nullptr));

        bool noArtists = !track.contains("artists") || track["artists"].is_null() || (track["artists"].is_array() && track["artists"].empty());

        if (noArtists && album.contains("artists")) track["artists"] = album["artists"];
    }

    album["duration_seconds"] = total;

    return album;
}



json YTMusic::getPlaylist(const std::string& playlistId, int limit) {
    std::string browseId = startsWith(playlistId, "VL") ? playlistId : "VL" + playlistId;

    json body = json::object();
    body["browseId"] = browseId;

    json response = sendRequest("browse", body);

    json playlist = json::object();
    playlist["tracks"] = json::array();

    const json* sectionList = nav(response, TWO_COLUMN_RENDERER + Path{"secondaryContents"} + SECTION);
    if (!sectionList) return playlist;

    const json* contentData = nav(sectionList, CONTENT + Path{"musicPlaylistShelfRenderer"});
    const json* headerData = nav(response, TWO_COLUMN_RENDERER + TAB_CONTENT + SECTION_LIST_ITEM);

    if (headerData) {
        bool owned = headerData->contains(EDITABLE_PLAYLIST_DETAIL_HEADER[0].key());
        playlist["owned"] = owned;

        const json* header = nullptr;

        if (owned) {
            playlist["id"] = navJson(headerData, EDITABLE_PLAYLIST_DETAIL_HEADER + PLAYLIST_ID);
            playlist["privacy"] = navJson(headerData, EDITABLE_PLAYLIST_DETAIL_HEADER + Path{"editHeader", "musicPlaylistEditHeaderRenderer", "privacy"});
            header = nav(headerData, EDITABLE_PLAYLIST_DETAIL_HEADER + HEADER + RESPONSIVE_HEADER);

        } else {
            header = nav(headerData, RESPONSIVE_HEADER);
            playlist["id"] = navJson(header, Path{"buttons", 1, "musicPlayButtonRenderer", "playNavigationEndpoint"} + WATCH_PLAYLIST_ID);
            playlist["privacy"] = "PUBLIC";
        }

        if (header) {
            const json* descShelf = nav(header, Path{"description"} + DESCRIPTION_SHELF);
            playlist["description"] = descShelf
                ? navJson(descShelf, Path{"description"} + RUN_TEXT)
                : json(nullptr);

            playlist.update(parse::playlistHeaderMeta(*header));

            const json* subtitleRuns = nav(header, SUBTITLE_RUNS);
            if (subtitleRuns && subtitleRuns->is_array()) {
                size_t skip = 2 + (owned ? 2 : 0);
                json   sliced = json::array();
                for (size_t i = skip; i < subtitleRuns->size(); ++i) sliced.push_back((*subtitleRuns)[i]);
                playlist.update(parse::songRuns(sliced));
            }
        }

    } else {
        // Auto-generated album playlists (OLAK...) have no header section.
        playlist["owned"] = false;
        playlist["privacy"] = "PUBLIC";
        playlist["id"] = navJson(contentData, {"targetId"});
    }

    if (contentData && contentData->contains("contents")) {
        playlist["tracks"] = parse::playlistItems((*contentData)["contents"]);

        json more = getContinuations2025(*contentData, limit, "browse", body, [](const json& contents) { return parse::playlistItems(contents); });

        for (json& t : more) playlist["tracks"].push_back(std::move(t));
    }

    if (!playlist.contains("title") || playlist["title"].is_null()) {
        // parse_audio_playlist takes the title from the first track's album.
        if (!playlist["tracks"].empty()) {
            playlist["title"] = navJson(playlist["tracks"][0], {"album", "name"});
        }
    }

    long total = 0;
    for (const json& track : playlist["tracks"]) {
        if (track.contains("duration_seconds") && track["duration_seconds"].is_number()) {
            total += track["duration_seconds"].get<long>();
        }
    }
    playlist["duration_seconds"] = total;

    return playlist;
}



json YTMusic::getLibraryPlaylists(int limit) {
    if (!authenticated) {
        setError(Error::Kind::Auth, "not authenticated");
        return json::array();
    }

    json body = json::object();
    body["browseId"] = "FEmusic_liked_playlists";

    json response = sendRequest("browse", body);

    const json* results = libraryContents(response, GRID);
    if (!results || !results->contains("items")) return json::array();

    const json& items = (*results)["items"];

    json rest = json::array();
    for (size_t i = 1; i < items.size(); ++i) rest.push_back(items[i]);

    json playlists = parse::contentList(rest, [](const json& d) { return parse::playlist(d); });

    if (results->contains("continuations")) {
        int remaining = limit < 0 ? -1 : limit - static_cast<int>(playlists.size());

        json more = getContinuations(*results, "gridContinuation", remaining, "browse", body,
                                     [](const json& contents) {
                                         return parse::contentList(
                                             contents, [](const json& d) { return parse::playlist(d); });
                                     });

        for (json& p : more) playlists.push_back(std::move(p));
    }

    return playlists;
}



json YTMusic::getLibraryAlbums(int limit) {
    if (!authenticated) {
        setError(Error::Kind::Auth, "not authenticated");
        return json::array();
    }

    json body = json::object();
    body["browseId"] = "FEmusic_liked_albums";

    json response = sendRequest("browse", body);

    const json* results = libraryContents(response, GRID);
    if (!results || !results->contains("items")) return json::array();

    // Albums have no "add" tile to skip, unlike playlists and podcasts.
    json albums = parse::libraryAlbums((*results)["items"]);

    if (results->contains("continuations")) {
        int remaining = limit < 0 ? -1 : limit - static_cast<int>(albums.size());

        json more = getContinuations(*results, "gridContinuation", remaining, "browse", body, [](const json& contents) { return parse::libraryAlbums(contents); });

        for (json& a : more) albums.push_back(std::move(a));
    }

    return albums;
}

json YTMusic::getLibrarySongs(int limit) {
    if (!authenticated) {
        setError(Error::Kind::Auth, "not authenticated");
        return json::array();
    }

    json body = json::object();
    body["browseId"] = "FEmusic_liked_videos";

    json response = sendRequest("browse", body);

    const json* results = libraryContents(response, MUSIC_SHELF);
    if (!results || !results->contains("contents")) return json::array();

    json contents = (*results)["contents"];

    // pop_songs_random_mix: a shuffle tile sometimes leads the list.
    if (contents.is_array() && contents.size() >= 2) contents.erase(contents.begin());

    json songs = parse::playlistItems(contents);

    if (results->contains("continuations")) {
        int remaining = limit < 0 ? -1 : limit - static_cast<int>(songs.size());

        json more = getContinuations(*results, "musicShelfContinuation", remaining, "browse", body,
                                     [](const json& c) { return parse::playlistItems(c); });

        for (json& song : more) songs.push_back(std::move(song));
    }

    return songs;
}

json YTMusic::getLibraryArtists(int limit) {
    if (!authenticated) {
        setError(Error::Kind::Auth, "not authenticated");
        return json::array();
    }

    json body = json::object();
    body["browseId"] = "FEmusic_library_corpus_track_artists";

    json response = sendRequest("browse", body);

    const json* results = libraryContents(response, MUSIC_SHELF);
    if (!results || !results->contains("contents")) return json::array();

    json artists = parse::libraryArtists((*results)["contents"]);

    if (results->contains("continuations")) {
        int remaining = limit < 0 ? -1 : limit - static_cast<int>(artists.size());

        json more = getContinuations(*results, "musicShelfContinuation", remaining, "browse", body, [](const json& c) { return parse::libraryArtists(c); });

        for (json& a : more) artists.push_back(std::move(a));
    }

    return artists;
}

json YTMusic::getLibraryPodcasts(int limit) {
    if (!authenticated) {
        setError(Error::Kind::Auth, "not authenticated");
        return json::array();
    }

    json body = json::object();
    body["browseId"] = "FEmusic_library_non_music_audio_list";

    json response = sendRequest("browse", body);

    const json* results = libraryContents(response, GRID);
    if (!results || !results->contains("items")) return json::array();

    const json& items = (*results)["items"];

    // The first tile is "Add podcast", same as the playlists page.
    json rest = json::array();
    for (size_t i = 1; i < items.size(); ++i) rest.push_back(items[i]);

    auto parsePodcasts = [](const json& contents) {
        return parse::contentList(contents, [](const json& d) { return parse::podcast(d); });
    };

    json podcasts = parsePodcasts(rest);

    if (results->contains("continuations")) {
        int remaining = limit < 0 ? -1 : limit - static_cast<int>(podcasts.size());

        json more = getContinuations(*results, "gridContinuation", remaining, "browse", body, parsePodcasts);

        for (json& p : more) podcasts.push_back(std::move(p));
    }

    return podcasts;
}



namespace {

// Builds the "next" request body. Shared by the initial call and continuations
// so the two can't drift apart the way the Python session cache allowed.
json watchBody(const std::string& videoId, std::string playlistId) {
    json body = json::object();

    body["enablePersistentPlaylistPanel"] = true;
    body["isAudioOnly"] = true;
    body["tunerSettingValue"] = "AUTOMIX_SETTING_NORMAL";

    if (!videoId.empty()) {
        body["videoId"] = videoId;

        if (playlistId.empty()) playlistId = "RDAMVM" + videoId;

        body["watchEndpointMusicSupportedConfigs"] = {
            {"watchEndpointMusicConfig",
             {{"hasPersistentPlaylistPanel", true}, {"musicVideoType", "MUSIC_VIDEO_TYPE_ATV"}}}};

        body["params"] = "wAEB";
    }

    if (!playlistId.empty()) {
        body["playlistId"] = parse::validatePlaylistId(playlistId);
    }

    return body;
}

}

WatchPlaylist YTMusic::getWatchPlaylist(const std::string& videoId, const std::string& playlistId, int limit) {
    WatchPlaylist out;

    json body = watchBody(videoId, playlistId);
    json response = sendRequest("next", body);

    const json* watchNext = nav(response, {"contents", "singleColumnMusicWatchNextResultsRenderer", "tabbedRenderer", "watchNextTabbedResultsRenderer"});
    const json* results = nav(watchNext, TAB_CONTENT + Path{"musicQueueRenderer", "content", "playlistPanelRenderer"});

    if (!results || !results->contains("contents")) {
        if (last_error.ok()) {
            std::string detail = "no radio content returned";
            if (!playlistId.empty()) detail += " for " + playlistId + " (private playlist?)";

            setError(Error::Kind::Parse, detail);
        }
        return out;
    }

    const json& contents = (*results)["contents"];

    out.tracks = parse::watchPlaylistTracks(contents);

    // The seed track is the song itself; the caller already has it queued.
    if (!videoId.empty() && !out.tracks.empty()) {
        out.tracks.erase(out.tracks.begin());
    }

    if (limit >= 0 && static_cast<int>(out.tracks.size()) > limit) {
        out.tracks.erase(out.tracks.begin() + limit, out.tracks.end());
    }

    // Playlist radios key their continuation differently from song radios.
    if (results->contains("continuations")) {
        std::string pid = body.value("playlistId", std::string());
        bool isSaved = startsWith(pid, "PL") || startsWith(pid, "OLA");

        const char* key = isSaved ? "nextContinuationData" : "nextRadioContinuationData";

        out.continuation = str(nav(*results, {"continuations", 0, key, "continuation"}));

        if (out.continuation.empty()) {
            const char* other = isSaved ? "nextRadioContinuationData" : "nextContinuationData";
            out.continuation = str(nav(*results, {"continuations", 0, other, "continuation"}));
        }
    }

    if (!videoId.empty()) {
        out.seedId = videoId;
    } else {
        // The panel reports the playlist it actually resolved to.
        for (const json& item : contents) {
            std::string pid = str(nav(item, Path{"playlistPanelVideoRenderer"} + NAVIGATION_PLAYLIST_ID));
            if (!pid.empty()) {
                out.seedId = pid;
                break;
            }
        }

        if (out.seedId.empty()) out.seedId = playlistId;
    }

    return out;
}

WatchPlaylist YTMusic::getWatchPlaylistNext(const std::string& videoId, const std::string& playlistId, const std::string& ctoken, int limit) {
    WatchPlaylist out;

    if (ctoken.empty()) return out;

    json body = watchBody(videoId, playlistId);
    json response = sendRequest("next", body, continuationString(ctoken));

    const json* results = nav(response, {"continuationContents", "playlistPanelContinuation"});
    if (!results) {
        out.seedId = videoId.empty() ? playlistId : videoId;
        return out;
    }

    const json* contents = results->contains("contents") ? &(*results)["contents"] : nav(*results, {"items"});

    if (contents) out.tracks = parse::watchPlaylistTracks(*contents);

    if (limit >= 0 && static_cast<int>(out.tracks.size()) > limit) {
        out.tracks.erase(out.tracks.begin() + limit, out.tracks.end());
    }

    for (const char* key : {"nextRadioContinuationData", "nextContinuationData"}) {
        std::string token = str(nav(*results, {"continuations", 0, key, "continuation"}));
        if (!token.empty()) {
            out.continuation = token;
            break;
        }
    }

    out.seedId = videoId.empty() ? playlistId : videoId;

    return out;
}

}
