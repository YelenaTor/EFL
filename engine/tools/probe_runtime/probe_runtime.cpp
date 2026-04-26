// EFL Runtime Probe v14 — YYC-compatible Aurie module
//
// v13 results (2026-04-24):
//   create_node_prototypes: one-shot fired 5s after startup — STARTUP call, not
//   dungeon entry. All 1565 entries have destructable=false. Two explanations:
//     (a) Startup call returns overworld objects only; dungeon entry call returns
//         a different/augmented array that includes dungeon rocks.
//     (b) All objects in the table have destructable=false; rocks use a different
//         field (check_pick, category_id, etc.) to identify them.
//   Strategy: skip cycle 1 (startup), dump on cycle 2 (dungeon entry). Also log
//   array length on every call so we can see if the count changes.
//
// ── Probe targets ──────────────────────────────────────────────────────────
//
//   PROBE A — dungeon-entry call dump                   [OPEN — needs mine access]
//     Skips cycle 1 (startup). On cycle 2 (dungeon entry):
//       - Logs array length to detect if count differs from startup.
//       - Scans for check_pick=true AND destructable=true entries (first 3 each).
//     Always logs array length on every cycle for comparison.
//
//   PROBE B — GridPrototypes candidate name scan         [runs at startup]
//     Still 0/12 — kept for reference.
//
// ── In-game action checklist ──────────────────────────────────────────────
//  [A]  Enter the mine / dungeon floor — cycle 2 fires the full dump
// ──────────────────────────────────────────────────────────────────────────
//
// DO NOT load alongside EFL.dll — they compete on the same hooks.

#include <Aurie/shared.hpp>
#include <YYToolkit/YYTK_Shared.hpp>

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

static YYTK::YYTKInterface* g_yytk = nullptr;
static Aurie::AurieModule*  g_mod  = nullptr;
static std::ofstream        g_log;
static std::mutex           g_logMux;

// PROBE A — fire once only
static std::atomic<int>  g_probeACycles{0};
static std::atomic<bool> g_selfDumped{false};
static constexpr int MAX_A_CYCLES = 3;   // log cycle fires up to this many times

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

static void dumpStructFields(const std::string& tag, const std::string& label,
                             YYTK::CInstance* self, const YYTK::RValue& rv) {
    {
        std::ostringstream ss;
        ss << label << ": kind=" << rv.GetKindName()
           << " ptr=0x" << std::hex
           << reinterpret_cast<uintptr_t>(rv.m_Object);
        plog(tag, ss.str());
    }

    // Guard: variable_struct_get_names only works on VALUE_OBJECT (struct/instance).
    // Calling it on an array or other type routes to variable_instance_get_names
    // internally, which expects a numeric ID and crashes if it gets an array.
    if (rv.m_Kind != YYTK::VALUE_OBJECT) {
        plog(tag, label + ": kind=" + std::string(rv.GetKindName()) +
             " — not a struct/object, skipping field dump");
        return;
    }

    if (!rv.m_Object) {
        plog(tag, label + ": null object pointer — cannot dump fields");
        return;
    }

    YYTK::RValue namesArr;
    auto s1 = g_yytk->CallBuiltinEx(
        namesArr, "variable_struct_get_names", self, nullptr, {rv});

    if (!Aurie::AurieSuccess(s1)) {
        plog(tag, label + ": variable_struct_get_names failed (status=" +
             std::to_string(static_cast<int>(s1)) + ")");
        plog(tag, label + ": use Cheat Engine / x64dbg to inspect struct ptr above");
        return;
    }

    YYTK::RValue lenRv;
    auto s2 = g_yytk->CallBuiltinEx(lenRv, "array_length", self, nullptr, {namesArr});

    int fieldCount = 0;
    if (Aurie::AurieSuccess(s2)) {
        if (lenRv.m_Kind == YYTK::VALUE_REAL)  fieldCount = static_cast<int>(lenRv.m_Real);
        if (lenRv.m_Kind == YYTK::VALUE_INT32) fieldCount = lenRv.m_i32;
    }

    plog(tag, label + ": field count = " + std::to_string(fieldCount));

    if (fieldCount <= 0 || fieldCount > 256) {
        plog(tag, label + ": field count out of expected range");
        return;
    }

    for (int i = 0; i < fieldCount && i < 64; ++i) {
        YYTK::RValue idxRv;
        idxRv.m_Kind = YYTK::VALUE_REAL;
        idxRv.m_Real = static_cast<double>(i);

        YYTK::RValue fieldNameRv;
        auto s3 = g_yytk->CallBuiltinEx(
            fieldNameRv, "array_get", self, nullptr, {namesArr, idxRv});
        if (!Aurie::AurieSuccess(s3)) continue;

        YYTK::RValue fieldVal;
        auto s4 = g_yytk->CallBuiltinEx(
            fieldVal, "variable_struct_get", self, nullptr, {rv, fieldNameRv});

        std::ostringstream ss;
        ss << "  ." << fieldNameRv.ToString()
           << " = " << (Aurie::AurieSuccess(s4) ? rvalStr(fieldVal) : "<get_failed>");
        plog(tag, ss.str());
    }

    if (fieldCount > 64)
        plog(tag, label + ": ... truncated at 64 fields");
}

