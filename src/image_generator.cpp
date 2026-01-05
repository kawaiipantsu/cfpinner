#include "image_generator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <cstring>
#include <zlib.h>
#include "config.h"

namespace cfpinner {

ImageGenerator::ImageGenerator() {
}

ImageGenerator::~ImageGenerator() {
}

std::string ImageGenerator::generateUniqueId() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 0xFFFFFF);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(12) << timestamp
        << std::setw(6) << dis(gen);

    return oss.str();
}

std::string ImageGenerator::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::vector<uint8_t> ImageGenerator::createUniqueImageData(
    const std::string& identifier, uint32_t width, uint32_t height) {

    std::vector<uint8_t> data(width * height * 3); // RGB

    // Create a unique pattern based on identifier
    std::hash<std::string> hasher;
    size_t hash = hasher(identifier);
    std::mt19937 gen(hash);
    std::uniform_int_distribution<> dis(0, 255);

    // Generate a unique gradient pattern with identifier embedded
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            size_t idx = (y * width + x) * 3;

            // Create unique pattern based on position and identifier
            uint8_t r = static_cast<uint8_t>((x * 255 / width + hash) % 256);
            uint8_t g = static_cast<uint8_t>((y * 255 / height + (hash >> 8)) % 256);
            uint8_t b = static_cast<uint8_t>(((x + y) * 128 / (width + height) + (hash >> 16)) % 256);

            // Add some randomness
            r = (r + dis(gen)) / 2;
            g = (g + dis(gen)) / 2;
            b = (b + dis(gen)) / 2;

            data[idx] = r;
            data[idx + 1] = g;
            data[idx + 2] = b;
        }
    }

    return data;
}

bool ImageGenerator::writePNG(const std::string& filename,
                              const std::vector<uint8_t>& data,
                              uint32_t width, uint32_t height) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    // PNG signature
    const uint8_t png_signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    file.write(reinterpret_cast<const char*>(png_signature), 8);

    // Helper to write big-endian uint32
    auto write_be32 = [&file](uint32_t value) {
        uint8_t bytes[4] = {
            static_cast<uint8_t>(value >> 24),
            static_cast<uint8_t>(value >> 16),
            static_cast<uint8_t>(value >> 8),
            static_cast<uint8_t>(value)
        };
        file.write(reinterpret_cast<const char*>(bytes), 4);
    };

    // Helper to calculate CRC32
    auto calculate_crc = [](const std::vector<uint8_t>& data) -> uint32_t {
        return crc32(0L, data.data(), data.size());
    };

    // Write IHDR chunk
    {
        std::vector<uint8_t> ihdr_data;
        ihdr_data.push_back('I'); ihdr_data.push_back('H');
        ihdr_data.push_back('D'); ihdr_data.push_back('R');

        // Width
        ihdr_data.push_back(width >> 24);
        ihdr_data.push_back(width >> 16);
        ihdr_data.push_back(width >> 8);
        ihdr_data.push_back(width);

        // Height
        ihdr_data.push_back(height >> 24);
        ihdr_data.push_back(height >> 16);
        ihdr_data.push_back(height >> 8);
        ihdr_data.push_back(height);

        ihdr_data.push_back(8);  // Bit depth
        ihdr_data.push_back(2);  // Color type (RGB)
        ihdr_data.push_back(0);  // Compression
        ihdr_data.push_back(0);  // Filter
        ihdr_data.push_back(0);  // Interlace

        write_be32(13); // IHDR data length
        file.write(reinterpret_cast<const char*>(ihdr_data.data()), ihdr_data.size());
        write_be32(calculate_crc(ihdr_data));
    }

    // Write IDAT chunk (compressed image data)
    {
        // Prepare scanlines with filter byte
        std::vector<uint8_t> raw_data;
        for (uint32_t y = 0; y < height; y++) {
            raw_data.push_back(0); // Filter type: None
            for (uint32_t x = 0; x < width * 3; x++) {
                raw_data.push_back(data[y * width * 3 + x]);
            }
        }

        // Compress data
        uLongf compressed_size = compressBound(raw_data.size());
        std::vector<uint8_t> compressed_data(compressed_size);

        int result = compress(compressed_data.data(), &compressed_size,
                            raw_data.data(), raw_data.size());

        if (result != Z_OK) {
            std::cerr << "Compression failed" << std::endl;
            return false;
        }

        compressed_data.resize(compressed_size);

        std::vector<uint8_t> idat_chunk;
        idat_chunk.push_back('I'); idat_chunk.push_back('D');
        idat_chunk.push_back('A'); idat_chunk.push_back('T');
        idat_chunk.insert(idat_chunk.end(), compressed_data.begin(), compressed_data.end());

        write_be32(compressed_data.size());
        file.write(reinterpret_cast<const char*>(idat_chunk.data()), idat_chunk.size());
        write_be32(calculate_crc(idat_chunk));
    }

    // Write IEND chunk
    {
        std::vector<uint8_t> iend_data = {'I', 'E', 'N', 'D'};
        write_be32(0); // IEND data length
        file.write(reinterpret_cast<const char*>(iend_data.data()), iend_data.size());
        write_be32(calculate_crc(iend_data));
    }

    file.close();
    return true;
}

ImageMetadata ImageGenerator::generate(const std::string& custom_output_dir) {
    Config config;

    ImageMetadata metadata;
    metadata.identifier = generateUniqueId();
    metadata.width = 512;
    metadata.height = 512;
    metadata.timestamp = getCurrentTimestamp();
    metadata.filename = metadata.identifier + ".png";

    // Determine output directory
    std::string output_dir;
    if (custom_output_dir.empty()) {
        output_dir = config.getImagesDir();
    } else {
        output_dir = custom_output_dir;
        // Remove trailing slash if present
        if (output_dir.back() == '/') {
            output_dir.pop_back();
        }
    }

    metadata.full_path = output_dir + "/" + metadata.filename;

    std::cout << "Generating unique image..." << std::endl;
    std::cout << "Identifier: " << metadata.identifier << std::endl;

    auto image_data = createUniqueImageData(metadata.identifier,
                                           metadata.width, metadata.height);

    if (!writePNG(metadata.full_path, image_data, metadata.width, metadata.height)) {
        throw std::runtime_error("Failed to write PNG file");
    }

    // Always save metadata to default location for tracking
    if (!config.saveImageMetadata(metadata)) {
        throw std::runtime_error("Failed to save image metadata");
    }

    std::cout << "Image saved: " << metadata.full_path << std::endl;
    std::cout << "Dimensions: " << metadata.width << "x" << metadata.height << std::endl;

    return metadata;
}

std::string ImageGenerator::getImagePath(const std::string& identifier) const {
    Config config;
    return config.getImagesDir() + "/" + identifier + ".png";
}

} // namespace cfpinner
