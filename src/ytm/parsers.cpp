#include "../../include/ytm/parsers.hpp"

#include <algorithm>
#include <cstdint>
#include <regex>
#include <string>

namespace ytm::parse {

using namespace ytm::path;

namespace {

// The literal separator run YouTube uses between metadata fields: " \u2022 ".
const std::string DOT_SEPARATOR = " \xE2\x80\xA2 ";

bool isDotSeparatorRun(const json& run) {
    return run.is_object() && run.size() == 1 &&
           run.contains("text") && run["text"] == DOT_SEPARATOR;
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool isAsciiDigit(char c) { return c >= '0' && c <= '9'; }

bool decodeUtf8(const std::string& s, size_t pos, uint32_t& cp, size_t& len) {
    if (pos >= s.size()) return false;

    unsigned char c = static_cast<unsigned char>(s[pos]);

    if (c < 0x80)        { cp = c;         len = 1; }
    else if (c < 0xE0)   { cp = c & 0x1F;  len = 2; }
    else if (c < 0xF0)   { cp = c & 0x0F;  len = 3; }
    else                 { cp = c & 0x07;  len = 4; }

    if (pos + len > s.size()) return false;

    for (size_t i = 1; i < len; ++i) {
        cp = (cp << 6) | (static_cast<unsigned char>(s[pos + i]) & 0x3F);
    }

    return true;
}

// The character class ytmusicapi strips before a localised view count:
// whitespace, ':', fullwidth colon, and the bidi marks.
bool isViewSeparator(uint32_t cp) {
    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' || cp == '\v') return true;
    if (cp == ':' || cp == 0x00A0 || cp == 0xFF1A) return true;
    if (cp == 0x200E || cp == 0x200F) return true;
    if (cp >= 0x202A && cp <= 0x202E) return true;
    return false;
}

const std::regex& durationRe() {
    static const std::regex re(R"(^(\d+:)*\d+:\d+$)");
    return re;
}

const std::regex& yearRe() {
    static const std::regex re(R"(^\d{4}$)");
    return re;
}

struct SongRun {
    std::string type;
    json        data;
};

SongRun classifySongRun(const json& run) {
    std::string text = run.is_object() && run.contains("text") && run["text"].is_string()
        ? run["text"].get<std::string>()
        : std::string();

    if (run.is_object() && run.contains("navigationEndpoint")) {
        std::string id = str(nav(run, NAVIGATION_BROWSE_ID));

        json item = json::object();
        item["name"] = text;
        item["id"] = id.empty() ? json(nullptr) : json(id);

        bool isAlbum = !id.empty() && (startsWith(id, "MPRE") || id.find("release_detail") != std::string::npos);

        return {isAlbum ? "album" : "artist", item};
    }

    if (std::regex_match(text, durationRe())) return {"duration", json(text)};
    if (std::regex_match(text, yearRe()))     return {"year", json(text)};

    json v = views(text);
    if (!v.is_null()) return {"views", v};

    json artist = json::object();
    artist["name"] = text;
    artist["id"] = nullptr;

    return {"artist", artist};
}

std::string joinRuns(const json* runs) {
    std::string out;
    if (!runs || !runs->is_array()) return out;

    for (const json& r : *runs) {
        if (r.is_object() && r.contains("text") && r["text"].is_string()) {
            out += r["text"].get<std::string>();
        }
    }

    return out;
}

json descriptionRuns(const json* runsList, std::string& descriptionOut) {
    json out = json::array();
    descriptionOut.clear();

    if (!runsList || !runsList->is_array()) return out;

    for (const json& run : *runsList) {
        std::string text = run.value("text", std::string());
        descriptionOut += text;

        json entry = json::object();
        entry["text"] = text;

        const json* link = nav(run, {"navigationEndpoint", "urlEndpoint", "url"});
        if (link && link->is_string()) entry["url"] = *link;

        out.push_back(entry);
    }

    return out;
}

}



const json* flexColumnItem(const json& item, int index) {
    const json* cols = nav(item, {"flexColumns"});
    if (!cols || !cols->is_array() || static_cast<int>(cols->size()) <= index || index < 0) {
        return nullptr;
    }

    const json* col = nav(*cols, {index, "musicResponsiveListItemFlexColumnRenderer"});
    if (!col) return nullptr;

    if (!navHas(col, {"text"}) || !navHas(col, {"text", "runs"})) return nullptr;

    return col;
}

const json* fixedColumnItem(const json& item, int index) {
    const json* col = nav(item, {"fixedColumns", index, "musicResponsiveListItemFixedColumnRenderer"});
    if (!col) return nullptr;

    // ytmusicapi requires both keys here, which makes its own simpleText branch
    // unreachable. Matching it exactly keeps behaviour identical.
    if (!navHas(col, {"text"}) || !navHas(col, {"text", "runs"})) return nullptr;

    return col;
}

json itemText(const json& item, int index, int runIndex, bool noneIfAbsent) {
    const json* column = flexColumnItem(item, index);
    if (!column) return json(nullptr);

    const json* runs = nav(column, {"text", "runs"});
    if (!runs || !runs->is_array()) return json(nullptr);

    if (noneIfAbsent && static_cast<int>(runs->size()) < runIndex + 1) return json(nullptr);

    const json* t = nav(*runs, {runIndex, "text"});
    return t ? *t : json(nullptr);
}

json idName(const json* subRun) {
    json out = json::object();
    out["id"] = navJson(subRun, NAVIGATION_BROWSE_ID);
    out["name"] = navJson(subRun, {"text"});
    return out;
}

int parseDuration(const std::string& duration) {
    std::string d = duration;

    // trim
    size_t b = d.find_first_not_of(" \t\n\r");
    if (b == std::string::npos) return -1;
    size_t e = d.find_last_not_of(" \t\n\r");
    d = d.substr(b, e - b + 1);

    if (d.empty()) return -1;

    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        size_t pos = d.find(':', start);
        parts.push_back(d.substr(start, pos == std::string::npos ? pos : pos - start));
        if (pos == std::string::npos) break;
        start = pos + 1;
    }

    for (const std::string& p : parts) {
        if (p.empty()) return -1;
        for (char c : p) {
            if (!isAsciiDigit(c)) return -1;
        }
    }

    static const int multipliers[] = {1, 60, 3600};

    int seconds = 0;
    for (size_t i = 0; i < parts.size() && i < 3; ++i) {
        seconds += multipliers[i] * std::stoi(parts[parts.size() - 1 - i]);
    }

