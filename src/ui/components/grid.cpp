#include "../../../include/ui/components/grid.hpp"
#include "../../../include/ui/constants/colors.hpp"

#include "ftxui-grid-container/grid-container.hpp"
#include "spdlog/spdlog.h"
#include "image_view.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

void ui::getGridData(services::Player::PlayerState* state, std::string category, GridData* data) {

    data->entries.clear();
    data->entries_spoof.clear();

    auto it = state->home.find(category);
    if (it == state->home.end()) {
        return;
    }

    category = "  " + category;

    for (auto& r : it->second) {
        std::string url, top, bottom;

        std::visit([&](auto&& item) {
            using T = std::decay_t<decltype(item)>;

            if constexpr (std::is_same_v<T, music::SongRef>) {
                top    = item.title;
                bottom = item.artists.empty() ? "" : item.artists[0].name;
                url    = item.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::AlbumRef>) {
                top    = item.title;
                bottom = item.artists.empty() ? "" : item.artists[0].name;
                url    = item.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::ArtistRef>) {
                top    = item.name;
                bottom = "Artist";
                url    = item.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::PlaylistRef>) {
                top    = item.title;
                bottom = item.author;
                url    = item.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::PodcastRef>) {
                top    = item.name;
                bottom = "Podcast";
                url    = item.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::EpisodeRef>) {
                top    = item.title;
                bottom = item.podcast.name;
                url    = item.thumbnail_small;
            }
        }, r.data);

        data->results.push_back(r);
        data->entries.push_back({
            url,
            top,
            bottom
        });

        data->entries_spoof.push_back(url);
    }

    data->category_name = category;
    data->is_loading = state->flags["home"] == services::Player::Flags::Ongoing;
}

ftxui::Component ui::Grid(GridData* data, int* selected, bool* focused, int rows) {

    int total = data->entries.size();
    if (total == 0) {
        return Renderer([] {
            return text("Empty grid");
        });
    }

    int cols = (total + rows - 1) / rows;

    std::vector<std::vector<Component>> grid_components(rows);

    for (int index = 0; index < total; ++index) {

        int r = index % rows;
        int c = index / rows;

        const auto& item = data->entries[index];
        const auto& result = data->results[index];

        ButtonOption option;
        option.transform = [item, focused](const EntryState& state) {
            Element thumb;

            if (item.image_url.empty()) {
                thumb = filler() | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2);
            } else {
                thumb = image_view(item.image_url) | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2);
            }

            return hbox({
                thumb,
                text("  "),
                vbox({
                    text(utils::trimSuffix(item.top, 20, "...")) | ((state.focused && *focused)
                        ? color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | bold
                        : color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY))),

                    text(utils::trimSuffix(item.bottom, 20, "...")) | ((state.focused && *focused)
                        ? color(ui::GetColor(ui::MColor::TEXT_BOTTOM_PRIMARY))
                        : color(ui::GetColor(ui::MColor::TEXT_BOTTOM_SECONDARY))),
                })
            }) | size(WIDTH, EQUAL, 40) | size(HEIGHT, EQUAL, 3);
        };

        option.on_click = [item, result]() {
            spdlog::info("GRID: enter pressed on item, " + item.top + " " + item.bottom + " " + result.resultType);
        };

        grid_components[r].push_back(Button(option));
    }

    auto grid = GridContainer(grid_components);

    return Renderer(grid, [grid, data] {
        return vbox({
            text(""),
            text(""),
            hbox({
                text(" "),
                text(data->category_name),
            }),
            text(""),
            text(""),
            hbox({
                text(" "),
                grid->Render() | xframe
            }) | xframe,
        });
    });
}
