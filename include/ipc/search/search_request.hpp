#pragma once

#include "../request.hpp"

namespace ipc {

class SearchRequest : public Request {
public:
    explicit SearchRequest(const std::string& query) : query_(std::move(query)) {}

    nlohmann::json to_json() const override {
        return {
            {"action", "search"},
            {"query", query_}
        };
    }

private:
    const std::string query_;
};

}