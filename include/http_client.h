#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <string>
#include <functional>

namespace cfpinner {

struct HTTPResponse {
    int status_code;
    std::string body;
    bool success;
    std::string error_message;
    bool is_cache_hit;
    std::string cf_cache_status;
    std::string cf_ray;
    std::string cf_iata_code;
    std::string cf_ip_country;
};

class HTTPClient {
public:
    HTTPClient();
    ~HTTPClient();

    // Make a HEAD request to check if image exists
    HTTPResponse head(const std::string& url, const std::string& host_header = "");

    // Set timeout for requests (in seconds)
    void setTimeout(int timeout_seconds);

    // Set custom User-Agent
    void setUserAgent(const std::string& user_agent);

private:
    int timeout_seconds_;
    std::string user_agent_;
};

} // namespace cfpinner

#endif // HTTP_CLIENT_H
