#ifndef IMAGE_GENERATOR_H
#define IMAGE_GENERATOR_H

#include <string>
#include <vector>
#include <cstdint>

namespace cfpinner {

struct ImageMetadata {
    std::string identifier;
    std::string filename;
    std::string full_path;
    uint32_t width;
    uint32_t height;
    std::string timestamp;
};

class ImageGenerator {
public:
    ImageGenerator();
    ~ImageGenerator();

    // Generate a unique PNG image and return its identifier
    // If custom_output_dir is empty, uses default ~/.cfpinner/images/
    ImageMetadata generate(const std::string& custom_output_dir = "");

    // Get the path to the generated image
    std::string getImagePath(const std::string& identifier) const;

private:
    std::string generateUniqueId() const;
    bool writePNG(const std::string& filename,
                  const std::vector<uint8_t>& data,
                  uint32_t width, uint32_t height);
    std::vector<uint8_t> createUniqueImageData(const std::string& identifier,
                                               uint32_t width, uint32_t height);
    std::string getCurrentTimestamp() const;
};

} // namespace cfpinner

#endif // IMAGE_GENERATOR_H
