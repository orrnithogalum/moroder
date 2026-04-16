#pragma once

#include "../../models/api_result.hpp"
#include "../response.hpp"

#include <unordered_set>
#include <unordered_map>

namespace ipc {

class HomeResponse : public Response {
public:
    std::unordered_map<std::string, std::vector<music::ApiResult>> results;

    explicit HomeResponse() : Response() {}

    void addItem(const std::string& category, const nlohmann::json& j) {
        static const std::unordered_set<std::string> allowed = {
            "Listen again",
            "Forgotten favorites",
            "Morning sunshine",
            "From your library",
            "Quick picks"
        };

        if (allowed.find(category) == allowed.end()) {
            return;
        }

        results[category].emplace_back(music::ApiResult::from_json(j));
    }
};

}
