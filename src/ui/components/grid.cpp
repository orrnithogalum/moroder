#include "../../../include/ui/components/grid.hpp"
#include "../../../include/ui/constants/colors.hpp"
#include "../../../include/utils/utils.hpp"

#include "ftxui-grid-container/grid-container.hpp"
#include "spdlog/spdlog.h"
#include "image_view.hpp"

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <string>

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

void ui::getLibraryGridData(services::Player::PlayerState* state, const std::string& filter, const std::string& query, GridData* data) {

    data->entries.clear();
    data->entries_spoof.clear();
    data->results.clear();

    const bool all = filter.empty();

    /* needle
    - Every category funnels through push(), so matching here covers all five
    - Matched against the title and the artist / author only, deliberately not
      the type label, otherwise typing "album" would return the whole shelf
    */
    const std::string needle = utils::lower(query);

    auto push = [&](const std::string& url, const std::string& top, const std::string& type_label, const std::string& detail, const std::string& result_type, music::ResultData ref) {

        if (!needle.empty()) {
            const std::string haystack = utils::lower(top) + " " + utils::lower(detail);

            if (haystack.find(needle) == std::string::npos) {
                return;
            }
        }

        music::ApiResult result;
        result.category = "library";
        result.resultType = result_type;
        result.data = std::move(ref);

        data->results.push_back(result);
        data->entries.push_back({
            url,
            top,
            detail.empty() ? type_label : type_label + " • " + detail
        });

        data->entries_spoof.push_back(url);
    };

    if (all || filter == "albums") {
        for (auto& album : state->library_albums) {
            push(
                album.ref.thumbnail_large,
                album.ref.title,
                "Album",
                album.ref.artists.empty() ? "" : album.ref.artists[0].name,
                "album",
                album.ref
            );
        }
    }

    if (all || filter == "playlists") {
        for (auto& playlist : state->library_playlists) {
            push(
                playlist.ref.thumbnail_large,
                playlist.ref.title,
                "Playlist",
                playlist.ref.author,
                "playlist",
                playlist.ref
            );
        }
    }

    // Songs are deliberately excluded from the unfiltered view, there are simply
    // too many of them to sit alongside everything else.
    if (!all && filter == "songs") {
        for (auto& streamable : state->library_songs) {
            auto song = std::dynamic_pointer_cast<music::Song>(streamable);
            if (!song) continue;

            push(
                song->ref.thumbnail_large,
                song->ref.title,
                "Song",
                song->ref.artists.empty() ? "" : song->ref.artists[0].name,
                "song",
                song->ref
            );
        }
    }

    if (all || filter == "artists") {
        for (auto& artist : state->library_artists) {
            push(
                artist.thumbnail_large,
                artist.name,
                "Artist",
                "",
                "artist",
                artist
            );
        }
    }

    if (all || filter == "podcasts") {
        for (auto& podcast : state->library_podcasts) {
            push(
                podcast.thumbnail_large,
                podcast.name,
                "Podcast",
                "",
                "podcast",
                podcast
            );
        }
    }

    auto ongoing = [&](const std::string& flag) {
        return state->flags[flag] == services::Player::Flags::Ongoing;
    };

    data->is_loading =
        (all || filter == "playlists") ? ongoing("library_playlists") : false;

    data->is_loading = data->is_loading
        || ((all || filter == "albums")   && ongoing("library_albums"))
        || ((!all && filter == "songs")   && ongoing("library_songs"))
        || ((all || filter == "artists")  && ongoing("library_artists"))
        || ((all || filter == "podcasts") && ongoing("library_podcasts"));

    data->category_name = "";
}

ftxui::Component ui::Grid(GridData* data, int* selected, bool* focused, int rows, std::function<bool(const ftxui::Event&, const music::ApiResult&)> on_press, GridStyle style) {

    int total = data->entries.size();
    if (total == 0) {
        return Renderer([] {
            return emptyElement();
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

        if (style == GridStyle::Tile) {
            auto self = std::make_shared<std::weak_ptr<ftxui::ComponentBase>>();

            option.transform = [item, self](const EntryState&) {
                auto cell = self->lock();
                bool active = cell && cell->Focused();

                Element cover = item.image_url.empty()
                    ? filler() | size(WIDTH, EQUAL, ui::GRID_TILE_COVER_WIDTH) | size(HEIGHT, EQUAL, ui::GRID_TILE_COVER_HEIGHT)
                    : image_view(item.image_url) | size(WIDTH, EQUAL, ui::GRID_TILE_COVER_WIDTH) | size(HEIGHT, EQUAL, ui::GRID_TILE_COVER_HEIGHT);

                return hbox({
                    vbox({
                        cover,
                        text(" "),
                        hbox({
                            vbox({
                                text(utils::trimSuffix(item.top, ui::GRID_TILE_COVER_WIDTH, "...")) | (active
                                    ? color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | bold
                                    : color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY))),

                                text(utils::trimSuffix(item.bottom, ui::GRID_TILE_COVER_WIDTH, "...")) | (active
                                    ? color(ui::GetColor(ui::MColor::TEXT_BOTTOM_PRIMARY))
                                    : color(ui::GetColor(ui::MColor::TEXT_BOTTOM_SECONDARY))),
                            }),
                        }),
                    }) | size(WIDTH, EQUAL, ui::GRID_TILE_WIDTH - 1) | size(HEIGHT, EQUAL, ui::GRID_TILE_HEIGHT),
                    text(" "),
                });
            };

            option.on_click = [item, result]() {
                spdlog::info("GRID: enter pressed on item, " + item.top + " " + item.bottom + " " + result.resultType);
            };

            auto button = Button(option);
            *self = button;

            grid_components[r].push_back(
                CatchEvent(button, [result, on_press](const ftxui::Event& event) {
                    return on_press(event, result);
                })
            );

            continue;
        }

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

        grid_components[r].push_back(
            CatchEvent(Button(option), [result, on_press](const ftxui::Event& event) {
                return on_press(event, result);
            })
        );
    }

    grid_components.erase(
        std::remove_if(
            grid_components.begin(),
            grid_components.end(),
            [](const std::vector<Component>& row) { return row.empty(); }
        ),
        grid_components.end()
    );

    auto grid = GridContainer(grid_components);

    return Renderer(grid, [grid, data] {
        if (data->category_name.empty()) {
            return vbox({
                text(""),
                hbox({
                    text(" "),
                    grid->Render() | xframe
                }) | xframe,
            });
        }

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
