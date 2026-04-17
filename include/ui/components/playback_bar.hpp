#pragma once

#include "../../services/player.hpp"

#include <ftxui/component/component.hpp>
#include <string>

namespace ui {

struct PlaybackData {
    std::string title;
    std::string artist;
    std::string image_url;

    int progress = 0;
    int dimx = 0;

    bool is_playing = false;
};

void getPlaybackData(services::Player::PlayerState* state, PlaybackData* data, int dimx);
ftxui::Component PlaybackBar(PlaybackData* data);

}
