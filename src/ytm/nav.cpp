#include "../../include/ytm/nav.hpp"

namespace ytm {

const json* nav(const json* root, const Path& path) noexcept {
    if (!root) return nullptr;

    const json* cur = root;

    for (const Key& k : path) {
        if (!cur || cur->is_null()) return nullptr;

        if (k.isIndex()) {
            if (!cur->is_array()) return nullptr;

            int size = static_cast<int>(cur->size());
            int i    = k.index() < 0 ? size + k.index() : k.index();

            if (i < 0 || i >= size) return nullptr;
            cur = &(*cur)[static_cast<size_t>(i)];

        } else {
            if (!cur->is_object()) return nullptr;

            auto it = cur->find(k.key());
            if (it == cur->end()) return nullptr;
            cur = &(*it);
        }
    }

    return cur;
}

std::string str(const json* j) {
    if (!j || !j->is_string()) return "";
    return j->get<std::string>();
}

std::string str(const json& j) {
    return str(&j);
}

std::string navStr(const json* root, const Path& path, const std::string& fallback) {
    const json* j = nav(root, path);
    if (!j || !j->is_string()) return fallback;
    return j->get<std::string>();
}

std::string navStr(const json& root, const Path& path, const std::string& fallback) {
    return navStr(&root, path, fallback);
}

json navJson(const json* root, const Path& path) {
    const json* j = nav(root, path);
    return j ? *j : json(nullptr);
}

json navJson(const json& root, const Path& path) {
    return navJson(&root, path);
}

bool navHas(const json* root, const Path& path) {
    return nav(root, path) != nullptr;
}

const json* findObjectByKey(const json* list, const std::string& key) {
    if (!list || !list->is_array()) return nullptr;

    for (const json& item : *list) {
        if (item.is_object() && item.contains(key)) return &item;
    }

    return nullptr;
}

std::vector<const json*> findObjectsByKey(const json* list, const std::string& key) {
    std::vector<const json*> out;
    if (!list || !list->is_array()) return out;

    for (const json& item : *list) {
        if (item.is_object() && item.contains(key)) out.push_back(&item);
    }

    return out;
}

}
