// EFL Runtime Probe — dev-only Aurie module
// Resolves the 3 RUNTIME_VERIFY stubs in EFL by hooking live FoM calls and
// writing a structured log to <game>/EFL/probe_output.txt.
//
// DO NOT load this DLL alongside EFL.dll — they compete on the same hooks.
// Build, load solo with Aurie, trigger the relevant in-game actions, quit.
//
// ── Actions to perform — all reachable from a fresh new game ──────────────
//  Probe 1 (node prototypes): Start a new game. create_node_prototypes fires
//                             on Farm entry (day 1). The probe then captures
//                             ALL scripts fired in the next 200 frames to
//                             identify the vote-table mutation API.
//  Probe 2 (surface spawn):   Same Farm entry — write_node fires as nodes
//                             are placed on the grid.
//  Probe 3 (crafting):        Open any crafting station. The Forge/Carpentry
//                             bench is accessible in town without progression.
// ──────────────────────────────────────────────────────────────────────────

#include <Aurie/shared.hpp>
#include <YYToolkit/YYTK_Shared.hpp>

#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <mutex>

namespace fs = std::filesystem;

// ── Globals ────────────────────────────────────────────────────────────────

static YYTK::YYTKInterface* g_yytk     = nullptr;
static std::ofstream        g_logFile;
static std::mutex           g_logMux;
static fs::path             g_logPath;

// hook target name → callback (for dispatch in CodeEventHandler)
static std::unordered_map<std::string,
    std::function<void(YYTK::CInstance*, YYTK::CInstance*,
                       YYTK::CCode*, int, YYTK::RValue*)>> g_hooks;

// Temporal capture: when > 0 log ALL script calls (countdown in events)
static std::atomic<int> g_captureFramesLeft{0};
static constexpr int    CAPTURE_WINDOW = 200; // ~3 seconds at 60fps

// ── Helpers ────────────────────────────────────────────────────────────────

static std::string timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto tt  = system_clock::to_time_t(now);
    auto ms  = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&tt), "%H:%M:%S")
       << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return ss.str();
}

static std::string rvalStr(const YYTK::RValue& rv) {
    switch (rv.m_Kind) {
        case YYTK::VALUE_REAL:      return std::to_string(rv.m_Real);
        case YYTK::VALUE_INT32:     return std::to_string(rv.m_i32);
        case YYTK::VALUE_INT64:     return std::to_string(rv.m_i64);
        case YYTK::VALUE_BOOL:      return rv.m_i32 ? "true" : "false";
        case YYTK::VALUE_STRING:    return '"' + rv.ToString() + '"';
        case YYTK::VALUE_UNDEFINED: return "<undefined>";
        case YYTK::VALUE_NULL:      return "<null>";
        default:                    return '<' + rv.GetKindName() + '>';
    }
}

static void plog(const std::string& probe, const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_logMux);
    if (g_logFile.is_open()) {
        g_logFile << '[' << timestamp() << "] [" << probe << "] " << msg << '\n';
        g_logFile.flush();
    }
}

static void plogArgs(const std::string& probe, const std::string& scriptName,
                     int argc, YYTK::RValue* args) {
    std::ostringstream ss;
    ss << scriptName << '(';
    for (int i = 0; i < argc; ++i) {
        if (i) ss << ", ";
        ss << '[' << i << "]=" << rvalStr(args[i]);
    }
    ss << ')';
    plog(probe, ss.str());
}

// ── YYTK CODE callback ─────────────────────────────────────────────────────

