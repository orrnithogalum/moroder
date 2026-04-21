#include "../../../include/ui/components/content.hpp"
#include "../../../include/ui/constants/colors.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
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

void ui::buildSearch(services::Player::PlayerState* state, std::deque<ContentEntry>& main_content_items, ftxui::Component main_content, std::function<bool(const ftxui::Event&, const music::ApiResult&)> on_search_result_press) {
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
                bottom   = !result.artists.empty() ? result.artists[0].name : "";
                url      = result.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::AlbumRef>) {
                category = result.id;
                top      = result.title;
                bottom   = !result.artists.empty() ? result.artists[0].name : "";
                url      = result.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::ArtistRef>) {
                category = result.id + result.name;
                top      = result.name;
                bottom   = "Artist";
                url      = result.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::PlaylistRef>) {
                category = result.id;
                top      = result.title;
                bottom   = result.author;
                url      = result.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::PodcastRef>) {
                category = result.id + result.name;
                top      = result.name;
                bottom   = "Podcast";
                url      = result.thumbnail_small;

            } else if constexpr (std::is_same_v<T, music::EpisodeRef>) {
                category = result.id;
                top      = result.title;
                bottom   = result.podcast.name;
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

        auto button = Button("", [] {}, opt);

        item.component = CatchEvent(button, [on_search_result_press, result = search_result](ftxui::Event event) {
            return on_search_result_press(event, result);
        });

        item.focused = false;
        main_content->Add(item.component);
    }

    for (auto& item : main_content_items) {
        item.focused = item.component->Focused();
    }
}

void ui::buildQueue(services::Player::PlayerState* state, std::deque<ContentEntry>& main_content_items, ftxui::Component main_content) {
    static std::string last_current_id;
    std::string current_id;

    if (state->current) {
        if (auto song = std::dynamic_pointer_cast<music::Song>(state->current))
            current_id = song->ref.id;
        else if (auto ep = std::dynamic_pointer_cast<music::Episode>(state->current))
            current_id = ep->ref.id;
    }

    std::deque<ui::QueueEntry> incoming_rows;

    if (!state->user_queue.empty()) {
        incoming_rows.push_back({ "__divider_user__", "", "", "", true, "Next in queue" });
    }

    for (auto& item : state->user_queue) {
        QueueEntry row;
        if (auto song = std::dynamic_pointer_cast<music::Song>(item)) {
            row.id     = song->ref.id;
            row.top    = song->ref.title;
            row.bottom = !song->ref.artists.empty() ? song->ref.artists[0].name : "";
            row.url    = song->ref.thumbnail_small;

        } else if (auto ep = std::dynamic_pointer_cast<music::Episode>(item)) {
            row.id     = ep->ref.id;
            row.top    = ep->ref.title;
            row.bottom = ep->ref.podcast.name;
            row.url    = ep->ref.thumbnail_small;

        } else {
            continue;
        }

        incoming_rows.push_back(row);
    }

    if (state->autoplay && !state->radio_queue.empty()) {
        incoming_rows.push_back({ "__divider_radio__", "", "", "", true, "Autoplay is on" });

        for (auto& item : state->radio_queue) {
            QueueEntry row;
            if (auto song = std::dynamic_pointer_cast<music::Song>(item)) {
                row.id     = song->ref.id;
                row.top    = song->ref.title;
                row.bottom = !song->ref.artists.empty() ? song->ref.artists[0].name : "";
                row.url    = song->ref.thumbnail_small;

            } else if (auto ep = std::dynamic_pointer_cast<music::Episode>(item)) {
                row.id     = ep->ref.id;
                row.top    = ep->ref.title;
                row.bottom = ep->ref.podcast.name;
                row.url    = ep->ref.thumbnail_small;
            } else {
                continue;
            }

            incoming_rows.push_back(row);
        }
    }

    bool same = (main_content_items.size() == incoming_rows.size());
    if (same) {
        for (size_t i = 0; i < main_content_items.size(); i++) {
            if (main_content_items[i].category != incoming_rows[i].id) { same = false; break; }
        }
    }

    if (same && current_id == last_current_id) {
        return;
    }

    last_current_id = current_id;

    std::string thumbnail_url;
    if (state->current) {
        if (auto song = std::dynamic_pointer_cast<music::Song>(state->current))
            thumbnail_url = song->ref.thumbnail_large;
        else if (auto ep = std::dynamic_pointer_cast<music::Episode>(state->current))
            thumbnail_url = ep->ref.thumbnail_large;
    }

    main_content_items.clear();
    main_content->DetachAllChildren();

    auto queue_container = Container::Vertical({});

    for (auto& row : incoming_rows) {
        main_content_items.push_back(ContentEntry{});
        ContentEntry& entry = main_content_items.back();

        entry.category = row.id;
        entry.selected = -1;
        entry.focused  = false;

        if (row.is_divider) {
            std::string label = row.divider_label;

            entry.component = Renderer([label] {
                return vbox({
                    text(" "),
                    text(" "),
                    text(label) | color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)),
                    separator() | color(ui::GetColor(ui::MColor::SEPARATOR_SECONDARY)),
                });
            });

        } else {
            ButtonOption opt;
            opt.transform = [top_cap = row.top, bottom_cap = row.bottom, url_cap = row.url](const EntryState& s) {

                Element thumb = url_cap.empty()
                    ? filler() | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2)
                    : image_view(url_cap) | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2);

                auto row_el = hbox({
                    thumb,
                    text("  "),
                    vbox({
                        text(utils::trimSuffix(top_cap, 20, "...")) | (s.focused
                            ? color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | bold
                            : color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY))),

                        text(utils::trimSuffix(bottom_cap, 20, "...")) | (s.focused
                            ? color(ui::GetColor(ui::MColor::TEXT_BOTTOM_PRIMARY))
                            : color(ui::GetColor(ui::MColor::TEXT_BOTTOM_SECONDARY))),
                    }) | flex,
                }) | size(HEIGHT, EQUAL, 2);

                return vbox({
                    text(" "),
                    row_el
                });
            };

            entry.component = Button("", [] {
                spdlog::info("QUEUE: clicked item");
            }, opt);
        }

        queue_container->Add(entry.component);
    }

    if(queue_container->ChildCount() > state->queue_position + 2) {
        queue_container->SetActiveChild(queue_container->ChildAt(state->queue_position + 1));
    }

    auto queue_wrapper = Renderer(queue_container, [thumbnail_url, queue_container] {
        return hbox({
            filler(),
            vbox({
                filler(),
                image_view(thumbnail_url) | size(WIDTH, EQUAL, 60) | size(HEIGHT, EQUAL, 30),
                filler()
            }),
            filler(),
            vbox({
                queue_container->Render() | yframe | yflex | size(WIDTH, ftxui::GREATER_THAN, 30),
            }) | yflex,
        }) | yflex;
    });

    main_content->Add(queue_wrapper);

    for (auto& entry : main_content_items) {
        entry.focused = entry.component->Focused();
    }
}
