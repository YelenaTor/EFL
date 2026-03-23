#include "efl/core/bootstrap.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef EFL_STUB_SDK
#include "efl/bridge/hooks.h"
#include "efl/bridge/room_tracker.h"
#include "efl/bridge/routine_invoker.h"
#include "efl/bridge/instance_walker.h"
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

    // Wire pipe to services for IPC message emission
    diagnostics_.setPipeWriter(pipe_.get());
    events_.setPipeWriter(pipe_.get());
    saves_.setPipeWriter(pipe_.get());

    log_.info("BOOT", "EFL v" + eflVersionString() + " initializing");

    // Phase: boot
    if (pipe_) {
        pipe_->write("phase.transition", nlohmann::json{
            {"phase", "boot"}, {"status", "started"}});
    }

    if (!stepVersionCheck())
        return false;

    if (!stepDiscoverManifests(contentDir))
        return false;

    if (!stepValidateManifests())
        return false;

    // Phase: diagnostics
    if (pipe_) {
        pipe_->write("phase.transition", nlohmann::json{
            {"phase", "diagnostics"}, {"status", "started"}});
    }

    stepLoadContent(contentDir);

    // Emit mod.status for each loaded manifest
    for (const auto& m : manifests_) {
        if (pipe_) {
            pipe_->write("mod.status", nlohmann::json{
                {"modId", m.modId},
                {"name", m.name},
                {"version", m.version},
                {"status", "loaded"}
            });
        }
    }

    // Phase: boot complete
    if (pipe_) {
        pipe_->write("phase.transition", nlohmann::json{
            {"phase", "boot"}, {"status", "complete"}});
    }

    emitBootStatus("init", "complete",
                   std::to_string(manifests_.size()) + " manifest(s) loaded");
    log_.info("BOOT", "Bootstrap complete — " +
              std::to_string(manifests_.size()) + " manifest(s) loaded");
    return true;
}

#ifndef EFL_STUB_SDK
bool EflBootstrap::initialize(const std::string& contentDir,
                               Aurie::AurieModule* module, YYTK::YYTKInterface* yytk) {
    // Run base initialization (pipe, discovery, validation, content loading)
    if (!initialize(contentDir))
        return false;

    emitBootStatus("bridge", "running");

    // Create bridge objects
    hooks_ = std::make_unique<bridge::HookRegistry>(module, yytk);
    hooks_->setPipeWriter(pipe_.get());
    roomTracker_ = std::make_unique<bridge::RoomTracker>(yytk);
    routineInvoker_ = std::make_unique<bridge::RoutineInvoker>(yytk);
    instanceWalker_ = std::make_unique<bridge::InstanceWalker>(yytk);

    // Register v1 hooks
    stepRegisterHooks();

    // Connect registries to bridge
    stepConnectAreaRegistry();
    stepConnectWarpService();

    emitBootStatus("bridge", "pass");
    log_.info("BOOT", "Bridge layer initialized");
    return true;
}

