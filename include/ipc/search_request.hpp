#pragma once

#include "request.hpp"

namespace ipc {

class SearchRequest : public Request {
public:
    explicit SearchRequest(std::string query) : query_(std::move(query)) {}

    nlohmann::json to_json() const override {
        return {
            {"action", "search"},
            {"query", query_}
        };
    }

private:
    std::string query_;
};

}