    return seconds;
}

long toInt(const std::string& text) {
    std::string digits;
    for (char c : text) {
        if (isAsciiDigit(c)) digits += c;
    }

    if (digits.empty()) return -1;

    try {
        return std::stol(digits);
    } catch (...) {
        return -1;
    }
}

void menuPlaylists(const json& data, json& result) {
    const json* menuItems = nav(data, MENU_ITEMS);
    if (!menuItems || !menuItems->is_array()) return;

    for (const json* wrapper : findObjectsByKey(menuItems, MNIR)) {
        const json& item = (*wrapper)[MNIR];

        std::string icon = str(nav(item, ICON_TYPE));

        std::string watchKey;
        if (icon == "MUSIC_SHUFFLE") watchKey = "shuffleId";
        else if (icon == "MIX") watchKey = "radioId";
        else continue;

        std::string watchId = str(nav(item, {"navigationEndpoint", "watchPlaylistEndpoint", "playlistId"}));
        if (watchId.empty()) {
            watchId = str(nav(item, {"navigationEndpoint", "watchEndpoint", "playlistId"}));
        }

        if (!watchId.empty()) result[watchKey] = watchId;
    }
}



json artistsRuns(const json& runs) {
    json artists = json::array();
    if (!runs.is_array()) return artists;

    // Skips every other run: the odd ones are " • " separators.
    for (size_t j = 0; j < runs.size() / 2 + 1; ++j) {
        size_t idx = j * 2;
        if (idx >= runs.size()) break;

        json a = json::object();
        a["name"] = runs[idx].value("text", std::string());
        a["id"] = navJson(runs[idx], NAVIGATION_BROWSE_ID);

        artists.push_back(a);
    }

    return artists;
}



json views(const std::string& input) {
    std::string text = input;
    bool prefixed = false;

    bool hasLatin = std::any_of(input.begin(), input.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    });

    // Only for non-latin scripts: "Maroon 5" is indistinguishable from a
    // prefixed count otherwise.
    if (!hasLatin) {
        size_t i = 0;
        while (i < text.size()) {
            uint32_t cp = 0;
            size_t   len = 0;
            if (!decodeUtf8(text, i, cp, len)) break;

            if (cp >= '0' && cp <= '9') break;  // a digit before any separator: no prefix

            if (isViewSeparator(cp)) {
                text = text.substr(i + len);
                prefixed = true;
                break;
            }

            i += len;
        }
    }

    if (text.empty() || !isAsciiDigit(text[0])) return json(nullptr);

    bool ascii = std::none_of(text.begin(), text.end(), [](char c) {
        return static_cast<unsigned char>(c) > 127;
    });

    // A bare ASCII token like "2Pac" is an artist, not a stripped view count.
    if (!prefixed && ascii && text.find(' ') == std::string::npos) return json(nullptr);

    std::string head = text.substr(0, text.find(' '));

    // \xa0 glues number and magnitude in some locales; keep at most two parts.
    static const std::string NBSP = "\xC2\xA0";

    std::vector<std::string> parts;
    size_t                   start = 0;
    while (parts.size() < 2) {
        size_t pos = head.find(NBSP, start);
        if (pos == std::string::npos) {
            parts.push_back(head.substr(start));
            break;
        }
        parts.push_back(head.substr(start, pos - start));
        start = pos + NBSP.size();
    }

    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += NBSP;
        out += parts[i];
    }

    return json(out);
}

json songRuns(const json& runs, bool skipTypeSpec) {
    json parsed = json::object();
    if (!runs.is_array()) return parsed;

    size_t start = 0;

    // Stop the leading type specifier ("Song • Eminem") being read as an artist.
    if (skipTypeSpec && runs.size() > 2 && !(runs[0].is_object() && runs[0].contains("navigationEndpoint")) && isDotSeparatorRun(runs[1])) {
        if (classifySongRun(runs[0]).type == "artist") {
            std::string t2 = classifySongRun(runs[2]).type;
            if (t2 == "artist" || t2 == "duration" || t2 == "views" || t2 == "year") {
                start = 2;
            }
        }
    }

    for (size_t i = start; i < runs.size(); ++i) {
        if ((i - start) % 2) continue;  // uneven items are always separators

        SongRun r = classifySongRun(runs[i]);

        if (r.type == "album") {
            parsed["album"] = r.data;

        } else if (r.type == "artist") {
            if (!parsed.contains("artists")) parsed["artists"] = json::array();
            parsed["artists"].push_back(r.data);

        } else if (r.type == "views") {
            parsed["views"] = r.data;

        } else if (r.type == "duration") {
            parsed["duration"] = r.data;
            int secs = parseDuration(r.data.get<std::string>());
            parsed["duration_seconds"] = secs < 0 ? json(nullptr) : json(secs);

        } else if (r.type == "year") {
            parsed["year"] = r.data;
        }
    }

    return parsed;
}

json songArtists(const json& data, int index) {
    const json* flex = flexColumnItem(data, index);
    if (!flex) return json::array();

    const json* runs = nav(flex, {"text", "runs"});
    if (!runs) return json::array();

    return artistsRuns(*runs);
}

json songAlbum(const json& data, int index) {
    const json* flex = flexColumnItem(data, index);
    if (!flex) return json(nullptr);

    json out = json::object();
    out["name"] = itemText(data, index);
    out["id"] = navJson(flex, TEXT_RUN + NAVIGATION_BROWSE_ID);

    return out;
}

std::string likeStatus(const json& service) {
    // ytmusicapi flips the value: the endpoint reports what tapping would do.
    std::string status = str(nav(service, {"likeEndpoint", "status"}));
    return status == "LIKE" ? "INDIFFERENT" : "LIKE";
}

