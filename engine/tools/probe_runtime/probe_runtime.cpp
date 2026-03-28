// EFL Runtime Probe v2 — YYC-compatible Aurie module
//
// FoM uses the YoYo Compiler (YYC). Scripts are native C++ functions; YYTK's
// EVENT_OBJECT_CALL (Code_Execute hook) never fires for them. This version
// uses two strategies that DO work for YYC:
//
//   Phase 1 — Discovery scan: iterate GetScriptData(0..N) and log every
//             script whose name contains a probe keyword. This tells us
//             the REAL FoM names regardless of what we guessed.
//
//   Phase 2 — Live hooks: for each candidate whose name resolves via
//             GetNamedRoutinePointer, patch its native function pointer
//             with MmCreateHook. When the script fires, we log its args.
//
// ── Actions to perform — all reachable from a fresh new game ──────────────
//  Probe 1+2 (nodes/spawn): Start new game, enter Farm (day 1)
//  Probe 3   (crafting):    Open any crafting station in town
// ──────────────────────────────────────────────────────────────────────────
//
// DO NOT load alongside EFL.dll — they compete on the same hooks.

#include <Aurie/shared.hpp>
#include <YYToolkit/YYTK_Shared.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ── Globals ────────────────────────────────────────────────────────────────

static YYTK::YYTKInterface*  g_yytk  = nullptr;
static Aurie::AurieModule*   g_mod   = nullptr;
static std::ofstream         g_log;
static std::mutex            g_logMux;

// Keywords that identify probe-relevant scripts during the discovery scan
static const std::vector<std::string> KEYWORDS = {
    "node", "craft", "recipe", "spawn", "register", "anchor",
    "prototype", "station", "forge", "biome", "vote", "resource",
    "write", "place", "grid"
};

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

static void plog(const std::string& tag, const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_logMux);
    if (g_log.is_open()) {
        g_log << '[' << timestamp() << "] [" << tag << "] " << msg << '\n';
        g_log.flush();
    }
}

// PFUNC_YYGMLScript passes args as RValue*[] (array of pointers), not RValue[].
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

static void plogArgs(const std::string& tag, const std::string& name,
                     int argc, YYTK::RValue* args[]) {
    std::ostringstream ss;
    ss << name << '(';
    for (int i = 0; i < argc && i < 10; ++i) {
        if (i) ss << ", ";
        ss << '[' << i << "]=";
        if (args[i]) ss << rvalStr(*args[i]);
        else         ss << "<null>";
    }
    if (argc > 10) ss << " ...+" << (argc - 10) << " more";
    ss << ')';
    plog(tag, ss.str());
}

// ── YYC script hook trampolines ────────────────────────────────────────────
//
// Each script hook has:
//   - A trampoline pointer (filled by MmCreateHook, used to call original)
//   - A static hook function with PFUNC_YYGMLScript signature

// PFUNC_YYGMLScript = RValue& (*)(CInstance*, CInstance*, RValue&, int, RValue*[])

