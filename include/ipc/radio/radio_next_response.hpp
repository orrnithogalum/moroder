#pragma once

#include "../../models/song.hpp"
#include "../response.hpp"

namespace ipc {

class RadioNextResponse : public Response {
public:
    std::string seed_id;
    std::string continuation;
    std::vector<std::shared_ptr<music::IStreamable>> results;

    explicit RadioNextResponse() : Response() {}

    void addItem(const nlohmann::json& j) {
        results.push_back(std::make_shared<music::Song>(music::Song::from_json(j)));
    }
};

}
