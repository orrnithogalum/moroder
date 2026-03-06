#pragma once

#include "../../models/search_result.hpp"
#include "../response.hpp"

namespace ipc {

class SearchResponse : public Response {
public:
    std::vector<SearchResult> results;

    explicit SearchResponse() : Response() {}

    explicit SearchResponse(const std::string& raw) : Response(raw) {
        if (!ok() || !json_.contains("results")) {
            return;
        }

        for (const auto& item : json_["results"]) {
            results.push_back(SearchResult::from_json(item));
        }
    }
};

}