// ── Prototype field scanner ───────────────────────────────────────────────
//
// Generic helper: scans the prototype array for entries where a named boolean
// field is true, dumping the first maxHits matches in full.
// fieldName must be a valid field present in every element (no guard needed —
// variable_struct_get returns undefined for missing fields, not a crash).
//
// The string RValue for fieldName is obtained by walking the names array of
// elem[0] so we never have to construct a GML string in C++.

static void scanForBoolField(const std::string& tag, YYTK::CInstance* ctx,
                             const YYTK::RValue& arr,
                             const std::string& fieldName, int maxHits = 3) {
    YYTK::RValue lenRv;
    if (!Aurie::AurieSuccess(g_yytk->CallBuiltinEx(lenRv, "array_length", ctx, nullptr, {arr}))) {
        plog(tag, fieldName + " scan: array_length failed"); return;
    }
    int len = 0;
    if (lenRv.m_Kind == YYTK::VALUE_REAL)  len = static_cast<int>(lenRv.m_Real);
    if (lenRv.m_Kind == YYTK::VALUE_INT32) len = lenRv.m_i32;

    YYTK::RValue idx0; idx0.m_Kind = YYTK::VALUE_REAL; idx0.m_Real = 0.0;
    YYTK::RValue elem0;
    if (!Aurie::AurieSuccess(g_yytk->CallBuiltinEx(elem0, "array_get", ctx, nullptr, {arr, idx0}))) {
        plog(tag, fieldName + " scan: array_get[0] failed"); return;
    }
    YYTK::RValue names0;
    if (!Aurie::AurieSuccess(g_yytk->CallBuiltinEx(names0, "variable_struct_get_names", ctx, nullptr, {elem0}))) {
        plog(tag, fieldName + " scan: variable_struct_get_names failed"); return;
    }
    YYTK::RValue nLenRv;
    g_yytk->CallBuiltinEx(nLenRv, "array_length", ctx, nullptr, {names0});
    int nfields = 0;
    if (nLenRv.m_Kind == YYTK::VALUE_REAL)  nfields = static_cast<int>(nLenRv.m_Real);
    if (nLenRv.m_Kind == YYTK::VALUE_INT32) nfields = nLenRv.m_i32;

    int fieldIdx = -1;
    YYTK::RValue cachedNameRv;
    for (int i = 0; i < nfields; ++i) {
        YYTK::RValue iRv; iRv.m_Kind = YYTK::VALUE_REAL; iRv.m_Real = static_cast<double>(i);
        YYTK::RValue nameRv;
        g_yytk->CallBuiltinEx(nameRv, "array_get", ctx, nullptr, {names0, iRv});
        if (nameRv.m_Kind == YYTK::VALUE_STRING && nameRv.ToString() == fieldName) {
            fieldIdx     = i;
            cachedNameRv = nameRv;
            break;
        }
    }
    if (fieldIdx < 0) {
        plog(tag, "'" + fieldName + "' not found in prototype fields — skipping");
        return;
    }
    plog(tag, "scanning for " + fieldName + "=true across " + std::to_string(len) + " prototypes...");

    int hits = 0;
    for (int i = 0; i < len && hits < maxHits; ++i) {
        YYTK::RValue iRv; iRv.m_Kind = YYTK::VALUE_REAL; iRv.m_Real = static_cast<double>(i);
        YYTK::RValue elem;
        if (!Aurie::AurieSuccess(g_yytk->CallBuiltinEx(elem, "array_get", ctx, nullptr, {arr, iRv})))
            continue;
        if (elem.m_Kind != YYTK::VALUE_OBJECT) continue;

        YYTK::RValue fval;
        if (!Aurie::AurieSuccess(g_yytk->CallBuiltinEx(fval, "variable_struct_get", ctx, nullptr, {elem, cachedNameRv})))
            continue;

        bool isTrue = false;
        if (fval.m_Kind == YYTK::VALUE_BOOL || fval.m_Kind == YYTK::VALUE_INT32)
            isTrue = (fval.m_i32 != 0);
        else if (fval.m_Kind == YYTK::VALUE_REAL)
            isTrue = (fval.m_Real != 0.0);

        if (!isTrue) continue;

        plog(tag, fieldName + "=true at prototype[" + std::to_string(i) + "]");
        dumpStructFields(tag, "prototype[" + std::to_string(i) + "]", ctx, elem);
        ++hits;
    }

    if (hits == 0)
        plog(tag, "no " + fieldName + "=true entries found in " + std::to_string(len) + " prototypes");
    else
        plog(tag, fieldName + " scan done — found " + std::to_string(hits) + " entries");
}