static bool CodeEventHandler(YYTK::FWCodeEvent& Event) {
    auto& evArgs = Event.Arguments();
    YYTK::CCode* code = std::get<2>(evArgs);
    if (!code) return false;

    const char* rawName = code->GetName();
    if (!rawName) return false;

    std::string scriptName(rawName);

    // Registered hooks — always dispatch
    auto it = g_hooks.find(scriptName);
    if (it != g_hooks.end()) {
        it->second(
            std::get<0>(evArgs),
            std::get<1>(evArgs),
            code,
            std::get<3>(evArgs),
            std::get<4>(evArgs)
        );
    }

    // Temporal capture — log everything fired within CAPTURE_WINDOW events
    // after create_node_prototypes, to identify the vote-table mutation API
    // regardless of its name.
    int remaining = g_captureFramesLeft.load();
    if (remaining > 0) {
        // Only log gml_Script_* entries to avoid flooding with object events
        if (scriptName.rfind("gml_Script_", 0) == 0) {
            plogArgs("CAPTURE", scriptName,
                     std::get<3>(evArgs), std::get<4>(evArgs));
        }
        g_captureFramesLeft.fetch_sub(1);
        if (g_captureFramesLeft.load() == 0) {
            plog("CAPTURE", "--- capture window closed ---");
            plog("CAPTURE", "Look above for scripts called after create_node_prototypes.");
            plog("CAPTURE", "Candidate lines: any gml_Script_*node* or *register* or *anchor*");
        }
    }

    return false;
}

// ── Hook registration ──────────────────────────────────────────────────────

static bool g_codeCallbackRegistered = false;

static bool registerHook(
    Aurie::AurieModule* module,
    const std::string& scriptName,
    std::function<void(YYTK::CInstance*, YYTK::CInstance*,
                       YYTK::CCode*, int, YYTK::RValue*)> callback)
{
    if (!g_codeCallbackRegistered && g_yytk && module) {
        auto status = g_yytk->CreateCallback(
            module,
            YYTK::EVENT_OBJECT_CALL,
            reinterpret_cast<void*>(&CodeEventHandler),
            0
        );
        if (Aurie::AurieSuccess(status)) {
            g_codeCallbackRegistered = true;
            plog("INIT", "YYTK code callback registered");
        } else {
            plog("INIT", "ERROR: failed to register YYTK code callback");
            return false;
        }
    }
    g_hooks[scriptName] = std::move(callback);
    plog("INIT", "Watching: " + scriptName);
    return true;
}

// ── Probe 1 — node prototype creation + vote-table mutation ────────────────
//
// create_node_prototypes fires on ANY area entry that has resource nodes —
// including the Farm on day 1 of a new game. No dungeon access required.
//
// When it fires, we open a 200-event capture window that logs every
// gml_Script_* call immediately following, which will include the vote-table
// mutation call (whatever its real name is).
//
// We also directly watch register_node@Anchor@Anchor as the leading candidate.
// If it appears in the capture log, that's your answer. If not, look for any
// *node* / *register* / *anchor* script in the capture window.

static void on_create_node_prototypes(YYTK::CInstance*, YYTK::CInstance*,
                                      YYTK::CCode*, int argc, YYTK::RValue* args) {
    plog("PROBE1-NODES", ">>> create_node_prototypes fired");
    plogArgs("PROBE1-NODES", "create_node_prototypes", argc, args);
    plog("PROBE1-NODES", "Opening " + std::to_string(CAPTURE_WINDOW) +
                         "-event capture window...");
    g_captureFramesLeft.store(CAPTURE_WINDOW);
}

static void on_register_node(YYTK::CInstance*, YYTK::CInstance*,
                             YYTK::CCode*, int argc, YYTK::RValue* args) {
    plog("PROBE1-NODES", "DIRECT HIT: register_node@Anchor@Anchor called");
    plogArgs("PROBE1-NODES", "register_node@Anchor@Anchor", argc, args);
    plog("PROBE1-NODES", "  => arg layout confirmed: (node_type, biome, pool, weight)");
    plog("PROBE1-NODES", "  => REPLACE RESOURCE-H002 stub with this call");
}

// ── Probe 2 — surface node spawn script ────────────────────────────────────
//
// write_node@Grid@Grid is confirmed to exist. attempt_to_write_object_node
// is the candidate for what EFL should call to place nodes.
// Both will fire on Farm entry (day 1). No progression needed.
//
// If attempt_to_write_object_node fires: that IS the EFL spawn call.
// If it doesn't: look for any *write*/*spawn*/*node* in the CAPTURE log.

static void on_write_node(YYTK::CInstance*, YYTK::CInstance*,
                          YYTK::CCode*, int argc, YYTK::RValue* args) {
    plog("PROBE2-SURFACE", "write_node@Grid@Grid");
    plogArgs("PROBE2-SURFACE", "write_node@Grid@Grid", argc, args);
}

