/* CONFIG
- This class handles loading Moroder's configuration from ~/.config/moroder/moroder.conf
- It provides default values if the file doesn't exist
- Singleton pattern ensures only one Config object exists at runtime
- The parser reads key=value lines, trims whitespace, and converts to the appropriate type
*/

#pragma once

#include "gen/defaults.hpp"

#include <string_view>
#include <filesystem>
#include <stdexcept>
#include <fstream>
#include <cstdlib>
#include <optional>
#include <string>

namespace fs = std::filesystem;

class Config {
public:
    inline static std::string_view app_name = "";

    std::string LASTFM_API_KEY;

    fs::path COOKIES_PATH;
    fs::path CACHE_PATH;

    int SEARCH_RESULT_LIMIT;
    int RADIO_RESULT_LIMIT;

    /* Lazy initialization using a lambda:
    - Ensures config is loaded once at first access
    - If the config file does not exist, create it with defaults
    - Parse the file, or throw if invalid
    */
    static const Config& get() {
        static Config instance = []() -> Config {
            if (!ensureConfigFile()) throw std::runtime_error("Cannot create config file");
            std::string path = getUserConfigPath();

            auto config = parseConfigFile(path);
            if (!config) throw std::runtime_error("Invalid config file");
            return *config;
        }();
        return instance;
    }

    /* Save the config file:
    - If user resizes the sidebar, then it's saved here
    - That is the only thing that can change programmatically
    - In case any other variables change programmatically in the future, rewrite everything.
    */
    bool writeToFile() const {
        std::string path = getUserConfigPath();
        if (path.empty()) return false;

        fs::path config_dir = fs::path(path).parent_path();
        if (!fs::exists(config_dir)) {
            if (!fs::create_directories(config_dir))
                return false;
        }

        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open()) return false;

        out << "LASTFM_API_KEY=\""
            << LASTFM_API_KEY << "\"\n";

        return true;
    }

private:
    // handles ~ in paths
    inline static fs::path expand_user(const std::string& path) {
        if (!path.empty() && path[0] == '~') {
            const char* home = std::getenv("HOME");
            if (home) {
                return fs::path(home) / path.substr(2);
            }
        }
        return fs::path(path);
    }

    // Returns path to ~/.config/moroder/moroder.conf or empty string if HOME not set
    inline static std::string getUserConfigPath() {
        const char* home = getenv("HOME");
        if (!home) { return ""; }

        return std::string(home) + "/.config/" + std::string(Config::app_name) + "/" + std::string(Config::app_name) + ".conf";
    }

    // Writes the default configuration binary to the given path
    inline static bool writeDefaultConfig(const std::string& path) {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) { return false; }

        out.write(reinterpret_cast<const char*>(moroder_default), moroder_default_len);
        return true;
    }

    /* Ensures the config file exists:
    - Creates parent directories if needed
    - Writes default config if file doesn't exist
    - Returns true if file is ready
    */
    inline static bool ensureConfigFile() {
        std::string config_path = getUserConfigPath();
        fs::path config_dir = fs::path(config_path).parent_path();

        if (!fs::exists(config_dir)) fs::create_directories(config_dir);
        if (!fs::exists(config_path)) return writeDefaultConfig(config_path);

        return true;
    }

    /* Reads a config file and populates a Config object:
    - Ignores empty lines and comments (#)
    - Splits lines by '=' into key/value
    - Trims leading/trailing spaces and tabs
    - Converts values to int, double, or string as appropriate
    - Special handling for CHART_COLORS array block
    */
    inline static std::optional<Config> parseConfigFile(const std::string& config_path) {
        std::ifstream in(config_path);
        if (!in.is_open()) return std::nullopt;

        Config cfg;
        std::string line;

        // Trim leading and trailing whitespace
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t"));
            s.erase(s.find_last_not_of(" \t") + 1);
        };

        while (std::getline(in, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#')
                continue;

            // Skip malformed lines
            size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);

            trim(key);
            trim(value);

            try {
                if (key == "LASTFM_API_KEY")
                    cfg.LASTFM_API_KEY = value.substr(1, value.size() - 2);

                else if(key == "COOKIES_PATH")
                    cfg.COOKIES_PATH = expand_user(value.substr(1, value.size() - 2));

                else if(key == "CACHE_PATH")
                    cfg.CACHE_PATH = value.substr(1, value.size() - 2);

                else if(key == "SEARCH_RESULT_LIMIT")
                    cfg.SEARCH_RESULT_LIMIT = std::stoi(value);

                else if(key == "RADIO_RESULT_LIMIT")
                    cfg.RADIO_RESULT_LIMIT = std::stoi(value);

            } catch (...) {
                return std::nullopt;
            }
        }

        return cfg;
    }
};
