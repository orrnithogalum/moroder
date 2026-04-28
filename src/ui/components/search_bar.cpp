#include "../../../include/ui/components/search_bar.hpp"
#include "../../../include/ui/constants/colors.hpp"

#include <ftxui/component/component_options.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

ftxui::Component ui::SearchBar(SearchBarData* data) {
    InputOption option;

    option.placeholder = "Search songs, albums, artists, podcasts";
    option.multiline = false;

    option.transform = [data](const InputState& state) {
        Element e = state.element;

        if (!data->value.empty())
            return e | color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | bold;

        return e | color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY));
    };

    option.on_enter = [data]() {
        data->onSearch(data->value);
    };

    auto input = Input(&data->value, option);

    input = CatchEvent(input, [](Event event){
        if(event == Event::ArrowUp) {
            return true;
        }

        return false;
    });

    return Renderer(input, [input] {
        return vbox({
            text(" "),
            hbox({
                text("  "),
                text("") | (input->Focused() ? color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) : color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY))),
                text("  "),
                input->Render(),
            }),
        });
    });
}
