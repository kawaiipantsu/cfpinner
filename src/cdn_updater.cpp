#include "cdn_updater.h"
#include "http_client.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <ctime>
#include <curl/curl.h>

namespace cfpinner {

CDNUpdater::CDNUpdater() {
    // Get home directory
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw->pw_dir;
    }

    config_dir_ = std::string(home) + "/.cfpinner";
    ip_ranges_file_ = config_dir_ + "/cf_cdn_ips.txt";
    alive_ips_file_ = config_dir_ + "/alive_ips.txt";

    // Ensure config directory exists
    struct stat st;
    if (stat(config_dir_.c_str(), &st) != 0) {
        mkdir(config_dir_.c_str(), 0755);
    }
}

CDNUpdater::~CDNUpdater() {
}

bool CDNUpdater::fileExists(const std::string& filepath) const {
    struct stat st;
    return (stat(filepath.c_str(), &st) == 0);
}

int CDNUpdater::getFileAge(const std::string& filepath) const {
    if (!fileExists(filepath)) {
        return -1;
    }

    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        return -1;
    }

    time_t now = time(nullptr);
    time_t file_time = st.st_mtime;
    double seconds = difftime(now, file_time);
    int days = static_cast<int>(seconds / 86400); // 86400 seconds in a day

    return days;
}

int CDNUpdater::getFileAgeDays() const {
    return getFileAge(ip_ranges_file_);
}

int CDNUpdater::getAliveIPsAgeDays() const {
    return getFileAge(alive_ips_file_);
}

bool CDNUpdater::needsUpdate() const {
    if (!fileExists(ip_ranges_file_)) {
        return true;
    }

    int age_days = getFileAgeDays();
    return (age_days < 0 || age_days > 30);
}

std::string CDNUpdater::getIPRangesFilePath() const {
    return ip_ranges_file_;
}

std::string CDNUpdater::getAliveIPsFilePath() const {
    return alive_ips_file_;
}

bool CDNUpdater::hasRecentAliveIPs() const {
    int age = getAliveIPsAgeDays();
    return (age >= 0 && age < 7); // Consider alive IPs recent if less than 7 days old
}

bool CDNUpdater::saveAliveIPs(const std::vector<std::string>& alive_ips) {
    std::ofstream file(alive_ips_file_);
    if (!file.is_open()) {
        std::cerr << "Failed to save alive IPs to: " << alive_ips_file_ << std::endl;
        return false;
    }

    // Write header
    file << "# Cloudflare CDN Alive IPs" << std::endl;
    file << "# IPs that responded with HIT or MISS status" << std::endl;

    time_t now = time(nullptr);
    char timestamp[100];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    file << "# Scanned: " << timestamp << std::endl;
    file << "# Total alive: " << alive_ips.size() << std::endl;
    file << std::endl;

    // Write IP addresses
    for (const auto& ip : alive_ips) {
        file << ip << std::endl;
    }

    file.close();
    return true;
}

bool CDNUpdater::loadAliveIPs(std::vector<std::string>& alive_ips) {
    std::ifstream file(alive_ips_file_);
    if (!file.is_open()) {
        return false;
    }

    alive_ips.clear();
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (!line.empty()) {
            alive_ips.push_back(line);
        }
    }

    file.close();
    return !alive_ips.empty();
}

bool CDNUpdater::downloadIPRanges(std::vector<std::string>& ipv4_ranges) {
    HTTPClient client;
    client.setTimeout(30);

    std::cout << "Downloading CloudFlare IP ranges..." << std::endl;

    // Download IPv4 ranges
    HTTPResponse response = client.head("https://www.cloudflare.com/ips-v4", "");

    // HEAD doesn't give us the body, so we need to make a GET request
    // For now, we'll use a simple approach - make a full request
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return false;
    }

    std::string response_body;
    auto write_callback = [](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
        std::string* str = static_cast<std::string*>(userdata);
        str->append(ptr, size * nmemb);
        return size * nmemb;
    };

    curl_easy_setopt(curl, CURLOPT_URL, "https://www.cloudflare.com/ips-v4");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "Failed to download IP ranges: " << curl_easy_strerror(res) << std::endl;
        return false;
    }

    // Parse the response body (one IP range per line)
    std::istringstream stream(response_body);
    std::string line;
    while (std::getline(stream, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (!line.empty() && line[0] != '#') {
            ipv4_ranges.push_back(line);
        }
    }

    std::cout << "Downloaded " << ipv4_ranges.size() << " IPv4 ranges" << std::endl;
    return !ipv4_ranges.empty();
}

bool CDNUpdater::saveIPRanges(const std::vector<std::string>& ipv4_ranges) {
    std::ofstream file(ip_ranges_file_);
    if (!file.is_open()) {
        std::cerr << "Failed to save IP ranges to: " << ip_ranges_file_ << std::endl;
        return false;
    }

    // Write header
    file << "# Cloudflare CDN IP Ranges (IPv4 only)" << std::endl;
    file << "# Source: https://www.cloudflare.com/ips-v4" << std::endl;
    file << "# Auto-downloaded by CFPinner" << std::endl;

    time_t now = time(nullptr);
    char timestamp[100];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    file << "# Last updated: " << timestamp << std::endl;
    file << std::endl;

    // Write IP ranges
    for (const auto& range : ipv4_ranges) {
        file << range << std::endl;
    }

    file.close();
    std::cout << "Saved IP ranges to: " << ip_ranges_file_ << std::endl;
    return true;
}

bool CDNUpdater::updateIPRanges(bool force) {
    if (!force && !needsUpdate()) {
        int age = getFileAgeDays();
        std::cout << "IP ranges file is up to date (age: " << age << " days)" << std::endl;
        return true;
    }

    std::vector<std::string> ipv4_ranges;
    if (!downloadIPRanges(ipv4_ranges)) {
        return false;
    }

    if (!saveIPRanges(ipv4_ranges)) {
        return false;
    }

    std::cout << "\033[32m✓ CloudFlare IP ranges updated successfully!\033[0m" << std::endl;
    return true;
}

} // namespace cfpinner