// Logs the array length and runs both destructable and check_pick scans.
static void runDungeonEntryDump(const std::string& tag, YYTK::CInstance* ctx,
                                const YYTK::RValue& arr) {
    YYTK::RValue lenRv;
    if (!Aurie::AurieSuccess(g_yytk->CallBuiltinEx(lenRv, "array_length", ctx, nullptr, {arr}))) {
        plog(tag, "array_length failed"); return;
    }
    int len = 0;
    if (lenRv.m_Kind == YYTK::VALUE_REAL)  len = static_cast<int>(lenRv.m_Real);
    if (lenRv.m_Kind == YYTK::VALUE_INT32) len = lenRv.m_i32;
    plog(tag, "array length on this call = " + std::to_string(len));

    scanForBoolField(tag, ctx, arr, "destructable");
    scanForBoolField(tag, ctx, arr, "check_pick");
}

// ── Helpers — instance-as-struct RValue ───────────────────────────────────

static YYTK::RValue instanceRv(YYTK::CInstance* inst) {
    YYTK::RValue rv;
    rv.m_Kind   = YYTK::VALUE_OBJECT;
    rv.m_Object = reinterpret_cast<YYTK::YYObjectBase*>(inst);
    return rv;
}

// ── PROBE A — self + other dump, global keyword scan ─────────────────────
//
// v9 result: self post-call = {from_game: false} — a 1-field config struct,
// NOT the GridPrototypes table. Dumping `o` (other) and enumerating globals
// with keyword filtering to locate the real table.
//
// Global scan runs inside the hook (not at init) so we have a valid
// CInstance* context for CallBuiltinEx. Keyword-filtered to avoid log flood.
//
// g_selfDumped gates the entire block to exactly one execution.

static YYTK::PFUNC_YYGMLScript g_orig_create_node_prototypes = nullptr;
static YYTK::RValue& Hook_create_node_prototypes(
        YYTK::CInstance* s, YYTK::CInstance* o,
        YYTK::RValue& r, int argc, YYTK::RValue* args[]) {

    int cycle = g_probeACycles.fetch_add(1) + 1;
    if (cycle <= MAX_A_CYCLES)
        plog("PROBEA", "create_node_prototypes FIRED (cycle " +
             std::to_string(cycle) + "/" + std::to_string(MAX_A_CYCLES) + ")");
    else if (cycle == MAX_A_CYCLES + 1)
        plog("PROBEA", "create_node_prototypes: cycle cap — further fires silent");

    // Let FoM build the table.
    auto& ret = g_orig_create_node_prototypes(s, o, r, argc, args);

    // Snapshot r before any GML builtins — r is a reference into the VM return
    // slot and will be overwritten by CallBuiltinEx (confirmed crash v11).
    YYTK::RValue retvalSnapshot = r;

    // Log array length every cycle so we can compare startup vs dungeon entry.
    if (cycle <= MAX_A_CYCLES && retvalSnapshot.m_Kind == YYTK::VALUE_ARRAY) {
        YYTK::RValue lenRv;
        if (Aurie::AurieSuccess(g_yytk->CallBuiltinEx(lenRv, "array_length", s, nullptr, {retvalSnapshot}))) {
            int len = 0;
            if (lenRv.m_Kind == YYTK::VALUE_REAL)  len = static_cast<int>(lenRv.m_Real);
            if (lenRv.m_Kind == YYTK::VALUE_INT32) len = lenRv.m_i32;
            plog("PROBEA", "cycle " + std::to_string(cycle) + ": array length = " + std::to_string(len));
        }
    }

    // Full dump fires exactly once, on cycle 2.
    // Cycle 1 = startup call (all overworld objects, all destructable=false).
    // Cycle 2 = first dungeon entry call — the one we want.
    if (cycle == 2 && !g_selfDumped.exchange(true)) {
        plog("PROBEA", "--- dungeon entry dump BEGIN ---");
        if (retvalSnapshot.m_Kind == YYTK::VALUE_ARRAY)
            runDungeonEntryDump("PROBEA", s, retvalSnapshot);
        else
            plog("PROBEA", "retval kind=" + std::string(retvalSnapshot.GetKindName()) + " — not an array");
        plog("PROBEA", "--- dungeon entry dump END ---");
        plog("PROBEA", "one-shot complete — hook passes through silently from here");
    }

    return ret;
}

