#include "efl/core/bootstrap.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace efl {

namespace {

std::string eflVersionString() {
    std::ostringstream ss;
    ss << EFL_VERSION_MAJOR << "." << EFL_VERSION_MINOR << "." << EFL_VERSION_PATCH;
    constexpr const char* prerelease = EFL_VERSION_PRERELEASE;
    if (prerelease[0] != '\0') {
        ss << "-" << prerelease;
    }
    return ss.str();
}

std::string buildPipeName() {
#ifdef _WIN32
    DWORD pid = GetCurrentProcessId();
#else
    int pid = 0;
#endif
    return "\\\\.\\pipe\\efl-" + std::to_string(pid);
}

} // anonymous namespace

EflBootstrap::EflBootstrap() = default;

EflBootstrap::~EflBootstrap() {
    shutdown();
}

bool EflBootstrap::initialize(const std::string& contentDir) {
    // Create the named pipe for TUI communication
    pipe_ = std::make_unique<PipeWriter>(buildPipeName());
    pipe_->create(); // Best-effort; bootstrap continues even if pipe fails

    log_.info("BOOT", "EFL v" + eflVersionString() + " initializing");

    if (!stepVersionCheck())
        return false;

    if (!stepDiscoverManifests(contentDir))
        return false;

    if (!stepValidateManifests())
        return false;

    stepLoadContent(contentDir);

    emitBootStatus("init", "complete",
                   std::to_string(manifests_.size()) + " manifest(s) loaded");
    log_.info("BOOT", "Bootstrap complete — " +
              std::to_string(manifests_.size()) + " manifest(s) loaded");
    return true;
}

void EflBootstrap::shutdown() {
    if (pipe_) {
        pipe_->close();
    }
    log_.info("BOOT", "EFL shutdown");
}

LogService& EflBootstrap::log() { return log_; }
DiagnosticEmitter& EflBootstrap::diagnostics() { return diagnostics_; }
PipeWriter& EflBootstrap::pipe() { return *pipe_; }
RegistryService& EflBootstrap::registries() { return registries_; }
const std::vector<Manifest>& EflBootstrap::manifests() const { return manifests_; }

bool EflBootstrap::stepVersionCheck() {
    emitBootStatus("version_check", "running");
    std::string ver = eflVersionString();
    log_.info("BOOT", "Version check: EFL v" + ver);
    emitBootStatus("version_check", "pass", "EFL v" + ver);
    return true;
}

bool EflBootstrap::stepDiscoverManifests(const std::string& contentDir) {
    emitBootStatus("discover", "running");

    namespace fs = std::filesystem;

    if (!fs::exists(contentDir) || !fs::is_directory(contentDir)) {
        log_.warn("BOOT", "Content directory not found: " + contentDir);
        diagnostics_.emit("BOOT-W001", Severity::Warning, "BOOT",
                          "Content directory does not exist: " + contentDir,
                          "Create the directory and place .efl manifest files in it");
        emitBootStatus("discover", "warn", "content directory not found");
        // Not a fatal error — zero manifests is valid
        return true;
    }

    int found = 0;
    for (const auto& entry : fs::directory_iterator(contentDir)) {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() != ".efl")
            continue;

        auto manifest = ManifestParser::parseFile(entry.path().string());
        if (manifest) {
            log_.info("BOOT", "Loaded manifest: " + manifest->modId +
                      " v" + manifest->version);
            manifests_.push_back(std::move(*manifest));
            ++found;
        } else {
            log_.error("BOOT", "Failed to parse manifest: " + entry.path().string());
            diagnostics_.emit("MANIFEST-E001", Severity::Error, "MANIFEST",
                              "Failed to parse manifest: " + entry.path().filename().string(),
                              "Check JSON syntax and required fields");
        }
    }

    emitBootStatus("discover", "pass", std::to_string(found) + " manifest(s) found");
    return true;
}