json songMenuData(const json& data) {
    json out = json::object();

    if (!data.is_object() || !data.contains("menu")) return out;

    const json* items = nav(data, MENU_ITEMS);
    if (!items || !items->is_array()) return out;

    for (const json& item : *items) {
        const json* menuItem = nav(item, {TOGGLE_MENU});
        if (!menuItem) menuItem = nav(item, {"menuServiceItemRenderer"});
        if (!menuItem) continue;

        if (!out.contains("inLibrary")) out["inLibrary"] = false;
        if (!out.contains("pinnedToListenAgain")) out["pinnedToListenAgain"] = false;

        std::string icon = str(nav(menuItem, {"defaultIcon", "iconType"}));
        if (icon.empty()) icon = str(nav(menuItem, {"icon", "iconType"}));

        auto token = [&](const char* endpoint) -> json {
            return navJson(menuItem, Path{endpoint} + FEEDBACK_TOKEN);
        };

        bool toggled = menuItem->is_object() && menuItem->value("isToggled", false);

        if (icon == "KEEP") {
            out["pinnedToListenAgain"] = toggled;
            out["listenAgainFeedbackTokens"] = {{"pin", token("defaultServiceEndpoint")}, {"unpin", token("toggledServiceEndpoint")}};

        } else if (icon == "KEEP_OFF") {
            out["pinnedToListenAgain"] = true;
            out["listenAgainFeedbackTokens"] = {{"pin", token("toggledServiceEndpoint")}, {"unpin", token("defaultServiceEndpoint")}};

        } else if (icon == "BOOKMARK_BORDER") {
            out["inLibrary"] = toggled;
            out["feedbackTokens"] = {{"add", token("defaultServiceEndpoint")}, {"remove", token("toggledServiceEndpoint")}};

        } else if (icon == "BOOKMARK") {
            out["inLibrary"] = true;
            out["feedbackTokens"] = {{"add", token("toggledServiceEndpoint")}, {"remove", token("defaultServiceEndpoint")}};

        } else if (icon == "REMOVE_FROM_HISTORY") {
            out["feedbackToken"] = token("serviceEndpoint");
        }
    }

    return out;
}



json album(const json& result) {
    json out = json::object();

    out["title"] = navJson(result, TITLE_TEXT);

    json artists = json::array();
    const json* subRuns = nav(result, SUBTITLE_RUNS);
    if (subRuns && subRuns->is_array()) {
        for (const json& x : *subRuns) {
            if (x.is_object() && x.contains("navigationEndpoint")) artists.push_back(idName(&x));
        }
    }
    out["artists"] = artists;

    out["browseId"] = navJson(result, TITLE + NAVIGATION_BROWSE_ID);
    out["audioPlaylistId"] = albumPlaylistIdIfExists(nav(result, THUMBNAIL_OVERLAY_NAVIGATION));
    out["thumbnails"] = navJson(result, THUMBNAIL_RENDERER);
    out["isExplicit"] = navHas(&result, SUBTITLE_BADGE_LABEL);

    // _parse_album_single_subtitle
    std::string typeOrYear = str(nav(result, SUBTITLE));
    if (!typeOrYear.empty()) {
        bool numeric = std::all_of(typeOrYear.begin(), typeOrYear.end(), isAsciiDigit);

        if (numeric) {
            out["year"] = typeOrYear;
        } else {
            out["type"] = typeOrYear;

            std::string year = str(nav(result, SUBTITLE2));
            if (!year.empty() && std::all_of(year.begin(), year.end(), isAsciiDigit)) {
                out["year"] = year;
            }
        }
    }

    return out;
}

json song(const json& result) {
    json out = json::object();

    out["title"] = navJson(result, TITLE_TEXT);
    out["videoId"] = navJson(result, NAVIGATION_VIDEO_ID);
    out["playlistId"] = navJson(result, NAVIGATION_PLAYLIST_ID);
    out["thumbnails"] = navJson(result, THUMBNAIL_RENDERER);

    const json* runs = nav(result, SUBTITLE_RUNS);
    if (runs) out.update(songRuns(*runs, true));

    return out;
}

json songFlat(const json& data, bool withPlaylistId) {
    json out = json::object();

    const json* col0 = flexColumnItem(data, 0);
    const json* col1 = flexColumnItem(data, 1);
    const json* col2 = flexColumnItem(data, 2);

    out["title"] = navJson(col0, TEXT_RUN_TEXT);
    out["videoId"] = navJson(col0, TEXT_RUN + NAVIGATION_VIDEO_ID);
    out["videoType"] = navJson(&data, PLAY_BUTTON + Path{"playNavigationEndpoint"} + NAVIGATION_VIDEO_TYPE);
    out["thumbnails"] = navJson(data, THUMBNAILS);
    out["isExplicit"] = navHas(&data, BADGE_LABEL);

    if (withPlaylistId) {
        out["playlistId"] = navJson(&data, PLAY_BUTTON + Path{"playNavigationEndpoint"} + WATCH_PLAYLIST_ID);
    }

    const json* runs = nav(col1, TEXT_RUNS);
    if (runs) out.update(songRuns(*runs, true));

    if (col2 && navHas(col2, TEXT_RUN + Path{"navigationEndpoint"})) {
        json alb = json::object();
        alb["name"] = navJson(col2, TEXT_RUN_TEXT);
        alb["id"] = navJson(col2, TEXT_RUN + NAVIGATION_BROWSE_ID);
        out["album"] = alb;
    }

    return out;
}

json playlist(const json& data) {
    std::string browseId = str(nav(data, TITLE + NAVIGATION_BROWSE_ID));
    std::string playlistId = browseId.size() > 2 ? browseId.substr(2) : browseId;

    json out = json::object();

    out["title"] = navJson(data, TITLE_TEXT);
    out["playlistId"] = playlistId;
    out["thumbnails"] = navJson(data, THUMBNAIL_RENDERER);

    bool owned = false;
    const json* menuItems = nav(data, MENU_ITEMS);
    if (menuItems && menuItems->is_array()) {
        for (const json& item : *menuItems) {
            std::string editId = str(nav(item, {"menuNavigationItemRenderer", "navigationEndpoint", "playlistEditorEndpoint", "playlistId"}));
            if (!editId.empty() && editId == playlistId) {
                owned = true;
                break;
            }
        }
    }
    out["owned"] = owned;

    const json* subtitle = nav(data, {"subtitle"});
    if (subtitle && subtitle->contains("runs")) {
        const json& runs = (*subtitle)["runs"];
        out["description"] = joinRuns(&runs);

        std::string sub2 = str(nav(data, SUBTITLE2));
        if (runs.size() == 3 && !sub2.empty()) {
            // matches r"\d+ " -- a count followed by a space
            static const std::regex countRe(R"(\d+ )");
            if (std::regex_search(sub2, countRe)) {
                out["count"] = sub2.substr(0, sub2.find(' '));

                json firstRun = json::array({runs[0]});
                out["author"] = artistsRuns(firstRun);
            }
        }
    }

    return out;
}

json relatedArtist(const json& data) {
    json out = json::object();

    std::string subs = str(nav(data, SUBTITLE));
    if (!subs.empty()) subs = subs.substr(0, subs.find(' '));

    out["title"] = navJson(data, TITLE_TEXT);
    out["browseId"] = navJson(data, TITLE + NAVIGATION_BROWSE_ID);
    out["subscribers"] = subs.empty() ? json(nullptr) : json(subs);
    out["thumbnails"] = navJson(data, THUMBNAIL_RENDERER);

    return out;
}

