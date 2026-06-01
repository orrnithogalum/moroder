#pragma once

#include "../../models/podcast.hpp"
#include "../response.hpp"

namespace ipc {

class LibraryPodcastsResponse : public Response {
public:
    std::vector<music::PodcastRef> results;

    explicit LibraryPodcastsResponse() : Response() {}

    void addItem(const nlohmann::json& j) {
        results.emplace_back(music::PodcastRef::from_json(j));
    }
};

}
