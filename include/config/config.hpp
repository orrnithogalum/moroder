/* CONFIG
- This class handles loading Moroder's configuration from ~/.config/moroder/moroder.conf
- It provides default values if the file doesn't exist
- Singleton pattern ensures only one Config object exists at runtime
- The parser reads key=value lines, trims whitespace, and converts to the appropriate type
- Every field is declared once in fieldTable(); parsing and saving both derive from it
*/

#pragma once

#include "gen/defaults.hpp"

#include "../utils/utils.hpp"

#include <ftxui/component/event.hpp>
#include <string_view>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <iterator>
#include <optional>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <string>
#include <vector>
#include <array>

namespace fs = std::filesystem;

class Config {
public:
    inline static std::string_view app_name = "";

    inline static bool anonymous = false;

    int MAX_IMAGE_CACHE_SIZE = 800;
    int MAX_RESIZED_IMAGE_CACHE_SIZE = 3200;
    int MAX_IMAGE_CHAR_CACHE_SIZE = 800000;
    int MAX_CONCURRENT_IMAGE_LOADS = 200;

    std::string LASTFM_API_KEY;

    /* BROWSER
    - Which browser holds the YouTube Music session
    - `moroder setup` reads it to know where to look and writes back whatever
      was actually used, and MPV turns it into a yt-dlp cookie spec
    */
    std::string BROWSER = "firefox";

    fs::path YTM_COOKIES_PATH;
    fs::path MPV_COOKIES_PATH;

    int SEARCH_RESULT_LIMIT = 40;
    int RADIO_RESULT_LIMIT  = 50;

    bool FETCH_ALBUMS = true;
    bool EXTRA_BOTTOM_PADDING = false;

    bool ENABLE_DISCORD_RICH_PRESENCE = true;
    std::string RICH_PRESENCE_STATUS_LABEL = "song";

    /* Colours
    - {r, g, b}, each 0-255
    - A malformed or out of range value parses to {-1, -1, -1}, which the UI
      reads as "keep the built-in", so a typo dims one element rather than
      leaving the interface unreadable
    */
    std::array<int, 3> COLOR_ACCENT_PRIMARY        = {255,   0,   0};
    std::array<int, 3> COLOR_TEXT_TOP_PRIMARY      = {255, 255, 255};
    std::array<int, 3> COLOR_TEXT_TOP_SECONDARY    = {170, 170, 170};
    std::array<int, 3> COLOR_TEXT_BOTTOM_PRIMARY   = {170, 170, 170};
    std::array<int, 3> COLOR_TEXT_BOTTOM_SECONDARY = { 70,  70,  70};
    std::array<int, 3> COLOR_SEPARATOR_PRIMARY     = {100, 100, 100};
    std::array<int, 3> COLOR_SEPARATOR_SECONDARY   = { 50,  50,  50};

    // Home categories in display order, lower-cased. Empty = keep API order.
    std::vector<std::string> HOME_ORDER;

    /* Keybinds
    - Single printable characters ("q", "/", "1") or a named key from keyEvent()
    - Prefix with "ctrl+" for a control combo, e.g. "ctrl+n"
    - An empty value unbinds the action
    */
    std::string KEY_QUIT              = "q";
    std::string KEY_QUEUE_VIEW        = "a";
    std::string KEY_TOGGLE_SIDEBAR    = "s";
    std::string KEY_FOCUS_SEARCH      = "f";
    std::string KEY_SKIP_BACKWARD     = "z";
    std::string KEY_SKIP_FORWARD      = "x";
    std::string KEY_TOGGLE_PAUSE      = "space";
    std::string KEY_TOGGLE_LOOP       = "r";
    std::string KEY_PLAY_NOW          = "enter";
    std::string KEY_ADD_TO_QUEUE      = "d";
    std::string KEY_REMOVE_FROM_QUEUE = "c";

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