json watchPlaylistItem(const json& data) {
    json out = json::object();

    out["title"] = navJson(data, TITLE_TEXT);
    out["playlistId"] = navJson(data, NAVIGATION_WATCH_PLAYLIST_ID);
    out["thumbnails"] = navJson(data, THUMBNAIL_RENDERER);

    return out;
}

json episode(const json& data) {
    json out = json::object();

    out["index"] = navJson(&data, Path{"onTap", "watchEndpoint", "index"});
    out["title"] = navJson(data, TITLE_TEXT);
    out["description"] = navJson(data, DESCRIPTION);
    out["duration"] = navJson(&data, Path{"playbackProgress"} + PROGRESS_RENDERER + DURATION_TEXT);
    out["videoId"] = navJson(&data, Path{"onTap"} + WATCH_VIDEO_ID);
    out["browseId"] = navJson(&data, TITLE + NAVIGATION_BROWSE_ID);
    out["videoType"] = navJson(&data, Path{"onTap"} + NAVIGATION_VIDEO_TYPE);
    out["date"] = navJson(data, SUBTITLE);
    out["thumbnails"] = navJson(data, THUMBNAILS);

    return out;
}

json podcast(const json& data) {
    json out = json::object();

    out["title"] = navJson(data, TITLE_TEXT);
    out["channel"] = idName(nav(data, SUBTITLE_RUNS + Path{0}));
    out["browseId"] = navJson(data, TITLE + NAVIGATION_BROWSE_ID);
    out["podcastId"] = navJson(data, THUMBNAIL_OVERLAY);
    out["thumbnails"] = navJson(data, THUMBNAIL_RENDERER);

    return out;
}

json contentList(const json& results, const std::function<json(const json&)>& fn, const char* key) {
    json contents = json::array();
    if (!results.is_array()) return contents;

    for (const json& result : results) {
        // Carousels mix renderer types, so skip anything fn doesn't expect.
        if (!result.is_object() || !result.contains(key)) continue;
        contents.push_back(fn(result[key]));
    }

    return contents;
}

json mixedContent(const json& rows) {
    json items = json::array();
    if (!rows.is_array()) return items;

    for (const json& row : rows) {
        if (!row.is_object() || row.empty()) continue;

        json title = json(nullptr);
        json contents = json::array();

        if (row.contains(DESCRIPTION_SHELF[0].key())) {
            const json* results = nav(row, DESCRIPTION_SHELF);
            title = navJson(results, Path{"header"} + RUN_TEXT);
            contents = navJson(results, DESCRIPTION);

        } else {
            // Named iterator: binding the reference through a temporary
            // iterator is safe in nlohmann but the compiler can't prove it.
            auto it = row.begin();
            const json& results = it.value();
            if (!results.is_object() || !results.contains("contents")) continue;

            title = navJson(&results, CAROUSEL_TITLE + Path{"text"});
            if (title.is_null()) title = navJson(&results, CAROUSEL_STRAPLINE + Path{"text"});

            for (const json& result : results["contents"]) {
                const json* data = nav(result, {MTRIR});

                if (data) {
                    const json* pt = nav(data, TITLE + NAVIGATION_BROWSE + PAGE_TYPE);
                    std::string pageType = str(pt);

                    if (!pt) {
                        // song or watch playlist
                        if (navHas(data, NAVIGATION_WATCH_PLAYLIST_ID)) {
                            contents.push_back(watchPlaylistItem(*data));
                        } else {
                            contents.push_back(song(*data));
                        }

                    } else if (pageType == "MUSIC_PAGE_TYPE_ALBUM" || pageType == "MUSIC_PAGE_TYPE_AUDIOBOOK") {
                        contents.push_back(album(*data));

                    } else if (pageType == "MUSIC_PAGE_TYPE_ARTIST" || pageType == "MUSIC_PAGE_TYPE_USER_CHANNEL") {
                        contents.push_back(relatedArtist(*data));

                    } else if (pageType == "MUSIC_PAGE_TYPE_PLAYLIST") {
                        contents.push_back(playlist(*data));

                    } else if (pageType == "MUSIC_PAGE_TYPE_PODCAST_SHOW_DETAIL_PAGE") {
                        contents.push_back(podcast(*data));

                    } else {
                        contents.push_back(json(nullptr));
                    }

                } else if ((data = nav(result, {MRLIR}))) {
                    contents.push_back(songFlat(*data));

                } else if ((data = nav(result, {MMRIR}))) {
                    contents.push_back(episode(*data));

                } else {
                    continue;
                }
            }
        }

        json entry = json::object();
        entry["title"] = title;
        entry["contents"] = contents;

        items.push_back(entry);
    }

    return items;
}



json albumPlaylistIdIfExists(const json* data) {
    if (!data) return json(nullptr);

    json v = navJson(data, WATCH_PID);
    if (!v.is_null()) return v;

    return navJson(data, WATCH_PLAYLIST_ID);
}

json albumHeader2024(const json& response) {
    const json* header = nav(response, TWO_COLUMN_RENDERER + TAB_CONTENT + SECTION_LIST_ITEM + RESPONSIVE_HEADER);

    json out = json::object();
    if (!header) return out;

    out["title"] = navJson(header, TITLE_TEXT);
    out["type"] = navJson(header, SUBTITLE);
    out["thumbnails"] = navJson(header, THUMBNAILS);
    out["isExplicit"] = navHas(header, SUBTITLE_BADGE_LABEL);

    std::string description;
    json runs = descriptionRuns(nav(header, Path{"description"} + DESCRIPTION_SHELF + DESCRIPTION_RUN_LIST), description);

    out["description"] = description;
    out["descriptionRuns"] = runs;

    const json* subtitleRuns = nav(header, SUBTITLE_RUNS);
    if (subtitleRuns && subtitleRuns->is_array() && subtitleRuns->size() > 2) {
        json sliced = json::array();

        for (size_t i = 2; i < subtitleRuns->size(); ++i) {
            sliced.push_back((*subtitleRuns)[i]);
        }

        out.update(songRuns(sliced));
    }

    const json* strapline = nav(header, {"straplineTextOne", "runs"});
    out["artists"] = strapline ? artistsRuns(*strapline) : json(nullptr);

    const json* second = nav(header, {"secondSubtitle", "runs"});
    if (second && second->is_array() && !second->empty()) {
        if (second->size() > 1) {
            long count = toInt(str(nav(*second, {0, "text"})));
            out["trackCount"] = count < 0 ? json(nullptr) : json(count);
            out["duration"] = navJson(*second, {2, "text"});
        } else {
            out["duration"] = navJson(*second, {0, "text"});
        }
    }

    const json* buttons = nav(header, {"buttons"});

    const json* playButton = findObjectByKey(buttons, "musicPlayButtonRenderer");
    out["audioPlaylistId"] = navJson(playButton,
        Path{"musicPlayButtonRenderer", "playNavigationEndpoint"} + WATCH_PID);

    if (out["audioPlaylistId"].is_null()) {
        out["audioPlaylistId"] = navJson(playButton, Path{"musicPlayButtonRenderer", "playNavigationEndpoint"} + WATCH_PLAYLIST_ID);
    }

    out["likeStatus"] = "INDIFFERENT";
    const json* toggle = findObjectByKey(buttons, "toggleButtonRenderer");
    const json* service = nav(toggle, {"toggleButtonRenderer", "defaultServiceEndpoint"});

    if (service) out["likeStatus"] = likeStatus(*service);

    return out;
}



