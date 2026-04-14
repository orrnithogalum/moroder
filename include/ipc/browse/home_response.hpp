#pragma once

#include "../../models/api_result.hpp"
#include "../response.hpp"

#include <unordered_map>

namespace ipc {

class HomeResponse : public Response {
public:
    std::unordered_map<std::string, std::vector<music::ApiResult>> results;

    explicit HomeResponse() : Response() {}

    void addItem(const std::string& category, const nlohmann::json& j) {
        results[category].emplace_back(music::ApiResult::from_json(j));
    }
};

}
