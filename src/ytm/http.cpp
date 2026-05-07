#include "../../include/ytm/http.hpp"

#include <curl/curl.h>

#include <mutex>

namespace {

std::once_flag curl_init_flag;

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto*  out = static_cast<std::string*>(userdata);
    size_t n   = size * nmemb;
    out->append(ptr, n);
    return n;
}

}

namespace ytm {

Http::Http() {
    std::call_once(curl_init_flag, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    curl = curl_easy_init();
}

Http::~Http() {
    if (curl) curl_easy_cleanup(curl);
}

std::string Http::urlEncode(const std::string& value) {
    // Uses curl's escaper so the behaviour matches what the server expects.
    CURL* tmp = curl_easy_init();
    if (!tmp) return value;

    char* escaped = curl_easy_escape(tmp, value.c_str(), static_cast<int>(value.size()));
    std::string out = escaped ? escaped : value;

    if (escaped) curl_free(escaped);
    curl_easy_cleanup(tmp);

    return out;
}

Http::Response Http::perform(const std::string& url, const std::string* body, const std::vector<std::string>& headers) {
    std::lock_guard lock(mtx);

    Response response;

    if (!curl) {
        response.error = "curl handle unavailable";
        return response;
    }

    curl_easy_reset(curl);

    struct curl_slist* list = nullptr;
    for (const std::string& h : headers) {
        list = curl_slist_append(list, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    /* Empty string means "every encoding libcurl was built with". YouTube always
    gzips these responses and they are large, so this matters.
    */
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    if (body) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body->size()));
    }

    CURLcode rc = curl_easy_perform(curl);

    if (rc != CURLE_OK) {
        response.error = curl_easy_strerror(rc);
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    }

    if (list) curl_slist_free_all(list);

    return response;
}

Http::Response Http::post(const std::string& url, const std::string& body, const std::vector<std::string>& headers) {
    return perform(url, &body, headers);
}

Http::Response Http::get(const std::string& url, const std::vector<std::string>& headers) {
    return perform(url, nullptr, headers);
}

}
