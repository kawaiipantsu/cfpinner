#ifndef CIDR_UTILS_H
#define CIDR_UTILS_H

#include <string>
#include <vector>
#include <cstdint>

namespace cfpinner {

class CIDRUtils {
public:
    // Expand a CIDR notation to a list of IP addresses
    // Returns a sample of IPs if the range is too large (e.g., /13 would be 500k+ IPs)
    static std::vector<std::string> expandCIDR(const std::string& cidr, size_t max_ips = 256);

    // Parse CIDR notation into base IP and prefix length
    static bool parseCIDR(const std::string& cidr, uint32_t& base_ip, int& prefix_len);

    // Convert IP string to uint32
    static uint32_t ipToUint32(const std::string& ip);

    // Convert uint32 to IP string
    static std::string uint32ToIp(uint32_t ip);

    // Calculate number of hosts in a CIDR range
    static uint32_t getHostCount(int prefix_len);
};

} // namespace cfpinner

#endif // CIDR_UTILS_H
