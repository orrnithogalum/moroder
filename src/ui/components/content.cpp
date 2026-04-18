#include "../../../include/ui/components/content.hpp"
#include "../../../include/ui/constants/colors.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <type_traits>

#include "image_view.hpp"
#include "spdlog/spdlog.h"

using namespace ftxui;

void ui::buildHome(services::Player::PlayerState* state, std::deque<ContentEntry>& main_content_items, ftxui::Component main_content) {
    for (auto& [category, _] : state->home) {
        const std::string& category_ref = category;

        auto already_exists = std::any_of(
            main_content_items.begin(), main_content_items.end(),
            [&](const ContentEntry& item) {
                return item.category == category_ref;
            }
        );

        if (!already_exists) {
            main_content_items.push_back(ContentEntry{});
            ContentEntry& item = main_content_items.back();

            item.category = category;
            item.selected = 0;
            item.focused = false;

            if (utils::lower(category) == "quick picks") {
                ui::getGridData(state, category, &item.grid_data);
                item.component = Grid(&item.grid_data, &item.selected, &item.focused, 4);
            } else {
                item.component = Carousel(&item.data, &item.selected, &item.focused);
            }

            main_content->Add(item.component);
        }
    }

    for (auto& item : main_content_items) {
        ui::getCarouselData(state, item.category, &item.data);
        item.focused = item.component->Focused();
    }
}

void ui::buildSearch(services::Player::PlayerState* state, std::deque<ContentEntry>& main_content_items, ftxui::Component main_content) {
    std::vector<std::string> incoming_categories;

    for (auto& search_result : state->search_results) {
        std::string category;
        std::visit([&](auto&& result) {
            using T = std::decay_t<decltype(result)>;
            if constexpr (std::is_same_v<T, music::SongRef>)          category = result.id;
            else if constexpr (std::is_same_v<T, music::AlbumRef>)    category = result.id;
            else if constexpr (std::is_same_v<T, music::ArtistRef>)   category = result.id + result.name;
            else if constexpr (std::is_same_v<T, music::PlaylistRef>) category = result.id;
            else if constexpr (std::is_same_v<T, music::PodcastRef>)  category = result.id + result.name;
            else if constexpr (std::is_same_v<T, music::EpisodeRef>)  category = result.id;
        }, search_result.data);
        incoming_categories.push_back(category);
    }

    bool same = (main_content_items.size() == incoming_categories.size());
    if (same) {
        for (size_t i = 0; i < main_content_items.size(); i++) {
            if (main_content_items[i].category != incoming_categories[i]) { same = false; break; }
        }
    }
    if (same) return;

    main_content_items.clear();
    main_content->DetachAllChildren();

    for (auto& search_result : state->search_results) {
        std::string category, top, bottom, url;

        std::visit([&](auto&& result) {
            using T = std::decay_t<decltype(result)>;
            if constexpr (std::is_same_v<T, music::SongRef>) {
                category = result.id;
                top      = result.title;
                bottom   = !result.artists.empty() ? result.artists[0].name : "unknown";
                url      = result.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::AlbumRef>) {
                category = result.id;
                top      = result.title;
                bottom   = !result.artists.empty() ? result.artists[0].name : "unknown";
                url      = result.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::ArtistRef>) {
                category = result.id + result.name;
                top      = result.name;
                bottom   = "Artist";
                url      = result.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::PlaylistRef>) {
                category = result.id;
                top      = result.title;
                bottom   = !result.author.empty() ? result.author : "unknown";
                url      = result.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::PodcastRef>) {
                category = result.id + result.name;
                top      = result.name;
                bottom   = "Podcast";
                url      = result.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::EpisodeRef>) {
                category = result.id;
                top      = result.title;
                bottom   = !result.podcast.name.empty() ? result.podcast.name : "unknown";
                url      = result.thumbnail_small;
            }
        }, search_result.data);

        main_content_items.push_back(ContentEntry{});
        ContentEntry& item = main_content_items.back();

        item.selected = -1;
        item.category = category;
        item.focused  = false;
        item.result   = search_result;

        ButtonOption opt;
        opt.transform = [top_cap = top, bottom_cap = bottom, url_cap = url](const EntryState& s) {
            Element thumb = url_cap.empty()
                ? filler() | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2)
                : image_view(url_cap) | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2);

            auto row = hbox({
                thumb,
                text("  "),
                vbox({
                    text(top_cap) | (s.focused
                        ? color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | bold
                        : color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY))),
                    text(bottom_cap) | (s.focused
                        ? color(ui::GetColor(ui::MColor::TEXT_BOTTOM_PRIMARY))
                        : color(ui::GetColor(ui::MColor::TEXT_BOTTOM_SECONDARY))),
                }) | flex,
            }) | size(HEIGHT, EQUAL, 2);

            return vbox({
                text(" "),
                hbox({
                    text(" "),
                    row
                })
            });
        };

        item.component = Button("", [&item] {
            spdlog::info("SEARCH: clicked item ", item.category);
        }, opt);

        item.focused = false;
        main_content->Add(item.component);
    }

    for (auto& item : main_content_items) {
        item.focused = item.component->Focused();
    }
}
