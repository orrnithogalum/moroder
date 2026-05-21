#include "../../../include/ui/components/error.hpp"
#include "../../../include/ui/constants/colors.hpp"

#include <ftxui/dom/elements.hpp>

using namespace ftxui;

ftxui::Component ui::ErrorBox(const std::string& message, std::function<void()> on_retry, const std::string& title, const std::string& retry_label) {

    if (!on_retry) {
        return Renderer([message, title] {
            return vbox({
                filler(),
                hbox({
                    filler(),
                    vbox({
                        text(title) | bold | color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | center,
                        separator() | color(ui::GetColor(ui::MColor::SEPARATOR_PRIMARY)),
                        paragraph(message) | color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY)) | center,
                    }),
                    filler(),
                }),
                filler(),
            }) | flex;
        }) | flex;
    }

    ButtonOption opt;
    opt.transform = [retry_label](const EntryState& s) {
        return hbox({
            filler(),
            text(" " + retry_label + " ") | (s.focused
                ? color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | bold
                : color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY))),
            filler(),
        });
    };

    Component button = Button("", std::move(on_retry), opt);

    return Renderer(button, [message, title, button] {
        return vbox({
            filler(),
            hbox({
                filler(),
                vbox({
                    text(title) | bold | color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | center,
                    separator() | color(ui::GetColor(ui::MColor::SEPARATOR_PRIMARY)),
                    paragraph(message) | color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY)) | center,
                    text(" "),
                    button->Render(),
                }),
                filler(),
            }),
            filler(),
        }) | flex;
    }) | flex;
}
