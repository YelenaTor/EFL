cmake_minimum_required(VERSION 3.20)
project(EFL VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Dependencies
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_Declare(
    json_schema_validator
    GIT_REPOSITORY https://github.com/pboettch/json-schema-validator.git
    GIT_TAG 2.3.0
)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json json_schema_validator googletest)

# Aurie/YYTK stubs (swap for real SDKs later)
include_directories(vendor/Aurie/include)
include_directories(vendor/YYToolkit/include)

# EFL public headers
include_directories(include)

# Collect sources
file(GLOB_RECURSE EFL_SOURCES src/*.cpp)

# Build as shared library (DLL)
add_library(EFL SHARED ${EFL_SOURCES})

target_link_libraries(EFL PRIVATE
    nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator
)

target_compile_definitions(EFL PRIVATE
    EFL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    EFL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    EFL_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    EFL_BUILDING_DLL
)

# Tests
enable_testing()
add_subdirectory(tests)