void EflBootstrap::stepRegisterHooks() {
    // Room transition detection (MUST)
    if (hooks_->registerScriptHook("room_transition",
            "gml_Object_obj_roomtransition_Create_0",
            [this](YYTK::CInstance* self, YYTK::CInstance*, YYTK::CCode* code,
                   int, YYTK::RValue*) {
                log_.info("HOOK", "Room transition triggered");
                // Try to read the target room from the transition instance for immediate detection.
                // Falls back to frame-based polling via update() if this fails.
                try {
                    YYTK::RValue targetVar = instanceWalker_->getVariable(self, "target_room");
                    YYTK::RValue nameVal = routineInvoker_->callBuiltin("room_get_name", {targetVar});
                    roomTracker_->onRoomTransition(nameVal.ToString());
                } catch (...) {
                    // Frame callback update() will catch the room change
                }
            })) {
        log_.info("HOOK", "Registered: room_transition");
        emitBootStatus("hook.registered", "pass", "room_transition");
    } else {
        log_.warn("HOOK", "Failed to register room_transition hook");
        diagnostics_.emit("HOOK-W003", Severity::Warning, "HOOK",
                          "Failed to register room_transition hook",
                          "Room transitions may not be detected");
    }

    // Grid initialization (MUST)
    if (hooks_->registerScriptHook("grid_init",
            "gml_Script_initialize_on_room_start@Grid@Grid",
            [this](YYTK::CInstance*, YYTK::CInstance*, YYTK::CCode*,
                   int, YYTK::RValue*) {
                log_.info("HOOK", "Grid initialization triggered");
            })) {
        log_.info("HOOK", "Registered: grid_init");
        emitBootStatus("hook.registered", "pass", "grid_init");
    } else {
        log_.warn("HOOK", "Failed to register grid_init hook");
        diagnostics_.emit("HOOK-W003", Severity::Warning, "HOOK",
                          "Failed to register grid_init hook",
                          "Room grid setup may not work");
    }

    // Frame callback for room tracking + trigger evaluation (MUST)
    if (hooks_->registerFrameCallback("frame_update", [this]() {
                roomTracker_->update();
            })) {
        log_.info("HOOK", "Registered: frame_update");
        emitBootStatus("hook.registered", "pass", "frame_update");
    } else {
        log_.warn("HOOK", "Failed to register frame_update callback");
        diagnostics_.emit("HOOK-W003", Severity::Warning, "HOOK",
                          "Failed to register frame_update callback",
                          "Room tracking will not function");
    }

    // Resource node hooks (SHOULD — graceful degradation)
    for (const auto& hookName : {"pick_node", "hoe_node", "water_node"}) {
        std::string target = std::string("gml_Script_") + hookName;
        if (hooks_->registerScriptHook(hookName, target,
                [this, hookName](YYTK::CInstance*, YYTK::CInstance*, YYTK::CCode*,
                       int, YYTK::RValue*) {
                    log_.info("HOOK", std::string("Resource node interaction: ") + hookName);
                })) {
            log_.info("HOOK", std::string("Registered: ") + hookName);
            emitBootStatus("hook.registered", "pass", hookName);
        } else {
            diagnostics_.emit("HOOK-W003", Severity::Warning, "HOOK",
                              std::string("Failed to register ") + hookName + " hook",
                              "Resource interactions may not be intercepted");
        }
    }
}

void EflBootstrap::stepConnectAreaRegistry() {
    // When room changes, check if the new room is hijacked by an EFL area.
    roomTracker_->onRoomChange([this](const std::string& oldRoom, const std::string& newRoom) {
        log_.info("ROOM", "Room changed: " + oldRoom + " -> " + newRoom);

        // Check if any registered area hijacks this room
        auto areas = registries_.areas().areasByHostRoom(newRoom);
        for (const auto* area : areas) {
            log_.info("AREA", "Activating EFL area '" + area->id +
                      "' in hijacked room: " + newRoom);

            // 1. Destroy default instances that conflict with the hijacked area
            try {
                auto defaultInstances = instanceWalker_->getAll("obj_interactable");
                for (auto* inst : defaultInstances) {
                    routineInvoker_->callBuiltin("instance_destroy", {YYTK::RValue(inst)});
                }
                log_.info("AREA", "Cleared " + std::to_string(defaultInstances.size()) +
                          " default instances in " + newRoom);
            } catch (const std::exception& ex) {
                log_.warn("AREA", "Failed to clear default instances: " + std::string(ex.what()));
            }

            // 2. Set music if specified
            if (!area->music.empty()) {
                try {
                    YYTK::RValue musicAsset = routineInvoker_->callBuiltin("asset_get_index",
                        {YYTK::RValue(area->music.c_str())});
                    routineInvoker_->callBuiltin("audio_play_music", {musicAsset});
                    log_.info("AREA", "Set music: " + area->music);
                } catch (const std::exception& ex) {
                    log_.warn("AREA", "Failed to set music '" + area->music + "': " + std::string(ex.what()));
                }
            }

            // 3. Publish area.activated event
            events_.publish("area.activated", nlohmann::json{
                {"areaId", area->id}, {"hostRoom", newRoom}});

            // 4. Emit IPC for TUI
            if (pipe_) {
                pipe_->write("area.activated", nlohmann::json{
                    {"areaId", area->id}, {"hostRoom", newRoom}});
            }

            // 5. Spawn NPCs assigned to this area (Step 3)
            auto npcs = registries_.npcs().npcsInArea(area->id);
            for (const auto* npc : npcs) {
                // Check unlock trigger — skip NPCs that aren't visible yet
                if (!npc->unlockTrigger.empty() &&
                    !registries_.triggers().evaluate(npc->unlockTrigger)) {
                    log_.info("NPC", "NPC '" + npc->id + "' locked (trigger: " +
                              npc->unlockTrigger + ")");
                    continue;
                }

                // Parse spawn anchor "x,y" format
                try {
                    auto commaPos = npc->spawnAnchor.find(',');
                    if (commaPos == std::string::npos) {
                        log_.warn("NPC", "Invalid spawnAnchor for NPC '" + npc->id +
                                  "': " + npc->spawnAnchor);
                        continue;
                    }
                    double x = std::stod(npc->spawnAnchor.substr(0, commaPos));
                    double y = std::stod(npc->spawnAnchor.substr(commaPos + 1));

                    YYTK::RValue npcObj = routineInvoker_->callBuiltin("asset_get_index",
                        {YYTK::RValue("par_NPC")});
                    YYTK::RValue layerName(YYTK::RValue("Instances"));
                    routineInvoker_->callBuiltin("instance_create_layer",
                        {YYTK::RValue(x), YYTK::RValue(y), layerName, npcObj});

                    log_.info("NPC", "Spawned NPC '" + npc->id + "' at (" +
                              std::to_string(x) + ", " + std::to_string(y) + ")");

                    if (pipe_) {
                        pipe_->write("npc.spawned", nlohmann::json{
                            {"npcId", npc->id}, {"areaId", area->id},
                            {"x", x}, {"y", y}});
                    }
                } catch (const std::exception& ex) {
                    log_.warn("NPC", "Failed to spawn NPC '" + npc->id + "': " +
                              std::string(ex.what()));
                }
            }
        }
    });
}

