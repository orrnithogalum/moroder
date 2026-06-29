/*
When I was fifteen, sixteen, when I really started to code,
I definitely wanted to make a YouTube player.
It was almost impossible because—it was—the dream was so big
That I didn't see any chance because
I was living in a little town; was studying.
And when I finally broke away from school and became a programmer
I thought, "Well, now I may have a little bit of a chance"
Because all I really wanted to do is code
And not only code, but design something clean.

At that time, on the internet, in '09, '10, they already had TUIs,
So I would open my laptop, would go to a café,
Code maybe thirty minutes,
I think I had about seven, eight functions,
I would partially sleep at the keyboard
Because I didn't want to drive home,
And that helped me for about
Almost two years to survive in the beginning.

I wanted to make a player with the feel of the '80s,
The look of the '90s, the speed of the 2000s,
And then have an interface of the future.
And I said, "Wait a second, I know C++,
Why don't I use C++ which is the language of the future?"

And I didn't have any idea what to do,
But I knew I needed a loop, so I put a loop on the main thread
Which then was synced to the ftxui display.
I knew that could be the feel of the future,
*/

#pragma once

#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <cstdlib>
#include <string>
#include <cctype>

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

/* ensure_dir
- Throws on failure. The exception used to be constructed and then dropped, so
  a directory that could not be created carried on silently and only surfaced
  later as an unrelated file error
*/
inline void ensure_dir(const fs::path& path) {
    std::error_code ec;

    if (fs::exists(path, ec)) return;

    fs::create_directories(path, ec);

    // create_directories returns false when the directory already exists, so
    // the error code is what decides, not the return value.
    if (ec) {
        throw std::runtime_error("Failed to create: " + path.string() + " (" + ec.message() + ")");
    }
}

/* home
- Empty rather than throwing, for the callers that want to fall back instead
  of failing
*/
inline fs::path home() {
    const char* h = std::getenv("HOME");
    return h ? fs::path(h) : fs::path();
}

inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";

    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/* tildify
- The inverse of resolve_path: puts ~ back in front of a path under HOME, so
  what gets written to the config stays readable and portable between machines
*/
inline std::string tildify(const fs::path& path) {
    const fs::path h = home();
    if (h.empty()) return path.string();

    const std::string full = path.string();
    const std::string prefix = h.string();

    if (full.rfind(prefix, 0) == 0) {
        if (full.size() == prefix.size()) return "~";
        if (full[prefix.size()] == '/') return "~" + full.substr(prefix.size());
    }

    return full;
}

inline std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
};

inline std::string trimSuffix(const std::string& str, size_t max_size, const std::string& suffix) {
    if (str.size() <= max_size)
        return str;

    if (suffix.size() >= max_size)
        return suffix.substr(0, max_size);

    size_t cut = max_size - suffix.size();
    return str.substr(0, cut) + suffix;
}

inline std::string trimWordsSuffix(const std::string& str, size_t max_size, const std::string& suffix) {
    if (str.size() <= max_size)
        return str;

    if (suffix.size() >= max_size)
        return suffix.substr(0, max_size);

    size_t limit = max_size - suffix.size();

    std::string result;
    std::istringstream iss(str);
    std::string word;

    while (iss >> word) {
        std::string candidate = result.empty() ? word : result + " " + word;

        if (candidate.size() > limit)
            break;

        result = candidate;
    }

    if (result.empty())
        return str.substr(0, limit) + suffix;

    return result + suffix;
}

}