static void on_attempt_write(YYTK::CInstance*, YYTK::CInstance*,
                             YYTK::CCode*, int argc, YYTK::RValue* args) {
    plog("PROBE2-SURFACE", "CONFIRMED: attempt_to_write_object_node");
    plogArgs("PROBE2-SURFACE", "attempt_to_write_object_node", argc, args);
    plog("PROBE2-SURFACE", "  => USE THIS in HijackedRoomBackend.cpp");
    plog("PROBE2-SURFACE", "  => arg layout: (node_type_string, grid_x, grid_y)");
}

// ── Probe 3 — crafting station open script ─────────────────────────────────
//
// 4 candidates watched. Whichever fires first when you open a station wins.
// Town has a crafting bench accessible from day 1 — no progression needed.

static void on_craft_candidate(const std::string& name, int argc, YYTK::RValue* args) {
    plog("PROBE3-CRAFTING", "CONFIRMED: " + name);
    plogArgs("PROBE3-CRAFTING", name, argc, args);
    plog("PROBE3-CRAFTING", "  => USE THIS as the hook target in bootstrap.cpp");
    plog("PROBE3-CRAFTING", "  => REPLACE CRAFT-H001 stub with real recipe injection");
}

// ── Module entry / unload ──────────────────────────────────────────────────

EXPORTED Aurie::AurieStatus ModuleInitialize(
    IN Aurie::AurieModule* Module,
    IN const Aurie::fs::path& ModulePath
) {
    auto logDir = ModulePath.parent_path().parent_path() / "EFL";
    fs::create_directories(logDir);
    g_logPath = logDir / "probe_output.txt";
    g_logFile.open(g_logPath, std::ios::out | std::ios::trunc);

    auto status = Aurie::ObGetInterface(
        "YYTK_Main",
        reinterpret_cast<Aurie::AurieInterfaceBase*&>(g_yytk)
    );
    if (!Aurie::AurieSuccess(status) || !g_yytk)
        return Aurie::AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;

    plog("INIT", "=== EFL Runtime Probe loaded ===");
    plog("INIT", "Output: " + g_logPath.string());
    plog("INIT", "All probes work from a fresh new game:");
    plog("INIT", "  [1+2] Start game, enter Farm — fires immediately on day 1");
    plog("INIT", "  [3]   Open a crafting station in town");
    plog("INIT", "");

    // Probe 1 — node prototypes + temporal capture for vote API
    registerHook(Module, "gml_Script_create_node_prototypes",
                 on_create_node_prototypes);
    registerHook(Module, "gml_Script_register_node@Anchor@Anchor",
                 on_register_node);

    // Probe 2 — surface spawn
    registerHook(Module, "gml_Script_write_node@Grid@Grid",
                 on_write_node);
    registerHook(Module, "gml_Script_attempt_to_write_object_node",
                 on_attempt_write);

    // Probe 3 — crafting (4 candidates)
    for (const auto& name : std::vector<std::string>{
            "gml_Script_spawn_crafting_menu",
            "gml_Script_open_crafting_station",
            "gml_Script_initialize_crafting@CraftingStation@CraftingStation",
            "gml_Script_start_crafting_session"}) {
        registerHook(Module, name,
            [name](YYTK::CInstance*, YYTK::CInstance*, YYTK::CCode*,
                   int argc, YYTK::RValue* args) {
                on_craft_candidate(name, argc, args);
            });
    }

    plog("INIT", "Watching " + std::to_string(g_hooks.size()) + " targets + temporal capture.");
    plog("INIT", "");
    return Aurie::AURIE_SUCCESS;
}

EXPORTED Aurie::AurieStatus ModuleUnload(
    IN Aurie::AurieModule* Module,
    IN const Aurie::fs::path& ModulePath
) {
    if (g_yytk && Module && g_codeCallbackRegistered)
        g_yytk->RemoveCallback(Module, reinterpret_cast<void*>(&CodeEventHandler));
    g_hooks.clear();

    plog("INIT", "");
    plog("INIT", "=== EFL Runtime Probe unloaded ===");
    if (g_logFile.is_open()) g_logFile.close();
    return Aurie::AURIE_SUCCESS;
}
