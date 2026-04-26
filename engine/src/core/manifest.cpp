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
    auto running  = parseSemVer(eflVersion);

    if (!required || !running)
        return false;

    // Major version must match (breaking changes)
    if (required->major != running->major)
        return false;

    // Running version must be >= required version
    if (running->minor > required->minor) return true;
    if (running->minor < required->minor) return false;

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
    m.modId         = j["modId"].get<std::string>();
    m.name          = j["name"].get<std::string>();
    m.version       = j["version"].get<std::string>();
    m.eflVersion    = j["eflVersion"].get<std::string>();

    if (j.contains("author"))      m.author      = j["author"].get<std::string>();
    if (j.contains("description")) m.description = j["description"].get<std::string>();

    // Features — array of string tokens (schema v2).
    // Each token sets the corresponding bool flag.
    if (j.contains("features") && j["features"].is_array()) {
        for (const auto& token : j["features"]) {
            if (!token.is_string()) continue;
            const auto& s = token.get<std::string>();
            if      (s == "areas")      m.features.areas      = true;
            else if (s == "warps")      m.features.warps      = true;
            else if (s == "resources")  m.features.resources  = true;
            else if (s == "crafting")   m.features.crafting   = true;
            else if (s == "npcs")       m.features.npcs       = true;
            else if (s == "quests")     m.features.quests     = true;
            else if (s == "triggers")   m.features.triggers   = true;
            else if (s == "dialogue")   m.features.dialogue   = true;
            else if (s == "story")      m.features.story      = true;
            else if (s == "ipc")        m.features.ipc        = true;
            else if (s == "assets")     m.features.assets     = true;
            else if (s == "calendar")   m.features.calendar   = true;
            else if (s == "migrations") m.features.migrations = true;
        }
    }

    // Settings
    if (j.contains("settings")) {
        const auto& s = j["settings"];
        if (s.contains("strictMode"))  m.settings.strictMode  = s["strictMode"].get<bool>();
        if (s.contains("areaBackend")) m.settings.areaBackend = s["areaBackend"].get<std::string>();
    }

    // Dependencies — nested object with required, optional, conflicts arrays
    if (j.contains("dependencies") && j["dependencies"].is_object()) {
        const auto& deps = j["dependencies"];

        if (deps.contains("required") && deps["required"].is_array()) {
            for (const auto& dep : deps["required"]) {
                ManifestDependency d;
                if (dep.contains("modId"))        d.modId        = dep["modId"].get<std::string>();
                if (dep.contains("versionRange")) d.versionRange = dep["versionRange"].get<std::string>();
                m.dependencies.required.push_back(std::move(d));
            }
        }

        if (deps.contains("optional") && deps["optional"].is_array()) {
            for (const auto& dep : deps["optional"]) {
                ManifestDependency d;
                if (dep.contains("modId"))        d.modId        = dep["modId"].get<std::string>();
                if (dep.contains("versionRange")) d.versionRange = dep["versionRange"].get<std::string>();
                m.dependencies.optional.push_back(std::move(d));
            }
        }

        if (deps.contains("conflicts") && deps["conflicts"].is_array()) {
            for (const auto& dep : deps["conflicts"]) {
                ManifestConflict c;
                if (dep.contains("modId"))  c.modId  = dep["modId"].get<std::string>();
                if (dep.contains("reason")) c.reason = dep["reason"].get<std::string>();
                m.dependencies.conflicts.push_back(std::move(c));
            }
        }
    }

    // IPC — channel declarations (only meaningful when features.ipc == true)
    if (j.contains("ipc") && j["ipc"].is_object()) {
        const auto& ipc = j["ipc"];
        if (ipc.contains("publish") && ipc["publish"].is_array()) {
            for (const auto& ch : ipc["publish"])
                if (ch.is_string()) m.ipc.publish.push_back(ch.get<std::string>());
        }
        if (ipc.contains("consume") && ipc["consume"].is_array()) {
            for (const auto& ch : ipc["consume"])
                if (ch.is_string()) m.ipc.consume.push_back(ch.get<std::string>());
        }
    }

    // Assets — runtime injection declarations (only meaningful when features.assets == true)
    if (j.contains("assets") && j["assets"].is_object()) {
        const auto& assets = j["assets"];
        if (assets.contains("sprites") && assets["sprites"].is_array()) {
            for (const auto& id : assets["sprites"])
                if (id.is_string()) m.assets.sprites.push_back(id.get<std::string>());
        }
        if (assets.contains("sounds") && assets["sounds"].is_array()) {
            for (const auto& id : assets["sounds"])
                if (id.is_string()) m.assets.sounds.push_back(id.get<std::string>());
        }
    }

    // Script hooks
    if (j.contains("scriptHooks") && j["scriptHooks"].is_array()) {
        for (const auto& sh : j["scriptHooks"]) {
            ManifestScriptHook hook;
            if (sh.contains("target"))  hook.target  = sh["target"].get<std::string>();
            if (sh.contains("handler")) hook.handler = sh["handler"].get<std::string>();
            if (sh.contains("mode"))    hook.mode    = sh["mode"].get<std::string>();
            m.scriptHooks.push_back(std::move(hook));
        }
    }

    return m;
}

} // namespace efl
