#include "../../../include/ui/components/search_bar.hpp"
#include "../../../include/ui/constants/colors.hpp"

#include <ftxui/component/component_options.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

ftxui::Component ui::SearchBar(SearchBarData* data) {
    InputOption option;

    option.placeholder = "Search songs, albums, artists, podcasts";
    option.multiline = false;

    option.transform = [](const InputState& state) {
        Element e = state.element;

        if (state.focused)
            return e | color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | bold;

        return e | color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY));
    };

    option.on_enter = [data]() {
        data->onSearch(data->value);
    };

    auto input = Input(&data->value, option);

    return Renderer(input, [input] {
        return vbox({
            text(" "),
            hbox({
                text("  "),
                text("") | color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY)),
                text("  "),
                input->Render(),
            }),
        });
    });
}
