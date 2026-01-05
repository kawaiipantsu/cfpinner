# CFPinner

**Cloudflare CDN Location Tracker**

A network security tool for tracking image distribution across Cloudflare's CDN infrastructure. CFPinner generates unique PNG images that can be uploaded to services suspected of using Cloudflare, then tracks which CDN nodes have cached the image.

## Use Case

Identify which Cloudflare CDN locations are serving content for a target domain. Useful for:
- Security research and reconnaissance
- CDN infrastructure analysis
- Origin server location discovery
- Cache behavior testing

## Features

- 🎨 **Unique Image Generation**: Creates fingerprinted PNG images with cryptographic uniqueness
- 🌐 **CDN Tracking**: Probes all known Cloudflare IP ranges
- ⚡ **Multi-threaded Scanning**: 10-thread parallel scanning for 10x faster alive node discovery
- 📊 **Interactive Results Table**: Beautiful ncurses-based table display with scrolling and color coding
- 🌍 **Geographic Tracking**: Displays IATA airport codes and country information from CF headers
- 🔍 **Detailed Headers**: Captures CF-Ray, CF-IPCountry, and CF-Cache-Status headers
- ⚙️ **Configurable Timeouts**: Custom timeout control for different network conditions
- 🔓 **Full CIDR Expansion**: Optional complete range expansion (500k+ IPs)
- 🔒 **Security Focused**: Built with network security and OSINT in mind
- 🚀 **High Performance**: Optimized for speed with intelligent sampling strategies

## Installation

### Prerequisites

```bash
# Debian/Ubuntu
sudo apt-get install build-essential cmake libcurl4-openssl-dev zlib1g-dev libncurses-dev

# Fedora/RHEL
sudo dnf install gcc-c++ cmake libcurl-devel zlib-devel ncurses-devel

# Arch Linux
sudo pacman -S base-devel cmake curl zlib ncurses
```

### Building

#### Quick Build (using Makefile)

```bash
# Install dependencies
make deps

# Build the project
make

# The binary will be at build/cfpinner
```

#### Manual Build (using CMake)

```bash
# Build
mkdir -p build && cd build
cmake ..
make

# The binary will be at build/cfpinner
```

## Usage

### 1. Generate a Unique Tracking Image

```bash
# Generate to default location (~/.cfpinner/images/)
./build/cfpinner --generate

# Generate to custom directory
./build/cfpinner --generate --save /tmp
./build/cfpinner --generate --save ./output
```

Output:
```
Generating unique image...
Identifier: abc123def456789
Image saved: ~/.cfpinner/images/abc123def456789.png
Dimensions: 512x512

✓ Image generated successfully!

Next steps:
  1. Upload this image to your target service
  2. Once uploaded, track it with:
     cfpinner --track abc123def456789 <URL_WHERE_YOU_UPLOADED>
```

### 2. Upload the Image

Manually upload the generated PNG to the target service you're investigating. The image will have a unique visual pattern based on its identifier.

### 3. Track Across Cloudflare CDN

```bash
./build/cfpinner --track abc123def456789 https://target-site.com/uploads/abc123def456789.png
```

Output example:
```
Tracking image: abc123def456789
Target URL: https://target-site.com/uploads/abc123def456789.png
Checking 22 Cloudflare CDN nodes...

[Progress indicators during scan...]

Scan complete! Press any key to view results...

[Interactive ncurses table display showing:]
- IP Address
- Status (HIT/MISS/ERROR)
- Cache Status
- IATA Airport Code (from CF-Ray header)
- Country (CF-IPCountry header)
- CF-Ray ID

Navigate with:
- UP/DOWN or j/k to scroll
- Page Up/Page Down for faster scrolling
- q or ESC to exit

Summary:
  Total: 22  |  HITS: 5  |  MISSES: 15  |  ERRORS: 2
```

### Command Reference

```bash
# Show help
./build/cfpinner --help

# Generate image (default location)
./build/cfpinner --generate
./build/cfpinner -g

# Generate image (custom directory)
./build/cfpinner --generate --save <directory>
./build/cfpinner -g -s <directory>

# Update Cloudflare IP ranges
./build/cfpinner --update-cdn
./build/cfpinner -u

# Scan for alive CDN nodes (speeds up tracking)
./build/cfpinner --alive
./build/cfpinner -a

# Track image
./build/cfpinner --track <identifier> <url>
./build/cfpinner -t <identifier> <url>
```

