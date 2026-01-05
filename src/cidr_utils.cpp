#include "cidr_utils.h"
#include <sstream>
#include <cmath>
#include <cstdint>
#include <arpa/inet.h>

namespace cfpinner {

uint32_t CIDRUtils::ipToUint32(const std::string& ip) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip.c_str(), &addr) != 1) {
        return 0;
    }
    return ntohl(addr.s_addr);
}

std::string CIDRUtils::uint32ToIp(uint32_t ip) {
    struct in_addr addr;
    addr.s_addr = htonl(ip);
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, str, INET_ADDRSTRLEN);
    return std::string(str);
}

uint32_t CIDRUtils::getHostCount(int prefix_len) {
    if (prefix_len < 0 || prefix_len > 32) {
        return 0;
    }
    return (1U << (32 - prefix_len));
}

bool CIDRUtils::parseCIDR(const std::string& cidr, uint32_t& base_ip, int& prefix_len) {
    size_t slash_pos = cidr.find('/');
    if (slash_pos == std::string::npos) {
        // No slash, treat as single IP (/32)
        base_ip = ipToUint32(cidr);
        prefix_len = 32;
        return base_ip != 0;
    }

    std::string ip_str = cidr.substr(0, slash_pos);
    std::string prefix_str = cidr.substr(slash_pos + 1);

    base_ip = ipToUint32(ip_str);
    if (base_ip == 0) {
        return false;
    }

    try {
        prefix_len = std::stoi(prefix_str);
    } catch (...) {
        return false;
    }

    if (prefix_len < 0 || prefix_len > 32) {
        return false;
    }

    // Normalize base IP to network address
    uint32_t mask = (prefix_len == 0) ? 0 : (~0U << (32 - prefix_len));
    base_ip &= mask;

    return true;
}

std::vector<std::string> CIDRUtils::expandCIDR(const std::string& cidr, size_t max_ips) {
    std::vector<std::string> ips;

    uint32_t base_ip;
    int prefix_len;

    if (!parseCIDR(cidr, base_ip, prefix_len)) {
        return ips;
    }

    uint32_t total_hosts = getHostCount(prefix_len);

    // Check if force-all mode (max_ips == SIZE_MAX means no limit)
    bool force_all = (max_ips == SIZE_MAX);

    // For very large ranges, sample strategically (unless force-all is enabled)
    if (total_hosts > max_ips && !force_all) {
        // Use strategic sampling to get good coverage across the range
        // Distribute samples evenly across the entire IP space
        uint32_t step = total_hosts / max_ips;

        for (size_t i = 0; i < max_ips; i++) {
            uint32_t offset = i * step;

            // Add some variation within each segment to avoid only checking
            // the first IP of each subnet
            if (i > 0 && step > 4) {
                // Add offset of 1-3 to check different IPs in subnet
                offset += (i % 4);
            }

            // Skip network address (first) and broadcast address (last)
            if (offset == 0 && total_hosts > 2) {
                offset = 1;
            }
            if (offset >= total_hosts - 1 && total_hosts > 2) {
                offset = total_hosts - 2;
            }

            uint32_t ip = base_ip + offset;
            ips.push_back(uint32ToIp(ip));
        }
    } else {
        // Small range, include all IPs
        for (uint32_t i = 0; i < total_hosts; i++) {
            // Skip network address and broadcast address for ranges > /31
            if (prefix_len < 31) {
                if (i == 0 || i == total_hosts - 1) {
                    continue;
                }
            }
            uint32_t ip = base_ip + i;
            ips.push_back(uint32ToIp(ip));
        }
    }

    return ips;
}

} // namespace cfpinner
