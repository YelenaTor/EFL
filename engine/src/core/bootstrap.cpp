#include "efl/core/bootstrap.h"
#include "efl/core/efpack_loader.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
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

std::string buildCommandPipeName() {
#ifdef _WIN32
    DWORD pid = GetCurrentProcessId();
#else
    int pid = 0;
#endif
    return "\\\\.\\pipe\\efl-" + std::to_string(pid) + "-cmd";
}

// What this engine build accepts as scriptHook handler ids. Kept in sync with
// `kBuiltinHandlers` inside `stepValidateScriptHooks`. Surfaced via
// `capabilities.snapshot` so the DevKit validator can lint against the live
// runtime instead of its own embedded whitelist.
const std::vector<std::string>& builtinScriptHookHandlers() {
    static const std::vector<std::string> kHandlers = {
        "efl_resource_despawn",
    };
    return kHandlers;
}

// Feature tags this engine build understands at runtime. Mirrors the manifest
// `features` enum; flags below clarify which features are wired vs. stubbed.
const std::vector<std::string>& supportedFeatureTags() {
    static const std::vector<std::string> kFeatures = {
        "areas", "warps", "npcs", "resources", "crafting", "quests",
        "dialogue", "story", "triggers", "assets", "ipc", "calendar",
    };
    return kFeatures;
}

// Hook kinds the bridge can register today. Used by the DevKit to gate
// scriptHook UI / diagnostics against the engine's actual reach.
const std::vector<std::string>& supportedHookKinds() {
    static const std::vector<std::string> kKinds = {
        "yyc_script", "frame", "detour",
    };
    return kKinds;
}

