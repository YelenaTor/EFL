cmake_minimum_required(VERSION 3.20)
project(EFL VERSION 1.0.0 LANGUAGES CXX)
set(EFL_VERSION_PRERELEASE "pre.2")

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

FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 3.0.2
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Allow FetchContent to configure miniz's older CMakeLists.txt
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(nlohmann_json json_schema_validator googletest miniz)

# EFL public headers
include_directories(include)

# Collect sources
file(GLOB_RECURSE EFL_SOURCES src/*.cpp)

# YYTK types implementation (RValue constructors, etc.)
set(YYTK_TYPES_CPP vendor/YYToolkit/include/YYToolkit/YYTK_Shared_Types.cpp)

# Build as shared library (DLL) — links against real Aurie/YYTK SDKs
add_library(EFL SHARED ${EFL_SOURCES} ${YYTK_TYPES_CPP})

target_include_directories(EFL PRIVATE
    vendor/Aurie/include
    vendor/YYToolkit/include
)

target_link_libraries(EFL PRIVATE
    nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator
    miniz
)

target_compile_definitions(EFL PRIVATE
    EFL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    EFL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    EFL_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    EFL_VERSION_PRERELEASE="${EFL_VERSION_PRERELEASE}"
    EFL_BUILDING_DLL
)

# Tests — use stub SDK for application-layer testing
enable_testing()
add_subdirectory(tests)
