#pragma once

#include "../../models/song.hpp"
#include "../response.hpp"

namespace ipc {

class RadioResponse : public Response {
public:
    std::vector<std::shared_ptr<music::IStreamable>> results;

    explicit RadioResponse() : Response() {}

    void addItem(const nlohmann::json& j) {
        results.push_back(std::make_shared<music::Song>(music::Song::from_json(j)));
    }
};

}
