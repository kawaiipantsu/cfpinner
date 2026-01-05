#ifndef CDN_TRACKER_H
#define CDN_TRACKER_H

#include <string>
#include <vector>
#include "http_client.h"

namespace cfpinner {

struct CDNCheckResult {
    std::string ip_range;
    std::string ip_address;
    bool is_hit;
    int status_code;
    std::string cache_status;
    std::string cf_ray;
    std::string cf_iata_code;
    std::string cf_ip_country;
    std::string error_message;
};

class CDNTracker {
public:
    CDNTracker();
    ~CDNTracker();

    // Track an image across Cloudflare CDN nodes
    // Uses multi-threading for fast scanning (default: 10 threads)
    void track(const std::string& identifier, const std::string& target_url, size_t num_threads = 10);

    // Scan all Cloudflare IPs to find alive nodes (returns list of responsive IPs)
    // Uses multi-threading for fast scanning (default: 10 threads)
    std::vector<std::string> scanAliveNodes(size_t num_threads = 10);

    // Set custom timeout for HTTP requests (in seconds)
    void setTimeout(int timeout_seconds);

    // Enable force-all mode (expand all CIDR ranges completely)
    void setForceAll(bool force_all);

    // Load Cloudflare IP ranges from file
    bool loadIPRanges(const std::string& filename);

    // Load specific IPs to check (for using alive list)
    void setSpecificIPs(const std::vector<std::string>& ips);

    // Set the target domain to check
    void setTargetDomain(const std::string& domain);

    // Set max IPs to check per CIDR range
    // Default: 10 for tracking, 100 for alive scan
    void setMaxIPsPerRange(size_t max_ips);

private:
    std::vector<std::string> ip_ranges_;
    std::vector<std::string> specific_ips_; // For using alive list
    std::string target_domain_;
    HTTPClient http_client_;
    size_t max_ips_per_range_;
    bool use_specific_ips_;
    bool force_all_;
    int timeout_seconds_;

    void displayResult(const CDNCheckResult& result) const;
    void displaySummary(const std::vector<CDNCheckResult>& results) const;
    void displayProgress(size_t current, size_t total) const;
    void displayResultsTable(const std::vector<CDNCheckResult>& results) const;
    std::vector<std::string> expandAllRanges() const;
};

} // namespace cfpinner

#endif // CDN_TRACKER_H