## How It Works

1. **Image Generation**: Creates a 512x512 PNG with a unique visual pattern derived from a cryptographic hash
2. **Metadata Storage**: Saves image metadata in `~/.cfpinner/` for later tracking
3. **IP Range Management**: Auto-downloads Cloudflare IP ranges (updated if older than 30 days)
4. **CIDR Expansion**: Expands CIDR notation to individual IPs (samples 10 IPs per range for large blocks)
5. **Alive Discovery** (Optional): Pre-scans all IPs to find responsive CDN nodes, cached for 7 days
6. **Smart Tracking**: Uses cached alive IPs list if available (much faster than scanning all IPs)
7. **CDN Probing**: Makes HTTP HEAD requests to all IPs with proper Host headers
8. **Cache Detection**: Parses the `CF-Cache-Status` header to determine HIT/MISS status
9. **Results Display**: Shows real-time progress and results with color coding (green=HIT, yellow=MISS, red=ERROR)

## Development

### Using Makefile

```bash
# Show all available commands
make help

# Build in release mode (default)
make

# Build in debug mode
make debug

# Clean and rebuild
make rebuild

# Run with help
make run

# Generate test image
make generate

# Install system-wide
make install

# Show code statistics
make stats

# Format code
make format
```

### Manual CMake Commands

```bash
# Build in Debug mode
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Build in Release mode
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Clean build
rm -rf build/*
cd build && cmake .. && make
```

## Project Structure

```
cfpinner/
├── src/                    # Source files (.cpp)
│   ├── main.cpp           # Entry point
│   ├── cfpinner.cpp       # Application logic
│   ├── image_generator.cpp # PNG generation
│   ├── http_client.cpp    # HTTP/HTTPS client
│   ├── cdn_tracker.cpp    # CDN tracking logic
│   └── config.cpp         # Configuration management
├── include/               # Header files (.h)
│   ├── cfpinner.h
│   ├── image_generator.h
│   ├── http_client.h
│   ├── cdn_tracker.h
│   └── config.h
├── build/                 # Build output (not in git)
├── cf_cdn_ips.txt        # Cloudflare IP ranges
├── CMakeLists.txt        # CMake configuration
├── CLAUDE.md             # AI assistant guidance
└── README.md             # This file
```

## Technical Details

- **Language**: C++17
- **Build System**: CMake 3.14+
- **Dependencies**: libcurl, zlib, ncurses, pthread
- **Image Format**: PNG (self-contained encoder, no external image libs)
- **HTTP Client**: libcurl with SSL support
- **UI Framework**: ncurses for interactive table display
- **Multi-threading**: 10-thread pool for parallel CDN scanning
- **Default Timeouts**:
  - Alive scan: 1 second per request
  - Tracking: 5 seconds per request
  - Customizable via `--timeout-overrule <seconds>`
- **CIDR Expansion**:
  - Tracking: Samples 10 IPs per range for speed (~150 IPs)
  - Alive scan: Samples 100 IPs per range (~1,500 IPs)
  - Force-all mode: Expands complete ranges (500k+ IPs possible)
- **IP Range Updates**: Auto-downloaded from cloudflare.com, cached for 30 days
- **Alive IPs Cache**: Cached for 7 days, automatically used by --track
- **Performance**:
  - Alive scan (default): ~2 minutes for 1,500 IPs (10x faster with threading)
  - Alive scan (--force-all): Several hours for 500k+ IPs
  - Tracking (with alive cache): ~2-3 minutes for 200-300 IPs
  - Tracking (without cache): ~1-2 minutes for ~150 IPs

## Security Notes

- This tool is designed for authorized security research and testing only
- SSL verification is disabled for IP-based CDN probing (required for the technique)
- Use responsibly and only on systems you have permission to test
- Rate limiting is not implemented - be mindful of request volume

## License

[Your License Here]

## Contributing

Contributions welcome! Areas for improvement:
- IPv6 support
- Parallel request processing
- Additional CDN providers
- Export results to JSON/CSV
- Interactive TUI mode

## Author

[Your Name/Organization]
