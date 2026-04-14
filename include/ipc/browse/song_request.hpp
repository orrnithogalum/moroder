/* GET SONG
- Returns extra details needed for mpris on a song.
- Needed as search doesn't reliably include duration for all results.
- Python endpoint in python/modules/get_song.py
*/

#pragma once

#include "../../models/song.hpp"
#include "../request.hpp"

namespace ipc {

class SongRequest : public Request {
public:
    explicit SongRequest(const music::SongRef& song_ref) : song_ref_(song_ref) {}

    nlohmann::json to_json() const override {

        nlohmann::json j{
            {"action", "get_song"},
            {"song_title", song_ref_.title},
            {"song_artist", song_ref_.artists[0].name}
        };

        return j;
    }

private:
    const music::SongRef song_ref_;
};

}