json libraryAlbums(const json& results) {
    json albums = json::array();
    if (!results.is_array()) return albums;

    for (const json& result : results) {
        if (!result.is_object() || !result.contains(MTRIR)) continue;

        const json& data = result[MTRIR];

        json album = json::object();

        album["browseId"]   = navJson(data, TITLE + NAVIGATION_BROWSE_ID);
        album["playlistId"] = navJson(data, MENU_PLAYLIST_ID);
        album["title"]      = navJson(data, TITLE_TEXT);
        album["thumbnails"] = navJson(data, THUMBNAIL_RENDERER);

        const json* subtitle = nav(data, {"subtitle"});

        if (subtitle && subtitle->contains("runs")) {
            album["type"] = navJson(data, SUBTITLE);

            const json& runs = (*subtitle)["runs"];

            json sliced = json::array();
            for (size_t i = 2; i < runs.size(); ++i) sliced.push_back(runs[i]);

            album.update(songRuns(sliced));
        }

        albums.push_back(album);
    }

    return albums;
}

json libraryArtists(const json& results) {
    json artists = json::array();
    if (!results.is_array()) return artists;

    for (const json& result : results) {
        if (!result.is_object() || !result.contains(MRLIR)) continue;

        const json& data = result[MRLIR];

        json artist = json::object();

        artist["browseId"] = navJson(data, NAVIGATION_BROWSE_ID);
        artist["artist"]   = itemText(data, 0);

        std::string pageType = str(nav(data, NAVIGATION_BROWSE + PAGE_TYPE));

        if (pageType == "MUSIC_PAGE_TYPE_USER_CHANNEL") artist["type"] = "channel";
        else if (pageType == "MUSIC_PAGE_TYPE_ARTIST")  artist["type"] = "artist";

        menuPlaylists(data, artist);

        json subtitle = itemText(data, 1);

        if (subtitle.is_string() && !subtitle.get<std::string>().empty()) {
            std::string text = subtitle.get<std::string>();
            artist["subscribers"] = text.substr(0, text.find(' '));
        }

        artist["thumbnails"] = navJson(data, THUMBNAILS);

        artists.push_back(artist);
    }

    return artists;
}

std::string validatePlaylistId(const std::string& playlistId) {
    return startsWith(playlistId, "VL") ? playlistId.substr(2) : playlistId;
}

json playlistHeaderMeta(const json& header) {
    json meta = json::object();

    meta["views"] = nullptr;
    meta["duration"] = nullptr;
    meta["trackCount"] = nullptr;
    meta["title"] = joinRuns(nav(header, {"title", "runs"}));
    meta["thumbnails"] = navJson(header, THUMBNAILS);

    if (header.is_object() && header.contains("facepile")) {
        const json* avatarRenderer = nav(header, {"facepile", "avatarStackViewModel", "rendererContext"});
        const json* avatarCommand = nav(avatarRenderer, {"commandContext", "onTap", "innertubeCommand"});

        std::string tag = str(nav(avatarCommand, {"showEngagementPanelEndpoint", "identifier", "tag"}));

        if (tag == "PAplaylist_collaborate") {
            json collaborators = json::object();
            collaborators["text"] = navJson(avatarRenderer, {"accessibilityContext", "label"});

            json avatars = json::array();
            const json* raw = nav(header, {"facepile", "avatarStackViewModel", "avatars"});
            if (raw && raw->is_array()) {
                for (const json& a : *raw) {
                    json src = navJson(a, {"avatarViewModel", "image", "sources", 0});
                    if (!src.is_null()) avatars.push_back(src);
                }
            }

            collaborators["avatars"] = avatars;
            meta["collaborators"] = collaborators;

        } else {
            json author = json::object();
            author["name"] = navJson(header, {"facepile", "avatarStackViewModel", "text", "content"});
            author["id"] = navJson(avatarCommand, {"browseEndpoint", "browseId"});
            meta["author"] = author;
        }
    }

    const json* runs = nav(header, {"secondSubtitle", "runs"});
    if (runs && runs->is_array()) {
        size_t hasViews = (runs->size() > 3) ? 2 : 0;
        size_t hasDuration = (runs->size() > 1) ? 2 : 0;

        if (hasViews) {
            long v = toInt(str(nav(*runs, {0, "text"})));
            meta["views"] = v < 0 ? json(nullptr) : json(v);
        }

        if (hasDuration) {
            meta["duration"] = navJson(*runs, {static_cast<int>(hasViews + hasDuration), "text"});
        }

        std::string countText = str(nav(*runs, {static_cast<int>(hasViews), "text"}));
        long count = toInt(countText);
        meta["trackCount"] = count < 0 ? json(nullptr) : json(count);
    }

    return meta;
}

