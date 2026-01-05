#include "config.h"
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

namespace cfpinner {

Config::Config() {
    // Get home directory
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw->pw_dir;
    }

    config_dir_ = std::string(home) + "/.cfpinner";
    images_dir_ = config_dir_ + "/images";

    ensureDirectoriesExist();
}

Config::~Config() {
}

bool Config::ensureDirectoriesExist() {
    // Create config directory
    struct stat st;
    if (stat(config_dir_.c_str(), &st) != 0) {
        if (mkdir(config_dir_.c_str(), 0755) != 0) {
            std::cerr << "Failed to create config directory: " << config_dir_ << std::endl;
            return false;
        }
    }

    // Create images directory
    if (stat(images_dir_.c_str(), &st) != 0) {
        if (mkdir(images_dir_.c_str(), 0755) != 0) {
            std::cerr << "Failed to create images directory: " << images_dir_ << std::endl;
            return false;
        }
    }

    return true;
}

std::string Config::getConfigDir() const {
    return config_dir_;
}

std::string Config::getImagesDir() const {
    return images_dir_;
}

std::string Config::getMetadataFilePath(const std::string& identifier) const {
    return config_dir_ + "/" + identifier + ".meta";
}

bool Config::saveImageMetadata(const ImageMetadata& metadata) {
    std::string filepath = getMetadataFilePath(metadata.identifier);
    std::ofstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Failed to save metadata: " << filepath << std::endl;
        return false;
    }

    file << "identifier=" << metadata.identifier << std::endl;
    file << "filename=" << metadata.filename << std::endl;
    file << "full_path=" << metadata.full_path << std::endl;
    file << "width=" << metadata.width << std::endl;
    file << "height=" << metadata.height << std::endl;
    file << "timestamp=" << metadata.timestamp << std::endl;

    file.close();
    return true;
}

bool Config::loadImageMetadata(const std::string& identifier, ImageMetadata& metadata) {
    std::string filepath = getMetadataFilePath(identifier);
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Metadata not found for identifier: " << identifier << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            if (key == "identifier") {
                metadata.identifier = value;
            } else if (key == "filename") {
                metadata.filename = value;
            } else if (key == "full_path") {
                metadata.full_path = value;
            } else if (key == "width") {
                metadata.width = std::stoul(value);
            } else if (key == "height") {
                metadata.height = std::stoul(value);
            } else if (key == "timestamp") {
                metadata.timestamp = value;
            }
        }
    }

    file.close();
    return true;
}

} // namespace cfpinner