// Sparse capability flags. `false` means "feature accepted by manifest /
// schema but not yet wired through the bridge". The DevKit shows these
// alongside the validator output so authors know what's runtime-truth vs.
// future-work.
nlohmann::json buildCapabilityFlags() {
    return nlohmann::json{
        {"dungeonVoteInjection", false},
        {"worldNpcSchedules", false},
        {"nativeRoomBackend", false},
        {"assetInjection", false},
        {"scriptHookCallbacks", true},
        {"scriptHookInject", false},
        {"calendarRegistry", true},
    };
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

    // Create the named pipe for DevKit communication
    pipe_ = std::make_unique<PipeWriter>(buildPipeName());
    pipe_->create(); // Best-effort; bootstrap continues even if pipe fails

    // Inbound command pipe (DevKit -> engine). Best-effort: if it fails to
    // bind we log once and continue without explicit reload signaling.
    commandPipe_ = std::make_unique<CommandPipeListener>(buildCommandPipeName());
    if (!commandPipe_->start([this](const CommandMessage& cmd) { handleCommand(cmd); })) {
        diagnostics_.emit("RELOAD-W002", Severity::Warning, "BOOT",
                          "Command pipe failed to start: " + commandPipe_->pipeName(),
                          "DevKit reload signaling will fall back to file watching");
        commandPipe_.reset();
    } else {
        log_.info("BOOT", "Command pipe ready: " + commandPipe_->pipeName());
        if (pipe_) {
            pipe_->write("command_pipe.ready", nlohmann::json{
                {"name", commandPipe_->pipeName()},
                {"protocol", "json-lines"},
                {"version", 1}
            });
        }
    }

    // Wire pipe to services for IPC message emission
    diagnostics_.setPipeWriter(pipe_.get());
    events_.setPipeWriter(pipe_.get());
    saves_.setPipeWriter(pipe_.get());
    registries_.story().setPipeWriter(pipe_.get());
    registries_.worldNpcs().setSaveService(&saves_);

    contentDir_ = contentDir;
    log_.info("BOOT", "EFL v" + eflVersionString() + " initializing");
    if (pipe_) {
        pipe_->write("efl.version", nlohmann::json{{"version", eflVersionString()}});
    }

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

        // Broadcast what this engine build actually supports so connected
        // DevKits can swap their static handler/feature whitelists for the
        // live snapshot. The DevKit treats absent fields as "use embedded
        // defaults", so this stays additive.
        pipe_->write("capabilities.snapshot", nlohmann::json{
            {"eflVersion", eflVersionString()},
            {"handlers", builtinScriptHookHandlers()},
            {"features", supportedFeatureTags()},
            {"hookKinds", supportedHookKinds()},
            {"flags", buildCapabilityFlags()},
        });
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
                *instanceWalker_, *routineInvoker_, *roomTracker_, pipe_.get(),
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

    // Wire StoryBridge quest callbacks so story events can start/advance EFL quests.
    // QuestRegistry tracks state internally; TriggerService evaluates questComplete
    // conditions against it. No FoM game script hooks needed for EFL-internal quests.
    registries_.story().onQuestStart = [this](const std::string& questId) {
        registries_.quests().startQuest(questId);
        log_.info("QUEST", "Quest started via story event: " + questId);
        if (pipe_) {
            pipe_->write("quest.updated", nlohmann::json{
                {"questId", questId}, {"action", "start"},
                {"state", "active"}});
        }
    };
    registries_.story().onItemGrant = [this](int itemId, int qty) {
        grantItem(itemId, qty);
        if (pipe_) {
            pipe_->write("item.granted", nlohmann::json{
                {"itemId", itemId}, {"qty", qty}, {"source", "cutscene"}});
        }
    };

    registries_.quests().onItemGrant = [this](int itemId, int qty) {
        grantItem(itemId, qty);
        if (pipe_) {
            pipe_->write("item.granted", nlohmann::json{
                {"itemId", itemId}, {"qty", qty}, {"source", "quest_reward"}});
        }
    };

    registries_.story().onQuestAdvance = [this](const std::string& questId) {
        std::string stageBefore = registries_.quests().getCurrentStage(questId);
        // completeStage requires the current stage ID; derive it from the registry.
        if (!stageBefore.empty()) {
            registries_.quests().completeStage(questId, stageBefore);
        }
        std::string stageAfter = registries_.quests().getCurrentStage(questId);
        log_.info("QUEST", "Quest advanced via story event: " + questId
                  + " (" + stageBefore + " -> " + (stageAfter.empty() ? "complete" : stageAfter) + ")");
        if (pipe_) {
            pipe_->write("quest.updated", nlohmann::json{
                {"questId", questId}, {"action", "advance"},
                {"prevStage", stageBefore},
                {"currentStage", stageAfter.empty() ? "complete" : stageAfter}});
        }
    };

    // Wire WorldNpc schedule change callback.
    // Fires when tickSchedule detects a boundary crossing mid-day.
    // If the NPC's new area is in the current room, spawn (or teleport) there.
    // If not, despawn any existing instance.
    registries_.worldNpcs().onScheduleChange = [this](const std::string& npcId,
                                                       const std::string& newAreaId,
                                                       const std::string& newAnchorId) {
        const WorldNpcDef* def = registries_.worldNpcs().getWorldNpc(npcId);
        if (!def) return;

        const AreaDef* newArea = newAreaId.empty() ? nullptr : registries_.areas().getArea(newAreaId);
        std::string currentRoom = roomTracker_->currentRoomName();
        bool inCurrentRoom = newArea &&
                             !newArea->hostRoom.empty() &&
                             newArea->hostRoom == currentRoom;

        if (inCurrentRoom) {
            // Spawn at new anchor (despawns existing instance first if present).
            spawnWorldNpc(*def, newAreaId, newAnchorId);
        } else {
            despawnWorldNpc(npcId);
        }
    };

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

    // Frame callback for room tracking + hot-reload (MUST)
    if (hooks_->registerFrameCallback("frame_update", [this]() {
                roomTracker_->update();
                hotReload_.drainQueue();
                if (commandPipe_)
                    commandPipe_->drainQueue();
                // WorldNpc schedule tick — read actual game time and detect boundary crossings.
                // readGameTime() returns -1 if Calendar is not yet initialised (early frames).
                int64_t t = readGameTime();
                if (t >= 0)
                    registries_.worldNpcs().tickSchedule(static_cast<int>(t % 86400));
            })) {
        log_.info("HOOK", "Registered: frame_update");
        emitBootStatus("hook.registered", "pass", "frame_update");
    } else {
        log_.warn("HOOK", "Failed to register frame_update callback");
        diagnostics_.emit("HOOK-W003", Severity::Warning, "HOOK",
                          "Failed to register frame_update callback",
                          "Room tracking will not function");
    }

    // Resource node hooks — yield roll + harvest tracking (SHOULD — graceful degradation)
    for (const auto& hookName : {"pick_node", "hoe_node", "water_node"}) {
        std::string target = std::string("gml_Script_") + hookName;
        if (hooks_->registerScriptHook(hookName, target,
                [this, hookName](YYTK::CInstance* self, YYTK::CInstance*, YYTK::CCode*,
                       int, YYTK::RValue*) {
                    // Read the FoM instance position and map to grid coords.
                    double px = 0.0, py = 0.0;
                    try {
                        px = instanceWalker_->getVariable(self, "x").m_Real;
                        py = instanceWalker_->getVariable(self, "y").m_Real;
                    } catch (...) { return; }
                    int gx = static_cast<int>(px / 32.0);
                    int gy = static_cast<int>(py / 32.0);

                    // Find the EFL resource at this grid position in the current room.
                    std::string currentRoom = roomTracker_->currentRoomName();
                    auto areas = registries_.areas().areasByHostRoom(currentRoom);
                    const ResourceDef* hit = nullptr;
                    std::string hitArea;
                    for (const auto* area : areas) {
                        for (const auto* res : registries_.resources().resourcesInArea(area->id)) {
                            auto it = res->spawnRules.anchors.find(area->id);
                            if (it != res->spawnRules.anchors.end()) {
                                const auto [rx, ry] = it->second;
                                if (rx == gx && ry == gy) { hit = res; hitArea = area->id; break; }
                            }
                        }
                        if (hit) break;
                    }

                    if (!hit || hit->yieldTable.empty()) {
                        log_.info("HOOK", std::string("Node interaction (no EFL resource): ") + hookName);
                        return;
                    }

                    // Roll yield from yieldTable.
                    const auto& entry = hit->yieldTable[
                        std::uniform_int_distribution<size_t>(
                            0, hit->yieldTable.size() - 1)(rng_)];
                    int qty = std::uniform_int_distribution<int>(entry.min, entry.max)(rng_);

                    // Record harvest day for respawn tracking.
                    harvestedAt_[hit->id] = currentDay_;

                    log_.info("RESOURCE", "Harvested " + hit->id
                              + ": " + entry.item + " x" + std::to_string(qty)
                              + " [" + std::string(hookName) + "]");

                    // Grant the harvested item if a numeric FoM item index is available.
                    if (entry.itemId > 0) {
                        grantItem(entry.itemId, qty);
                    } else {
                        log_.info("RESOURCE", "No itemId on yield entry '" + entry.item
                                  + "' — harvest logged but item not granted");
                    }
                    if (pipe_) {
                        pipe_->write("resource.harvested", nlohmann::json{
                            {"resourceId", hit->id}, {"areaId", hitArea},
                            {"item", entry.item}, {"itemId", entry.itemId},
                            {"quantity", qty},
                            {"tool", std::string(hookName)}});
                    }
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
                    diagnostics_.emit("RESOURCE-H001", Severity::Hazard, "RESOURCE",
                                     "Manifest-declared efl_resource_despawn hook fired but dispatch is not wired",
                                     "Standard resource interactions via hoe/pick/water node hooks are active. "
                                     "Manifest-declared custom-target dispatch is a V2.5 feature.");
                });
            log_.info("HOOK", "Registered manifest hook: " + sh.target + " -> " + sh.handler);
        }
    }

    // Crafting station hook — inject EFL recipes when any crafting menu opens.
    // gml_Script_spawn_crafting_menu confirmed via EFL_Probe (index 1635, SCPT 3251).
    //   Args: (menu_struct, x: real, y: real, station_id: real)
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

    // Dungeon vote injection — add EFL nodes to FoM biome vote pools (RESOURCE-H002)
    //
    // Architecture (confirmed via __fiddle__.json + Ghidra analysis):
    //   FoM loads dungeons/dungeons/biomes[N].votes.POOL from __fiddle__.json at startup.
    //   Each biome (0=Upper Mines/floor1, 1=Tide Caverns/20, 2=Deep Earth/40,
    //   3=Lava Caves/60, 4=Ancient Ruins/80) has a votes struct with named pools.
    //   Pool entry format: {object: "gml_obj_id", votes: N} (integer weight).
    //   Pool names: "ore_rock", "small_rock", "seam_rock", "large_rock", "enemy",
    //               "fish", "bug", "forageable", "breakable", "chest", "junk", etc.
    //
    // Injection strategy:
    //   Hook gml_Script_load_dungeon (post-call) — fires once when dungeon data is
    //   populated from fiddle. Navigate: self -> biomes[N] -> votes -> pool_arr,
    //   then array_push({object, votes}) into the live GML array.
    //
    //   The biomes array path on self is discovered at runtime via property enumeration
    //   (self.biomes or self.dungeon.biomes). Property names are logged on first fire
    //   so the injection path can be verified or corrected without re-analysis.
    if (!registries_.resources().resourcesWithDungeonVotes().empty()) {
        bool hooked = hooks_->registerScriptHook(
                "efl_dungeon_vote_inject", "gml_Script_load_dungeon",
                [this](YYTK::CInstance* self, YYTK::CInstance*, YYTK::CCode*,
                       int, YYTK::RValue*) {
                    static bool fired = false;
                    if (fired) return;
                    fired = true;

                    auto candidates = registries_.resources().resourcesWithDungeonVotes();
                    if (candidates.empty()) return;

                    // Build a VALUE_OBJECT RValue from the CInstance* for struct ops.
                    YYTK::RValue selfRv;
                    selfRv.m_Kind   = YYTK::VALUE_OBJECT;
                    selfRv.m_Object = reinterpret_cast<YYTK::YYObjectBase*>(self);

                    // Log self's property names once for runtime diagnosis.
                    try {
                        YYTK::RValue namesArr = routineInvoker_->callBuiltin(
                            "variable_struct_get_names", {selfRv});
                        YYTK::RValue lenRv = routineInvoker_->callBuiltin(
                            "array_length", {namesArr});
                        int n = (lenRv.m_Kind == YYTK::VALUE_REAL)  ? static_cast<int>(lenRv.m_Real)
                              : (lenRv.m_Kind == YYTK::VALUE_INT32) ? lenRv.m_i32 : 0;
                        std::string propList;
                        for (int i = 0; i < n && i < 32; ++i) {
                            YYTK::RValue iRv;
                            iRv.m_Kind = YYTK::VALUE_REAL;
                            iRv.m_Real = static_cast<double>(i);
                            YYTK::RValue nameRv = routineInvoker_->callBuiltin(
                                "array_get", {namesArr, iRv});
                            if (i > 0) propList += ", ";
                            propList += nameRv.ToString();
                        }
                        log_.info("RESOURCE", "load_dungeon self props: " + propList);
                    } catch (...) {
                        log_.warn("RESOURCE", "load_dungeon: self property enumeration failed");
                    }

                    // Locate the biomes array: try self.biomes, then self.dungeon.biomes.
                    auto getField = [&](const YYTK::RValue& obj, const char* name) {
                        return routineInvoker_->callBuiltin("variable_struct_get",
                            {obj, YYTK::RValue(name)});
                    };

                    YYTK::RValue biomesArr = getField(selfRv, "biomes");
                    if (biomesArr.m_Kind != YYTK::VALUE_ARRAY) {
                        YYTK::RValue dungeonRv = getField(selfRv, "dungeon");
                        if (dungeonRv.m_Kind == YYTK::VALUE_OBJECT)
                            biomesArr = getField(dungeonRv, "biomes");
                    }

                    if (biomesArr.m_Kind != YYTK::VALUE_ARRAY) {
                        log_.warn("RESOURCE", "RESOURCE-H002: biomes array not found in "
                            "load_dungeon self — see self props above; injection skipped");
                        diagnostics_.emit("RESOURCE-H002", Severity::Hazard, "RESOURCE",
                            "Dungeon vote injection: biomes array not found at runtime",
                            "load_dungeon self did not expose self.biomes or self.dungeon.biomes; "
                            "check self prop log for correct path");
                        return;
                    }

                    // Confirmed biome name -> index from __fiddle__.json
                    static const std::unordered_map<std::string, int> kBiomeIdx = {
                        {"upper_mines", 0}, {"tide_caverns", 1}, {"deep_earth", 2},
                        {"lava_caves",  3}, {"ancient_ruins", 4}
                    };

                    int injected = 0;
                    for (const auto* res : candidates) {
                        for (const auto& vote : res->spawnRules.dungeonVotes) {
                            if (vote.objectId.empty()) {
                                log_.warn("RESOURCE", res->id + ": dungeonVote missing objectId — skipped");
                                continue;
                            }
                            auto biomeIt = kBiomeIdx.find(vote.biome);
                            if (biomeIt == kBiomeIdx.end()) {
                                log_.warn("RESOURCE", res->id + ": unknown biome '" + vote.biome + "' — skipped");
                                continue;
                            }
                            try {
                                YYTK::RValue biomeIdxRv;
                                biomeIdxRv.m_Kind = YYTK::VALUE_REAL;
                                biomeIdxRv.m_Real = static_cast<double>(biomeIt->second);

                                YYTK::RValue biomeRv = routineInvoker_->callBuiltin(
                                    "array_get", {biomesArr, biomeIdxRv});
                                if (biomeRv.m_Kind != YYTK::VALUE_OBJECT) {
                                    log_.warn("RESOURCE", "biomes[" + vote.biome + "] is not a struct");
                                    continue;
                                }

                                YYTK::RValue votesRv = getField(biomeRv, "votes");
                                if (votesRv.m_Kind != YYTK::VALUE_OBJECT) {
                                    log_.warn("RESOURCE", "biomes[" + vote.biome + "].votes is not a struct");
                                    continue;
                                }

                                YYTK::RValue poolArr = routineInvoker_->callBuiltin("variable_struct_get",
                                    {votesRv, YYTK::RValue(vote.pool.c_str())});
                                if (poolArr.m_Kind != YYTK::VALUE_ARRAY) {
                                    log_.warn("RESOURCE", "votes." + vote.pool + " is not an array in biome "
                                              + vote.biome);
                                    continue;
                                }

                                YYTK::RValue weightRv;
                                weightRv.m_Kind = YYTK::VALUE_REAL;
                                weightRv.m_Real = static_cast<double>(vote.weight);
                                YYTK::RValue entryRv(std::map<std::string, YYTK::RValue>{
                                    {"object", YYTK::RValue(vote.objectId.c_str())},
                                    {"votes",  weightRv}
                                });

                                routineInvoker_->callBuiltin("array_push", {poolArr, entryRv});

                                log_.info("RESOURCE", "vote injected: " + res->id
                                    + " biome=" + vote.biome + " pool=" + vote.pool
                                    + " object=" + vote.objectId
                                    + " weight=" + std::to_string(vote.weight));
                                ++injected;
                            } catch (const std::exception& ex) {
                                log_.warn("RESOURCE", "Vote inject failed: " + res->id
                                    + " biome=" + vote.biome + ": " + ex.what());
                            }
                        }
                    }

                    if (injected > 0) {
                        log_.info("RESOURCE", "RESOURCE-H002: " + std::to_string(injected)
                                  + " dungeon vote(s) injected");
                        if (pipe_) pipe_->write("resource.dungeon_votes_injected",
                                                nlohmann::json{{"count", injected}});
                    }
                });
        if (hooked) {
            log_.info("HOOK", "Registered: efl_dungeon_vote_inject (load_dungeon)");
            emitBootStatus("hook.registered", "pass", "efl_dungeon_vote_inject");
        } else {
            diagnostics_.emit("RESOURCE-W001", Severity::Warning, "RESOURCE",
                "Failed to register dungeon vote hook",
                "EFL resource nodes will not appear in FoM dungeon floors");
        }
    }

    // Cutscene system hooks (StoryBridge — V2)
    //
    // load_cutscenes [3962] — fires once at boot when Mist reads __mist__.json.
    // EFL uses this as a signal that FoM's cutscene table is ready; logs registered
    // EFL cutscene keys to IPC so DevKit can show them.
    bool hasStory = std::any_of(manifests_.begin(), manifests_.end(),
                                [](const auto& m) { return m.features.story; });
    if (hasStory) {
        if (hooks_->registerScriptHook(
                "efl_load_cutscenes", "gml_Script_load_cutscenes",
                [this](YYTK::CInstance*, YYTK::CInstance*, YYTK::CCode*, int, YYTK::RValue*) {
                    registries_.story().onLoadCutscenes();
                    log_.info("STORY", "load_cutscenes fired — EFL cutscene keys registered");
                })) {
            log_.info("HOOK", "Registered: efl_load_cutscenes");
            emitBootStatus("hook.registered", "pass", "efl_load_cutscenes");
        } else {
            diagnostics_.emit("STORY-W001", Severity::Warning, "STORY",
                "Failed to register load_cutscenes hook",
                "EFL cutscene keys will not be reported to DevKit at boot");
        }

        // check_cutscene_eligible [3964] — fires every day (AM scan + end-of-day scan).
        // Probe confirmed: argc=1 (key string) in AM scan, argc=2 (key, bool) at EOD.
        // EFL intercepts calls for its own keys and returns true when trigger conditions
        // are met; falls through to FoM's original check for all other keys.
        if (hooks_->registerInterceptHook(
                "efl_cutscene_eligible",
                "gml_Script_check_cutscene_eligible@Mist@Mist",
                [this](YYTK::RValue& result, int argc, YYTK::RValue** args) -> bool {
                    if (argc < 1 || !args[0]) return false;
                    std::string key = args[0]->ToString();
                    if (!registries_.story().evaluateEligibility(key, registries_.triggers()))
                        return false;
                    // EFL commits this cutscene: return true to FoM.
                    result.m_Kind = YYTK::VALUE_BOOL;
                    result.m_i32  = 1;
                    log_.info("STORY", "Cutscene eligible (EFL): " + key);
                    return true;
                })) {
            log_.info("HOOK", "Registered: efl_cutscene_eligible (check_cutscene_eligible)");
            emitBootStatus("hook.registered", "pass", "efl_cutscene_eligible");
        } else {
            diagnostics_.emit("STORY-E002", Severity::Error, "STORY",
                "Failed to register check_cutscene_eligible hook",
                "EFL cutscenes will never be triggered in-game");
        }

        // new_day [4124] — fires at the start of each in-game day, argc=0.
        // Probe confirmed: fires after the sleep fade, before FoM's own write_node calls.
        // EFL uses this to drive accurate day-based resource respawn.
        if (hooks_->registerScriptHook("efl_new_day", "gml_Script_new_day",
                [this](YYTK::CInstance*, YYTK::CInstance*, YYTK::CCode*, int, YYTK::RValue*) {
                    currentDay_++;

                    // Check for season change (probe B: season() returns 0-3).
                    int newSeason = readSeason();
                    bool seasonChanged = (newSeason != currentSeason_ && currentSeason_ != -1);
                    if (newSeason >= 0) currentSeason_ = newSeason;

                    log_.info("RESOURCE", "New day " + std::to_string(currentDay_)
                              + ", season=" + std::to_string(currentSeason_)
                              + (seasonChanged ? " (season changed)" : ""));

                    // Respawn daily resources whose threshold has elapsed.
                    // Respawn seasonal resources when the season turns.
                    for (auto it = harvestedAt_.begin(); it != harvestedAt_.end(); ) {
                        const ResourceDef* res = registries_.resources().getResource(it->first);
                        if (!res) { it = harvestedAt_.erase(it); continue; }

                        const auto& policy = res->spawnRules.respawnPolicy;
                        bool shouldRespawn = false;
                        if (policy == "seasonal") {
                            shouldRespawn = seasonChanged;
                        } else {
                            uint64_t days = respawnThresholdDays(policy);
                            shouldRespawn = (days > 0 && (currentDay_ - it->second) >= days);
                        }

                        if (!shouldRespawn) { ++it; continue; }

                        for (const auto& [areaId, _] : res->spawnRules.anchors)
                            spawnResourceNode(*res, areaId);

                        log_.info("RESOURCE", "Respawned: " + res->id);
                        it = harvestedAt_.erase(it);
                    }

                    // V3 pilot: dispatch any calendar events that match today.
                    // Skipped silently when the registry is empty (no per-pack
                    // 'calendar' feature declared) so the cost stays at zero.
                    if (!registries_.calendar().allEvents().empty()) {
                        int dayOfSeason = readDayOfSeason();
                        if (currentSeason_ >= 0 && dayOfSeason >= 1) {
                            fireCalendarEvents(currentSeason_, dayOfSeason);
                        }
                    }
                })) {
            log_.info("HOOK", "Registered: efl_new_day");
            emitBootStatus("hook.registered", "pass", "efl_new_day");
        } else {
            diagnostics_.emit("RESOURCE-W002", Severity::Warning, "RESOURCE",
                "Failed to register new_day hook",
                "EFL resource respawn will not fire on day change");
        }
    }
}