    /* Turns a config keybind string into an ftxui event
    - Named keys are matched case-insensitively
    - "ctrl+<letter>" maps to the corresponding control character
    - Anything else is taken as a literal character sequence
    - An empty bind returns a sentinel that no real key can produce
    */
    static ftxui::Event keyEvent(const std::string& bind) {
        if (bind.empty()) return ftxui::Event::Special("__moroder_unbound__");

        const std::string key = utils::lower(bind);

        if (key == "space")     return ftxui::Event::Character(" ");
        if (key == "enter")     return ftxui::Event::Return;
        if (key == "tab")       return ftxui::Event::Tab;
        if (key == "escape")    return ftxui::Event::Escape;
        if (key == "backspace") return ftxui::Event::Backspace;
        if (key == "delete")    return ftxui::Event::Delete;
        if (key == "up")        return ftxui::Event::ArrowUp;
        if (key == "down")      return ftxui::Event::ArrowDown;
        if (key == "left")      return ftxui::Event::ArrowLeft;
        if (key == "right")     return ftxui::Event::ArrowRight;
        if (key == "home")      return ftxui::Event::Home;
        if (key == "end")       return ftxui::Event::End;
        if (key == "pageup")    return ftxui::Event::PageUp;
        if (key == "pagedown")  return ftxui::Event::PageDown;

        // Ctrl combos are Special events holding the raw control byte,
        // so ctrl+a is 0x01, ctrl+b is 0x02, and so on.
        if (key.rfind("ctrl+", 0) == 0 && key.size() == 6) {
            char c = key[5];
            if (c >= 'a' && c <= 'z') {
                return ftxui::Event::Special(std::string(1, static_cast<char>(c - 'a' + 1)));
            }
        }

        return ftxui::Event::Character(bind);
    }

    // Convenience for call sites: `if (Config::isKey(event, cfg.KEY_QUIT))`
    static bool isKey(const ftxui::Event& event, const std::string& bind) {
        return event == keyEvent(bind);
    }

    /* Save the config file:
    - Starts from the embedded default config, not from the user's file, so keys
      added in newer versions (and their comments) appear after an upgrade
    - Passthrough fields keep the user's value verbatim, quotes, ~ and all
    - Any field this class can change at runtime is written from memory instead
    */
    bool writeToFile() const {
        std::string path = getUserConfigPath();
        if (path.empty())
            return false;

        std::ifstream in(path);
        if (!in.is_open())
            return false;

        std::string user_config(
            (std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>()
        );
        in.close();

        std::string config(
            reinterpret_cast<const char*>(moroder_default),
            moroder_default_len
        );

        // Every field's key lives in fieldTable() exactly once; passthrough
        // fields get copied verbatim from the user's existing file.
        for (const auto& field : fieldTable()) {
            if (field.passthrough) {
                copyValue(config, user_config, field.key);
            }
        }

        // Nothing is runtime-mutable yet. When something is, flip its
        // passthrough to false and write it from memory here, e.g.:
        // copyValue(config, "SEARCH_RESULT_LIMIT=" + std::to_string(SEARCH_RESULT_LIMIT), "SEARCH_RESULT_LIMIT");

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
            return false;

        out.write(config.data(), static_cast<std::streamsize>(config.size()));
        return out.good();
    }

    /* setValues
    - Rewrites individual keys in the user's config file, in place
    - Unlike writeToFile() the user's own file is the base rather than the
      embedded default, so their comments, ordering, spacing and any key this
      build does not know about all survive
    - A key that is not in the file yet is appended rather than dropped, which
      is what happens when an older config meets a newer build
    - The write goes through a temp file and a rename, so an interrupted save
      cannot leave a truncated config behind
    */
    static bool setValues(const std::vector<std::pair<std::string, std::string>>& updates) {
        if (!ensureConfigFile()) return false;

        const std::string path = getUserConfigPath();
        if (path.empty()) return false;

        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return false;

        std::string config(
            (std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>()
        );
        in.close();

        for (const auto& [key, value] : updates) {
            auto [start, len] = findValueSpan(config, key);

            if (start == std::string::npos) {
                if (!config.empty() && config.back() != '\n') config += "\n";
                config += key + " = " + value + "\n";
                continue;
            }

            // findValueSpan stops before any trailing comment, so an inline
            // comment on the line survives the replacement.
            config.replace(start, len, value);
        }

        const std::string temp = path + ".tmp";

        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out.is_open()) return false;

            out.write(config.data(), static_cast<std::streamsize>(config.size()));
            if (!out.good()) return false;
        }

        std::error_code ec;
        fs::rename(temp, path, ec);

        if (ec) {
            fs::remove(temp, ec);
            return false;
        }

        return true;
    }

