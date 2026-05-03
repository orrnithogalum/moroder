#pragma once

#include <mutex>
#include <string>
#include <vector>

typedef void CURL;

namespace ytm {

// libcurl wrapper, basically
class Http {
public:
    struct Response {
        long status = 0;
        std::string body;
        std::string error;

        bool ok() const {
            return error.empty() && status >= 200 && status < 300;
        }
    };

    Http();
    ~Http();

    Http(const Http&)            = delete;
    Http& operator=(const Http&) = delete;

    Response post(const std::string& url, const std::string& body, const std::vector<std::string>& headers);
    Response get(const std::string& url, const std::vector<std::string>& headers);

    void setTimeout(long seconds) {
        timeout = seconds;
    }

    static std::string urlEncode(const std::string& value);

private:
    Response perform(const std::string& url, const std::string* body, const std::vector<std::string>& headers);

    CURL* curl = nullptr;

    std::mutex mtx;
    long timeout = 30;
};

}
