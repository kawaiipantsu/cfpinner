#ifndef CDN_UPDATER_H
#define CDN_UPDATER_H

#include <string>
#include <vector>

namespace cfpinner {

class CDNUpdater {
public:
    CDNUpdater();
    ~CDNUpdater();

    // Download and save latest CloudFlare IP ranges
    bool updateIPRanges(bool force = false);

    // Check if IP ranges file needs updating (older than 30 days)
    bool needsUpdate() const;

    // Get the path to the IP ranges file
    std::string getIPRangesFilePath() const;

    // Get the path to the alive IPs file
    std::string getAliveIPsFilePath() const;

    // Get file age in days
    int getFileAgeDays() const;

    // Get alive IPs file age in days
    int getAliveIPsAgeDays() const;

    // Save list of alive IPs to file
    bool saveAliveIPs(const std::vector<std::string>& alive_ips);

    // Load list of alive IPs from file
    bool loadAliveIPs(std::vector<std::string>& alive_ips);

    // Check if alive IPs file exists and is recent (< 7 days)
    bool hasRecentAliveIPs() const;

private:
    std::string config_dir_;
    std::string ip_ranges_file_;
    std::string alive_ips_file_;

    bool downloadIPRanges(std::vector<std::string>& ipv4_ranges);
    bool saveIPRanges(const std::vector<std::string>& ipv4_ranges);
    bool fileExists(const std::string& filepath) const;
    int getFileAge(const std::string& filepath) const;
};

} // namespace cfpinner

#endif // CDN_UPDATER_H