json playlistItem(const json& data, bool isAlbum, bool isCollaborative) {
    json videoId = nullptr;
    json setVideoId = nullptr;
    json creditsBrowseId = nullptr;
    json like = nullptr;

    if (data.is_object() && data.contains("menu")) {
        const json* items = nav(data, MENU_ITEMS);
        if (items && items->is_array()) {
            for (const json& item : *items) {
                if (item.contains("menuServiceItemRenderer")) {
                    const json* service = nav(item, MENU_SERVICE);
                    if (service && service->contains("playlistEditEndpoint")) {
                        setVideoId = navJson(service, {"playlistEditEndpoint", "actions", 0, "setVideoId"});
                        videoId = navJson(service, {"playlistEditEndpoint", "actions", 0, "removedVideoId"});
                    }

                } else if (item.contains(MNIR)) {
                    std::string maybe = str(nav(item, Path{MNIR} + NAVIGATION_BROWSE_ID));
                    if (startsWith(maybe, "MPTC")) creditsBrowseId = maybe;
                }
            }
        }
    }

    json menuData = json::object();
    menuData["inLibrary"] = nullptr;
    menuData["pinnedToListenAgain"] = nullptr;
    menuData.update(songMenuData(data));

    const json* playButton = nav(data, PLAY_BUTTON);
    if (playButton && playButton->contains("playNavigationEndpoint")) {
        json vid = navJson(playButton, {"playNavigationEndpoint", "watchEndpoint", "videoId"});
        if (!vid.is_null()) videoId = vid;

        if (data.contains("menu")) like = navJson(data, MENU_LIKE_STATUS);
    }

    bool isAvailable = true;
    if (data.is_object() && data.contains("musicItemRendererDisplayPolicy")) {
        isAvailable = data["musicItemRendererDisplayPolicy"] != "MUSIC_ITEM_RENDERER_DISPLAY_POLICY_GREY_OUT";
    }

    // For unavailable items and album track lists the flex column meaning can't
    // be derived from navigationEndpoint, so the indexes are preset.
    bool usePreset = !isAvailable || isAlbum;

    int titleIndex = usePreset ? 0 : -1;
    int artistIndex = usePreset ? 1 : -1;
    int durationIndex = -1;
    int albumIndex = isCollaborative ? 3 : (usePreset ? 2 : -1);

    std::vector<int> userChannelIndexes;
    int              unrecognizedIndex = -1;

    const json* flexColumns = nav(data, {"flexColumns"});
    int columnCount = (flexColumns && flexColumns->is_array()) ? static_cast<int>(flexColumns->size()) : 0;

    for (int index = 0; index < columnCount; ++index) {
        const json* flex = flexColumnItem(data, index);
        const json* endpoint = nav(flex, TEXT_RUN + Path{"navigationEndpoint"});

        if (!endpoint) {
            const json* run = nav(flex, TEXT_RUN);
            if (run && run->contains("text")) {
                if (classifySongRun(*run).type == "duration") {
                    durationIndex = index;
                } else if (unrecognizedIndex == -1) {
                    unrecognizedIndex = index;
                }
            }
            continue;
        }

        if (endpoint->contains("watchEndpoint")) {
            titleIndex = index;

        } else if (endpoint->contains("browseEndpoint")) {
            std::string pageType = str(nav(endpoint, Path{"browseEndpoint"} + PAGE_TYPE));

            if (pageType == "MUSIC_PAGE_TYPE_ARTIST" || pageType == "MUSIC_PAGE_TYPE_UNKNOWN") {
                artistIndex = index;
            } else if (pageType == "MUSIC_PAGE_TYPE_ALBUM" || pageType == "MUSIC_PAGE_TYPE_AUDIOBOOK") {
                albumIndex = index;
            } else if (pageType == "MUSIC_PAGE_TYPE_USER_CHANNEL") {
                userChannelIndexes.push_back(index);
            } else if (pageType == "MUSIC_PAGE_TYPE_NON_MUSIC_AUDIO_TRACK_PAGE") {
                titleIndex = index;
            }
        }
    }

    if (artistIndex == -1 && unrecognizedIndex != -1)   artistIndex = unrecognizedIndex;
    if (artistIndex == -1 && !userChannelIndexes.empty()) artistIndex = userChannelIndexes.back();

    json title = titleIndex != -1 ? itemText(data, titleIndex) : json(nullptr);
    if (title.is_string() && title.get<std::string>() == "Song deleted") return json(nullptr);

    json artists = artistIndex != -1 ? songArtists(data, artistIndex) : json(nullptr);
    json alb = albumIndex  != -1 ? songAlbum(data, albumIndex)    : json(nullptr);
    json views_ = isAlbum ? itemText(data, 2) : json(nullptr);

    // Python uses "if duration_index", which is falsy for column 0 too.
    json duration = durationIndex > 0 ? itemText(data, durationIndex) : json(nullptr);

    if (data.is_object() && data.contains("fixedColumns")) {
        const json* fixed = fixedColumnItem(data, 0);
        if (fixed) {
            const json* simple = nav(fixed, {"text", "simpleText"});
            duration = simple ? *simple : navJson(fixed, TEXT_RUN_TEXT);
        }
    }

    json song_ = json::object();

    song_["videoId"] = videoId;
    song_["title"] = title;
    song_["artists"] = artists;
    song_["album"] = alb;
    song_["likeStatus"] = like;
    song_.update(menuData);
    song_["thumbnails"] = navJson(data, THUMBNAILS);
    song_["isAvailable"] = isAvailable;
    song_["isExplicit"] = navHas(&data, BADGE_LABEL);
    song_["videoType"] = navJson(&data, MENU_ITEMS + Path{0, MNIR, "navigationEndpoint"} + NAVIGATION_VIDEO_TYPE);
    song_["views"] = views_;

    const json* voting = nav(data, ENGAGEMENT_BAR);
    if (!voting) {
        song_["communityVoteStatus"] = nullptr;
    } else {
        json vote = json::object();
        vote["netVoteValue"] = navJson(voting, {"votes"});
        vote["status"] = navJson(voting, {"status"});
        song_["communityVoteStatus"] = vote;
    }

    if (isAlbum) {
        std::string idx = str(nav(data, {"index", "runs", 0, "text"}));
        song_["trackNumber"] = (isAvailable && !idx.empty()) ? json(toInt(idx)) : json(nullptr);
    }

    if (duration.is_string()) {
        song_["duration"] = duration;
        int secs = parseDuration(duration.get<std::string>());
        song_["duration_seconds"] = secs < 0 ? json(nullptr) : json(secs);
    }

    if (!setVideoId.is_null())      song_["setVideoId"] = setVideoId;
    if (!creditsBrowseId.is_null()) song_["creditsBrowseId"] = creditsBrowseId;

    return song_;
}

json playlistItems(const json& results, bool isAlbum, bool isCollaborative) {
    json songs = json::array();
    if (!results.is_array()) return songs;

    for (const json& result : results) {
        if (!result.is_object() || !result.contains(MRLIR)) continue;

        json s = playlistItem(result[MRLIR], isAlbum, isCollaborative);
        if (!s.is_null()) songs.push_back(s);
    }

    return songs;
}



