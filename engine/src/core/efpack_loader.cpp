#include "efl/core/efpack_loader.h"

#include <fstream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef EFL_STUB_SDK
#include <miniz.h>
#endif

namespace efl {

namespace {

std::filesystem::path systemTempDir() {
#ifdef _WIN32
    char buf[MAX_PATH + 1] = {};
    DWORD len = GetTempPathA(static_cast<DWORD>(sizeof(buf)), buf);
    if (len > 0 && len < sizeof(buf))
        return std::filesystem::path(buf);
#endif
    return std::filesystem::temp_directory_path();
}

std::string readHashFromDir(const std::filesystem::path& dir) {
    std::filesystem::path metaPath = dir / "pack-meta.json";
    std::ifstream f(metaPath);
    if (!f.is_open())
        return {};
    try {
        nlohmann::json j;
        f >> j;
        if (j.contains("manifestHash") && j["manifestHash"].is_string())
            return j["manifestHash"].get<std::string>();
    } catch (...) {}
    return {};
}

} // anonymous namespace

std::string EfpackLoader::readPackedHash(const std::filesystem::path& efpackPath) {
#ifndef EFL_STUB_SDK
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, efpackPath.string().c_str(), 0))
        return {};

    int idx = mz_zip_reader_locate_file(&zip, "pack-meta.json", nullptr, 0);
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
        if (j.contains("manifestHash") && j["manifestHash"].is_string())
            return j["manifestHash"].get<std::string>();
    } catch (...) {}
    return {};
#else
    (void)efpackPath;
    return {};
#endif
}

std::string EfpackLoader::readExtractedHash(const std::filesystem::path& tempDir) {
    return readHashFromDir(tempDir);
}

std::optional<std::filesystem::path> EfpackLoader::unpackToTemp(
        const std::filesystem::path& efpackPath) {
#ifdef EFL_STUB_SDK
    (void)efpackPath;
    return std::nullopt;
#else
    std::string hash = readPackedHash(efpackPath);
    if (hash.empty())
        return std::nullopt;

    std::string first8 = hash.size() >= 8 ? hash.substr(0, 8) : hash;
    std::string stem   = efpackPath.stem().string();

    std::filesystem::path tempDir =
        systemTempDir() / "efl-packs" / (stem + "-" + first8);

    // Cache hit: reuse existing extraction if the hash still matches.
    if (std::filesystem::exists(tempDir) && readExtractedHash(tempDir) == hash)
        return tempDir;

    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, efpackPath.string().c_str(), 0))
        return std::nullopt;

    std::error_code ec;
    std::filesystem::create_directories(tempDir, ec);
    if (ec) {
        mz_zip_reader_end(&zip);
        return std::nullopt;
    }

    mz_uint numFiles = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < numFiles; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            mz_zip_reader_end(&zip);
            return std::nullopt;
        }

        std::filesystem::path destPath = tempDir / stat.m_filename;

        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            std::filesystem::create_directories(destPath, ec);
            if (ec) {
                mz_zip_reader_end(&zip);
                return std::nullopt;
            }
            continue;
        }

        std::filesystem::create_directories(destPath.parent_path(), ec);
        if (ec) {
            mz_zip_reader_end(&zip);
            return std::nullopt;
        }

        if (!mz_zip_reader_extract_to_file(&zip, i, destPath.string().c_str(), 0)) {
            mz_zip_reader_end(&zip);
            return std::nullopt;
        }
    }

    mz_zip_reader_end(&zip);
    return tempDir;
#endif
}

} // namespace efl
