#include "cdn_tracker.h"
#include "cidr_utils.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>

namespace cfpinner {

CDNTracker::CDNTracker() : max_ips_per_range_(10), use_specific_ips_(false), force_all_(false), timeout_seconds_(5) {
    http_client_.setTimeout(timeout_seconds_);
}

CDNTracker::~CDNTracker() {
}

void CDNTracker::setTimeout(int timeout_seconds) {
    timeout_seconds_ = timeout_seconds;
    http_client_.setTimeout(timeout_seconds);
}

void CDNTracker::setForceAll(bool force_all) {
    force_all_ = force_all;
}

void CDNTracker::setMaxIPsPerRange(size_t max_ips) {
    max_ips_per_range_ = max_ips;
}

void CDNTracker::setSpecificIPs(const std::vector<std::string>& ips) {
    specific_ips_ = ips;
    use_specific_ips_ = !ips.empty();
}

bool CDNTracker::loadIPRanges(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open IP ranges file: " << filename << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        size_t end = line.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            ip_ranges_.push_back(line.substr(start, end - start + 1));
        }
    }

    file.close();
    std::cout << "Loaded " << ip_ranges_.size() << " IP ranges" << std::endl;
    return !ip_ranges_.empty();
}

void CDNTracker::setTargetDomain(const std::string& domain) {
    target_domain_ = domain;
}

void CDNTracker::displayProgress(size_t current, size_t total) const {
    int percent = (current * 100) / total;
    std::cout << "\r[" << std::setw(3) << percent << "%] Checking IP "
              << current << " of " << total << "..." << std::flush;
}

std::vector<std::string> CDNTracker::expandAllRanges() const {
    std::vector<std::string> all_ips;

    // If force_all is enabled, use SIZE_MAX to expand everything
    size_t expansion_limit = force_all_ ? SIZE_MAX : max_ips_per_range_;

    for (const auto& ip_range : ip_ranges_) {
        // Skip IPv6 for now
        if (ip_range.find(':') != std::string::npos) {
            continue;
        }

        std::vector<std::string> ips = CIDRUtils::expandCIDR(ip_range, expansion_limit);
        all_ips.insert(all_ips.end(), ips.begin(), ips.end());
    }

    return all_ips;
}

void CDNTracker::displayResult(const CDNCheckResult& result) const {
    std::string status_icon;
    std::string status_text;
    std::string color_code;

    if (!result.error_message.empty()) {
        status_icon = "✗";
        status_text = "ERROR";
        color_code = "\033[31m"; // Red
    } else if (result.is_hit) {
        status_icon = "✓";
        status_text = "HIT";
        color_code = "\033[32m"; // Green
    } else {
        status_icon = "○";
        status_text = "MISS";
        color_code = "\033[33m"; // Yellow
    }

    std::string reset_code = "\033[0m";

    std::cout << std::left
              << std::setw(20) << result.ip_address
              << " " << color_code << status_icon << " " << status_text << reset_code;

    if (!result.error_message.empty()) {
        std::cout << " (" << result.error_message << ")";
    } else if (!result.cache_status.empty()) {
        std::cout << " [" << result.cache_status << "]";
    }

    std::cout << std::endl;
}

void CDNTracker::displaySummary(const std::vector<CDNCheckResult>& results) const {
    int hits = 0;
    int misses = 0;
    int errors = 0;

    for (const auto& result : results) {
        if (!result.error_message.empty()) {
            errors++;
        } else if (result.is_hit) {
            hits++;
        } else {
            misses++;
        }
    }

    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "Summary:" << std::endl;
    std::cout << "  Total checked: " << results.size() << std::endl;
    std::cout << "  \033[32mHITS:  " << hits << "\033[0m" << std::endl;
    std::cout << "  \033[33mMISSES: " << misses << "\033[0m" << std::endl;
    std::cout << "  \033[31mERRORS: " << errors << "\033[0m" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}

