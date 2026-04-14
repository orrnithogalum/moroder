#pragma once

#include "../../models/api_result.hpp"
#include "../response.hpp"

namespace ipc {

class SearchResponse : public Response {
public:
    std::vector<music::ApiResult> results;

    explicit SearchResponse() : Response() {}

    void addItem(const nlohmann::json& j) {
        results.emplace_back(music::ApiResult::from_json(j));
    }
};

}
