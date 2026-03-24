#pragma once

#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace utils {

inline fs::path resolve_path(const char* path_macro) {
    fs::path p(path_macro);

    if (p.is_absolute()) {
        return p;
    }

    const char* home = std::getenv("HOME");
    if (!home) {
        throw std::runtime_error("HOME not set");
    }

    return fs::path(home) / p;
}

inline void ensure_dir(const fs::path& path) {
    std::error_code ec;

    if (!fs::exists(path, ec)) {
        if (!fs::create_directories(path, ec)) {
            std::runtime_error("Failed to create: " + path.string() + " (" + ec.message() + ")\n");
        }
    }
}

}