    static bool setValue(const std::string& key, const std::string& value) {
        return setValues({{key, value}});
    }

    /* quote
    - Values written back are quoted the way the default config writes them,
      so a path with a space in it survives the next parse
    */
    static std::string quote(const std::string& value) {
        std::string out = "\"";

        for (char c : value) {
            if (c == '"' || c == '\\') out += '\\';
            out += c;
        }

        return out + "\"";
    }

    // Public so `moroder setup` can tell the user which file it edited.
    static std::string path() {
        return getUserConfigPath();
    }

    /* ytdlBrowser
    - The name yt-dlp knows a browser by, which is not always what the user
      typed: yt-dlp only accepts a fixed list
    - The Firefox forks all use an identical cookie store, so they go through
      as firefox, and only the profile path tells them apart
    - Empty means yt-dlp cannot read this browser
    */
    static std::string ytdlBrowser(const std::string& browser) {
        const std::string name = utils::lower(browser);

        // Anything already carrying a profile is passed through untouched.
        if (name.find(':') != std::string::npos) return name;

        if (name == "firefox"  || name == "librewolf" || name == "waterfox" ||
            name == "floorp"   || name == "zen"       || name == "mercury"  ||
            name == "icecat"   || name == "cachy"     || name == "tor") {
            return "firefox";
        }

        if (name == "chrome" || name == "chromium" || name == "brave" ||
            name == "edge"   || name == "opera"    || name == "vivaldi" ||
            name == "whale") {
            return name;
        }

        return "";
    }

    static bool isFirefoxFamily(const std::string& browser) {
        return ytdlBrowser(browser) == "firefox";
    }

    /* ytdlCookieSpec
    - What --cookies-from-browser, and so mpv's ytdl-raw-options, wants
    - A fork is indistinguishable from Firefox by name alone, so it only works
      with a profile path; plain Firefox and the Chromium browsers can find
      their own default profile without one
    - Empty means there is nothing usable to hand to yt-dlp
    */
    std::string ytdlCookieSpec() const {
        if (BROWSER.empty()) return "";

        const std::string name = ytdlBrowser(BROWSER);
        if (name.empty()) return "";

        // An explicit browser:profile from the config wins outright.
        if (name.find(':') != std::string::npos) return name;

        const std::string profile = MPV_COOKIES_PATH.string();

        if (!profile.empty() && fs::exists(MPV_COOKIES_PATH)) {
            return name + ":" + profile;
        }

        // A fork without a profile would be read as stock Firefox, whose
        // profile directory holds a different session or none at all.
        if (name == "firefox" && utils::lower(BROWSER) != "firefox") return "";

        return name;
    }

private:
    // Returns path to ~/.config/moroder/moroder.conf or empty string if HOME not set
    static std::string getUserConfigPath() {
        const fs::path home = utils::home();
        if (home.empty()) return "";

        const std::string name(Config::app_name);

        return (home / ".config" / name / (name + ".conf")).string();
    }

