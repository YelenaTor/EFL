#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

// Internal feature flags — derived from the features array in the manifest JSON.
// Only declare features you use; absence means false. In strictMode, accessing
// an undeclared feature is a fatal error.
struct ManifestFeatures {
    bool areas      = false;
    bool warps      = false;
    bool resources  = false;
    bool crafting   = false;
    bool npcs       = false;
    bool quests     = false;
    bool triggers   = false;
    bool dialogue   = false;
    bool story      = false;
    bool ipc        = false;  // replaces ipcPublish + ipcConsume; use manifest.ipc for channel details
    bool assets     = false;  // runtime asset injection via YYTK
    bool calendar   = false;  // V3 pilot: calendar / world-event registry
    bool migrations = false;
};

struct ManifestSettings {
    bool        strictMode  = false;
    std::string areaBackend = "hijacked"; // "hijacked" or "native"
    // saveScope is auto-derived as "EFL/<modId>" — not stored in manifest
};

struct ManifestDependency {
    std::string modId;
    std::string versionRange;
};

struct ManifestConflict {
    std::string modId;
    std::string reason; // shown in MANIFEST-E004 error message
};

struct ManifestDependencies {
    std::vector<ManifestDependency> required;   // MANIFEST-E003 if missing
    std::vector<ManifestDependency> optional;   // MANIFEST-W001 if missing
    std::vector<ManifestConflict>   conflicts;  // MANIFEST-E004 if present alongside this pack
};

// Channel names for cross-mod IPC. Only populated when features.ipc == true.
struct ManifestIpc {
    std::vector<std::string> publish; // channels this pack writes to (must be owned by this pack)
    std::vector<std::string> consume; // channels this pack reads from
};

// Asset IDs for runtime injection via YYTK. Only populated when features.assets == true.
// Corresponding files must exist at assets/sprites/<id>.png and assets/sounds/<id>.ogg
// inside the compiled .efpack.
struct ManifestAssets {
    std::vector<std::string> sprites;
    std::vector<std::string> sounds;
};

struct ManifestScriptHook {
    std::string target;            // GML script name, e.g. "gml_Script_hoe_node"
    std::string handler;           // Built-in EFL handler name, e.g. "efl_resource_despawn"
    std::string mode = "callback"; // "callback" (default) or "inject" (stubs with HOOK-W002)
};

struct Manifest {
    int         schemaVersion = 2;
    std::string modId;
    std::string name;
    std::string version;
    std::string eflVersion;
    std::string author;
    std::string description;

    ManifestDependencies        dependencies;
    ManifestFeatures            features;
    ManifestSettings            settings;
    ManifestIpc                 ipc;
    ManifestAssets              assets;
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