#define DEFINE_HOOK(NICK, LABEL, EXTRA)                                       \
    static YYTK::PFUNC_YYGMLScript g_orig_##NICK = nullptr;                  \
    static YYTK::RValue& Hook_##NICK(                                         \
            YYTK::CInstance* s, YYTK::CInstance* o,                           \
            YYTK::RValue& r, int argc, YYTK::RValue* args[]) {                \
        plog(LABEL, "FIRED: " #NICK);                                         \
        plogArgs(LABEL, #NICK, argc, args);                                   \
        EXTRA                                                                  \
        return g_orig_##NICK(s, o, r, argc, args);                            \
    }

DEFINE_HOOK(create_node_prototypes, "PROBE1",
    plog("PROBE1", ">>> This IS the node-prototype script. Check discovery log for");
    plog("PROBE1", "    any *register*/*vote*/*anchor* script that fires AFTER this.");
)

DEFINE_HOOK(register_node, "PROBE1",
    plog("PROBE1", "CONFIRMED vote-table mutation API — use this for RESOURCE-H002");
)

DEFINE_HOOK(write_node, "PROBE2", )

DEFINE_HOOK(attempt_to_write_object_node, "PROBE2",
    plog("PROBE2", "CONFIRMED surface spawn API — use in HijackedRoomBackend");
)

DEFINE_HOOK(spawn_crafting_menu, "PROBE3",
    plog("PROBE3", "CONFIRMED: spawn_crafting_menu — use for CRAFT-H001");
)

DEFINE_HOOK(open_crafting_station, "PROBE3",
    plog("PROBE3", "CONFIRMED: open_crafting_station — use for CRAFT-H001");
)

DEFINE_HOOK(initialize_crafting, "PROBE3",
    plog("PROBE3", "CONFIRMED: initialize_crafting — use for CRAFT-H001");
)

DEFINE_HOOK(start_crafting_session, "PROBE3",
    plog("PROBE3", "CONFIRMED: start_crafting_session — use for CRAFT-H001");
)

// ── Hook installation ──────────────────────────────────────────────────────

struct HookDef {
    const char*              scriptName;
    YYTK::PFUNC_YYGMLScript  hookFn;
    YYTK::PFUNC_YYGMLScript* trampolineDst;
};

static void installHook(const HookDef& hd) {
    PVOID rawPtr = nullptr;
    auto s = g_yytk->GetNamedRoutinePointer(hd.scriptName, &rawPtr);
    if (!Aurie::AurieSuccess(s) || !rawPtr) {
        plog("HOOK", std::string("NOT FOUND: ") + hd.scriptName);
        return;
    }

    auto* cs = reinterpret_cast<YYTK::CScript*>(rawPtr);
    if (!cs->m_Functions || !cs->m_Functions->m_ScriptFunction) {
        plog("HOOK", std::string("NO FUNCTION PTR: ") + hd.scriptName);
        return;
    }

    PVOID trampRaw = nullptr;
    auto hs = Aurie::MmCreateHook(
        g_mod,
        hd.scriptName,
        reinterpret_cast<PVOID>(cs->m_Functions->m_ScriptFunction),
        reinterpret_cast<PVOID>(hd.hookFn),
        &trampRaw
    );

    if (!Aurie::AurieSuccess(hs)) {
        plog("HOOK", std::string("HOOK FAILED (") + std::to_string(hs) + "): " + hd.scriptName);
        return;
    }

    *hd.trampolineDst = reinterpret_cast<YYTK::PFUNC_YYGMLScript>(trampRaw);
    plog("HOOK", std::string("HOOKED: ") + hd.scriptName);
}

// ── Phase 1 — Discovery scan ───────────────────────────────────────────────
//
// Iterates GetScriptData(0..N) and logs every script name matching a keyword.
// This runs at ModuleInitialize time (before the game creates any objects),
// so it's safe to call from the main thread during late-init.
// Result: a dictionary of all FoM scripts with probe-relevant names.

static void runDiscoveryScan() {
    plog("DISCOVER", "=== Scanning FoM script table for probe keywords ===");
    int found = 0;
    int consecMiss = 0;

    for (int i = 0; i < 20000 && consecMiss < 500; ++i) {
        YYTK::CScript* cs = nullptr;
        auto s = g_yytk->GetScriptData(i, cs);

        if (!Aurie::AurieSuccess(s)) {
            ++consecMiss;
            continue;
        }
        consecMiss = 0;
        if (!cs) continue;

        // Prefer CCode name (bytecode path, may be null in YYC), fall back to
        // YYGMLFuncs name.
        const char* name = nullptr;
        if (cs->m_Code)      name = cs->m_Code->GetName();
        if (!name && cs->m_Functions) name = cs->m_Functions->m_Name;
        if (!name) continue;

        std::string sname(name);
        if (sname.rfind("gml_Script_", 0) != 0) continue; // only user scripts

        std::string lower = sname;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        for (const auto& kw : KEYWORDS) {
            if (lower.find(kw) != std::string::npos) {
                plog("DISCOVER", "[" + std::to_string(i) + "] " + sname);
                ++found;
                break;
            }
        }
    }

    plog("DISCOVER", "Scan complete. Keyword-matching scripts found: " + std::to_string(found));
    if (found == 0) {
        plog("DISCOVER", "WARNING: zero matches — m_Code may be null in YYC mode.");
        plog("DISCOVER", "Check HOOK lines below; if GetNamedRoutinePointer hits, hooks work.");
    }
}

// ── Module entry / unload ──────────────────────────────────────────────────

EXPORTED Aurie::AurieStatus ModuleInitialize(
    IN Aurie::AurieModule* Module,
    IN const Aurie::fs::path& ModulePath
) {
    g_mod = Module;

    auto logDir = ModulePath.parent_path().parent_path() / "EFL";
    fs::create_directories(logDir);
    auto logPath = logDir / "probe_output.txt";
    g_log.open(logPath, std::ios::out | std::ios::trunc);

    auto status = Aurie::ObGetInterface(
        "YYTK_Main",
        reinterpret_cast<Aurie::AurieInterfaceBase*&>(g_yytk)
    );
    if (!Aurie::AurieSuccess(status) || !g_yytk)
        return Aurie::AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;

    plog("INIT", "=== EFL Runtime Probe v2 (YYC-compatible) ===");
    plog("INIT", "Output: " + logPath.string());

    // Phase 1: enumerate script table → log matching names
    runDiscoveryScan();

    // Phase 2: hook each candidate by exact name
    plog("HOOK", "=== Hook registration (exact name lookup) ===");

    const std::vector<HookDef> hooks = {
        // Probe 1 — node prototype creation + vote-table mutation
        {"gml_Script_create_node_prototypes",
            Hook_create_node_prototypes, &g_orig_create_node_prototypes},
        {"gml_Script_register_node@Anchor@Anchor",
            Hook_register_node, &g_orig_register_node},

        // Probe 2 — surface node spawn
        {"gml_Script_write_node@Grid@Grid",
            Hook_write_node, &g_orig_write_node},
        {"gml_Script_attempt_to_write_object_node",
            Hook_attempt_to_write_object_node, &g_orig_attempt_to_write_object_node},

        // Probe 3 — crafting station open (4 candidates)
        {"gml_Script_spawn_crafting_menu",
            Hook_spawn_crafting_menu, &g_orig_spawn_crafting_menu},
        {"gml_Script_open_crafting_station",
            Hook_open_crafting_station, &g_orig_open_crafting_station},
        {"gml_Script_initialize_crafting@CraftingStation@CraftingStation",
            Hook_initialize_crafting, &g_orig_initialize_crafting},
        {"gml_Script_start_crafting_session",
            Hook_start_crafting_session, &g_orig_start_crafting_session},
    };

    int hookedCount = 0;
    for (const auto& h : hooks) installHook(h);

    plog("INIT", "");
    plog("INIT", "Probe ready. Perform in-game actions:");
    plog("INIT", "  [1+2] Start new game → enter Farm (day 1)");
    plog("INIT", "  [3]   Open a crafting station in town");
    plog("INIT", "Then quit and check this log.");
    plog("INIT", "");

    return Aurie::AURIE_SUCCESS;
}

EXPORTED Aurie::AurieStatus ModuleUnload(
    IN Aurie::AurieModule* Module,
    IN const Aurie::fs::path& ModulePath
) {
    // MmCreateHook detours are automatically removed by Aurie when the module unloads.
    plog("INIT", "=== EFL Runtime Probe unloaded ===");
    if (g_log.is_open()) g_log.close();
    return Aurie::AURIE_SUCCESS;
}
