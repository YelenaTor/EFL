#include "efl/core/bootstrap.h"
#include "efl/core/efpack_loader.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef EFL_STUB_SDK
#include "efl/bridge/hooks.h"
#include "efl/bridge/room_tracker.h"
#include "efl/bridge/routine_invoker.h"
#include "efl/bridge/instance_walker.h"
#include "efl/areas/HijackedRoomBackend.h"
#include "efl/areas/NativeRoomBackend.h"
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
    // Wire file logging: <gameDir>/EFL/logs/efl.log
    // contentDir is <game>/mods/efl, so gameDir is two levels up
    {
        namespace fs = std::filesystem;
        fs::path logDir = fs::path(contentDir).parent_path().parent_path() / "EFL" / "logs";
        std::error_code ec;
        fs::create_directories(logDir, ec);
        std::string logPath = (logDir / "efl.log").string();
        log_.setFileOutput(logPath);
        if (!log_.isFileOutputOpen()) {
            diagnostics_.emit("BOOT-W001", Severity::Warning, "BOOT",
                              "Failed to open log file: " + logPath,
                              "File logging disabled for this session");
        }
    }

    // Create the named pipe for TUI communication
    pipe_ = std::make_unique<PipeWriter>(buildPipeName());
    pipe_->create(); // Best-effort; bootstrap continues even if pipe fails

    // Wire pipe to services for IPC message emission
    diagnostics_.setPipeWriter(pipe_.get());
    events_.setPipeWriter(pipe_.get());
    saves_.setPipeWriter(pipe_.get());
    registries_.story().setPipeWriter(pipe_.get());
    registries_.worldNpcs().setSaveService(&saves_);

    contentDir_ = contentDir;
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

    // Load per-mod config from <contentDir>/config.json (silent no-op if missing)
    config_.loadFromFile((std::filesystem::path(contentDir) / "config.json").string());

    // Validate script hooks declared in all manifests (emits W002/W003 in both SDK modes)
    stepValidateScriptHooks();

    emitBootStatus("init", "complete",
                   std::to_string(manifests_.size()) + " manifest(s) loaded");
    log_.info("BOOT", "Bootstrap complete — " +
              std::to_string(manifests_.size()) + " manifest(s) loaded");

#ifndef EFL_STUB_SDK
    {
        bool watchStarted = hotReload_.start(std::filesystem::path(contentDir_),
            [this](const ReloadEvent& ev) {
                reloadContentType(ev.contentType, ev.filePath);
                if (pipe_) {
                    pipe_->write("reload.event", nlohmann::json{
                        {"contentType", ev.contentType},
                        {"path", ev.filePath.string()},
                        {"status", "ok"}
                    });
                }
            });
        if (!watchStarted) {
            diagnostics_.emit("RELOAD-W001", Severity::Warning, "BOOT",
                              "Hot-reload watcher failed to start for: " + contentDir_,
                              "Content changes will not be detected at runtime");
        }
    }
