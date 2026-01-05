#include "cfpinner.h"
#include "image_generator.h"
#include "cdn_tracker.h"
#include "cdn_updater.h"
#include "config.h"
#include <iostream>

namespace cfpinner {

Application::Application() {
}

Application::~Application() {
}

void Application::printBanner() const {
    std::cout << "\033[36m" << std::endl;
    std::cout << "  ____ _____ ____  _                       " << std::endl;
    std::cout << " / ___|  ___|  _ \\(_)_ __  _ __   ___ _ __ " << std::endl;
    std::cout << "| |   | |_  | |_) | | '_ \\| '_ \\ / _ \\ '__|" << std::endl;
    std::cout << "| |___|  _| |  __/| | | | | | | |  __/ |   " << std::endl;
    std::cout << " \\____|_|   |_|   |_|_| |_|_| |_|\\___|_|   " << std::endl;
    std::cout << "\033[0m" << std::endl;
    std::cout << "Cloudflare CDN Location Tracker v1.0\n" << std::endl;
}

int Application::run(int argc, char* argv[]) {
    printBanner();

    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];

    // Parse global options
    int timeout = -1; // -1 means use default
    bool force_all = false;
    size_t num_threads = 10; // Default number of threads

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--timeout-overrule" && i + 1 < argc) {
            timeout = std::stoi(argv[i + 1]);
            i++; // Skip next arg
        } else if (arg == "--force-all") {
            force_all = true;
        } else if ((arg == "--threads" || arg == "--num-threads") && i + 1 < argc) {
            num_threads = std::stoul(argv[i + 1]);
            i++; // Skip next arg
        }
    }

    if (command == "--help" || command == "-h") {
        printUsage();
        return 0;
    } else if (command == "--generate" || command == "-g") {
        // Check for --save option
        std::string output_dir = "";
        for (int i = 2; i < argc; i++) {
            std::string arg = argv[i];
            if ((arg == "--save" || arg == "-s") && i + 1 < argc) {
                output_dir = argv[i + 1];
                break;
            }
        }
        return handleGenerate(output_dir);
    } else if (command == "--track" || command == "-t") {
        if (argc < 4) {
            std::cerr << "Error: --track requires <identifier> and <url>" << std::endl;
            std::cerr << "Example: cfpinner --track abc123def456 https://example.com/image.png" << std::endl;
            return 1;
        }
        std::string identifier = argv[2];
        std::string url = argv[3];
        int track_timeout = (timeout == -1) ? 5 : timeout;
        return handleTrack(identifier, url, track_timeout, force_all, num_threads);
    } else if (command == "--update-cdn" || command == "-u") {
        return handleUpdateCDN();
    } else if (command == "--alive" || command == "-a") {
        int alive_timeout = (timeout == -1) ? 1 : timeout;
        return handleAlive(alive_timeout, force_all, num_threads);
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        printUsage();
        return 1;
    }
}

void Application::printUsage() const {
    std::cout << "Usage: cfpinner [command] [options]" << std::endl;
    std::cout << "\nCommands:" << std::endl;
    std::cout << "  -g, --generate [--save <dir>]   Generate a unique PNG image" << std::endl;
    std::cout << "  -a, --alive [options]           Scan and cache alive CDN nodes (multi-threaded)" << std::endl;
    std::cout << "  -t, --track <id> <url> [opts]   Track image across Cloudflare CDN" << std::endl;
    std::cout << "  -u, --update-cdn                Update Cloudflare IP ranges" << std::endl;
    std::cout << "  -h, --help                      Show this help message" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  -s, --save <dir>                Custom output directory for generated image" << std::endl;
    std::cout << "                                  (default: ~/.cfpinner/images/)" << std::endl;
    std::cout << "  --threads <num>                 Number of parallel threads for scanning" << std::endl;
    std::cout << "                                  (default: 10)" << std::endl;
    std::cout << "  --timeout-overrule <seconds>    Override default timeout" << std::endl;
    std::cout << "                                  (default: 1s for --alive, 5s for --track)" << std::endl;
    std::cout << "  --force-all                     Expand FULL CIDR ranges (no sampling)" << std::endl;
    std::cout << "                                  WARNING: May result in 500k+ IPs!" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  cfpinner --generate" << std::endl;
    std::cout << "  cfpinner --generate --save /tmp" << std::endl;
    std::cout << "  cfpinner --generate --save ./images" << std::endl;
    std::cout << "  cfpinner --update-cdn" << std::endl;
    std::cout << "  cfpinner --alive" << std::endl;
    std::cout << "  cfpinner --alive --threads 5" << std::endl;
    std::cout << "  cfpinner --alive --timeout-overrule 2" << std::endl;
    std::cout << "  cfpinner --alive --force-all --timeout-overrule 1" << std::endl;
    std::cout << "  cfpinner --track abc123def456 https://example.com/images/abc123def456.png" << std::endl;
    std::cout << "  cfpinner --track abc123def456 https://example.com/image.png --threads 20" << std::endl;
    std::cout << "  cfpinner --track abc123def456 https://example.com/image.png --force-all" << std::endl;
    std::cout << "\nWorkflow:" << std::endl;
    std::cout << "  1. (Optional) Run --alive to discover responsive CDN nodes (speeds up tracking)" << std::endl;
    std::cout << "  2. Generate a unique image with --generate" << std::endl;
    std::cout << "  3. Upload the image to your target service" << std::endl;
    std::cout << "  4. Track the image with --track to see which CDN nodes have it cached" << std::endl;
    std::cout << "\nNotes:" << std::endl;
    std::cout << "  - IP ranges are auto-updated if older than 30 days" << std::endl;
    std::cout << "  - Alive IPs cache expires after 7 days" << std::endl;
    std::cout << "  - --alive uses 10 threads for fast scanning" << std::endl;
    std::cout << "  - Default sampling: 100 IPs per range (--alive), 10 IPs per range (--track)" << std::endl;
    std::cout << "  - Use --force-all for complete CIDR expansion (very slow, 500k+ IPs)" << std::endl;
}

