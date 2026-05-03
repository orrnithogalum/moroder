/* NAV
- C++ reimplmentation of python ytmusicapi's navigation.py
- Youtube's InnerTube API returns deeply nested "renderer" objects.
- Every parser in ytmusicapi is written as a path walk over that structure, so sorting the parsers cleanly means porting navigation.py first
- Everything here is a direct translation of navigation.py from ytmusicapi 1.12.2.
*/

#pragma once

#include <nlohmann/json.hpp>

#include <initializer_list>
#include <string>
#include <vector>

namespace ytm {

using json = nlohmann::json;

// A single step in a navigation path: either an object key or an array index.
// Negative indices count from the end, like Python.
class Key {
public:
    Key(std::string k) : key_(std::move(k)), is_index_(false) {}
    Key(const char* k) : key_(k), is_index_(false) {}
    Key(int i) : index_(i), is_index_(true)  {}

    bool isIndex() const {
        return is_index_;
    }

    int index() const {
        return index_;
    }

    const std::string& key() const {
        return key_;
    }

private:
    std::string key_;
    int index_ = 0;
    bool is_index_ = false;
};

using Path = std::vector<Key>;

inline Path operator+(Path a, const Path& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

/* Walks "path" from "root". Returns nullptr if any step is missing, rather than throwing:
- ytmusicapi has both a throwing and a non-throwing variant, but in practice every call site that matters passes none_if_absent=True
- and a missing key should degrade a single field rather than kill a whole response.
*/
const json* nav(const json* root, const Path& path) noexcept;

inline const json* nav(const json& root, const Path& path) noexcept {
    return nav(&root, path);
}

/* Convenience accessors.
- All of them treat "absent" and "wrong type" the same way.
*/
std::string navStr(const json* root, const Path& path, const std::string& fallback = "");
std::string navStr(const json& root, const Path& path, const std::string& fallback = "");

/* Returns a null json (not an exception)
- When the path is absent/invalid, results can be assigned straight into an output object without errors.
*/
json navJson(const json* root, const Path& path);
json navJson(const json& root, const Path& path);

bool navHas(const json* root, const Path& path);

// str(j) -> the string value, or "" for null / non-string / nullptr.
std::string str(const json* j);
std::string str(const json& j);

/* ytmusicapi's find_object_by_key / find_objects_by_key.
- Minus the `is_key` variant which nothing we need uses.
*/
const json* findObjectByKey(const json* list, const std::string& key);
std::vector<const json*> findObjectsByKey(const json* list, const std::string& key);

/* Navigation paths
- Names match navigation.py so the ported parsers stay diffable against Python.
*/
namespace path {

inline const Path CONTENT                = {"contents", 0};
inline const Path RUN_TEXT               = {"runs", 0, "text"};
inline const Path TAB_CONTENT            = {"tabs", 0, "tabRenderer", "content"};
inline const Path TAB_1_CONTENT          = {"tabs", 1, "tabRenderer", "content"};
inline const Path TAB_2_CONTENT          = {"tabs", 2, "tabRenderer", "content"};
inline const Path TWO_COLUMN_RENDERER    = {"contents", "twoColumnBrowseResultsRenderer"};
inline const Path SINGLE_COLUMN          = {"contents", "singleColumnBrowseResultsRenderer"};
inline const Path SINGLE_COLUMN_TAB      = SINGLE_COLUMN + TAB_CONTENT;
inline const Path SECTION                = {"sectionListRenderer"};
inline const Path SECTION_LIST           = SECTION + Path{"contents"};
inline const Path SECTION_LIST_ITEM      = SECTION + CONTENT;
inline const Path RESPONSIVE_HEADER      = {"musicResponsiveHeaderRenderer"};
inline const Path ITEM_SECTION           = Path{"itemSectionRenderer"} + CONTENT;
inline const Path MUSIC_SHELF            = {"musicShelfRenderer"};
inline const Path GRID                   = {"gridRenderer"};
inline const Path GRID_ITEMS             = GRID + Path{"items"};
inline const Path MENU                   = {"menu", "menuRenderer"};
inline const Path MENU_ITEMS             = MENU + Path{"items"};
inline const Path MENU_LIKE_STATUS       = MENU + Path{"topLevelButtons", 0, "likeButtonRenderer", "likeStatus"};
inline const Path MENU_SERVICE           = {"menuServiceItemRenderer", "serviceEndpoint"};
inline const Path OVERLAY_RENDERER       = {"musicItemThumbnailOverlayRenderer", "content", "musicPlayButtonRenderer"};
inline const Path PLAY_BUTTON            = Path{"overlay"} + OVERLAY_RENDERER;
inline const Path NAVIGATION_BROWSE      = {"navigationEndpoint", "browseEndpoint"};
inline const Path NAVIGATION_BROWSE_ID   = NAVIGATION_BROWSE + Path{"browseId"};
inline const Path PAGE_TYPE              = {"browseEndpointContextSupportedConfigs", "browseEndpointContextMusicConfig", "pageType"};
inline const Path WATCH_VIDEO_ID         = {"watchEndpoint", "videoId"};
inline const Path PLAYLIST_ID            = {"playlistId"};
inline const Path WATCH_PLAYLIST_ID      = Path{"watchEndpoint"} + PLAYLIST_ID;
inline const Path NAVIGATION_VIDEO_ID    = Path{"navigationEndpoint"} + WATCH_VIDEO_ID;
inline const Path QUEUE_VIDEO_ID         = {"queueAddEndpoint", "queueTarget", "videoId"};
inline const Path NAVIGATION_PLAYLIST_ID = Path{"navigationEndpoint"} + WATCH_PLAYLIST_ID;
inline const Path WATCH_PID              = Path{"watchPlaylistEndpoint"} + PLAYLIST_ID;
inline const Path NAVIGATION_WATCH_PLAYLIST_ID = Path{"navigationEndpoint"} + WATCH_PID;
inline const Path NAVIGATION_VIDEO_TYPE  = {"watchEndpoint", "watchEndpointMusicSupportedConfigs", "watchEndpointMusicConfig", "musicVideoType"};
inline const Path ICON_TYPE              = {"icon", "iconType"};
inline const Path TITLE                  = {"title", "runs", 0};
inline const Path TITLE_TEXT             = {"title", "runs", 0, "text"};
inline const Path TEXT_RUNS              = {"text", "runs"};
inline const Path TEXT_RUN               = {"text", "runs", 0};
inline const Path TEXT_RUN_TEXT          = {"text", "runs", 0, "text"};
inline const Path SUBTITLE               = Path{"subtitle"} + RUN_TEXT;
inline const Path SUBTITLE_RUNS          = {"subtitle", "runs"};
inline const Path SUBTITLE_RUN           = {"subtitle", "runs", 0};
inline const Path SUBTITLE2              = {"subtitle", "runs", 2, "text"};
inline const Path SUBTITLE3              = {"subtitle", "runs", 4, "text"};
inline const Path THUMBNAIL              = {"thumbnail", "thumbnails"};
inline const Path THUMBNAILS             = Path{"thumbnail", "musicThumbnailRenderer"} + THUMBNAIL;
inline const Path THUMBNAIL_RENDERER     = Path{"thumbnailRenderer", "musicThumbnailRenderer"} + THUMBNAIL;
inline const Path THUMBNAIL_OVERLAY_NAVIGATION = Path{"thumbnailOverlay"} + OVERLAY_RENDERER + Path{"playNavigationEndpoint"};
inline const Path THUMBNAIL_OVERLAY      = THUMBNAIL_OVERLAY_NAVIGATION + WATCH_PID;
inline const Path THUMBNAIL_CROPPED      = Path{"thumbnail", "croppedSquareThumbnailRenderer"} + THUMBNAIL;
inline const Path FEEDBACK_TOKEN         = {"feedbackEndpoint", "feedbackToken"};
inline const Path BADGE_PATH             = {0, "musicInlineBadgeRenderer", "accessibilityData", "accessibilityData", "label"};
inline const Path BADGE_LABEL            = Path{"badges"} + BADGE_PATH;
inline const Path SUBTITLE_BADGE_LABEL   = Path{"subtitleBadges"} + BADGE_PATH;

inline const char* const MRLIR = "musicResponsiveListItemRenderer";
inline const char* const MTRIR = "musicTwoRowItemRenderer";
inline const char* const MMRIR = "musicMultiRowListItemRenderer";
inline const char* const MNIR  = "menuNavigationItemRenderer";
inline const char* const TOGGLE_MENU = "toggleMenuServiceItemRenderer";

inline const Path SECTION_LIST_CONTINUATION = {"continuationContents", "sectionListContinuation"};
inline const Path MENU_PLAYLIST_ID = MENU_ITEMS + Path{0, MNIR} + NAVIGATION_WATCH_PLAYLIST_ID;
inline const Path HEADER                 = {"header"};
inline const Path HEADER_DETAIL          = {"header", "musicDetailHeaderRenderer"};
inline const Path EDITABLE_PLAYLIST_DETAIL_HEADER = {"musicEditablePlaylistDetailHeaderRenderer"};
inline const Path DESCRIPTION_SHELF      = {"musicDescriptionShelfRenderer"};
inline const Path DESCRIPTION            = Path{"description"} + RUN_TEXT;
inline const Path DESCRIPTION_RUN_LIST   = {"description", "runs"};
inline const Path CAROUSEL               = {"musicCarouselShelfRenderer"};
inline const Path CAROUSEL_CONTENTS      = CAROUSEL + Path{"contents"};
inline const Path CAROUSEL_TITLE         = HEADER + Path{"musicCarouselShelfBasicHeaderRenderer"} + TITLE;
inline const Path CAROUSEL_STRAPLINE     = HEADER + Path{"musicCarouselShelfBasicHeaderRenderer", "strapline", "runs", 0};
inline const Path CARD_SHELF_TITLE       = HEADER + Path{"musicCardShelfHeaderBasicRenderer"} + TITLE_TEXT;
inline const Path ENGAGEMENT_BAR         = {"engagementBar", "engagementBarViewModel", "actions", 0, "votingViewModel", "initialState"};
inline const Path PROGRESS_RENDERER      = {"musicPlaybackProgressRenderer"};
inline const Path DURATION_TEXT          = Path{"durationText"} + RUN_TEXT;

// these are from continuations.py
inline const Path CONTINUATION_TOKEN     = {"continuationItemRenderer", "continuationEndpoint", "continuationCommand", "token"};
inline const Path COMMAND_EXECUTOR_COMMANDS = {"continuationItemRenderer", "continuationEndpoint", "commandExecutorCommand", "commands"};
inline const Path CONTINUATION_ITEMS     = {"onResponseReceivedActions", 0, "appendContinuationItemsAction", "continuationItems"};

}

}
