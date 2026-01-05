#include "cfpinner.h"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        cfpinner::Application app;
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
