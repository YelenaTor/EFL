#pragma once
#include <filesystem>
#include <optional>

namespace efl {

class EfpackLoader {
public:
    // Extracts efpack to loadedRoot/<modId>/.
    // Reads manifest.efl from the archive to determine modId.
    // If the target directory already exists, reuses it without re-extracting.
    // Returns the extracted directory path, or nullopt on failure.
    static std::optional<std::filesystem::path> unpackToLoadedDir(
        const std::filesystem::path& efpackPath,
        const std::filesystem::path& loadedRoot);

    // Renames my_mod.efpack → my_mod.efpack.loaded as a skip marker.
    // Returns true on success.
    static bool markAsLoaded(const std::filesystem::path& efpackPath);
};

} // namespace efl
