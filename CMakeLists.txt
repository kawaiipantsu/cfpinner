cmake_minimum_required(VERSION 3.14)
project(cfpinner VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find required packages
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${ZLIB_INCLUDE_DIRS})

# Source files
file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

# Main executable
add_executable(cfpinner ${SOURCES})

# Link libraries
target_link_libraries(cfpinner
    ${CURL_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

# Tests (optional - uncomment when adding tests)
# option(BUILD_TESTS "Build test programs" OFF)
# if(BUILD_TESTS)
#     enable_testing()
#     add_subdirectory(tests)
# endif()
