#ifndef CFPINNER_H
#define CFPINNER_H

#include <string>

namespace cfpinner {

class Application {
public:
    Application();
    ~Application();

    int run(int argc, char* argv[]);

private:
    void printUsage() const;
    void printBanner() const;
    int handleGenerate(const std::string& output_dir = "");
    int handleTrack(const std::string& identifier, const std::string& url, int timeout = 5, bool force_all = false, size_t num_threads = 10);
    int handleUpdateCDN();
    int handleAlive(int timeout = 1, bool force_all = false, size_t num_threads = 10);
};

} // namespace cfpinner

#endif // CFPINNER_H
