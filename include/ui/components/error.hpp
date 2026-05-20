/* ERROR BOX
- A centred panel with a failure message and an optional retry button
- Reusable across home, search, library and anywhere else a request can fail
- The message is captured by value: rebuild the component when it changes, the same way the build* functions rebuild when their identity key changes
*/

#pragma once

#include <ftxui/component/component.hpp>

#include <functional>
#include <string>

namespace ui {

ftxui::Component ErrorBox(const std::string& message, std::function<void()> on_retry, const std::string& title = "Error :(", const std::string& retry_label = "Retry");

}
