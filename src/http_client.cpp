#include "http_client.h"
#include <curl/curl.h>
#include <iostream>
#include <cstring>

namespace cfpinner {

// Callback for CURL to write headers
static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total_size = size * nitems;
    std::string* headers = static_cast<std::string*>(userdata);
    headers->append(buffer, total_size);
    return total_size;
}

HTTPClient::HTTPClient()
    : timeout_seconds_(5),
      user_agent_("CFPinner/1.0") {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HTTPClient::~HTTPClient() {
    curl_global_cleanup();
}

void HTTPClient::setTimeout(int timeout_seconds) {
    timeout_seconds_ = timeout_seconds;
}

void HTTPClient::setUserAgent(const std::string& user_agent) {
    user_agent_ = user_agent;
}

HTTPResponse HTTPClient::head(const std::string& url, const std::string& host_header) {
    HTTPResponse response;
    response.success = false;
    response.status_code = 0;
    response.is_cache_hit = false;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error_message = "Failed to initialize CURL";
        return response;
    }

    std::string headers_data;

    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent_.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers_data);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Skip SSL verification for CDN testing
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Custom headers
    struct curl_slist* chunk = nullptr;
    if (!host_header.empty()) {
        std::string host_line = "Host: " + host_header;
        chunk = curl_slist_append(chunk, host_line.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }

    // Perform the request
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        response.error_message = curl_easy_strerror(res);
    } else {
        response.success = true;

        // Get response code
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        response.status_code = static_cast<int>(response_code);

        // Parse headers for CF-Cache-Status
        size_t pos = headers_data.find("CF-Cache-Status:");
        if (pos != std::string::npos) {
            size_t start = pos + 16; // Length of "CF-Cache-Status:"
            size_t end = headers_data.find("\r\n", start);
            if (end != std::string::npos) {
                response.cf_cache_status = headers_data.substr(start, end - start);
                // Trim whitespace
                response.cf_cache_status.erase(0, response.cf_cache_status.find_first_not_of(" \t"));
                response.cf_cache_status.erase(response.cf_cache_status.find_last_not_of(" \t") + 1);

                // Determine if it's a cache hit
                response.is_cache_hit = (response.cf_cache_status == "HIT");
            }
        }

        // Parse CF-Ray header
        pos = headers_data.find("CF-Ray:");
        if (pos != std::string::npos) {
            size_t start = pos + 7; // Length of "CF-Ray:"
            size_t end = headers_data.find("\r\n", start);
            if (end != std::string::npos) {
                response.cf_ray = headers_data.substr(start, end - start);
                // Trim whitespace
                response.cf_ray.erase(0, response.cf_ray.find_first_not_of(" \t"));
                response.cf_ray.erase(response.cf_ray.find_last_not_of(" \t") + 1);

                // Extract IATA code (last 3 characters after dash)
                // Format: "8428f15b8a9c1234-SJC"
                size_t dash_pos = response.cf_ray.find_last_of('-');
                if (dash_pos != std::string::npos && dash_pos + 3 < response.cf_ray.length()) {
                    response.cf_iata_code = response.cf_ray.substr(dash_pos + 1, 3);
                }
            }
        }

        // Parse CF-IPCountry header (optional)
        pos = headers_data.find("CF-IPCountry:");
        if (pos != std::string::npos) {
            size_t start = pos + 13; // Length of "CF-IPCountry:"
            size_t end = headers_data.find("\r\n", start);
            if (end != std::string::npos) {
                response.cf_ip_country = headers_data.substr(start, end - start);
                // Trim whitespace
                response.cf_ip_country.erase(0, response.cf_ip_country.find_first_not_of(" \t"));
                response.cf_ip_country.erase(response.cf_ip_country.find_last_not_of(" \t") + 1);
            }
        }
    }

    if (chunk) {
        curl_slist_free_all(chunk);
    }

    curl_easy_cleanup(curl);
    return response;
}

} // namespace cfpinner