std::vector<std::string> CDNTracker::scanAliveNodes(size_t num_threads) {
    if (ip_ranges_.empty()) {
        std::cerr << "No IP ranges loaded. Use loadIPRanges() first." << std::endl;
        return {};
    }

    std::cout << "\nScanning Cloudflare CDN for alive nodes..." << std::endl;
    std::cout << "Expanding " << ip_ranges_.size() << " CIDR ranges";
    if (force_all_) {
        std::cout << " (FULL expansion - no sampling)";
    }
    std::cout << "..." << std::endl;

    // For alive scan, we want comprehensive coverage
    // Sample more IPs per range than default tracking (100 vs 10)
    size_t saved_max = max_ips_per_range_;
    if (!force_all_) {
        max_ips_per_range_ = 100;  // Much more aggressive sampling for alive scan
    }

    std::vector<std::string> all_ips = expandAllRanges();

    // Restore original setting
    max_ips_per_range_ = saved_max;

    std::cout << "Testing " << all_ips.size() << " Cloudflare CDN IPs using " << num_threads << " threads...\n" << std::endl;
    std::cout << "\033[33mNote: This will take approximately "
              << (all_ips.size() * timeout_seconds_ / 60 / num_threads) << " minutes to complete.\033[0m\n" << std::endl;

    // Thread-safe containers
    std::vector<std::string> alive_ips;
    std::mutex alive_ips_mutex;
    std::mutex console_mutex;
    std::atomic<size_t> completed_count(0);

    // Worker function for each thread
    auto worker = [&](size_t start_idx, size_t end_idx) {
        HTTPClient thread_http_client;
        thread_http_client.setTimeout(timeout_seconds_);

        std::string domain = "www.cloudflare.com";

        for (size_t i = start_idx; i < end_idx && i < all_ips.size(); i++) {
            const std::string& ip_address = all_ips[i];

            // Build test URL with IP
            std::string url = "https://" + ip_address + "/";

            // Make request
            HTTPResponse response = thread_http_client.head(url, domain);

            // Consider IP alive if we got any response
            bool is_alive = response.success && response.status_code > 0;

            if (is_alive) {
                {
                    std::lock_guard<std::mutex> lock(alive_ips_mutex);
                    alive_ips.push_back(ip_address);
                }

                // Display result
                {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    CDNCheckResult result;
                    result.ip_address = ip_address;
                    result.status_code = response.status_code;
                    result.is_hit = false;
                    result.cache_status = "ALIVE";
                    std::cout << "\r" << std::string(60, ' ') << "\r";
                    displayResult(result);
                }
            }

            // Update progress
            size_t current = ++completed_count;
            if (current % 10 == 0 || current == all_ips.size()) {
                std::lock_guard<std::mutex> lock(console_mutex);
                displayProgress(current, all_ips.size());
            }
        }
    };

    // Create thread pool
    std::vector<std::thread> threads;
    size_t ips_per_thread = (all_ips.size() + num_threads - 1) / num_threads;

    for (size_t t = 0; t < num_threads; t++) {
        size_t start_idx = t * ips_per_thread;
        size_t end_idx = std::min(start_idx + ips_per_thread, all_ips.size());

        if (start_idx < all_ips.size()) {
            threads.emplace_back(worker, start_idx, end_idx);
        }
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "\r" << std::string(60, ' ') << "\r"; // Clear progress line
    std::cout << "\n\033[32m✓ Scan complete!\033[0m" << std::endl;
    std::cout << "Found " << alive_ips.size() << " alive CDN nodes out of "
              << all_ips.size() << " tested" << std::endl;

    return alive_ips;
}

void CDNTracker::displayResultsTable(const std::vector<CDNCheckResult>& results) const {
    // ANSI color codes
    const std::string color_green = "\033[32m";
    const std::string color_yellow = "\033[33m";
    const std::string color_red = "\033[31m";
    const std::string color_reset = "\033[0m";

    // Calculate statistics
    int hits = 0, misses = 0, errors = 0;
    for (const auto& result : results) {
        if (!result.error_message.empty()) {
            errors++;
        } else if (result.is_hit) {
            hits++;
        } else {
            misses++;
        }
    }

    // Column widths
    const int col_ip = 18;
    const int col_status = 12;
    const int col_cache = 15;
    const int col_iata = 8;
    const int col_country = 10;
    const int col_ray = 25;

    // Print table header
    std::cout << "\n";
    std::cout << "+" << std::string(col_ip, '-')
              << "+" << std::string(col_status, '-')
              << "+" << std::string(col_cache, '-')
              << "+" << std::string(col_iata, '-')
              << "+" << std::string(col_country, '-')
              << "+" << std::string(col_ray, '-')
              << "+\n";

    std::cout << "| " << std::left << std::setw(col_ip - 1) << "IP Address"
              << "| " << std::setw(col_status - 1) << "Status"
              << "| " << std::setw(col_cache - 1) << "Cache"
              << "| " << std::setw(col_iata - 1) << "IATA"
              << "| " << std::setw(col_country - 1) << "Country"
              << "| " << std::setw(col_ray - 1) << "CF-Ray"
              << "|\n";

    std::cout << "+" << std::string(col_ip, '-')
              << "+" << std::string(col_status, '-')
              << "+" << std::string(col_cache, '-')
              << "+" << std::string(col_iata, '-')
              << "+" << std::string(col_country, '-')
              << "+" << std::string(col_ray, '-')
              << "+\n";

    // Print each result
    for (const auto& result : results) {
        // Determine status and color
        std::string status_text;
        std::string color_code;

        if (!result.error_message.empty()) {
            status_text = "ERROR";
            color_code = color_red;
        } else if (result.is_hit) {
            status_text = "HIT";
            color_code = color_green;
        } else {
            status_text = "MISS";
            color_code = color_yellow;
        }

        // Format fields
        std::string ip = result.ip_address;
        if (ip.length() > col_ip - 2) ip = ip.substr(0, col_ip - 5) + "...";

        std::string cache = result.cache_status;
        if (cache.empty()) cache = "-";
        if (cache.length() > col_cache - 2) cache = cache.substr(0, col_cache - 5) + "...";

        std::string iata = result.cf_iata_code;
        if (iata.empty()) iata = "-";
        if (iata.length() > col_iata - 2) iata = iata.substr(0, col_iata - 5) + "...";

        std::string country = result.cf_ip_country;
        if (country.empty()) country = "-";
        if (country.length() > col_country - 2) country = country.substr(0, col_country - 5) + "...";

        std::string ray = result.cf_ray;
        if (ray.empty()) ray = "-";
        if (ray.length() > col_ray - 2) ray = ray.substr(0, col_ray - 5) + "...";

        // Print row with color for status column
        std::cout << "| " << std::left << std::setw(col_ip - 1) << ip
                  << "| " << color_code << std::setw(col_status - 1) << status_text << color_reset
                  << "| " << std::setw(col_cache - 1) << cache
                  << "| " << std::setw(col_iata - 1) << iata
                  << "| " << std::setw(col_country - 1) << country
                  << "| " << std::setw(col_ray - 1) << ray
                  << "|\n";
    }

    // Bottom border
    std::cout << "+" << std::string(col_ip, '-')
              << "+" << std::string(col_status, '-')
              << "+" << std::string(col_cache, '-')
              << "+" << std::string(col_iata, '-')
              << "+" << std::string(col_country, '-')
              << "+" << std::string(col_ray, '-')
              << "+\n";

    // Print summary
    float hit_percent = results.size() > 0 ? (hits * 100.0f / results.size()) : 0.0f;
    float miss_percent = results.size() > 0 ? (misses * 100.0f / results.size()) : 0.0f;
    float error_percent = results.size() > 0 ? (errors * 100.0f / results.size()) : 0.0f;

    std::cout << "\nSummary: " << results.size() << " total checks, "
              << color_green << hits << " HITs (" << std::fixed << std::setprecision(1) << hit_percent << "%)" << color_reset << ", "
              << color_yellow << misses << " MISSes (" << miss_percent << "%)" << color_reset << ", "
              << color_red << errors << " ERRORs (" << error_percent << "%)" << color_reset << "\n";
}

void CDNTracker::track(const std::string& identifier, const std::string& target_url, size_t num_threads) {
    if (!use_specific_ips_ && ip_ranges_.empty()) {
        std::cerr << "No IP ranges loaded. Use loadIPRanges() first." << std::endl;
        return;
    }

    std::cout << "\nTracking image: " << identifier << std::endl;
    std::cout << "Target URL: " << target_url << std::endl;

    // Get IPs to check (either specific alive list or expanded ranges)
    std::vector<std::string> all_ips;
    if (use_specific_ips_) {
        all_ips = specific_ips_;
        std::cout << "Using cached alive IPs list (" << all_ips.size() << " IPs)" << std::endl;
    } else {
        std::cout << "Expanding " << ip_ranges_.size() << " CIDR ranges..." << std::endl;
        all_ips = expandAllRanges();
    }

    std::cout << "Checking " << all_ips.size() << " Cloudflare CDN IPs using " << num_threads << " threads...\n" << std::endl;

    // Extract domain from URL if not set
    std::string url_to_check = target_url;
    if (url_to_check.find("http://") != 0 && url_to_check.find("https://") != 0) {
        url_to_check = "https://" + url_to_check;
    }

    std::string domain = target_domain_;
    if (domain.empty()) {
        size_t start = url_to_check.find("://");
        if (start != std::string::npos) {
            start += 3;
            size_t end = url_to_check.find('/', start);
            if (end != std::string::npos) {
                domain = url_to_check.substr(start, end - start);
            } else {
                domain = url_to_check.substr(start);
            }
        }
    }

    // Thread-safe containers
    std::vector<CDNCheckResult> results;
    std::mutex results_mutex;
    std::mutex console_mutex;
    std::atomic<size_t> completed_count(0);

    // Worker function for each thread
    auto worker = [&](size_t start_idx, size_t end_idx) {
        HTTPClient thread_http_client;
        thread_http_client.setTimeout(timeout_seconds_);

        for (size_t i = start_idx; i < end_idx && i < all_ips.size(); i++) {
            const std::string& ip_address = all_ips[i];

            CDNCheckResult result;
            result.ip_address = ip_address;
            result.ip_range = "";

            // Replace domain with IP in URL
            std::string test_url = url_to_check;
            size_t domain_start = test_url.find("://");
            if (domain_start != std::string::npos) {
                domain_start += 3;
                size_t domain_end = test_url.find('/', domain_start);
                if (domain_end != std::string::npos) {
                    test_url = test_url.substr(0, domain_start) +
                              ip_address +
                              test_url.substr(domain_end);
                } else {
                    test_url = test_url.substr(0, domain_start) + ip_address;
                }
            }

            // Make request
            HTTPResponse response = thread_http_client.head(test_url, domain);

            result.status_code = response.status_code;
            result.is_hit = response.is_cache_hit;
            result.cache_status = response.cf_cache_status;
            result.cf_ray = response.cf_ray;
            result.cf_iata_code = response.cf_iata_code;
            result.cf_ip_country = response.cf_ip_country;

            if (!response.success) {
                result.error_message = response.error_message;
            }

            // Add result to results vector (thread-safe)
            {
                std::lock_guard<std::mutex> lock(results_mutex);
                results.push_back(result);
            }

            // Display result if HIT or no error (thread-safe)
            if (result.is_hit || result.error_message.empty()) {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "\r" << std::string(60, ' ') << "\r";
                displayResult(result);
            }

            // Update progress
            size_t current = ++completed_count;
            if (current % 10 == 0 || current == all_ips.size()) {
                std::lock_guard<std::mutex> lock(console_mutex);
                displayProgress(current, all_ips.size());
            }
        }
    };

    // Create thread pool
    std::vector<std::thread> threads;
    size_t ips_per_thread = (all_ips.size() + num_threads - 1) / num_threads;

    for (size_t t = 0; t < num_threads; t++) {
        size_t start_idx = t * ips_per_thread;
        size_t end_idx = std::min(start_idx + ips_per_thread, all_ips.size());

        if (start_idx < all_ips.size()) {
            threads.emplace_back(worker, start_idx, end_idx);
        }
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "\r" << std::string(60, ' ') << "\r"; // Clear progress line
    std::cout << "\nScan complete!\n";

    // Display results in ASCII table
    displayResultsTable(results);
}

} // namespace cfpinner