// ── PROBE B — GridPrototypes candidate name scan ──────────────────────────
//
// Tries a fixed set of plausible registration method names at startup.
// Logs FOUND/NOT FOUND for each so we can identify the public API for
// vote registration without a full script enumeration loop.

static void runCandidateScan() {
    static const std::vector<std::string> candidates = {
        "gml_Script_register_node@GridPrototypes@GridPrototypes",
        "gml_Script_add_node@GridPrototypes@GridPrototypes",
        "gml_Script_add_vote@GridPrototypes@GridPrototypes",
        "gml_Script_vote@GridPrototypes@GridPrototypes",
        "gml_Script_build@GridPrototypes@GridPrototypes",
        "gml_Script_initialize@GridPrototypes@GridPrototypes",
        "gml_Script_add@GridPrototypes@GridPrototypes",
        "gml_Script_set_node@GridPrototypes@GridPrototypes",
        "gml_Script_add_prototype@GridPrototypes@GridPrototypes",
        "gml_Script_register_node@Grid@Grid",
        "gml_Script_add_node@Grid@Grid",
        "gml_Script_node_vote@Grid@Grid",
    };

    plog("PROBEB", "=== GridPrototypes candidate name scan ===");
    int found = 0;
    for (const auto& name : candidates) {
        PVOID raw = nullptr;
        auto s = g_yytk->GetNamedRoutinePointer(name.c_str(), &raw);
        if (Aurie::AurieSuccess(s) && raw) {
            plog("PROBEB", "FOUND:     " + name);
            ++found;
        } else {
            plog("PROBEB", "not found: " + name);
        }
    }
    plog("PROBEB", "scan complete — " + std::to_string(found) + "/" +
         std::to_string(candidates.size()) + " candidates found");
}

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

    std::ostringstream addrSs;
    addrSs << "HOOKED: " << hd.scriptName
           << "  fn=0x" << std::hex
           << reinterpret_cast<uintptr_t>(cs->m_Functions->m_ScriptFunction);
    plog("HOOK", addrSs.str());
}

// ── Module entry / unload ──────────────────────────────────────────────────

EXPORTED Aurie::AurieStatus ModuleInitialize(
    IN Aurie::AurieModule* Module,
    IN const Aurie::fs::path& ModulePath
) {
    g_mod = Module;

    auto logDir  = ModulePath.parent_path().parent_path() / "EFL";
    fs::create_directories(logDir);
    auto logPath = logDir / "probe_output.txt";
    g_log.open(logPath, std::ios::out | std::ios::trunc);

    auto status = Aurie::ObGetInterface(
        "YYTK_Main",
        reinterpret_cast<Aurie::AurieInterfaceBase*&>(g_yytk)
    );
    if (!Aurie::AurieSuccess(status) || !g_yytk)
        return Aurie::AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;

    plog("INIT", "=== EFL Runtime Probe v14 ===");
    plog("INIT", "Output: " + logPath.string());
    plog("INIT", "");
    plog("INIT", "Active probes:");
    plog("INIT", "  [A] dungeon-entry dump (cycle 2) — enter mine to trigger");
    plog("INIT", "  [B] Candidate name scan     — runs now at startup");
    plog("INIT", "");

    runCandidateScan();

    plog("INIT", "");
    plog("HOOK", "=== Hook registration ===");

    const std::vector<HookDef> hooks = {
        {"gml_Script_create_node_prototypes",
            Hook_create_node_prototypes, &g_orig_create_node_prototypes},
    };

    for (const auto& h : hooks) installHook(h);

    plog("INIT", "");
    plog("INIT", "Ready. Enter the mine/dungeon to trigger Probe A (fires once, then silent).");
    plog("INIT", "");

    return Aurie::AURIE_SUCCESS;
}

EXPORTED Aurie::AurieStatus ModuleUnload(
    IN Aurie::AurieModule* Module,
    IN const Aurie::fs::path& ModulePath
) {
    plog("INIT", "=== EFL Runtime Probe v14 unloaded ===");
    if (g_log.is_open()) g_log.close();
    return Aurie::AURIE_SUCCESS;
}
