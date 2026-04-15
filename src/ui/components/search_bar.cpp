#include "../../../include/ui/components/search_bar.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace ui {

ftxui::Component SearchBar(SearchBarData* data) {
    InputOption option;

    option.placeholder = "Search songs, albums, artists, podcasts";

    option.transform = [](const InputState& state) {
        Element e = state.element;

        if (state.focused)
            return e | color(Color::White) | bold;

        return e | color(Color::RGB(170, 170, 170));
    };

    auto input = Input(&data->value, option);

    return Renderer(input, [input] {
        return vbox({
            text(" "),
            hbox({
                text("  "),
                text("") | color(Color::RGB(170, 170, 170)),
                text("  "),
                input->Render(),
            }),
        });
    });
}

}
