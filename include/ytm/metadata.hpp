#pragma once

#include "http.hpp"

#include <nlohmann/json.hpp>
#include <string>

/* Extra APis used to lookup album names if ytm doesn't return them.
- last.fm as a first fallback, then iTunes.
- lives here rather than inside services::Music so it can be tested separatly.
*/
namespace ytm {

/* Returns the same JSON shape the Python get_song endpoint produced, so ipc::SongResponse stays unchanged:
- {"status": "ok", "song": {"album": "..."}, "source": "lastfm" | "itunes"}
- {"status": "error", "message": "..."}
- Pass an empty lastfmApiKey to skip straight to iTunes.
 */
nlohmann::json lookupAlbum(Http& http, const std::string& lastfmApiKey, const std::string& title, const std::string& artist);
}
