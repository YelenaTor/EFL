#include "efl/core/efpack_loader.h"

#include <fstream>
#include <nlohmann/json.hpp>

#ifndef EFL_STUB_SDK
#include <miniz.h>
#endif

namespace efl {

namespace {

#ifndef EFL_STUB_SDK

// Read manifest.efl from inside a ZIP archive and return the modId field.
std::string readModIdFromArchive(const std::filesystem::path& efpackPath) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, efpackPath.string().c_str(), 0))
        return {};

    int idx = mz_zip_reader_locate_file(&zip, "manifest.efl", nullptr, 0);
    if (idx < 0) {
        mz_zip_reader_end(&zip);
        return {};
    }

    size_t size = 0;
    void* data = mz_zip_reader_extract_to_heap(&zip, static_cast<mz_uint>(idx), &size, 0);
    mz_zip_reader_end(&zip);
    if (!data)
        return {};

    std::string raw(static_cast<char*>(data), size);
    mz_free(data);

    try {
        nlohmann::json j = nlohmann::json::parse(raw);
        if (j.contains("modId") && j["modId"].is_string())
            return j["modId"].get<std::string>();
    } catch (...) {}
    return {};
}

// Sanitize an archived path to prevent traversal attacks.
// Returns an empty path if the entry is unsafe.
std::filesystem::path sanitizeArchivePath(const char* archiveName) {
    std::filesystem::path p = std::filesystem::path(archiveName).lexically_normal();

    // Reject absolute paths and paths that escape via ..
    if (p.is_absolute())
        return {};
    for (const auto& part : p) {
        if (part == "..")
            return {};
    }
    return p;
}

// Extract all files from a ZIP archive into destDir.
// Returns false if any file fails or has an unsafe path.
bool extractArchive(const std::filesystem::path& efpackPath,
                    const std::filesystem::path& destDir) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, efpackPath.string().c_str(), 0))
        return false;

    mz_uint numFiles = mz_zip_reader_get_num_files(&zip);
    std::error_code ec;

    for (mz_uint i = 0; i < numFiles; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            mz_zip_reader_end(&zip);
            return false;
        }

        std::filesystem::path rel = sanitizeArchivePath(stat.m_filename);
        if (rel.empty())
            continue; // skip unsafe entries silently

        std::filesystem::path dest = destDir / rel;

        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            std::filesystem::create_directories(dest, ec);
            if (ec) { mz_zip_reader_end(&zip); return false; }
            continue;
        }

        std::filesystem::create_directories(dest.parent_path(), ec);
        if (ec) { mz_zip_reader_end(&zip); return false; }

        if (!mz_zip_reader_extract_to_file(&zip, i, dest.string().c_str(), 0)) {
            mz_zip_reader_end(&zip);
            return false;
        }
    }

    mz_zip_reader_end(&zip);
    return true;
}

#endif // !EFL_STUB_SDK

} // anonymous namespace

std::optional<std::filesystem::path> EfpackLoader::unpackToLoadedDir(
        const std::filesystem::path& efpackPath,
        const std::filesystem::path& loadedRoot) {
#ifdef EFL_STUB_SDK
    (void)efpackPath;
    (void)loadedRoot;
    return std::nullopt;
#else
    std::string modId = readModIdFromArchive(efpackPath);
    if (modId.empty())
        return std::nullopt;

    std::filesystem::path targetDir = loadedRoot / modId;

    // If already extracted from a previous boot (or failed rename), reuse it.
    if (std::filesystem::exists(targetDir))
        return targetDir;

    std::error_code ec;
    std::filesystem::create_directories(targetDir, ec);
    if (ec)
        return std::nullopt;

    if (!extractArchive(efpackPath, targetDir)) {
        // Clean up partial extraction so next boot retries.
        std::filesystem::remove_all(targetDir, ec);
        return std::nullopt;
    }

    return targetDir;
#endif
}

bool EfpackLoader::markAsLoaded(const std::filesystem::path& efpackPath) {
    std::filesystem::path loadedPath(efpackPath.string() + ".loaded");
    std::error_code ec;
    std::filesystem::rename(efpackPath, loadedPath, ec);
    return !ec;
}

} // namespace efl
