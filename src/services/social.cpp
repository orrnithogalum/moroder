#include <cstdint>
#define DISCORDPP_IMPLEMENTATION

#include "../../include/services/social.hpp"

#include "spdlog/spdlog.h"

#include <chrono>

namespace services {

Social::Social(uint64_t application_id) : app_id(application_id) {
    client = std::make_shared<discordpp::Client>();
    client->SetApplicationId(app_id);

    client->AddLogCallback([](const std::string& message, discordpp::LoggingSeverity severity) {
            switch (severity) {
                case discordpp::LoggingSeverity::Verbose:
                    spdlog::debug("{}", message);
                    break;
                case discordpp::LoggingSeverity::Info:
                    spdlog::info("{}", message);
                    break;
                case discordpp::LoggingSeverity::Warning:
                    spdlog::warn("{}", message);
                    break;
                case discordpp::LoggingSeverity::Error:
                    spdlog::error("{}", message);
                    break;
                case discordpp::LoggingSeverity::None:
                    spdlog::trace("{}", message);
                    break;
                default:
                    spdlog::info("{}", message);
                    break;
            }
        },
        discordpp::LoggingSeverity::Info
    );

    client->SetStatusChangedCallback([](discordpp::Client::Status status, discordpp::Client::Error error, int32_t detail) {
        spdlog::info("Social server: {}", discordpp::Client::StatusToString(status));

        if (error != discordpp::Client::Error::None) {
            spdlog::error("Social server error: {} ({})", discordpp::Client::ErrorToString(error), detail);
        }
    });

    worker = std::thread(&Social::threadLoop, this);
}

Social::~Social() {
    running = false;

    if (worker.joinable()) {
        worker.join();
    }
}

void Social::setStatus(const std::string& t, const std::string& a, const std::string& alb, const std::string& cover, uint64_t d) {
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

void Social::pause() {
    std::lock_guard lock(mutex);

    if (!has_status) return;

    paused = true;
    updatePresence();
}

void Social::resume() {
    std::lock_guard lock(mutex);

    if (!has_status) return;

    paused = false;

    start_time = std::time(nullptr);
    updatePresence();
}

void Social::setPosition(uint64_t p) {
    std::lock_guard lock(mutex);

    if (!has_status) return;

    start_time = std::time(nullptr) - (p / 1000000ULL);
    updatePresence();
}

void Social::removeStatus() {
    std::lock_guard lock(mutex);

    has_status = false;

    client->ClearRichPresence();
}

void Social::updatePresence() {
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

    client->UpdateRichPresence(activity,
        [](discordpp::ClientResult result)
        {
            if (!result.Successful()) {}
                spdlog::warn("Social server wasn't able to update status.");
        });
}

void Social::threadLoop() {
    while (running) {
        discordpp::RunCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

}