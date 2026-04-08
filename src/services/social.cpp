#define DISCORDPP_IMPLEMENTATION

#include "../../include/services/social.hpp"

#include "spdlog/spdlog.h"

#include <algorithm>

services::Social::Social(const uint64_t application_id) : app_id(application_id) {
    client = std::make_shared<discordpp::Client>();
    client->SetApplicationId(app_id);

    client->AddLogCallback([](const std::string& message, discordpp::LoggingSeverity severity) {
            // Remove trailing line returns
            std::string clean = message;
            std::replace(clean.begin(), clean.end(), '\n', ' ');
            std::replace(clean.begin(), clean.end(), '\r', ' ');

            // split on first '):' and keep the rest
            auto pos = clean.find("):");
            if (pos != std::string::npos) {
                clean = clean.substr(pos + 2);
                clean.erase(0, clean.find_first_not_of(" "));
            }

            switch (severity) {
                case discordpp::LoggingSeverity::Verbose:
                    spdlog::debug("SOCIAL: {}", clean);
                    break;
                case discordpp::LoggingSeverity::Info:
                    spdlog::info("SOCIAL: {}", clean);
                    break;
                case discordpp::LoggingSeverity::Warning:
                    spdlog::warn("SOCIAL: {}", clean);
                    break;
                case discordpp::LoggingSeverity::Error:
                    spdlog::error("SOCIAL: {}", clean);
                    break;
                case discordpp::LoggingSeverity::None:
                    spdlog::trace("SOCIAL: {}", clean);
                    break;
                default:
                    spdlog::info("SOCIAL: {}", clean);
                    break;
            }
        },
        discordpp::LoggingSeverity::Info
    );

    client->SetStatusChangedCallback([](discordpp::Client::Status status, discordpp::Client::Error error, int32_t detail) {
        spdlog::info("SOCIAL: {}", discordpp::Client::StatusToString(status));

        if (error != discordpp::Client::Error::None) {
            spdlog::error("SOCIAL error: {} ({})", discordpp::Client::ErrorToString(error), detail);
        }
    });

    worker = std::thread(&Social::threadLoop, this);
}

services::Social::~Social() {
    running = false;

    if (worker.joinable()) {
        worker.join();
    }
}

void services::Social::setStatus(const std::string& t, const std::string& a, const std::string& alb, const std::string& cover, uint64_t d) {
    std::lock_guard lock(mutex);

    this->title = t;
    this->author = a;
    this->album = alb;
    this->cover_url = cover;
    this->duration = d / 1000000ULL;;

    this->start_time = std::time(nullptr);
    this->paused = false;
    this->has_status = true;

    this->updatePresence();
}

void services::Social::pause() {
    std::lock_guard lock(mutex);

    if (!has_status) return;

    paused = true;
    updatePresence();
}

void services::Social::resume() {
    std::lock_guard lock(mutex);

    if (!has_status) return;

    paused = false;

    start_time = std::time(nullptr);
    updatePresence();
}

void services::Social::setPosition(const uint64_t p) {
    std::lock_guard lock(mutex);

    if (!has_status) return;

    start_time = std::time(nullptr) - (p / 1000000ULL);
    updatePresence();
}

void services::Social::removeStatus() {
    std::lock_guard lock(mutex);

    has_status = false;

    client->ClearRichPresence();
}

void services::Social::updatePresence() {
    if (!has_status)
        return;

    discordpp::Activity activity;

    activity.SetType(discordpp::ActivityTypes::Listening);
    activity.SetDetails(title);
    activity.SetState(author);
    activity.SetStatusDisplayType(discordpp::StatusDisplayTypes::Details);

    if (!paused) {
        discordpp::ActivityTimestamps timestamps;
        timestamps.SetStart(start_time);
        timestamps.SetEnd(start_time + duration);
        activity.SetTimestamps(timestamps);

    } else {
        activity.Timestamps().reset();
    }

    discordpp::ActivityAssets assets;

    if (!cover_url.empty())
        assets.SetLargeImage(cover_url);

    if (!album.empty())
        assets.SetLargeText(album);

    activity.SetAssets(assets);

    client->UpdateRichPresence(activity, [](discordpp::ClientResult result) {
        if (!result.Successful()) {
            spdlog::warn("SOCIAL: couldn't update status.");
        }
    });
}

void services::Social::threadLoop() {
    while (running) {
        discordpp::RunCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
