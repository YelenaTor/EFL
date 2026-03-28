#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

struct ManifestFeatures {
    bool areas = false;
    bool warps = false;
    bool resources = false;
    bool crafting = false;
    bool npcs = false;
    bool quests = false;
    bool triggers = false;
    bool dialogue = false;
    bool story = false;
    bool ipcPublish = false;
    bool ipcConsume = false;
    bool migrations = false;
};

struct ManifestSettings {
    bool strictMode = false;
    std::string areaBackend = "hijacked";
    std::string saveScope;
};

struct ManifestDependency {
    std::string modId;
    std::string versionRange;
};

struct ManifestScriptHook {
    std::string target;            // GML script name, e.g. "gml_Script_hoe_node"
    std::string handler;           // Built-in EFL handler name, e.g. "efl_resource_despawn"
    std::string mode = "callback"; // "callback" (default) or "inject" (future, stubs with HOOK-W002)
};

struct Manifest {
    int schemaVersion = 1;
    std::string modId;
    std::string name;
    std::string version;
    std::string eflVersion;
    std::vector<ManifestDependency> requiredDeps;
    std::vector<ManifestDependency> optionalDeps;
    ManifestFeatures features;
    ManifestSettings settings;
    std::vector<ManifestScriptHook> scriptHooks;
    std::string packDir; // absolute path to the directory containing this manifest
};

class ManifestParser {
public:
    static std::optional<Manifest> parseFile(const std::string& path);
    static std::optional<Manifest> parseString(const std::string& json);
    static bool isCompatible(const Manifest& manifest, const std::string& eflVersion);
private:
    static std::optional<Manifest> parseJson(const nlohmann::json& j);
    static bool validateRequired(const nlohmann::json& j);
};

} // namespace efl
