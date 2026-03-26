#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace efl {

class EfpackLoader {
public:
    // Extracts efpack to %TEMP%/efl-packs/<stem>-<first8ofHash>/
    // Returns the temp directory path, or nullopt on failure
    static std::optional<std::filesystem::path> unpackToTemp(
        const std::filesystem::path& efpackPath);

    // Returns the manifestHash stored in pack-meta.json, or empty string if not found
    static std::string readPackedHash(const std::filesystem::path& efpackPath);

    // Returns the manifestHash stored in pack-meta.json inside the already-extracted temp dir
    static std::string readExtractedHash(const std::filesystem::path& tempDir);
};

} // namespace efl