void EflBootstrap::stepConnectAreaRegistry() {
    // When room changes, deactivate old EFL areas and activate new ones.
    roomTracker_->onRoomChange([this](const std::string& oldRoom, const std::string& newRoom) {
        log_.info("ROOM", "Room changed: " + oldRoom + " -> " + newRoom);

        // Despawn all WorldNpc instances before processing old room deactivation.
        // They belong to areas in the room we are leaving.
        despawnAllWorldNpcs();

        // Deactivate areas in the old room and fire their exit events.
        auto oldAreas = registries_.areas().areasByHostRoom(oldRoom);
        for (const auto* area : oldAreas) {
            if (roomBackend_)
                roomBackend_->deactivate();
            if (!area->exitEvent.empty())
                registries_.story().fireEffects(area->exitEvent, registries_.triggers());
        }

        // Read current time of day once for all area activations in the new room.
        int64_t t = readGameTime();
        int timeOfDay = (t >= 0) ? static_cast<int>(t % 86400) : 21600; // default 6AM

        // Activate areas in the new room and fire their entry events.
        auto newAreas = registries_.areas().areasByHostRoom(newRoom);
        for (const auto* area : newAreas) {
            if (roomBackend_)
                roomBackend_->activate(*area);
            if (!area->entryEvent.empty())
                registries_.story().fireEffects(area->entryEvent, registries_.triggers());

            // Spawn resource nodes for this area via spawnResourceNode().
            // Uses instance_create_layer if ResourceDef.objectName is set (interim path).
            // Full grid-native spawn via attempt_to_write_object_node deferred pending
            // register_node@Anchor@Anchor struct probe (RESOURCE-H003).
            auto resources = registries_.resources().resourcesInArea(area->id);
            for (const auto* res : resources) {
                if (res->spawnRules.anchors.find(area->id) == res->spawnRules.anchors.end()) {
                    log_.warn("RESOURCE", "No anchor for " + res->id + " in area " + area->id
                              + " — skipping spawn (add spawnRules.anchors in resource JSON)");
                    continue;
                }
                if (res->objectName.empty()) {
                    diagnostics_.emit("RESOURCE-W003", Severity::Warning, "RESOURCE",
                        "Resource '" + res->id + "' has no objectName — spawn skipped",
                        "Add \"objectName\": \"obj_<name>\" to resource JSON for interim spawn");
                    continue;
                }
                spawnResourceNode(*res, area->id);
            }

            // Spawn WorldNpcs whose schedule puts them in this area at the current time.
            auto worldNpcs = registries_.worldNpcs().worldNpcsForArea(area->id, timeOfDay);
            for (const auto* wnpc : worldNpcs) {
                auto [activeArea, anchorId] =
                    registries_.worldNpcs().activeLocationForNpc(wnpc->id, timeOfDay);
                spawnWorldNpc(*wnpc, activeArea, anchorId);
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
    if (commandPipe_) {
        commandPipe_->stop();
        commandPipe_.reset();
    }
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

        for (const auto& dep : m.dependencies.required) {
            if (!CompatibilityService::isExternalModLoaded(dep.modId)) {
                log_.error("BOOT", "Mod '" + m.modId + "' requires '" + dep.modId +
                           "' which is not loaded");
                diagnostics_.emit("MANIFEST-E003", Severity::Error, "MANIFEST",
                                  "Required dependency '" + dep.modId + "' not loaded for mod '" + m.modId + "'",
                                  "Install '" + dep.modId + "' via MOMI before loading this pack");
                allOk = false;
            }
        }

        for (const auto& dep : m.dependencies.optional) {
            if (!CompatibilityService::isExternalModLoaded(dep.modId)) {
                log_.warn("BOOT", "Optional dependency '" + dep.modId +
                          "' not loaded for mod '" + m.modId + "' — related features disabled");
                diagnostics_.emit("MANIFEST-W001", Severity::Warning, "MANIFEST",
                                  "Optional dependency '" + dep.modId + "' not loaded for mod '" + m.modId + "'",
                                  "Install '" + dep.modId + "' to enable related features");
            }
        }

        for (const auto& c : m.dependencies.conflicts) {
            if (CompatibilityService::isExternalModLoaded(c.modId)) {
                log_.error("BOOT", "Mod '" + m.modId + "' conflicts with loaded mod '" + c.modId + "'");
                diagnostics_.emit("MANIFEST-E004", Severity::Error, "MANIFEST",
                                  "Conflicting mod '" + c.modId + "' is loaded alongside '" + m.modId + "'",
                                  c.reason.empty() ? "Remove one of the conflicting mods" : c.reason);
                allOk = false;
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

        // Story / cutscene triggers
        if (manifest.features.story) {
            loadDir("events", [&](const nlohmann::json& j, const std::string& file) {
                auto def = CutsceneDef::fromJson(j);
                if (def) {
                    registries_.story().registerCutscene(*def);
                    log_.info("BOOT", "Registered cutscene trigger: " + def->id);
                } else {
                    log_.error("BOOT", "Failed to parse event: " + file);
                    diagnostics_.emit("STORY-E001", Severity::Error, "STORY",
                                      "Failed to parse cutscene definition: " + file,
                                      "Check required fields: id, trigger");
                }
            });
        }

        // Calendar / world events (V3 pilot)
        if (manifest.features.calendar) {
            loadDir("calendar", [&](const nlohmann::json& j, const std::string& file) {
                auto def = CalendarEventDef::fromJson(j);
                if (def) {
                    registries_.calendar().registerEvent(*def);
                    log_.info("BOOT", "Registered calendar event: " + def->id);
                } else {
                    log_.error("BOOT", "Failed to parse calendar event: " + file);
                    diagnostics_.emit("CALENDAR-E001", Severity::Error, "CALENDAR",
                                      "Failed to parse calendar event definition: " + file,
                                      "Check required field: id");
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
            auto def = CutsceneDef::fromJson(j);
            if (def) {
                registries_.story().registerCutscene(*def);
                log_.info("RELOAD", "Reloaded cutscene trigger: " + def->id);
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
        } else if (contentType == "calendar") {
            auto def = CalendarEventDef::fromJson(j);
            if (def) {
                registries_.calendar().registerEvent(*def);
                log_.info("RELOAD", "Reloaded calendar event: " + def->id);
            } else {
                log_.warn("RELOAD", "Failed to parse calendar event: " + fname);
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

void EflBootstrap::handleCommand(const CommandMessage& cmd) {
    if (cmd.type == "ping") {
        if (pipe_) {
            pipe_->write("pong", nlohmann::json{{"echo", cmd.payload}});
        }
        return;
    }

    if (cmd.type == "reload") {
        std::string reason = cmd.payload.value("reason", std::string("devkit-command"));
        reloadAllContent(reason);
        return;
    }

    if (cmd.type == "caps" || cmd.type == "capabilities") {
        if (pipe_) {
            pipe_->write("capabilities.snapshot", nlohmann::json{
                {"eflVersion", eflVersionString()},
                {"handlers", builtinScriptHookHandlers()},
                {"features", supportedFeatureTags()},
                {"hookKinds", supportedHookKinds()},
                {"flags", buildCapabilityFlags()},
            });
        }
        return;
    }

    log_.warn("COMMAND", "Unknown DevKit command: " + cmd.type);
    if (pipe_) {
        pipe_->write("command.unknown", nlohmann::json{{"type", cmd.type}});
    }
}

void EflBootstrap::reloadAllContent(const std::string& reason) {
    namespace fs = std::filesystem;

    if (contentDir_.empty()) {
        log_.warn("RELOAD", "Cannot honour reload command: contentDir is empty");
        return;
    }

    fs::path root = fs::path(contentDir_);
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        log_.warn("RELOAD", "Reload target is not a directory: " + root.string());
        return;
    }

    if (pipe_) {
        pipe_->write("reload.requested", nlohmann::json{
            {"path", root.string()},
            {"reason", reason}
        });
    }

    size_t files = 0;
    size_t failures = 0;

    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::end(it);
         it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        const auto& entry = *it;
        if (!entry.is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }
        if (entry.path().extension() != ".json") {
            continue;
        }

        // The contentType is the immediate parent directory name relative to root.
        fs::path rel = fs::relative(entry.path(), root, ec);
        if (ec) {
            ec.clear();
            continue;
        }
        if (!rel.has_parent_path()) {
            continue; // top-level files like config.json are not registry content
        }
        std::string contentType = rel.begin()->string();
        if (contentType.empty() || contentType == ".") {
            continue;
        }

        size_t before = failures;
        try {
            reloadContentType(contentType, entry.path());
            ++files;
        } catch (const std::exception& ex) {
            ++failures;
            log_.error("RELOAD", std::string{"reload exception: "} + ex.what());
        }
        if (failures > before) {
            log_.warn("RELOAD", "Reload error in " + entry.path().string());
        }
    }

    log_.info("RELOAD", "DevKit reload complete (reason=" + reason + ", files="
              + std::to_string(files) + ", failures=" + std::to_string(failures) + ")");
    if (pipe_) {
        pipe_->write("reload.complete", nlohmann::json{
            {"path", root.string()},
            {"reason", reason},
            {"files", files},
            {"failures", failures},
            {"status", failures == 0 ? "ok" : "partial"}
        });
    }
}

// ─── Resource lifecycle helpers (real SDK only) ───────────────────────────────

#ifndef EFL_STUB_SDK

void EflBootstrap::spawnResourceNode(const ResourceDef& res, const std::string& areaId) {
    auto anchorIt = res.spawnRules.anchors.find(areaId);
    if (anchorIt == res.spawnRules.anchors.end()) return;
    const auto [gx, gy] = anchorIt->second;
    if (res.objectName.empty()) return; // RESOURCE-W003 already emitted at area entry

    try {
        double px = static_cast<double>(gx) * 32.0;
        double py = static_cast<double>(gy) * 32.0;

        YYTK::RValue assetId = routineInvoker_->callBuiltin(
            "asset_get_index", {YYTK::RValue(res.objectName.c_str())});

        if (assetId.m_Kind == YYTK::VALUE_REAL && assetId.m_Real < 0) {
            log_.warn("RESOURCE", "Unknown FoM object '" + res.objectName
                      + "' for resource " + res.id + " — check objectName in resource JSON");
            return;
        }

        routineInvoker_->callBuiltin("instance_create_layer",
            {YYTK::RValue(px), YYTK::RValue(py), YYTK::RValue("Instances"), assetId});

        log_.info("RESOURCE", "Spawned " + res.id + " (" + res.objectName
                  + ") at grid (" + std::to_string(gx) + "," + std::to_string(gy)
                  + ") in " + areaId);

        if (pipe_) {
            pipe_->write("resource.spawned", nlohmann::json{
                {"resourceId", res.id}, {"areaId", areaId},
                {"objectName", res.objectName}, {"gridX", gx}, {"gridY", gy}});
        }
    } catch (const std::exception& ex) {
        log_.warn("RESOURCE", "Spawn failed for " + res.id + ": " + ex.what());
    }
}

// static
uint64_t EflBootstrap::respawnThresholdDays(const std::string& policy) {
    if (policy == "none")  return 0; // never respawn
    if (policy == "daily") return 1;
    if (policy == "seasonal") return 0; // handled via season change detection, not day count
    return 1;
}

// Probe B confirmed: unified_time@Calendar@Calendar returns int64 total seconds
// from midnight of day 0. time_of_day = value % 86400, day_index = value / 86400 - 1.
int64_t EflBootstrap::readGameTime() const {
    try {
        auto rv = routineInvoker_->callGameScript(
            "gml_Script_unified_time@Calendar@Calendar", {});
        if (rv.m_Kind == YYTK::VALUE_INT64) return rv.m_i64;
        if (rv.m_Kind == YYTK::VALUE_REAL)  return static_cast<int64_t>(rv.m_Real);
        if (rv.m_Kind == YYTK::VALUE_INT32) return rv.m_i32;
    } catch (...) {}
    return -1;
}

// season@Calendar@Calendar returns number: 0=spring 1=summer 2=fall 3=winter.
int EflBootstrap::readSeason() const {
    try {
        auto rv = routineInvoker_->callGameScript(
            "gml_Script_season@Calendar@Calendar", {});
        if (rv.m_Kind == YYTK::VALUE_REAL)  return static_cast<int>(rv.m_Real);
        if (rv.m_Kind == YYTK::VALUE_INT32) return rv.m_i32;
    } catch (...) {}
    return -1;
}

// Day-of-season derived from unified_time. FoM's Calendar uses 28-day months,
// so we derive the day from the absolute day index rather than asking GML for
// it (no probed script returns it directly). Returns 1..28 on success, -1 if
// Calendar isn't ready yet.
int EflBootstrap::readDayOfSeason() const {
    int64_t t = readGameTime();
    if (t < 0) return -1;
    int64_t absoluteDay = t / 86400;
    if (absoluteDay < 0) return -1;
    int day = static_cast<int>((absoluteDay % 28) + 1);
    return day;
}

// Drive CalendarRegistry's tick and fire onActivate side-effects through the
// same TriggerService + StoryBridge that quests/cutscenes use. Kept here so
// the registry stays decoupled from the live engine bridges.
void EflBootstrap::fireCalendarEvents(int season, int dayOfSeason) {
    auto& cal = registries_.calendar();
    cal.onActivate = [this](const CalendarEventDef& def) {
        if (!def.condition.empty()) {
            if (!registries_.triggers().evaluate(def.condition)) {
                log_.info("CALENDAR",
                          "Event '" + def.id + "' skipped: condition '"
                          + def.condition + "' not satisfied");
                return;
            }
        }

        log_.info("CALENDAR", "Firing calendar event: " + def.id);

        if (!def.onActivate.empty()) {
            registries_.story().fireEffects(def.onActivate, registries_.triggers());
        }
    };

    size_t fired = cal.tickNewDay(season, dayOfSeason);
    if (fired > 0) {
        log_.info("CALENDAR",
                  "Fired " + std::to_string(fired) + " calendar event(s) for season="
                  + std::to_string(season) + " day=" + std::to_string(dayOfSeason));
    }
}

// ─── WorldNpc spawn / despawn helpers ────────────────────────────────────────

void EflBootstrap::spawnWorldNpc(const WorldNpcDef& def,
                                  const std::string& areaId,
                                  const std::string& anchorId) {
    if (def.objectName.empty()) {
        log_.warn("NPC", "WorldNpc '" + def.id + "' has no objectName — spawn skipped");
        return;
    }
    if (def.unlockTrigger && !registries_.triggers().evaluate(*def.unlockTrigger)) {
        log_.info("NPC", "WorldNpc '" + def.id + "' locked (trigger: " + *def.unlockTrigger + ")");
        return;
    }

    // Despawn any existing instance first (handles teleport case).
    despawnWorldNpc(def.id);

    auto commaPos = anchorId.find(',');
    if (commaPos == std::string::npos) {
        log_.warn("NPC", "WorldNpc '" + def.id + "': invalid anchor '" + anchorId
                  + "' — expected \"x,y\" format");
        return;
    }

    try {
        double x = std::stod(anchorId.substr(0, commaPos));
        double y = std::stod(anchorId.substr(commaPos + 1));

        YYTK::RValue obj = routineInvoker_->callBuiltin("asset_get_index",
            {YYTK::RValue(def.objectName.c_str())});
        if (obj.m_Kind == YYTK::VALUE_REAL && obj.m_Real < 0) {
            log_.warn("NPC", "WorldNpc '" + def.id + "': unknown object '" + def.objectName + "'");
            return;
        }

        YYTK::RValue inst = routineInvoker_->callBuiltin("instance_create_layer",
            {YYTK::RValue(x), YYTK::RValue(y), YYTK::RValue("Instances"), obj});

        worldNpcInstanceIds_[def.id] = inst.m_Real;

        // Keep registry in sync so the next tickSchedule frame doesn't re-fire for this NPC.
        registries_.worldNpcs().setLastKnown(def.id, areaId, anchorId);

        log_.info("NPC", "Spawned WorldNpc '" + def.id + "' at ("
                  + std::to_string(x) + "," + std::to_string(y) + ") area=" + areaId);
        if (pipe_) {
            pipe_->write("npc.spawned", nlohmann::json{
                {"npcId", def.id}, {"areaId", areaId},
                {"x", x}, {"y", y}, {"kind", "world"}});
        }
    } catch (const std::exception& ex) {
        log_.warn("NPC", "Failed to spawn WorldNpc '" + def.id + "': " + ex.what());
    }
}

void EflBootstrap::despawnWorldNpc(const std::string& npcId) {
    auto it = worldNpcInstanceIds_.find(npcId);
    if (it == worldNpcInstanceIds_.end()) return;

    try {
        routineInvoker_->callBuiltin("instance_destroy",
            {YYTK::RValue(it->second)});
    } catch (...) {}

    worldNpcInstanceIds_.erase(it);
    log_.info("NPC", "Despawned WorldNpc '" + npcId + "'");
    if (pipe_) {
        pipe_->write("npc.despawned", nlohmann::json{{"npcId", npcId}, {"kind", "world"}});
    }
}

void EflBootstrap::despawnAllWorldNpcs() {
    if (worldNpcInstanceIds_.empty()) return;
    std::vector<std::string> ids;
    ids.reserve(worldNpcInstanceIds_.size());
    for (auto& [id, _] : worldNpcInstanceIds_)
        ids.push_back(id);
    for (const auto& id : ids)
        despawnWorldNpc(id);
}

// Probe D confirmed: give_item@Ari@Ari(itemId: int, qty: real).
// itemId is the numeric index into t2_input.json's items array.
void EflBootstrap::grantItem(int itemId, int qty) {
    try {
        YYTK::RValue idRv, qtyRv;
        idRv.m_Kind  = YYTK::VALUE_INT32; idRv.m_i32   = itemId;
        qtyRv.m_Kind = YYTK::VALUE_REAL;  qtyRv.m_Real = static_cast<double>(qty);
        routineInvoker_->callGameScript(
            "gml_Script_give_item@Ari@Ari", {idRv, qtyRv});
        log_.info("ITEM", "Granted item " + std::to_string(itemId) + " x" + std::to_string(qty));
    } catch (...) {
        log_.warn("ITEM", "give_item@Ari@Ari call failed for id=" + std::to_string(itemId));
    }
}

#endif // EFL_STUB_SDK

} // namespace efl