#endif

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

    // Select area backend based on manifest settings (first manifest wins, default "hijacked")
    {
        std::string backendType = "hijacked";
        if (!manifests_.empty()) {
            backendType = manifests_.front().settings.areaBackend;
        }

        if (backendType == "native") {
            roomBackend_ = std::make_unique<NativeRoomBackend>(
                *instanceWalker_, *routineInvoker_, pipe_.get(),
                log_, events_,
                registries_.npcs(), registries_.story(),
                registries_.triggers(), diagnostics_);
            log_.info("AREA", "Area backend: NativeRoomBackend");
        } else {
            roomBackend_ = std::make_unique<HijackedRoomBackend>(
                *instanceWalker_, *routineInvoker_, pipe_.get(),
                log_, events_,
                registries_.npcs(), registries_.story(),
                registries_.triggers(), diagnostics_);
            log_.info("AREA", "Area backend: HijackedRoomBackend");
        }
    }

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

    // Frame callback for room tracking + trigger evaluation + hot-reload drain (MUST)
    if (hooks_->registerFrameCallback("frame_update", [this]() {
                roomTracker_->update();
                hotReload_.drainQueue();
                // TODO: pass real FoM time when time hook is wired (Phase 6+)
                registries_.worldNpcs().tickSchedule(0);
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

    // Manifest-declared script hook callbacks (v2.2 — callback mode only)
    static const std::unordered_set<std::string> kBuiltinHandlers = {
        "efl_resource_despawn",
    };
    for (const auto& manifest : manifests_) {
        for (const auto& sh : manifest.scriptHooks) {
            if (sh.mode == "inject" ||
                kBuiltinHandlers.find(sh.handler) == kBuiltinHandlers.end())
                continue; // already diagnosed in stepValidateScriptHooks

            std::string hookName = sh.handler + ":" + sh.target;
            hooks_->registerScriptHook(hookName, sh.target,
                [this, sh](YYTK::CInstance*, YYTK::CInstance*, YYTK::CCode*,
                            int, YYTK::RValue*) {
                    log_.info("HOOK", "Script hook fired: " + sh.handler + " @ " + sh.target);
                    // v2.4: real resource spawn/despawn logic wired here
                    diagnostics_.emit("RESOURCE-H001", Severity::Hazard, "RESOURCE",
                                     "efl_resource_despawn stub — not yet implemented",
                                     "Resource spawning is wired in v2.4");
                });
            log_.info("HOOK", "Registered manifest hook: " + sh.target + " -> " + sh.handler);
        }
    }

    // Crafting station hook — inject EFL recipes when any crafting menu opens (v2.4)
    // gml_Script_spawn_crafting_menu confirmed in data.win.
    // gml_Script_unlock_recipe@Ari@Ari confirmed in data.win.
    // Station-specific filtering (recipesAtStation) deferred until recipe_context enum
    // strings are confirmed via Ghidra — injects all trigger-unlocked EFL recipes for now.
    bool hasCrafting = std::any_of(manifests_.begin(), manifests_.end(),
                                   [](const auto& m) { return m.features.crafting; });
    if (hasCrafting) {
        if (hooks_->registerScriptHook(
                "efl_crafting_inject", "gml_Script_spawn_crafting_menu",
                [this](YYTK::CInstance*, YYTK::CInstance*, YYTK::CCode*, int, YYTK::RValue*) {
                    auto recipes = registries_.crafting().availableRecipes(registries_.triggers());
                    int injected = 0;
                    for (const auto* recipe : recipes) {
                        try {
                            routineInvoker_->callGameScript(
                                "gml_Script_unlock_recipe@Ari@Ari",
                                {YYTK::RValue(recipe->id.c_str())});
                            ++injected;
                        } catch (...) {
                            log_.warn("CRAFT", "Failed to inject recipe: " + recipe->id);
                        }
                    }
                    log_.info("CRAFT", "Injected " + std::to_string(injected) +
                              " EFL recipe(s) into crafting menu");
                })) {
            log_.info("HOOK", "Registered: efl_crafting_inject (spawn_crafting_menu)");
            emitBootStatus("hook.registered", "pass", "efl_crafting_inject");
        } else {
            diagnostics_.emit("CRAFT-W001", Severity::Warning, "CRAFT",
                              "Failed to register crafting menu hook",
                              "EFL recipes will not be injected into crafting stations");
        }
    }

    // Dungeon vote injection — add EFL nodes to FoM biome vote pools (v2.4+)
    // gml_Script_create_node_prototypes fires once per room on grid init.
    // RUNTIME_VERIFY: inspect prototype struct after creation to find the vote
    // table mutation API. Candidate: gml_Script_register_node@Anchor@Anchor.
    if (!registries_.resources().resourcesWithDungeonVotes().empty()) {
        if (hooks_->registerScriptHook(
                "efl_dungeon_vote_inject", "gml_Script_create_node_prototypes",
                [this](YYTK::CInstance*, YYTK::CInstance*, YYTK::CCode*,
                       int, YYTK::RValue*) {
                    auto candidates = registries_.resources().resourcesWithDungeonVotes();
                    for (const auto* res : candidates) {
                        for (const auto& vote : res->spawnRules.dungeonVotes) {
#ifndef EFL_STUB_SDK
                            // TODO: call runtime vote-table mutation once API confirmed
                            // e.g. routineInvoker_->callGameScript(
                            //     "gml_Script_register_node@Anchor@Anchor",
                            //     {res->kind, vote.biome, vote.pool, vote.weight});
                            diagnostics_.emit("RESOURCE-H002", Severity::Hazard, "RESOURCE",
                                "Dungeon vote stub: " + res->id + " not injected into "
                                + vote.biome + "/" + vote.pool,
                                "Hook runtime vote API once confirmed");
#else
                            log_.info("RESOURCE", "Stub: dungeon vote for " + res->id
                                      + " biome=" + vote.biome
                                      + " pool=" + vote.pool
                                      + " weight=" + std::to_string(vote.weight));
#endif
                        }
                    }
                })) {
            log_.info("HOOK", "Registered: efl_dungeon_vote_inject (create_node_prototypes)");
            emitBootStatus("hook.registered", "pass", "efl_dungeon_vote_inject");
        } else {
            diagnostics_.emit("RESOURCE-W001", Severity::Warning, "RESOURCE",
                "Failed to register dungeon vote hook",
                "EFL resource nodes will not appear in FoM dungeon floors");
        }
    }
}

void EflBootstrap::stepConnectAreaRegistry() {
    // When room changes, deactivate old EFL areas and activate new ones.
    roomTracker_->onRoomChange([this](const std::string& oldRoom, const std::string& newRoom) {
        log_.info("ROOM", "Room changed: " + oldRoom + " -> " + newRoom);

        // Deactivate areas in the old room and fire their exit events.
        auto oldAreas = registries_.areas().areasByHostRoom(oldRoom);
        for (const auto* area : oldAreas) {
            if (roomBackend_)
                roomBackend_->deactivate();
            if (!area->exitEvent.empty())
                registries_.story().fireEvent(area->exitEvent, registries_.triggers());
        }

        // Activate areas in the new room and fire their entry events.
        auto newAreas = registries_.areas().areasByHostRoom(newRoom);
        for (const auto* area : newAreas) {
            if (roomBackend_)
                roomBackend_->activate(*area);
            if (!area->entryEvent.empty())
                registries_.story().fireEvent(area->entryEvent, registries_.triggers());

            // Spawn resource nodes for this area.
            // Script: gml_Script_attempt_to_write_object_node (safe wrapper, SCPT 3251).
            // Args: (kind: string, grid_x: int, grid_y: int) — best-guess from static analysis.
            // RUNTIME_VERIFY: hook write_rock_to_location to confirm argc/arg types if spawn fails.
            auto resources = registries_.resources().resourcesInArea(area->id);
            for (const auto* res : resources) {
                auto it = res->spawnRules.anchors.find(area->id);
                if (it == res->spawnRules.anchors.end()) {
                    log_.warn("RESOURCE", "No anchor for " + res->id + " in area " + area->id
                              + " — skipping spawn (add spawnRules.anchors in resource JSON)");
                    continue;
                }
                const auto [gx, gy] = it->second;
#ifndef EFL_STUB_SDK
                try {
                    routineInvoker_->callGameScript(
                        "gml_Script_attempt_to_write_object_node",
                        {YYTK::RValue(res->kind.c_str()),
                         YYTK::RValue(static_cast<double>(gx)),
                         YYTK::RValue(static_cast<double>(gy))});
                    log_.info("RESOURCE", "Spawned " + res->id + " at (" +
                              std::to_string(gx) + "," + std::to_string(gy) + ")");
                } catch (const std::exception& ex) {
                    log_.warn("RESOURCE", "Spawn failed for " + res->id + ": " + ex.what());
                }
#else
                log_.info("RESOURCE", "Stub: would spawn " + res->id + " at (" +
                          std::to_string(gx) + "," + std::to_string(gy) + ") in " + area->id);
#endif
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
    // If a warp's trigger isn't met, destroy the transition instance to suppress the transition.
    hooks_->registerScriptHook("warp_gate",
        "gml_Object_obj_roomtransition_Create_0",
        [this](YYTK::CInstance* self, YYTK::CInstance*, YYTK::CCode*,
               int, YYTK::RValue*) {
            try {
                YYTK::RValue targetVar = instanceWalker_->getVariable(self, "target_room");
                YYTK::RValue nameVal = routineInvoker_->callBuiltin("room_get_name", {targetVar});
                std::string targetRoom = nameVal.ToString();

                auto warpsTo = registries_.warps().warpsTo(targetRoom);
                for (const auto* warp : warpsTo) {
                    if (!registries_.warps().canWarp(warp->id, registries_.triggers())) {
                        instanceWalker_->destroyInstance(self);
                        diagnostics_.emit("WARP-W001", Severity::Warning, "WARP",
                                          "Warp '" + warp->id + "' suppressed — trigger condition not met",
                                          "Fulfill trigger '" + warp->requireTrigger + "' to unlock");
                        if (pipe_) {
                            pipe_->write("warp.suppressed", nlohmann::json{{"warpId", warp->id}});
                        }
                        log_.warn("WARP", "Warp '" + warp->id + "' suppressed — trigger '" +
                                  warp->requireTrigger + "' not met");
                    }
                }
            } catch (...) {
                // If we can't read the target, allow the transition (fail-open)
            }
        });
}

#endif // EFL_STUB_SDK

void EflBootstrap::shutdown() {
    hotReload_.stop();
#ifndef EFL_STUB_SDK
    roomBackend_.reset();
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

    if (!fs::exists(contentDir)) {
        std::error_code ec;
        fs::create_directories(contentDir, ec);
        if (ec) {
            log_.warn("BOOT", "Content directory missing and could not be created: " + contentDir);
            diagnostics_.emit("BOOT-W001", Severity::Warning, "BOOT",
                              "Content directory could not be created: " + contentDir,
                              ec.message());
            emitBootStatus("discover", "warn", "content directory missing");
            return true;
        }
        log_.info("BOOT", "Created content directory: " + contentDir);
    }

    if (!fs::is_directory(contentDir)) {
        log_.warn("BOOT", "Content path is not a directory: " + contentDir);
        emitBootStatus("discover", "warn", "content path is not a directory");
        return true;
    }

    int found = 0;
    for (const auto& entry : fs::directory_iterator(contentDir)) {
        if (entry.is_directory()) {
            // Unpacked content pack: subdir containing manifest.efl
            fs::path manifestPath = entry.path() / "manifest.efl";
            if (!fs::exists(manifestPath))
                continue;

            auto manifest = ManifestParser::parseFile(manifestPath.string());
            if (manifest) {
                manifest->packDir = entry.path().string();
                log_.info("BOOT", "Loaded manifest: " + manifest->modId +
                          " v" + manifest->version);
                manifests_.push_back(std::move(*manifest));
                ++found;
            } else {
                log_.error("BOOT", "Failed to parse manifest: " + manifestPath.string());
                diagnostics_.emit("MANIFEST-E001", Severity::Error, "MANIFEST",
                                  "Failed to parse manifest: " + manifestPath.filename().string(),
                                  "Check JSON syntax and required fields");
            }
        } else if (entry.is_regular_file() &&
                   entry.path().extension() == ".efpack") {
            // Packed content: ZIP archive — extract to loaded_efpack/<modId>/ then mark done.
            fs::path loadedRoot = fs::path(contentDir) / "loaded_efpack";
            auto extractedDir = EfpackLoader::unpackToLoadedDir(entry.path(), loadedRoot);
            if (!extractedDir) {
                log_.error("BOOT", "Failed to load pack: " + entry.path().filename().string());
                diagnostics_.emit("PACK-E001", Severity::Error, "PACK",
                                  "Failed to load pack: " + entry.path().filename().string(),
                                  "Ensure the file is a valid efpack archive with a manifest.efl");
                continue;
            }

            fs::path packedManifestPath = *extractedDir / "manifest.efl";
            if (!fs::exists(packedManifestPath))
                continue;

            auto manifest = ManifestParser::parseFile(packedManifestPath.string());
            if (manifest) {
                manifest->packDir = extractedDir->string();
                log_.info("BOOT", "Loaded packed manifest: " + manifest->modId +
                          " v" + manifest->version);
                manifests_.push_back(std::move(*manifest));
                ++found;
                // Rename .efpack → .efpack.loaded so it is skipped on next boot.
                if (!EfpackLoader::markAsLoaded(entry.path())) {
                    log_.warn("BOOT", "Could not mark pack as loaded: " +
                              entry.path().filename().string());
                }
            } else {
                log_.error("BOOT", "Failed to parse manifest in pack: " +
                           entry.path().filename().string());
                diagnostics_.emit("PACK-E002", Severity::Error, "PACK",
                                  "Failed to parse manifest in pack: " +
                                  entry.path().filename().string(),
                                  "Check JSON syntax and required fields in manifest.efl");
            }
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

        for (const auto& dep : m.requiredDeps) {
            if (!CompatibilityService::isExternalModLoaded(dep.modId)) {
                log_.error("BOOT", "Mod '" + m.modId + "' requires '" + dep.modId +
                           "' which is not loaded");
                diagnostics_.emit("MANIFEST-E003", Severity::Error, "MANIFEST",
                                  "Required dependency '" + dep.modId + "' not loaded for mod '" + m.modId + "'",
                                  "Install '" + dep.modId + "' via MOMI before loading this pack");
                allOk = false;
            }
        }

        for (const auto& dep : m.optionalDeps) {
            if (!CompatibilityService::isExternalModLoaded(dep.modId)) {
                log_.warn("BOOT", "Optional dependency '" + dep.modId +
                          "' not loaded for mod '" + m.modId + "' — related features disabled");
                diagnostics_.emit("MANIFEST-W001", Severity::Warning, "MANIFEST",
                                  "Optional dependency '" + dep.modId + "' not loaded for mod '" + m.modId + "'",
                                  "Install '" + dep.modId + "' to enable related features");
            }
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

    for (const auto& manifest : manifests_) {
        fs::path packDir = fs::path(manifest.packDir);

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

            loadDir("world_npcs", [&](const nlohmann::json& j, const std::string& file) {
                auto def = WorldNpcDef::fromJson(j);
                if (def) {
                    log_.info("BOOT", "Registered WorldNpc: " + def->id);
                    registries_.worldNpcs().registerWorldNpc(std::move(*def));
                } else {
                    log_.error("BOOT", "Failed to parse WorldNpc: " + file);
                    diagnostics_.emit("NPC-E001", Severity::Error, "NPC",
                                      "Failed to parse WorldNpc definition: " + file,
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

void EflBootstrap::reloadContentType(const std::string& contentType,
                                      const std::filesystem::path& filePath) {
    if (filePath.extension() != ".json")
        return;

    std::ifstream reloadFile(filePath);
    if (!reloadFile.is_open()) {
        log_.warn("RELOAD", "Cannot open changed file: " + filePath.string());
        return;
    }

    nlohmann::json j;
    try {
        reloadFile >> j;
    } catch (const std::exception& ex) {
        log_.error("RELOAD", "JSON parse error in " + filePath.string() + ": " + ex.what());
        diagnostics_.emit("RELOAD-W001", Severity::Warning, "BOOT",
                          "Hot-reload parse error: " + filePath.filename().string(),
                          ex.what());
        return;
    }

    const std::string fname = filePath.filename().string();

    try {
        if (contentType == "triggers") {
            registries_.triggers().registerFromJson(j);
            log_.info("RELOAD", "Reloaded trigger from " + fname);
        } else if (contentType == "areas") {
            auto def = AreaDef::fromJson(j);
            if (def) {
                registries_.areas().registerArea(*def);
                log_.info("RELOAD", "Reloaded area: " + def->id);
            } else {
                log_.warn("RELOAD", "Failed to parse area: " + fname);
            }
        } else if (contentType == "warps") {
            auto def = WarpDef::fromJson(j);
            if (def) {
                registries_.warps().registerWarp(*def);
                log_.info("RELOAD", "Reloaded warp: " + def->id);
            } else {
                log_.warn("RELOAD", "Failed to parse warp: " + fname);
            }
        } else if (contentType == "resources") {
            auto def = ResourceDef::fromJson(j);
            if (def) {
                registries_.resources().registerResource(*def);
                log_.info("RELOAD", "Reloaded resource: " + def->id);
            } else {
                log_.warn("RELOAD", "Failed to parse resource: " + fname);
            }
        } else if (contentType == "quests") {
            auto def = QuestDef::fromJson(j);
            if (def) {
                registries_.quests().registerQuest(*def);
                log_.info("RELOAD", "Reloaded quest: " + def->id);
            } else {
                log_.warn("RELOAD", "Failed to parse quest: " + fname);
            }
        } else if (contentType == "npcs") {
            auto def = NpcDef::fromJson(j);
            if (def) {
                registries_.npcs().registerNpc(*def);
                log_.info("RELOAD", "Reloaded NPC: " + def->id);
            } else {
                log_.warn("RELOAD", "Failed to parse NPC: " + fname);
            }
        } else if (contentType == "recipes") {
            auto def = RecipeDef::fromJson(j);
            if (def) {
                registries_.crafting().registerRecipe(*def);
                log_.info("RELOAD", "Reloaded recipe: " + def->id);
            } else {
                log_.warn("RELOAD", "Failed to parse recipe: " + fname);
            }
        } else if (contentType == "dialogue") {
            auto def = DialogueDef::fromJson(j);
            if (def) {
                registries_.dialogue().registerDialogue(*def);
                log_.info("RELOAD", "Reloaded dialogue: " + def->id);
            } else {
                log_.warn("RELOAD", "Failed to parse dialogue: " + fname);
            }
        } else if (contentType == "events") {
            auto def = EventDef::fromJson(j);
            if (def) {
                registries_.story().registerEvent(*def);
                log_.info("RELOAD", "Reloaded event: " + def->id);
            } else {
                log_.warn("RELOAD", "Failed to parse event: " + fname);
            }
        } else if (contentType == "world_npcs") {
            auto def = WorldNpcDef::fromJson(j);
            if (def) {
                registries_.worldNpcs().registerWorldNpc(*def);
                log_.info("RELOAD", "Reloaded world NPC: " + def->id);
            } else {
                log_.warn("RELOAD", "Failed to parse world NPC: " + fname);
            }
        } else {
            log_.info("RELOAD", "Unknown content type '" + contentType + "' ignored");
        }
    } catch (const std::exception& ex) {
        log_.error("RELOAD", "Error reloading " + contentType + "/" + fname + ": " + ex.what());
        diagnostics_.emit("RELOAD-W001", Severity::Warning, "BOOT",
                          "Hot-reload failed for: " + fname, ex.what());
    }
}

void EflBootstrap::stepValidateScriptHooks() {
    static const std::unordered_set<std::string> kBuiltinHandlers = {
        "efl_resource_despawn",
    };

    for (const auto& manifest : manifests_) {
        for (const auto& sh : manifest.scriptHooks) {
            if (sh.mode == "inject") {
                diagnostics_.emit("HOOK-W002", Severity::Warning, "HOOK",
                                  "GML injection not yet available: hook skipped (" + sh.target + ")",
                                  "Remove mode:\"inject\" or wait for EFL GML injection support");
                log_.warn("HOOK", "Script hook skipped (inject mode not yet supported): " + sh.target);
            } else if (kBuiltinHandlers.find(sh.handler) == kBuiltinHandlers.end()) {
                diagnostics_.emit("HOOK-W004", Severity::Warning, "HOOK",
                                  "Unknown script hook handler: " + sh.handler,
                                  "Check handler name against EFL built-in handlers list");
                log_.warn("HOOK", "Unknown manifest hook handler: " + sh.handler);
            }
        }
    }
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