bool EflBootstrap::stepValidateManifests() {
    emitBootStatus("validate", "running");

    std::string eflVer = eflVersionString();
    bool allOk = true;

    for (const auto& m : manifests_) {
        if (!CompatibilityService::isCompatible(eflVer, m.eflVersion)) {
            log_.error("BOOT", "Manifest '" + m.modId +
                       "' requires EFL v" + m.eflVersion +
                       " but running v" + eflVer);
            diagnostics_.emit("MANIFEST-E002", Severity::Error, "MANIFEST",
                              "Mod '" + m.modId + "' requires EFL v" + m.eflVersion +
                              " (running v" + eflVer + ")",
                              "Update EFL or use a compatible mod version");
            allOk = false;
        }
    }

    if (allOk) {
        emitBootStatus("validate", "pass");
    } else {
        emitBootStatus("validate", "fail", "version incompatibilities found");
    }

    return allOk;
}

void EflBootstrap::stepLoadContent(const std::string& contentDir) {
    namespace fs = std::filesystem;

    emitBootStatus("load_content", "running");

    if (manifests_.empty()) {
        emitBootStatus("load_content", "skip", "no manifests");
        return;
    }

    // For each manifest, scan the content pack directory alongside its manifest file.
    // The content pack directory is the directory containing the manifest.
    for (const auto& manifest : manifests_) {
        // Find the manifest's directory (contentDir itself is the pack root when
        // there is one manifest.efl directly inside contentDir).
        fs::path packDir = fs::path(contentDir);

        log_.info("BOOT", "Loading content for: " + manifest.modId);

        // Helper: load all JSON files from a subdirectory, call a loader per file
        auto loadDir = [&](const std::string& subdir, auto loader) {
            fs::path dir = packDir / subdir;
            if (!fs::exists(dir) || !fs::is_directory(dir))
                return;

            for (const auto& entry : fs::directory_iterator(dir)) {
                if (!entry.is_regular_file())
                    continue;
                if (entry.path().extension() != ".json")
                    continue;

                std::ifstream f(entry.path());
                if (!f.is_open()) {
                    log_.warn("BOOT", "Cannot open: " + entry.path().string());
                    continue;
                }

                try {
                    nlohmann::json j;
                    f >> j;
                    loader(j, entry.path().filename().string());
                } catch (const std::exception& ex) {
                    log_.error("BOOT", "JSON parse error in " +
                               entry.path().string() + ": " + ex.what());
                    diagnostics_.emit("BOOT-E001", Severity::Error, "BOOT",
                                      "JSON parse error: " + entry.path().filename().string(),
                                      ex.what());
                }
            }
        };

        // Triggers — load first so other systems can reference them
        if (manifest.features.triggers) {
            loadDir("triggers", [&](const nlohmann::json& j, const std::string& file) {
                try {
                    registries_.triggers().registerFromJson(j);
                    log_.info("BOOT", "Registered trigger from " + file);
                } catch (const std::exception& ex) {
                    log_.error("BOOT", "Failed to register trigger from " + file + ": " + ex.what());
                    diagnostics_.emit("TRIGGER-E001", Severity::Error, "TRIGGER",
                                      "Failed to register trigger: " + file, ex.what());
                }
            });
        }

        // Areas
        if (manifest.features.areas) {
            loadDir("areas", [&](const nlohmann::json& j, const std::string& file) {
                auto def = AreaDef::fromJson(j);
                if (def) {
                    registries_.areas().registerArea(*def);
                    log_.info("BOOT", "Registered area: " + def->id);
                } else {
                    log_.error("BOOT", "Failed to parse area: " + file);
                    diagnostics_.emit("AREA-E001", Severity::Error, "AREA",
                                      "Failed to parse area definition: " + file,
                                      "Check required fields: id, displayName, backend");
                }
            });
        }

        // Warps
        if (manifest.features.warps) {
            loadDir("warps", [&](const nlohmann::json& j, const std::string& file) {
                auto def = WarpDef::fromJson(j);
                if (def) {
                    registries_.warps().registerWarp(*def);
                    log_.info("BOOT", "Registered warp: " + def->id);
                } else {
                    log_.error("BOOT", "Failed to parse warp: " + file);
                    diagnostics_.emit("WARP-E001", Severity::Error, "WARP",
                                      "Failed to parse warp definition: " + file,
                                      "Check required fields: id, sourceArea, targetArea");
                }
            });
        }

        // Resources
        if (manifest.features.resources) {
            loadDir("resources", [&](const nlohmann::json& j, const std::string& file) {
                auto def = ResourceDef::fromJson(j);
                if (def) {
                    registries_.resources().registerResource(*def);
                    log_.info("BOOT", "Registered resource: " + def->id);
                } else {
                    log_.error("BOOT", "Failed to parse resource: " + file);
                    diagnostics_.emit("RESOURCE-E001", Severity::Error, "RESOURCE",
                                      "Failed to parse resource definition: " + file,
                                      "Check required fields: id, kind");
                }
            });
        }

        // Quests
        if (manifest.features.quests) {
            loadDir("quests", [&](const nlohmann::json& j, const std::string& file) {
                auto def = QuestDef::fromJson(j);
                if (def) {
                    registries_.quests().registerQuest(*def);
                    log_.info("BOOT", "Registered quest: " + def->id);
                } else {
                    log_.error("BOOT", "Failed to parse quest: " + file);
                    diagnostics_.emit("QUEST-E001", Severity::Error, "QUEST",
                                      "Failed to parse quest definition: " + file,
                                      "Check required fields: id, title, stages");
                }
            });
        }

        // NPCs
        if (manifest.features.npcs) {
            loadDir("npcs", [&](const nlohmann::json& j, const std::string& file) {
                auto def = NpcDef::fromJson(j);
                if (def) {
                    registries_.npcs().registerNpc(*def);
                    log_.info("BOOT", "Registered NPC: " + def->id);
                } else {
                    log_.error("BOOT", "Failed to parse NPC: " + file);
                    diagnostics_.emit("NPC-E001", Severity::Error, "NPC",
                                      "Failed to parse NPC definition: " + file,
                                      "Check required fields: id, displayName");
                }
            });
        }

        // Crafting / recipes
        if (manifest.features.crafting) {
            loadDir("recipes", [&](const nlohmann::json& j, const std::string& file) {
                auto def = RecipeDef::fromJson(j);
                if (def) {
                    registries_.crafting().registerRecipe(*def);
                    log_.info("BOOT", "Registered recipe: " + def->id);
                } else {
                    log_.error("BOOT", "Failed to parse recipe: " + file);
                    diagnostics_.emit("CRAFT-E001", Severity::Error, "CRAFT",
                                      "Failed to parse recipe definition: " + file,
                                      "Check required fields: id, output, station, ingredients");
                }
            });
        }

        // Dialogue
        if (manifest.features.dialogue) {
            loadDir("dialogue", [&](const nlohmann::json& j, const std::string& file) {
                auto def = DialogueDef::fromJson(j);
                if (def) {
                    registries_.dialogue().registerDialogue(*def);
                    log_.info("BOOT", "Registered dialogue: " + def->id);
                } else {
                    log_.error("BOOT", "Failed to parse dialogue: " + file);
                    diagnostics_.emit("DIALOGUE-E001", Severity::Error, "DIALOGUE",
                                      "Failed to parse dialogue definition: " + file,
                                      "Check required fields: id, npc, entries");
                }
            });
        }

        // Story / events
        if (manifest.features.story) {
            loadDir("events", [&](const nlohmann::json& j, const std::string& file) {
                auto def = EventDef::fromJson(j);
                if (def) {
                    registries_.story().registerEvent(*def);
                    log_.info("BOOT", "Registered event: " + def->id);
                } else {
                    log_.error("BOOT", "Failed to parse event: " + file);
                    diagnostics_.emit("STORY-E001", Severity::Error, "STORY",
                                      "Failed to parse event definition: " + file,
                                      "Check required fields: id, mode, trigger");
                }
            });
        }
    }

    emitBootStatus("load_content", "pass");
}

void EflBootstrap::emitBootStatus(const std::string& step, const std::string& status,
                                   const std::string& detail) {
    if (!pipe_)
        return;

    nlohmann::json payload;
    payload["step"] = step;
    payload["status"] = status;
    if (!detail.empty())
        payload["detail"] = detail;

    pipe_->write("boot.status", payload);
}

} // namespace efl
