// EFL Runtime Probe — dev-only Aurie module
// Resolves the 3 RUNTIME_VERIFY stubs in EFL by hooking live FoM calls and
// writing a structured log to <game>/EFL/probe_output.txt.
//
// DO NOT load this DLL alongside EFL.dll — they compete on the same hooks.
// Build, load solo with Aurie, trigger the relevant in-game actions, quit.
//
// ── Actions to perform after loading ──────────────────────────────────────
//  Probe 1 (dungeon votes):  Enter any dungeon floor.
//  Probe 2 (surface spawn):  Enter the Farm, Narrows, or Deep Woods.
//  Probe 3 (crafting):       Open a crafting station (Forge, Carpentry, etc.)
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
#include <mutex>

namespace fs = std::filesystem;

// ── Globals ────────────────────────────────────────────────────────────────

static YYTK::YYTKInterface* g_yytk    = nullptr;
static std::ofstream        g_logFile;
static std::mutex           g_logMux;
static fs::path             g_logPath;

// hook name → target script name (for dispatch in CodeEventHandler)
static std::unordered_map<std::string,
    std::function<void(YYTK::CInstance*, YYTK::CInstance*,
                       YYTK::CCode*, int, YYTK::RValue*)>> g_hooks;

// ── Helpers ────────────────────────────────────────────────────────────────

static std::string timestamp() {
    using namespace std::chrono;
    auto now  = system_clock::now();
    auto tt   = system_clock::to_time_t(now);
    auto ms   = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&tt), "%H:%M:%S")
       << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return ss.str();
}

static std::string rvalStr(const YYTK::RValue& rv) {
    switch (rv.m_Kind) {
        case YYTK::VALUE_REAL:   return std::to_string(rv.m_Real);
        case YYTK::VALUE_INT32:  return std::to_string(rv.m_i32);
        case YYTK::VALUE_INT64:  return std::to_string(rv.m_i64);
        case YYTK::VALUE_BOOL:   return rv.m_i32 ? "true" : "false";
        case YYTK::VALUE_STRING: return '"' + rv.ToString() + '"';
        case YYTK::VALUE_UNDEFINED: return "<undefined>";
        case YYTK::VALUE_NULL:   return "<null>";
        default:
            return '<' + rv.GetKindName() + '>';
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
        ss << "args[" << i << "]=" << rvalStr(args[i]);
    }
    ss << ')';
    plog(probe, ss.str());
}

// ── YYTK CODE callback (single, routes to all registered hooks) ────────────

static bool CodeEventHandler(YYTK::FWCodeEvent& Event) {
    auto& args = Event.Arguments();
    YYTK::CCode* code = std::get<2>(args);
    if (!code) return false;

    const char* name = code->GetName();
    if (!name) return false;

    std::string scriptName(name);
    auto it = g_hooks.find(scriptName);
    if (it != g_hooks.end()) {
        it->second(
            std::get<0>(args),   // self
            std::get<1>(args),   // other
            code,
            std::get<3>(args),   // argc
            std::get<4>(args)    // argv
        );
    }
    return false; // don't block further processing
}

// ── Hook registration helper ───────────────────────────────────────────────

static bool codeCallbackRegistered = false;

static bool registerHook(
    Aurie::AurieModule* module,
    const std::string& scriptName,
    std::function<void(YYTK::CInstance*, YYTK::CInstance*,
                       YYTK::CCode*, int, YYTK::RValue*)> callback)
{
    // Register the global CODE event callback once
    if (!codeCallbackRegistered && g_yytk && module) {
        auto status = g_yytk->CreateCallback(
            module,
            YYTK::EVENT_OBJECT_CALL,
            reinterpret_cast<void*>(&CodeEventHandler),
            0
        );
        if (Aurie::AurieSuccess(status)) {
            codeCallbackRegistered = true;
            plog("INIT", "CODE event callback registered with YYTK");
        } else {
            plog("INIT", "Failed to register CODE event callback");
            return false;
        }
    }

    g_hooks[scriptName] = std::move(callback);
    plog("INIT", "Watching: " + scriptName);
    return true;
}

// ── Probe 1 — Dungeon vote table mutation ──────────────────────────────────
//
// What to look for in the log:
//   - "create_node_prototypes" fires once per dungeon floor load
//   - If "register_node@Anchor@Anchor" fires, args[0..3] reveal the API:
//     likely (node_type_string, biome_string, pool_string, weight_real)
//   - If register_node never fires but create_node_prototypes does, the vote
//     mutation API is unknown — run discover_scripts.py and grep for "node"

static void on_create_node_prototypes(YYTK::CInstance*, YYTK::CInstance*,
                                      YYTK::CCode*, int argc, YYTK::RValue* args) {
    plog("PROBE1-DUNGEON", ">>> create_node_prototypes fired (dungeon floor init)");
    plogArgs("PROBE1-DUNGEON", "create_node_prototypes", argc, args);
}

static void on_register_node(YYTK::CInstance*, YYTK::CInstance*,
                             YYTK::CCode*, int argc, YYTK::RValue* args) {
    plog("PROBE1-DUNGEON", "CONFIRMED: register_node@Anchor@Anchor called");
    plogArgs("PROBE1-DUNGEON", "register_node@Anchor@Anchor", argc, args);
    plog("PROBE1-DUNGEON", "  => arg layout: (node_type, biome, pool, weight)");
    plog("PROBE1-DUNGEON", "  => REPLACE RESOURCE-H002 stub with this call");
}