int Application::handleGenerate(const std::string& output_dir) {
    try {
        ImageGenerator generator;
        ImageMetadata metadata = generator.generate(output_dir);

        std::cout << "\n\033[32m✓ Image generated successfully!\033[0m" << std::endl;
        std::cout << "\nNext steps:" << std::endl;
        std::cout << "  1. Upload this image to your target service" << std::endl;
        std::cout << "  2. Once uploaded, track it with:" << std::endl;
        std::cout << "     cfpinner --track " << metadata.identifier
                  << " <URL_WHERE_YOU_UPLOADED>" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

int Application::handleUpdateCDN() {
    try {
        CDNUpdater updater;

        std::cout << "Updating Cloudflare CDN IP ranges..." << std::endl;

        if (!updater.updateIPRanges(true)) {
            std::cerr << "Error: Failed to update IP ranges" << std::endl;
            return 1;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

int Application::handleAlive(int timeout, bool force_all, size_t num_threads) {
    try {
        // Ensure IP ranges are up to date
        CDNUpdater updater;
        if (updater.needsUpdate()) {
            std::cout << "Updating IP ranges first..." << std::endl;
            if (!updater.updateIPRanges(false)) {
                std::cerr << "Warning: Failed to update IP ranges" << std::endl;
            }
        }

        CDNTracker tracker;

        // Set timeout and force_all options
        tracker.setTimeout(timeout);
        tracker.setForceAll(force_all);

        // Load IP ranges
        std::string ip_ranges_file = updater.getIPRangesFilePath();
        if (!tracker.loadIPRanges(ip_ranges_file)) {
            std::cerr << "Error: Failed to load Cloudflare IP ranges" << std::endl;
            std::cerr << "Try running: cfpinner --update-cdn" << std::endl;
            return 1;
        }

        // Scan for alive nodes (multi-threaded)
        std::vector<std::string> alive_ips = tracker.scanAliveNodes(num_threads);

        if (alive_ips.empty()) {
            std::cerr << "Error: No alive CDN nodes found" << std::endl;
            return 1;
        }

        // Save alive IPs
        if (!updater.saveAliveIPs(alive_ips)) {
            std::cerr << "Error: Failed to save alive IPs" << std::endl;
            return 1;
        }

        std::cout << "Saved alive IPs to: " << updater.getAliveIPsFilePath() << std::endl;
        std::cout << "\n\033[32m✓ Use --track to leverage this optimized list!\033[0m" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

int Application::handleTrack(const std::string& identifier, const std::string& url, int timeout, bool force_all, size_t num_threads) {
    try {
        Config config;
        ImageMetadata metadata;

        if (!config.loadImageMetadata(identifier, metadata)) {
            std::cerr << "Error: Image with identifier '" << identifier
                      << "' not found in local database." << std::endl;
            std::cerr << "Generate a new image with: cfpinner --generate" << std::endl;
            return 1;
        }

        std::cout << "Image found in local database:" << std::endl;
        std::cout << "  Generated: " << metadata.timestamp << std::endl;
        std::cout << "  Size: " << metadata.width << "x" << metadata.height << std::endl;

        // Check and update CDN IP ranges if needed
        CDNUpdater updater;
        if (updater.needsUpdate()) {
            int age = updater.getFileAgeDays();
            if (age < 0) {
                std::cout << "\nCloudflare IP ranges file not found. Downloading..." << std::endl;
            } else {
                std::cout << "\nCloudflare IP ranges are " << age << " days old. Updating..." << std::endl;
            }

            if (!updater.updateIPRanges(false)) {
                std::cerr << "Warning: Failed to update IP ranges. Using existing file if available." << std::endl;
            }
        } else {
            int age = updater.getFileAgeDays();
            std::cout << "Using CloudFlare IP ranges (age: " << age << " days)" << std::endl;
        }

        CDNTracker tracker;

        // Set timeout and force_all options
        tracker.setTimeout(timeout);
        tracker.setForceAll(force_all);

        // Check if we have a recent alive IPs list
        if (updater.hasRecentAliveIPs()) {
            std::vector<std::string> alive_ips;
            if (updater.loadAliveIPs(alive_ips)) {
                int age = updater.getAliveIPsAgeDays();
                std::cout << "Using alive IPs cache (" << alive_ips.size()
                          << " IPs, age: " << age << " days)" << std::endl;
                tracker.setSpecificIPs(alive_ips);
            }
        } else {
            // Load all IP ranges
            std::string ip_ranges_file = updater.getIPRangesFilePath();
            if (!tracker.loadIPRanges(ip_ranges_file)) {
                std::cerr << "Error: Failed to load Cloudflare IP ranges from " << ip_ranges_file << std::endl;
                std::cerr << "Try running: cfpinner --update-cdn" << std::endl;
                return 1;
            }

            int alive_age = updater.getAliveIPsAgeDays();
            if (alive_age < 0) {
                std::cout << "\033[33mTip: Run 'cfpinner --alive' first to speed up tracking!\033[0m" << std::endl;
            } else {
                std::cout << "\033[33mAlive IPs cache is " << alive_age
                          << " days old. Run 'cfpinner --alive' to refresh.\033[0m" << std::endl;
            }
        }

        // Track the image
        tracker.track(identifier, url, num_threads);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

} // namespace cfpinner
