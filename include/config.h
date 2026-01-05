#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include "image_generator.h"

namespace cfpinner {

class Config {
public:
    Config();
    ~Config();

    // Save image metadata
    bool saveImageMetadata(const ImageMetadata& metadata);

    // Load image metadata by identifier
    bool loadImageMetadata(const std::string& identifier, ImageMetadata& metadata);

    // Get the config directory path
    std::string getConfigDir() const;

    // Get the images directory path
    std::string getImagesDir() const;

private:
    std::string config_dir_;
    std::string images_dir_;

    bool ensureDirectoriesExist();
    std::string getMetadataFilePath(const std::string& identifier) const;
};

} // namespace cfpinner

#endif // CONFIG_H