// ── Probe 2 — Surface node spawn script ────────────────────────────────────
//
// What to look for in the log:
//   - "write_node@Grid@Grid" fires each time a node is placed on the grid
//   - "attempt_to_write_object_node" — if this fires, it IS the right EFL call
//     (upstream caller of write_node). Note its arg layout for HijackedRoomBackend.
//   - If neither candidate fires but nodes appear, run discover_scripts.py
//     and grep for "write" or "object_node" to find the real name.

static void on_write_node(YYTK::CInstance*, YYTK::CInstance*,
                          YYTK::CCode*, int argc, YYTK::RValue* args) {
    plog("PROBE2-SURFACE", "write_node@Grid@Grid");
    plogArgs("PROBE2-SURFACE", "write_node@Grid@Grid", argc, args);
}

static void on_attempt_write(YYTK::CInstance*, YYTK::CInstance*,
                             YYTK::CCode*, int argc, YYTK::RValue* args) {
    plog("PROBE2-SURFACE", "CONFIRMED: attempt_to_write_object_node called");
    plogArgs("PROBE2-SURFACE", "attempt_to_write_object_node", argc, args);
    plog("PROBE2-SURFACE", "  => USE THIS as the spawn call in HijackedRoomBackend.cpp");
}

// ── Probe 3 — Crafting station open script ─────────────────────────────────
//
// What to look for in the log:
//   - One of the candidates fires when you open a crafting station
//   - args[0] likely contains station type or object ID
//   - Note WHICH candidate fires — that goes in bootstrap.cpp stepRegisterHooks()

static void on_craft_candidate(const std::string& name, int argc, YYTK::RValue* args) {
    plog("PROBE3-CRAFTING", "CONFIRMED: " + name + " fired");
    plogArgs("PROBE3-CRAFTING", name, argc, args);
    plog("PROBE3-CRAFTING", "  => USE THIS as the hook target in bootstrap.cpp");
    plog("PROBE3-CRAFTING", "  => REPLACE CRAFT-H001 stub with real recipe injection");
}

// ── Module entry / unload ──────────────────────────────────────────────────

EXPORTED Aurie::AurieStatus ModuleInitialize(
    IN Aurie::AurieModule* Module,
    IN const Aurie::fs::path& ModulePath
) {
    // Log to <game>/EFL/probe_output.txt
    auto logDir = ModulePath.parent_path().parent_path() / "EFL";
    fs::create_directories(logDir);
    g_logPath = logDir / "probe_output.txt";
    g_logFile.open(g_logPath, std::ios::out | std::ios::trunc);

    // Acquire YYTK
    auto status = Aurie::ObGetInterface(
        "YYTK_Main",
        reinterpret_cast<Aurie::AurieInterfaceBase*&>(g_yytk)
    );
    if (!Aurie::AurieSuccess(status) || !g_yytk) {
        return Aurie::AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    }

    plog("INIT", "=== EFL Runtime Probe loaded ===");
    plog("INIT", "Log: " + g_logPath.string());
    plog("INIT", "Actions needed:");
    plog("INIT", "  [1] Enter a dungeon floor   (Probe 1: vote mutation API)");
    plog("INIT", "  [2] Enter Farm/Narrows/Woods (Probe 2: surface spawn API)");
    plog("INIT", "  [3] Open a crafting station  (Probe 3: crafting hook name)");
    plog("INIT", "");

    // Probe 1 — dungeon vote mutation
    registerHook(Module, "gml_Script_create_node_prototypes", on_create_node_prototypes);
    registerHook(Module, "gml_Script_register_node@Anchor@Anchor", on_register_node);

    // Probe 2 — surface node spawn
    registerHook(Module, "gml_Script_write_node@Grid@Grid", on_write_node);
    registerHook(Module, "gml_Script_attempt_to_write_object_node", on_attempt_write);

    // Probe 3 — crafting station open (4 candidates; only one will fire)
    const std::vector<std::string> craftCandidates = {
        "gml_Script_spawn_crafting_menu",
        "gml_Script_open_crafting_station",
        "gml_Script_initialize_crafting@CraftingStation@CraftingStation",
        "gml_Script_start_crafting_session"
    };
    for (const auto& name : craftCandidates) {
        registerHook(Module, name,
            [name](YYTK::CInstance*, YYTK::CInstance*, YYTK::CCode*,
                   int argc, YYTK::RValue* args) {
                on_craft_candidate(name, argc, args);
            });
    }

    plog("INIT", "Watching " + std::to_string(g_hooks.size()) + " script names.");
    plog("INIT", "Play the game — probe data will appear below.");
    plog("INIT", "");

    return Aurie::AURIE_SUCCESS;
}

EXPORTED Aurie::AurieStatus ModuleUnload(
    IN Aurie::AurieModule* Module,
    IN const Aurie::fs::path& ModulePath
) {
    if (g_yytk && Module && codeCallbackRegistered) {
        g_yytk->RemoveCallback(Module, reinterpret_cast<void*>(&CodeEventHandler));
    }
    g_hooks.clear();

    plog("INIT", "");
    plog("INIT", "=== EFL Runtime Probe unloaded ===");
    plog("INIT", "Output: " + g_logPath.string());
    if (g_logFile.is_open()) g_logFile.close();

    return Aurie::AURIE_SUCCESS;
}
