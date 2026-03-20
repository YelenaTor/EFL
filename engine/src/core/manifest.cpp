#include "efl/core/manifest.h"

#include <fstream>
#include <sstream>
#include <array>

namespace efl {

namespace {

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

std::optional<SemVer> parseSemVer(const std::string& s) {
    SemVer v;
    char dot1, dot2;
    std::istringstream ss(s);
    if (!(ss >> v.major >> dot1 >> v.minor >> dot2 >> v.patch))
        return std::nullopt;
    if (dot1 != '.' || dot2 != '.')
        return std::nullopt;
    return v;
}

} // anonymous namespace

std::optional<Manifest> ManifestParser::parseFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        return std::nullopt;

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(file);
    } catch (const nlohmann::json::parse_error&) {
        return std::nullopt;
    }

    return parseJson(j);
}

std::optional<Manifest> ManifestParser::parseString(const std::string& json) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
    } catch (const nlohmann::json::parse_error&) {
        return std::nullopt;
    }

    return parseJson(j);
}

bool ManifestParser::isCompatible(const Manifest& manifest, const std::string& eflVersion) {
    auto required = parseSemVer(manifest.eflVersion);
    auto running = parseSemVer(eflVersion);

    if (!required || !running)
        return false;

    // Major version must match (breaking changes)
    if (required->major != running->major)
        return false;

    // Running version must be >= required version
    if (running->minor > required->minor)
        return true;
    if (running->minor < required->minor)
        return false;

    return running->patch >= required->patch;
}

bool ManifestParser::validateRequired(const nlohmann::json& j) {
    static constexpr std::array<const char*, 5> requiredFields = {
        "schemaVersion", "modId", "name", "version", "eflVersion"
    };

    for (const auto& field : requiredFields) {
        if (!j.contains(field))
            return false;
    }
    return true;
}

std::optional<Manifest> ManifestParser::parseJson(const nlohmann::json& j) {
    if (!validateRequired(j))
        return std::nullopt;

    Manifest m;

    m.schemaVersion = j["schemaVersion"].get<int>();
    m.modId = j["modId"].get<std::string>();
    m.name = j["name"].get<std::string>();
    m.version = j["version"].get<std::string>();
    m.eflVersion = j["eflVersion"].get<std::string>();

    // Features
    if (j.contains("features")) {
        const auto& f = j["features"];
        if (f.contains("areas"))       m.features.areas       = f["areas"].get<bool>();
        if (f.contains("warps"))       m.features.warps       = f["warps"].get<bool>();
        if (f.contains("resources"))   m.features.resources   = f["resources"].get<bool>();
        if (f.contains("crafting"))    m.features.crafting    = f["crafting"].get<bool>();
        if (f.contains("npcs"))        m.features.npcs        = f["npcs"].get<bool>();
        if (f.contains("quests"))      m.features.quests      = f["quests"].get<bool>();
        if (f.contains("triggers"))    m.features.triggers    = f["triggers"].get<bool>();
        if (f.contains("dialogue"))    m.features.dialogue    = f["dialogue"].get<bool>();
        if (f.contains("story"))       m.features.story       = f["story"].get<bool>();
        if (f.contains("ipcPublish"))  m.features.ipcPublish  = f["ipcPublish"].get<bool>();
        if (f.contains("ipcConsume"))  m.features.ipcConsume  = f["ipcConsume"].get<bool>();
        if (f.contains("migrations"))  m.features.migrations  = f["migrations"].get<bool>();
    }

    // Settings
    if (j.contains("settings")) {
        const auto& s = j["settings"];
        if (s.contains("strictMode"))   m.settings.strictMode   = s["strictMode"].get<bool>();
        if (s.contains("areaBackend"))  m.settings.areaBackend  = s["areaBackend"].get<std::string>();
        if (s.contains("saveScope"))    m.settings.saveScope    = s["saveScope"].get<std::string>();
    }

    // Required dependencies
    if (j.contains("requiredDeps") && j["requiredDeps"].is_array()) {
        for (const auto& dep : j["requiredDeps"]) {
            ManifestDependency d;
            if (dep.contains("modId"))        d.modId = dep["modId"].get<std::string>();
            if (dep.contains("versionRange")) d.versionRange = dep["versionRange"].get<std::string>();
            m.requiredDeps.push_back(std::move(d));
        }
    }

    // Optional dependencies
    if (j.contains("optionalDeps") && j["optionalDeps"].is_array()) {
        for (const auto& dep : j["optionalDeps"]) {
            ManifestDependency d;
            if (dep.contains("modId"))        d.modId = dep["modId"].get<std::string>();
            if (dep.contains("versionRange")) d.versionRange = dep["versionRange"].get<std::string>();
            m.optionalDeps.push_back(std::move(d));
        }
    }

    return m;
}

} // namespace efl