json watchTrack(const json& data) {
    json track = json::object();

    json like = nullptr;
    const json* items = nav(data, MENU_ITEMS);
    if (items && items->is_array()) {
        for (const json& item : *items) {
            if (!item.contains(TOGGLE_MENU)) continue;

            const json* service = nav(item, {TOGGLE_MENU, "defaultServiceEndpoint"});
            if (service && service->contains("likeEndpoint")) like = likeStatus(*service);
        }
    }

    track["videoId"] = navJson(data, {"videoId"});
    track["title"] = navJson(data, TITLE_TEXT);
    track["length"] = navJson(data, {"lengthText", "runs", 0, "text"});
    track["thumbnail"] = navJson(data, THUMBNAIL);
    track["likeStatus"] = like;
    track["videoType"] = navJson(&data, Path{"navigationEndpoint"} + NAVIGATION_VIDEO_TYPE);

    track["inLibrary"] = nullptr;
    track["feedbackTokens"] = nullptr;
    track["pinnedToListenAgain"] = nullptr;
    track["listenAgainFeedbackTokens"] = nullptr;
    track.update(songMenuData(data));

    const json* byline = nav(data, {"longBylineText", "runs"});
    if (byline) track.update(songRuns(*byline));

    return track;
}

json watchPlaylistTracks(const json& results) {
    static const char* PPVWR = "playlistPanelVideoWrapperRenderer";
    static const char* PPVR = "playlistPanelVideoRenderer";

    json tracks = json::array();
    if (!results.is_array()) return tracks;

    for (const json& entry : results) {
        const json* result = &entry;
        const json* counterpart = nullptr;

        if (entry.contains(PPVWR)) {
            counterpart = nav(entry, {PPVWR, "counterpart", 0, "counterpartRenderer", PPVR});
            result = nav(entry, {PPVWR, "primaryRenderer"});
        }

        if (!result || !result->contains(PPVR)) continue;

        const json& data = (*result)[PPVR];
        if (data.contains("unplayableText")) continue;

        json track = watchTrack(data);
        if (counterpart) track["counterpart"] = watchTrack(*counterpart);

        tracks.push_back(track);
    }

    return tracks;
}



const std::vector<std::string> ALL_RESULT_TYPES = {
    "album", "artist", "playlist", "song", "video", "station", "profile", "podcast", "episode"
};

namespace {

json searchResultType(const std::string& localised) {
    if (localised.empty()) return json(nullptr);

    std::string lower = localised;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });

    auto it = std::find(ALL_RESULT_TYPES.begin(), ALL_RESULT_TYPES.end(), lower);

    // Albums carry several labels ("Single", "EP", ...) so they're the default.
    if (it == ALL_RESULT_TYPES.end()) return json("album");

    return json(*it);
}

}

json topResult(const json& data) {
    json resultType = searchResultType(str(nav(data, SUBTITLE)));
    std::string rt = resultType.is_string() ? resultType.get<std::string>() : "";

    json out = json::object();

    std::string category = str(nav(data, CARD_SHELF_TITLE));
    out["category"] = category.empty() ? "Top result" : category;
    out["resultType"] = resultType;

    if (rt == "artist") {
        std::string subs = str(nav(data, SUBTITLE2));
        if (!subs.empty()) out["subscribers"] = subs.substr(0, subs.find(' '));

        const json* titleRuns = nav(data, {"title", "runs"});
        if (titleRuns) out.update(songRuns(*titleRuns));
    }

    if (rt == "song" || rt == "video") {
        if (data.contains("onTap")) {
            out["videoId"] = navJson(&data, Path{"onTap"} + WATCH_VIDEO_ID);
            out["videoType"] = navJson(&data, Path{"onTap"} + NAVIGATION_VIDEO_TYPE);
        }
    }

    if (rt == "song" || rt == "video" || rt == "album") {
        out["videoId"] = navJson(&data, Path{"onTap"} + WATCH_VIDEO_ID);
        out["videoType"] = navJson(&data, Path{"onTap"} + NAVIGATION_VIDEO_TYPE);
        out["title"] = navJson(data, TITLE_TEXT);

        const json* runs = nav(data, SUBTITLE_RUNS);
        if (runs && runs->is_array() && runs->size() > 2) {
            json sliced = json::array();

            for (size_t i = 2; i < runs->size(); ++i) {
                sliced.push_back((*runs)[i]);
            }

            out.update(songRuns(sliced));
        }
    }

    if (rt == "album") {
        out["browseId"] = navJson(data, TITLE + NAVIGATION_BROWSE_ID);
        out["playlistId"] = albumPlaylistIdIfExists(nav(data, {"buttons", 0, "buttonRenderer", "command"}));
    }

    if (rt == "playlist") {
        out["playlistId"] = navJson(data, MENU_PLAYLIST_ID);
        out["title"] = navJson(data, TITLE_TEXT);

        const json* runs = nav(data, SUBTITLE_RUNS);
        if (runs && runs->is_array() && runs->size() > 2) {
            json sliced = json::array();

            for (size_t i = 2; i < runs->size(); ++i) {
                sliced.push_back((*runs)[i]);
            }

            out["author"] = artistsRuns(sliced);
        }
    }

    if (rt == "episode") {
        out["title"] = navJson(data, TITLE_TEXT);
        out["videoId"] = navJson(&data, THUMBNAIL_OVERLAY_NAVIGATION + WATCH_VIDEO_ID);
        out["videoType"] = navJson(&data, THUMBNAIL_OVERLAY_NAVIGATION + NAVIGATION_VIDEO_TYPE);

        const json* runs = nav(data, SUBTITLE_RUNS);
        if (runs && runs->is_array() && runs->size() > 4) {
            out["date"] = navJson(*runs, {2, "text"});
            out["podcast"] = idName(&(*runs)[4]);
        }
    }

    out["thumbnails"] = navJson(data, THUMBNAILS);

    return out;
}

