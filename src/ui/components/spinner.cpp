#include "../../../include/ui/components/spinner.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <atomic>
#include <memory>

using namespace ftxui;

namespace ui {

struct SpinnerState {
    std::atomic<int> frame{0};
};

ftxui::Component Spinner() {
    auto state = std::make_shared<SpinnerState>();

    return Renderer([state] {
        state->frame++;

        return spinner(15, state->frame.load()) | center;
    });
}

}