    // Writes the default configuration binary to the given path
    static bool writeDefaultConfig(const std::string& path) {
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
    static bool ensureConfigFile() {
        std::string config_path = getUserConfigPath();
        if (config_path.empty()) return false;

        try {
            utils::ensure_dir(fs::path(config_path).parent_path());

        } catch (const std::exception&) {
            // Used to be ignored, and the failure then resurfaced as an
            // unexplained "Cannot create config file".
            return false;
        }

        if (!fs::exists(config_path)) return writeDefaultConfig(config_path);

        return true;
    }

    static void stripComment(std::string& s) {
        bool in_quotes = false;

        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '"') in_quotes = !in_quotes;
            else if (s[i] == '#' && !in_quotes) { s.resize(i); return; }
        }
    }

    /* Finds the value for a given key in a config string
    - Returns the starting index of the value and its length
    - for example, findValueSpan("PORT=8080", "PORT") would return <5, 4>
    - Matching is line-anchored: the key must be the whole left-hand side of a
      non-comment line, so comments mentioning a key and keys that are
      substrings of other keys can't produce a false hit
    */
    static std::pair<size_t, size_t> findValueSpan(const std::string& source, const std::string& key) {
        size_t line_start = 0;

        while (line_start <= source.size()) {
            size_t line_end = source.find('\n', line_start);
            if (line_end == std::string::npos) line_end = source.size();

            size_t key_start = line_start;
            while (key_start < line_end &&
                   (source[key_start] == ' ' || source[key_start] == '\t')) {
                ++key_start;
            }

            size_t eq = source.find('=', key_start);

            if (key_start < line_end && source[key_start] != '#' &&
                eq != std::string::npos && eq < line_end) {

                size_t key_end = eq;
                while (key_end > key_start &&
                       (source[key_end - 1] == ' ' || source[key_end - 1] == '\t')) {
                    --key_end;
                }

                if (key_end - key_start == key.size() &&
                    source.compare(key_start, key.size(), key) == 0) {

                    size_t value_start = eq + 1;
                    while (value_start < line_end &&
                           (source[value_start] == ' ' || source[value_start] == '\t')) {
                        ++value_start;
                    }

                    size_t value_end = value_start;
                    bool in_quotes = false;

                    while (value_end < line_end) {
                        if (source[value_end] == '"') in_quotes = !in_quotes;
                        else if (source[value_end] == '#' && !in_quotes) break;
                        ++value_end;
                    }

                    while (value_end > value_start &&
                            (source[value_end - 1] == ' ' ||
                            source[value_end - 1] == '\t' ||
                            source[value_end - 1] == '\r')) {
                        --value_end;
                    }

                    return {value_start, value_end - value_start};
                }
            }

            if (line_end == source.size()) break;
            line_start = line_end + 1;
        }

        return {std::string::npos, 0};
    }

    /* Replaces the value for a given key in config with that key's value from source
    - for example, copyValue("PORT=8080", "PORT=3000", "PORT") would change "PORT=8080" to "PORT=3000"
    - Returns false when either side lacks the key, leaving config untouched
    */
    static bool copyValue(std::string& config, const std::string& source, const std::string& key) {
        auto [source_start, source_len] = findValueSpan(source, key);
        if (source_start == std::string::npos) return false;
        std::string value = source.substr(source_start, source_len);

        auto [config_start, config_len] = findValueSpan(config, key);
        if (config_start == std::string::npos) return false;

        config.replace(config_start, config_len, value);
        return true;
    }

    /* parseColor
    - Reads {r, g, b}, tolerating whatever spacing the user left around it
    - {-1, -1, -1} on anything malformed or out of range, which is the signal
      to fall back rather than a colour anyone could have meant
    */
    static std::array<int, 3> parseColor(const std::string& value) {
        static constexpr std::array<int, 3> invalid = {-1, -1, -1};

        std::stringstream ss(value);
        std::array<int, 3> color{};
        char separator;

        if (!(ss >> separator >> color[0] >> separator >> color[1] >> separator >> color[2])) {
            return invalid;
        }

        for (int component : color) {
            if (component < 0 || component > 255) return invalid;
        }

        return color;
    }

    static std::string unquote(const std::string& value) {
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            return value.substr(1, value.size() - 2);
        }
        return value;
    }

    // handles ~ in paths
    static fs::path expand_user(const std::string& path) {
        if (path == "~" || path.rfind("~/", 0) == 0) {
            const fs::path home = utils::home();

            if (!home.empty()) {
                return path.size() > 2 ? home / path.substr(2) : home;
            }
        }

        return fs::path(path);
    }

    // Splits "a, b, c" into a lower-cased, trimmed list
    static std::vector<std::string> parse_list(const std::string& value) {
        std::vector<std::string> out;
        std::stringstream ss(value);
        std::string item;

        while (std::getline(ss, item, ',')) {
            item = utils::lower(utils::trim(item));
            if (item.empty()) continue;

            out.push_back(item);
        }

        return out;
    }

    /* Declarative field table
    - Source for every "simple" config field (as in anything that is one key = one value)
    - Previously this lived as separate hand-written lists in parseConfigFile()
      and writeToFile() that had to be kept in sync by hand
    */
    static int parseValue(int*, const std::string& v) {
        return std::stoi(v);
    }

    static double parseValue(double*, const std::string& v) {
        return std::stod(v);
    }

    static bool parseValue(bool*, const std::string& v) {
        return v == "true";
    }

    static std::string parseValue(std::string*, const std::string& v) {
        return unquote(v);
    }

    static fs::path parseValue(fs::path*, const std::string& v) {
        return expand_user(unquote(v));
    }

    static std::vector<std::string> parseValue(std::vector<std::string>*, const std::string& v) {
        return parse_list(unquote(v));
    }

    static std::array<int, 3> parseValue(std::array<int, 3>*, const std::string& v) {
        return parseColor(v);
    }

    using Setter = std::function<void(Config&, const std::string&)>;

    struct FieldSpec {
        const char* key;
        Setter setter;
        bool passthrough;
    };

    /* Builds a Setter for `member` with zero repetition of its type.
    - T is deduced from the member pointer.
    - parseValue() is picked by overload resolution on that same T.
    */
    template <typename T> static Setter makeSetter(T Config::* member) {
        return [member](Config& c, const std::string& v) {
            c.*member = parseValue(static_cast<T*>(nullptr), v);
        };
    }

    static const std::vector<FieldSpec>& fieldTable() {
        static const std::vector<FieldSpec> table = {
            {"MAX_IMAGE_CACHE_SIZE",         makeSetter(&Config::MAX_IMAGE_CACHE_SIZE),         true},
            {"MAX_RESIZED_IMAGE_CACHE_SIZE", makeSetter(&Config::MAX_RESIZED_IMAGE_CACHE_SIZE), true},
            {"MAX_IMAGE_CHAR_CACHE_SIZE",    makeSetter(&Config::MAX_IMAGE_CHAR_CACHE_SIZE),    true},
            {"MAX_CONCURRENT_IMAGE_LOADS",   makeSetter(&Config::MAX_CONCURRENT_IMAGE_LOADS),   true},
            {"ENABLE_DISCORD_RICH_PRESENCE", makeSetter(&Config::ENABLE_DISCORD_RICH_PRESENCE), true},
            {"RICH_PRESENCE_STATUS_LABEL",   makeSetter(&Config::RICH_PRESENCE_STATUS_LABEL),   true},

            {"LASTFM_API_KEY",       makeSetter(&Config::LASTFM_API_KEY),        true},
            {"BROWSER",              makeSetter(&Config::BROWSER),               true},
            {"YTM_COOKIES_PATH",     makeSetter(&Config::YTM_COOKIES_PATH),      true},
            {"MPV_COOKIES_PATH",     makeSetter(&Config::MPV_COOKIES_PATH),      true},
            {"SEARCH_RESULT_LIMIT",  makeSetter(&Config::SEARCH_RESULT_LIMIT),   true},
            {"RADIO_RESULT_LIMIT",   makeSetter(&Config::RADIO_RESULT_LIMIT),    true},
            {"FETCH_ALBUMS",         makeSetter(&Config::FETCH_ALBUMS),          true},
            {"EXTRA_BOTTOM_PADDING", makeSetter(&Config::EXTRA_BOTTOM_PADDING),  true},
            {"HOME_ORDER",           makeSetter(&Config::HOME_ORDER),            true},

            {"COLOR_ACCENT_PRIMARY",        makeSetter(&Config::COLOR_ACCENT_PRIMARY),        true},
            {"COLOR_TEXT_TOP_PRIMARY",      makeSetter(&Config::COLOR_TEXT_TOP_PRIMARY),      true},
            {"COLOR_TEXT_TOP_SECONDARY",    makeSetter(&Config::COLOR_TEXT_TOP_SECONDARY),    true},
            {"COLOR_TEXT_BOTTOM_PRIMARY",   makeSetter(&Config::COLOR_TEXT_BOTTOM_PRIMARY),   true},
            {"COLOR_TEXT_BOTTOM_SECONDARY", makeSetter(&Config::COLOR_TEXT_BOTTOM_SECONDARY), true},
            {"COLOR_SEPARATOR_PRIMARY",     makeSetter(&Config::COLOR_SEPARATOR_PRIMARY),     true},
            {"COLOR_SEPARATOR_SECONDARY",   makeSetter(&Config::COLOR_SEPARATOR_SECONDARY),   true},
            {"KEY_QUIT",             makeSetter(&Config::KEY_QUIT),              true},
            {"KEY_QUEUE_VIEW",       makeSetter(&Config::KEY_QUEUE_VIEW),        true},
            {"KEY_TOGGLE_SIDEBAR",   makeSetter(&Config::KEY_TOGGLE_SIDEBAR),    true},
            {"KEY_FOCUS_SEARCH",     makeSetter(&Config::KEY_FOCUS_SEARCH),      true},
            {"KEY_SKIP_BACKWARD",    makeSetter(&Config::KEY_SKIP_BACKWARD),     true},
            {"KEY_SKIP_FORWARD",     makeSetter(&Config::KEY_SKIP_FORWARD),      true},
            {"KEY_TOGGLE_PAUSE",     makeSetter(&Config::KEY_TOGGLE_PAUSE),      true},
            {"KEY_TOGGLE_LOOP",      makeSetter(&Config::KEY_TOGGLE_LOOP),       true},
            {"KEY_PLAY_NOW",         makeSetter(&Config::KEY_PLAY_NOW),          true},
            {"KEY_ADD_TO_QUEUE",     makeSetter(&Config::KEY_ADD_TO_QUEUE),      true},
            {"KEY_REMOVE_FROM_QUEUE",makeSetter(&Config::KEY_REMOVE_FROM_QUEUE), true},
        };

        return table;
    }

    static const FieldSpec* findField(const std::string& key) {
        for (const auto& field : fieldTable()) {
            if (key == field.key) return &field;
        }
        return nullptr;
    }

    /* Reads a config file and populates a Config object:
    - Ignores empty lines and comments (#)
    - Splits lines by '=' into key/value
    - Trims leading/trailing spaces and tabs
    - Converts values via the field table's setters
    - Unknown keys are silently ignored
    */
    static std::optional<Config> parseConfigFile(const std::string& config_path) {
        std::ifstream in(config_path);
        if (!in.is_open()) return std::nullopt;

        Config cfg;
        std::string line;

        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#')
                continue;

            size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);

            key = utils::trim(key);

            stripComment(value);
            value = utils::trim(value);

            try {
                if (const FieldSpec* field = findField(key)) {
                    field->setter(cfg, value);
                }
            } catch (...) {
                return std::nullopt;
            }
        }

        return cfg;
    }
};