json searchResult(const json& data, const json& resultTypeIn, const json& category) {
    std::string resultType = resultTypeIn.is_string() ? resultTypeIn.get<std::string>() : "";

    int defaultOffset = (resultType.empty() || resultType == "album") ? 2 : 0;

    json out = json::object();
    out["category"] = category;

    json videoType = navJson(&data, PLAY_BUTTON + Path{"playNavigationEndpoint"} + NAVIGATION_VIDEO_TYPE);

    // With no shelf title (extra Top Result rows) the type comes from the browseId.
    if (resultType.empty()) {
        std::string browseId = str(nav(data, NAVIGATION_BROWSE_ID));

        if (!browseId.empty()) {
            static const std::vector<std::pair<std::string, std::string>> mapping = {
                {"VM", "playlist"}, {"RD", "playlist"}, {"VL", "playlist"},
                {"MPLA", "artist"}, {"MPRE", "album"},  {"MPSP", "podcast"},
                {"MPED", "episode"}, {"UC", "artist"},
            };

            for (const auto& [prefix, type] : mapping) {
                if (startsWith(browseId, prefix)) {
                    resultType = type;
                    break;
                }
            }

        } else {
            std::string vt = videoType.is_string() ? videoType.get<std::string>() : "";
            if (vt == "MUSIC_VIDEO_TYPE_ATV")                  resultType = "song";
            else if (vt == "MUSIC_VIDEO_TYPE_PODCAST_EPISODE") resultType = "episode";
            else                                               resultType = "video";
        }
    }

    out["resultType"] = resultType.empty() ? json(nullptr) : json(resultType);

    if (resultType != "artist") out["title"] = itemText(data, 0);

    if (resultType == "artist") {
        out["artist"] = itemText(data, 0);
        menuPlaylists(data, out);

    } else if (resultType == "album") {
        out["type"] = itemText(data, 1);
        out["playlistId"] = albumPlaylistIdIfExists(nav(&data, PLAY_BUTTON + Path{"playNavigationEndpoint"}));

    } else if (resultType == "playlist") {
        const json* flexRuns = nav(flexColumnItem(data, 1), TEXT_RUNS);
        int  runCount = (flexRuns && flexRuns->is_array()) ? static_cast<int>(flexRuns->size()) : 0;
        bool hasAuthor = runCount == defaultOffset + 3;

        json infoJson = itemText(data, 1, hasAuthor ? 2 : 0);
        std::string info = str(infoJson);

        std::vector<std::string> words;
        size_t start = 0;

        while (start <= info.size()) {
            size_t pos = info.find(' ', start);
            words.push_back(info.substr(start, pos == std::string::npos ? pos : pos - start));
            if (pos == std::string::npos) break;
            start = pos + 1;
        }

        out["itemCount"] = nullptr;
        if (words.size() >= 2 && words[1] == "songs") {
            long n = toInt(words[0]);
            out["itemCount"] = n < 0 ? json(words[0]) : json(n);
        }

        out["author"] = hasAuthor ? itemText(data, 1, defaultOffset) : json(nullptr);

    } else if (resultType == "station") {
        out["videoId"] = navJson(data, NAVIGATION_VIDEO_ID);
        out["playlistId"] = navJson(data, NAVIGATION_PLAYLIST_ID);

    } else if (resultType == "profile") {
        out["name"] = itemText(data, 1, 2, true);

    } else if (resultType == "song") {
        out["album"] = nullptr;
        out.update(songMenuData(data));
    }

    if (resultType == "song" || resultType == "video" || resultType == "episode") {
        out["videoId"] = navJson(&data, PLAY_BUTTON + Path{"playNavigationEndpoint", "watchEndpoint", "videoId"});
        out["videoType"] = videoType;
    }

    if (resultType == "song" || resultType == "video" || resultType == "album") {
        out["duration"] = nullptr;
        out["year"] = nullptr;

        const json* flex = flexColumnItem(data, 1);
        if (flex) {
            json runs = navJson(flex, TEXT_RUNS);
            if (!runs.is_array()) runs = json::array();

            if (const json* flex2 = flexColumnItem(data, 2)) {
                runs.push_back(json{{"text", ""}});
                const json* runs2 = nav(flex2, TEXT_RUNS);
                if (runs2 && runs2->is_array()) {
                    for (const json& r : *runs2) runs.push_back(r);
                }
            }

            out.update(songRuns(runs, true));
        }
    }

    if (resultType == "artist" || resultType == "album" || resultType == "playlist" ||
        resultType == "profile" || resultType == "podcast") {
        out["browseId"] = navJson(data, NAVIGATION_BROWSE_ID);
    }

    if (resultType == "song" || resultType == "album") {
        out["isExplicit"] = navHas(&data, BADGE_LABEL);
    }

    if (resultType == "episode") {
        const json* flex = flexColumnItem(data, 1);
        const json* allRuns = nav(flex, TEXT_RUNS);

        json runs = json::array();
        if (allRuns && allRuns->is_array()) {
            for (size_t i = static_cast<size_t>(defaultOffset); i < allRuns->size(); ++i) {
                runs.push_back((*allRuns)[i]);
            }
        }

        bool hasDate = runs.size() > 1;
        out["live"] = navHas(&data, {"badges", 0, "liveBadgeRenderer"});

        if (hasDate) out["date"] = navJson(runs, {0, "text"});

        size_t podcastIdx = hasDate ? 2 : 0;
        out["podcast"] = podcastIdx < runs.size() ? idName(&runs[podcastIdx]) : json(nullptr);
    }

    out["thumbnails"] = navJson(data, THUMBNAILS);

    return out;
}

json searchResults(const json& contents, const json& resultType, const json& category) {
    json out = json::array();
    if (!contents.is_array()) return out;

    for (const json& result : contents) {
        if (!result.is_object() || !result.contains(MRLIR)) continue;
        out.push_back(searchResult(result[MRLIR], resultType, category));
    }

    return out;
}

std::string searchParams(const std::string& filter, bool ignoreSpelling) {
    if (filter.empty() && !ignoreSpelling) return "";

    if (filter.empty()) return "EhGKAQ4IARABGAEgASgAOAFAAUICCAE%3D";

    if (filter == "playlists") {
        return ignoreSpelling ? "Eg-KAQwIABAAGAAgACgBMABCAggBagoQBBADEAkQBRAK"
                              : "Eg-KAQwIABAAGAAgACgBMABqChAEEAMQCRAFEAo%3D";
    }

    if (filter.find("playlists") != std::string::npos) {
        std::string p2 = (filter == "featured_playlists") ? "Dg" : "EA";
        std::string p3 = ignoreSpelling ? "BQgIIAWoMEA4QChADEAQQCRAF" : "BagwQDhAKEAMQBBAJEAU%3D";
        return "EgeKAQQoA" + p2 + p3;
    }

    static const std::vector<std::pair<std::string, std::string>> filterParams = {
        {"songs", "II"},    {"videos", "IQ"},   {"albums", "IY"},   {"artists", "Ig"},
        {"playlists", "Io"}, {"profiles", "JY"}, {"podcasts", "JQ"}, {"episodes", "JI"},
    };

    std::string p2;
    for (const auto& [k, v] : filterParams) {
        if (k == filter) { p2 = v; break; }
    }

    if (p2.empty()) return "";

    std::string p3 = ignoreSpelling ? "AUICCAFqDBAOEAoQAxAEEAkQBQ%3D%3D" : "AWoMEA4QChADEAQQCRAF";

    return "EgWKAQ" + p2 + p3;
}

}
