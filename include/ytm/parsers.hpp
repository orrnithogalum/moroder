/* PARSERS
- Port of ytmusicapi's parsers/ package (1.12.2)
- Each function here produces the same JSON shape as its Python counterpart, so anything downstream that already consumes ytmusicapi output keeps working unchanged.
- Where Python returns None, these return a JSON null rather than omitting the key, again to keep the output shape identical.
*/

#pragma once

#include "nav.hpp"

#include <functional>
#include <string>
#include <vector>

namespace ytm::parse {

// -> utils.py
const json* flexColumnItem(const json& item, int index);
const json* fixedColumnItem(const json& item, int index);

/* get_item_text
- JSON null when the column or run is missing.
*/
json itemText(const json& item, int index, int runIndex = 0, bool noneIfAbsent = false);

json idName(const json* subRun);

/* parse_duration
- seconds, or -1 when the string isn't a duration.
*/
int parseDuration(const std::string& duration);

/* to_int
- strips non-digits then parses. -1 on failure.
*/
long toInt(const std::string& text);

void menuPlaylists(const json& data, json& result);



// -> artists.py / songs.py
json artistsRuns(const json& runs);

/* parse_views
- JSON null when the text isn't a view count.
*/
json views(const std::string& text);

json songRuns(const json& runs, bool skipTypeSpec = false);
json songArtists(const json& data, int index);
json songAlbum(const json& data, int index);
json songMenuData(const json& data);
std::string likeStatus(const json& service);



// -> browsing.py
json mixedContent(const json& rows);
json contentList(const json& results, const std::function<json(const json&)>& fn, const char* key = path::MTRIR);

json album(const json& result);
json song(const json& result);
json songFlat(const json& data, bool withPlaylistId = false);
json playlist(const json& data);
json relatedArtist(const json& data);
json watchPlaylistItem(const json& data);
json episode(const json& data);
json podcast(const json& data);



// -> albums.py
json albumHeader2024(const json& response);
json albumPlaylistIdIfExists(const json* data);



/* -> library.py
- parse_albums (MTRIR, distinct from browsing.py's parse_album) and parse_artists (MRLIR).
- Podcasts reuse contentList(podcast), songs reuse playlistItems.
*/
json libraryAlbums(const json& results);
json libraryArtists(const json& results);



// -> playlists.py
json playlistItems(const json& results, bool isAlbum = false, bool isCollaborative = false);
json playlistItem(const json& data, bool isAlbum, bool isCollaborative);
json playlistHeaderMeta(const json& header);
std::string validatePlaylistId(const std::string& playlistId);



// -> watch.py
json watchPlaylistTracks(const json& results);
json watchTrack(const json& data);



/* -> search.py
- The English result-type labels. ytmusicapi localises these;
- We always request hl=en so the English list is the right one.
*/
extern const std::vector<std::string> ALL_RESULT_TYPES;

json searchResult(const json& data, const json& resultType, const json& category);
json searchResults(const json& contents, const json& resultType, const json& category);
json topResult(const json& data);

// get_search_params for the filters we support. Empty string means "no params".
std::string searchParams(const std::string& filter, bool ignoreSpelling);

}