void EflBootstrap::stepConnectWarpService() {
    // On room enter, log available warps and their trigger status.
    // Warp gating: the game's native obj_roomtransition handles the actual room_goto.
    // EFL's role is to check trigger requirements and block transitions that aren't unlocked.
    roomTracker_->onRoomChange([this](const std::string& oldRoom, const std::string& newRoom) {
        auto warps = registries_.warps().warpsFrom(newRoom);
        for (const auto* warp : warps) {
            bool allowed = registries_.warps().canWarp(warp->id, registries_.triggers());
            if (allowed) {
                log_.info("WARP", "Warp available: " + warp->id +
                          " (" + warp->sourceArea + " -> " + warp->targetArea + ")");
            } else {
                log_.info("WARP", "Warp locked: " + warp->id +
                          " (requires trigger: " + warp->requireTrigger + ")");
            }

            if (pipe_) {
                pipe_->write("warp.status", nlohmann::json{
                    {"warpId", warp->id},
                    {"from", warp->sourceArea},
                    {"to", warp->targetArea},
                    {"available", allowed}});
            }
        }
    });

    // Gate room transitions via the room_transition hook.
    // When obj_roomtransition fires, check if the target room has warps that require triggers.
    // If a warp's trigger isn't met, suppress the transition.
    hooks_->registerScriptHook("warp_gate",
        "gml_Object_obj_roomtransition_Create_0",
        [this](YYTK::CInstance* self, YYTK::CInstance*, YYTK::CCode*,
               int, YYTK::RValue*) {
            // v1: observation only — logs locked warps but cannot suppress transitions.
            // Suppression requires wiring CodeEventCallback returns through YYTK Event.Call().
            try {
                YYTK::RValue targetVar = instanceWalker_->getVariable(self, "target_room");
                YYTK::RValue nameVal = routineInvoker_->callBuiltin("room_get_name", {targetVar});
                std::string targetRoom = nameVal.ToString();

                auto warpsTo = registries_.warps().warpsTo(targetRoom);
                for (const auto* warp : warpsTo) {
                    if (!registries_.warps().canWarp(warp->id, registries_.triggers())) {
                        log_.warn("WARP", "Transition to " + targetRoom +
                                  " blocked by trigger '" + warp->requireTrigger +
                                  "' on warp '" + warp->id + "' (suppression not yet wired)");
                    }
                }
            } catch (...) {
                // If we can't read the target, allow the transition (fail-open)
            }
        });
}

#endif // EFL_STUB_SDK

void EflBootstrap::shutdown() {
#ifndef EFL_STUB_SDK
    hooks_.reset();
    roomTracker_.reset();
    routineInvoker_.reset();
    instanceWalker_.reset();
#endif
    if (pipe_) {
        pipe_->close();
    }
    log_.info("BOOT", "EFL shutdown");
}

LogService& EflBootstrap::log() { return log_; }
DiagnosticEmitter& EflBootstrap::diagnostics() { return diagnostics_; }
PipeWriter& EflBootstrap::pipe() { return *pipe_; }
RegistryService& EflBootstrap::registries() { return registries_; }
EventBus& EflBootstrap::events() { return events_; }
SaveService& EflBootstrap::saves() { return saves_; }
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
