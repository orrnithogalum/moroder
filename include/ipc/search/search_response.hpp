#pragma once

#include "../../models/search_result.hpp"
#include "../response.hpp"

namespace ipc {

class SearchResponse : public Response {
public:
    std::vector<music::SearchResult> results;

    explicit SearchResponse() : Response() {}

    void addItem(const nlohmann::json& j) {
        results.push_back(music::SearchResult::from_json(j));
    }
};

}
