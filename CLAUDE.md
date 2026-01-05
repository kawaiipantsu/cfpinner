# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CFPinner is a network security tool for tracking image distribution across Cloudflare's CDN infrastructure. It generates unique PNG images that can be uploaded to services suspected of using Cloudflare, then tracks which CDN nodes have cached the image.

**Use Case**: Identify which Cloudflare CDN locations are serving content for a target domain, useful for security research and CDN analysis.

## Build Commands

### Using Makefile (Recommended)

The project includes a Makefile that wraps CMake commands for convenience:

```bash
# Install dependencies (Debian/Ubuntu)
make deps

# Build (Release mode)
make

# Build in Debug mode
make debug

# Clean and rebuild
make rebuild

# Deep clean
make distclean

# Run with help
make run

# Generate test image
make generate

# Install system-wide to /usr/local/bin
make install

# Show all commands
make help
```

### Manual CMake Commands

```bash
# Install required packages (Debian/Ubuntu)
sudo apt-get install build-essential cmake libcurl4-openssl-dev zlib1g-dev

# Initial build setup
mkdir -p build && cd build
cmake ..
make

# Rebuild after changes
cd build && make

# Debug build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Release build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Clean and rebuild
rm -rf build/* && cd build && cmake .. && make
```

## Usage Commands

### Generate a unique tracking image
```bash
# Generate to default location (~/.cfpinner/images/)
./build/cfpinner --generate

# Generate to custom directory
./build/cfpinner --generate --save /tmp
./build/cfpinner --generate --save ./output
./build/cfpinner -g -s /path/to/dir
```

### Track an image across Cloudflare CDN
```bash
./build/cfpinner --track <identifier> <url>
./build/cfpinner -t <identifier> <url>

# Use custom number of threads (default: 10)
./build/cfpinner --track <identifier> <url> --threads 20
```

### Update Cloudflare IP ranges
```bash
# Force update of IP ranges
./build/cfpinner --update-cdn
./build/cfpinner -u

# IP ranges are stored in ~/.cfpinner/cf_cdn_ips.txt
# Auto-updated if older than 30 days during tracking
```

### Scan for alive CDN nodes (optional but recommended)
```bash
# Discover which Cloudflare CDN nodes are responsive
# Uses expanded CIDR ranges (100 IPs per range vs 10 for tracking)
# Multi-threaded scanning (default: 10 threads)
./build/cfpinner --alive
./build/cfpinner -a

# Use custom number of threads for faster/slower scanning
./build/cfpinner --alive --threads 20  # Faster with more threads
./build/cfpinner --alive --threads 5   # Slower, less resource usage

# Output: Scans 1,500 IPs comprehensively (~2 hours with 10 threads)
# Saves 200-300 alive IPs typically
# Alive IPs stored in ~/.cfpinner/alive_ips.txt
# Cache valid for 7 days
```

### Example workflow
```bash
# 0. (Optional but recommended) Discover alive CDN nodes first
./build/cfpinner --alive
# Comprehensive scan: 1,500 IPs checked (~2 hours)
# One-time investment for faster subsequent tracking

# 1. Generate image (custom location)
./build/cfpinner --generate --save /var/www/uploads
# Output: Identifier: abc123def456

# 2. Upload generated image to target service
# (Manual step - or image may already be in web-accessible directory)

# 3. Track where it's cached (uses alive IPs cache if available)
# Multi-threaded tracking (default: 10 threads)
./build/cfpinner --track abc123def456 https://target-site.com/uploads/abc123def456.png
# With alive cache + 10 threads: checks ~250 alive IPs (2-3 minutes)
# Without cache + 10 threads: checks ~150 sampled IPs (1-2 minutes)
# Alive cache provides better coverage across all Cloudflare regions

# Use more threads for even faster tracking
./build/cfpinner --track abc123def456 https://target-site.com/uploads/abc123def456.png --threads 20
```

## Architecture

### Core Modules

1. **Application** (`cfpinner.h/cpp`): Main application entry point and command-line interface
2. **ImageGenerator** (`image_generator.h/cpp`): Generates unique PNG images with embedded fingerprints
3. **HTTPClient** (`http_client.h/cpp`): HTTP/HTTPS client using libcurl for CDN probing
4. **CDNTracker** (`cdn_tracker.h/cpp`): Orchestrates tracking and alive node scanning
5. **CDNUpdater** (`cdn_updater.h/cpp`): Downloads and manages Cloudflare IP ranges and alive IPs cache
6. **CIDRUtils** (`cidr_utils.h/cpp`): CIDR notation parsing and IP range expansion
7. **Config** (`config.h/cpp`): Manages configuration and image metadata storage

### Data Flow

```
Generate Mode:
  Application → ImageGenerator → Config
                     ↓
              Unique PNG created
                     ↓
          ~/.cfpinner/images/*.png

Track Mode:
  Application → Config (load metadata)
                     ↓
              CDNTracker (load IP ranges)
                     ↓
              HTTPClient (probe each IP)
                     ↓
            Display HIT/MISS results
```

### Key Design Patterns

