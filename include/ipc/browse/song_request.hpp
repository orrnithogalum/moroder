/* GET SONG
- Returns extra details needed for mpris on a song.
- Needed as search doesn't reliably include duration for all results.
- Python endpoint in python/services/get_song.py
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
            {"id", song_ref_.id}
        };

        return j;
    }

private:
    const music::SongRef song_ref_;
};

}