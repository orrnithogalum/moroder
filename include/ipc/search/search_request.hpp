/* SEARCH
- Returns search results for a given query.
- Python endpoint in python/services/search.py
*/

#pragma once

#include "../../config/config.hpp"
#include "../request.hpp"

namespace ipc {

class SearchRequest : public Request {
public:
    explicit SearchRequest(const std::string& query) : query_(std::move(query)) {}

    nlohmann::json to_json() const override {
        Config cfg = Config::get();
        return {
            {"action", "search"},
            {"limit", cfg.SEARCH_RESULT_LIMIT},
            {"query", query_}
        };
    }

private:
    const std::string query_;
};

}