- **Separation of Concerns**: Each module has a single, well-defined responsibility
- **Header/Implementation Split**: Public interfaces in `include/`, implementations in `src/`
- **RAII**: Resource management through constructors/destructors
- **Namespace Encapsulation**: All code under `cfpinner` namespace

### Image Generation

- Creates 512x512 PNG images with unique visual patterns
- Pattern derived from cryptographic hash of identifier
- Uses raw PNG encoding with zlib compression
- No external image libraries required (self-contained PNG writer)
- Supports custom output directory via `--save` option
- Metadata always stored in `~/.cfpinner/` for tracking, regardless of image location

### CDN Tracking

- Auto-downloads Cloudflare IP ranges from cloudflare.com/ips-v4
- Stores ranges in `~/.cfpinner/cf_cdn_ips.txt` (cached for 30 days)
- **Multi-threaded Scanning**: Both tracking and alive scanning use parallel threads
  - Default: 10 threads for optimal performance
  - Configurable via `--threads <num>` option
  - Massive speed improvement: 10x faster than single-threaded
- **Two-tier CIDR expansion strategy**:
  - **Tracking mode**: Samples 10 IPs per range (~150 total) for fast results
  - **Alive scan mode**: Samples 100 IPs per range (~1,500 total) for comprehensive discovery
- **Alive Node Discovery**: `--alive` performs extensive scan with strategic sampling
  - Tests 1,500 IPs across all Cloudflare ranges
  - Takes ~2 hours with 10 threads (or ~1 hour with 20 threads)
  - Typically finds 200-300 responsive CDN nodes
- Alive IPs cached in `~/.cfpinner/alive_ips.txt` (valid for 7 days)
- **Smart Tracking**: Automatically uses alive cache if available
  - Better coverage: checks all known-alive nodes
  - Fast: ~2-3 minutes with 10 threads for 250 IPs
- Makes HTTP HEAD requests to each IP with proper Host headers
- **CloudFlare Header Capture**:
  - `CF-Cache-Status`: HIT/MISS/EXPIRED/etc.
  - `CF-Ray`: Ray ID with IATA airport code extraction (e.g., "8428f15b8a9c1234-SJC" → "SJC")
  - `CF-IPCountry`: Country code of the CDN node (if present)
- **Results Display**:
  - Real-time progress indicator during scan
  - After scan: ASCII table with results printed to stdout
  - Columns: IP Address, Status, Cache Status, IATA Code, Country, CF-Ray
  - Color-coded with ANSI codes: Green (HIT), Yellow (MISS), Red (ERROR)
  - All results displayed at once (use terminal scrollback to review)
  - Summary statistics with percentages at bottom
  - Output can be piped or redirected for scripting/automation

### CIDR Expansion

- IPv4 CIDR ranges expanded to individual IP addresses
- **Strategic sampling** for large ranges (like /13 with 500k+ IPs):
  - Distributes samples evenly across entire IP space
  - Adds variation within segments to check different subnets
  - Avoids checking only the first IP of each block
- **Two sampling modes**:
  - **Tracking**: 10 IPs per range (fast, adequate coverage)
  - **Alive scan**: 100 IPs per range (comprehensive, better discovery)
- Skips network and broadcast addresses
- Small ranges (<256 IPs): all IPs checked
- IPv6 ranges currently ignored

**Example for 104.16.0.0/13 (524,288 IPs)**:
- Tracking: samples 10 IPs strategically
- Alive scan: samples 100 IPs with full coverage

### Security Considerations

- Disables SSL verification for CDN testing (necessary for IP-based requests)
- 5-second timeout per request to prevent hangs
- IPv6 ranges currently skipped (can be enhanced)
- No persistent network state between requests

## Directory Structure

- `src/` - Implementation files (.cpp)
- `include/` - Public header files (.h)
- `tests/` - Test files (currently empty)
- `build/` - CMake build output (gitignored)
- `cf_cdn_ips.txt` - Cloudflare IP ranges (required for tracking)
- `~/.cfpinner/` - User data directory (created at runtime)
  - `images/` - Generated PNG files
  - `*.meta` - Image metadata files

## Build System

- Uses CMake 3.14+
- C++17 standard required
- Dependencies: libcurl, zlib
- Compiler warnings enabled: `-Wall -Wextra -Wpedantic`
- Debug builds use `-g -O0`
- Release builds use `-O3 -DNDEBUG`

## Code Organization

- All project code is under the `cfpinner` namespace
- Headers in `include/` define public interfaces
- Implementation in `src/` provides the definitions
- Header guards follow the pattern `FILENAME_H`
- Error handling via exceptions (caught in Application class)

## Development Workflow

When adding new features:
1. Add header file to `include/`
2. Add implementation to `src/`
3. Include the header in files that need it
4. Update Application class if adding new commands
5. Rebuild with `make` from the `build/` directory

CMake automatically picks up new `.cpp` files in `src/` via `GLOB_RECURSE`.

## Testing

To test the application:
1. Ensure `cf_cdn_ips.txt` exists in the project root
2. Generate a test image: `./build/cfpinner --generate`
3. Check that image appears in `~/.cfpinner/images/`
4. Test tracking with a known Cloudflare-hosted site
