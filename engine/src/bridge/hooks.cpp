#include "efl/bridge/hooks.h"

#include <cassert>

#ifndef EFL_STUB_SDK
#include "efl/ipc/pipe_writer.h"
#endif

namespace efl::bridge {

#ifdef EFL_STUB_SDK

// ── Stub implementation (for tests) ─────────────────────────────────────────

HookRegistry::HookRegistry(YYTK::YYTKInterface* yytk)
    : yytk_(yytk) {}

HookRegistry::~HookRegistry() {
    removeAll();
}

bool HookRegistry::registerHook(const std::string& name, const std::string& target,
                                 YYTK::CodeCallback callback) {
    if (!yytk_) return false;
    if (hooks_.count(name)) return false;

    YYTK::HookHandle handle = yytk_->CreateHook(target, std::move(callback));
    hooks_[name] = HookEntry{target, handle};
    return true;
}

void HookRegistry::removeHook(const std::string& name) {
    auto it = hooks_.find(name);
    if (it == hooks_.end()) return;

    if (yytk_ && it->second.handle) {
        yytk_->RemoveHook(it->second.handle);
    }
    hooks_.erase(it);
}

void HookRegistry::removeAll() {
    if (yytk_) {
        for (auto& [name, entry] : hooks_) {
            if (entry.handle) {
                yytk_->RemoveHook(entry.handle);
            }
        }
    }
    hooks_.clear();
}

bool HookRegistry::isRegistered(const std::string& name) const {
    return hooks_.count(name) > 0;
}

size_t HookRegistry::count() const {
    return hooks_.size();
}

#else // Real SDK

// ── YYC shim slot system ─────────────────────────────────────────────────────
//
// Fields of Mistria uses the YoYo Compiler (YYC), so GML scripts are native
// C++ functions. EVENT_OBJECT_CALL (Code_Execute hook) never fires for them.
// Instead, registerScriptHook installs a per-script MmCreateHook detour using
// a pre-generated static shim that dispatches to the stored CodeEventCallback.
//
// The shim must be a plain C function pointer (no closure). We pre-generate 32
// slots at compile time; each shim hardcodes its own index into g_yycSlots.

struct YycSlot {
    YYTK::PFUNC_YYGMLScript trampoline = nullptr;
    CodeEventCallback       callback;
};

static constexpr int MAX_YYC_SLOTS = 32;
static YycSlot g_yycSlots[MAX_YYC_SLOTS];
static int     g_yycSlotCount = 0;

// Each shim: invoke the stored callback, then tail-call the trampoline.
// Callbacks in EFL ignore CCode*/argc/args (they observe via capture).
// PFUNC_YYGMLScript = RValue& (*)(CInstance*, CInstance*, RValue&, int, RValue*[])
#define YYC_SHIM(N)                                                            \
    static YYTK::RValue& YycShim_##N(                                          \
            YYTK::CInstance* s, YYTK::CInstance* o,                            \
            YYTK::RValue& r,    int argc, YYTK::RValue** args) {               \
        auto& slot = g_yycSlots[N];                                            \
        if (slot.callback) slot.callback(s, o, nullptr, 0, nullptr);           \
        if (slot.trampoline) return slot.trampoline(s, o, r, argc, args);      \
        return r;                                                               \
    }
YYC_SHIM(0)  YYC_SHIM(1)  YYC_SHIM(2)  YYC_SHIM(3)
YYC_SHIM(4)  YYC_SHIM(5)  YYC_SHIM(6)  YYC_SHIM(7)
YYC_SHIM(8)  YYC_SHIM(9)  YYC_SHIM(10) YYC_SHIM(11)
YYC_SHIM(12) YYC_SHIM(13) YYC_SHIM(14) YYC_SHIM(15)
YYC_SHIM(16) YYC_SHIM(17) YYC_SHIM(18) YYC_SHIM(19)
YYC_SHIM(20) YYC_SHIM(21) YYC_SHIM(22) YYC_SHIM(23)
YYC_SHIM(24) YYC_SHIM(25) YYC_SHIM(26) YYC_SHIM(27)
YYC_SHIM(28) YYC_SHIM(29) YYC_SHIM(30) YYC_SHIM(31)

static YYTK::PFUNC_YYGMLScript g_yycShimFns[MAX_YYC_SLOTS] = {
    YycShim_0,  YycShim_1,  YycShim_2,  YycShim_3,
    YycShim_4,  YycShim_5,  YycShim_6,  YycShim_7,
    YycShim_8,  YycShim_9,  YycShim_10, YycShim_11,
    YycShim_12, YycShim_13, YycShim_14, YycShim_15,
    YycShim_16, YycShim_17, YycShim_18, YycShim_19,
    YycShim_20, YycShim_21, YycShim_22, YycShim_23,
    YycShim_24, YycShim_25, YycShim_26, YycShim_27,
    YycShim_28, YycShim_29, YycShim_30, YycShim_31,
};

// ── Real implementation ─────────────────────────────────────────────────────

// Global pointer for static callback dispatch (VM/CODE_EXECUTE path only)
static HookRegistry* g_hookRegistry = nullptr;

// Static YYTK callback for EVENT_OBJECT_CALL
static bool CodeEventHandler(YYTK::FWCodeEvent& Event) {
    if (!g_hookRegistry) return false;

    auto& eventArgs = Event.Arguments();
    g_hookRegistry->dispatchCodeEvent(
        std::get<0>(eventArgs),  // self
        std::get<1>(eventArgs),  // other
        std::get<2>(eventArgs),  // code
        std::get<3>(eventArgs),  // argc
        std::get<4>(eventArgs)   // args
    );
    return false;
}

// Static YYTK callback for EVENT_FRAME
static void FrameEventHandler(YYTK::FWFrame& Event) {
    if (!g_hookRegistry) return;
    g_hookRegistry->dispatchFrameEvent();
}

HookRegistry::HookRegistry(Aurie::AurieModule* module, YYTK::YYTKInterface* yytk)
    : module_(module), yytk_(yytk) {
    assert(!g_hookRegistry && "Only one HookRegistry instance may exist");
    g_hookRegistry = this;
}

void HookRegistry::setPipeWriter(efl::PipeWriter* pipe) {
    pipe_ = pipe;
}

HookRegistry::~HookRegistry() {
    removeAll();
    if (g_hookRegistry == this) {
        g_hookRegistry = nullptr;
    }
}

bool HookRegistry::registerScriptHook(const std::string& name, const std::string& target,
                                       CodeEventCallback callback) {
    if (hooks_.count(name)) return false;

    // ── YYC path (e.g. Fields of Mistria) ────────────────────────────────────
    // Try to resolve the target as a native compiled function. If it has a
    // ScriptFunction pointer, it's YYC-compiled; install a MmCreateHook detour
    // using a pre-generated shim slot instead of the global CODE callback.
    if (yytk_ && module_) {
        PVOID rawPtr = nullptr;
        auto lookupStatus = yytk_->GetNamedRoutinePointer(target.c_str(), &rawPtr);
        if (Aurie::AurieSuccess(lookupStatus) && rawPtr) {
            auto* cs = reinterpret_cast<YYTK::CScript*>(rawPtr);
            if (cs->m_Functions && cs->m_Functions->m_ScriptFunction) {
                if (g_yycSlotCount >= MAX_YYC_SLOTS) {
                    if (pipe_) {
                        pipe_->write("hook.error", nlohmann::json{
                            {"name", name}, {"error", "YYC slot table full"}});
                    }
                    return false;
                }
                int slotIdx = g_yycSlotCount++;
                PVOID trampRaw = nullptr;
                Aurie::AurieStatus hookStatus = Aurie::MmCreateHook(
                    module_,
                    name,   // EFL hook name as the Aurie hook ID
                    reinterpret_cast<PVOID>(cs->m_Functions->m_ScriptFunction),
                    reinterpret_cast<PVOID>(g_yycShimFns[slotIdx]),
                    &trampRaw
                );
                if (!Aurie::AurieSuccess(hookStatus)) {
                    --g_yycSlotCount; // reclaim slot
                    if (pipe_) {
                        pipe_->write("hook.error", nlohmann::json{
                            {"name", name}, {"error", "MmCreateHook failed"},
                            {"status", static_cast<int>(hookStatus)}});
                    }
                    return false;
                }
                g_yycSlots[slotIdx].trampoline =
                    reinterpret_cast<YYTK::PFUNC_YYGMLScript>(trampRaw);
                g_yycSlots[slotIdx].callback = std::move(callback);

                hooks_[name] = HookEntry{
                    .kind = HookKind::YycScript,
                    .target = target,
                    .scriptCb = {},
                    .frameCb = {},
                    .yycSlotIndex = slotIdx
                };

                if (pipe_) {
                    pipe_->write("hook.registered", nlohmann::json{
                        {"name", name}, {"target", target}, {"kind", "yyc_script"},
                        {"slot", slotIdx}});
                }
                return true;
            }
        }
    }

    // ── VM / CODE_EXECUTE path (fallback for non-YYC games) ──────────────────
    // Ensure the global CODE callback is registered with YYTK.
    // reinterpret_cast: ISO UB but standard YYTK/Windows callback pattern.
    // MSVC guarantees this works. Do not "fix" to std::bit_cast or similar.
    if (!codeCallbackRegistered_ && yytk_ && module_) {
        Aurie::AurieStatus status = yytk_->CreateCallback(
            module_,
            YYTK::EVENT_OBJECT_CALL,
            reinterpret_cast<void*>(&CodeEventHandler),
            0
        );
        if (Aurie::AurieSuccess(status)) {
            codeCallbackRegistered_ = true;
        }
    }

    hooks_[name] = HookEntry{
        .kind = HookKind::Script,
        .target = target,
        .scriptCb = std::move(callback),
        .frameCb = {}
    };

    if (pipe_) {
        pipe_->write("hook.registered", nlohmann::json{
            {"name", name}, {"target", target}, {"kind", "script"}});
    }
    return true;
}

bool HookRegistry::registerFrameCallback(const std::string& name, FrameCallback callback) {
    if (hooks_.count(name)) return false;

    // Ensure the global FRAME callback is registered with YYTK
    if (!frameCallbackRegistered_ && yytk_ && module_) {
        Aurie::AurieStatus status = yytk_->CreateCallback(
            module_,
            YYTK::EVENT_FRAME,
            reinterpret_cast<void*>(&FrameEventHandler),
            0
        );
        if (Aurie::AurieSuccess(status)) {
            frameCallbackRegistered_ = true;
        }
    }

    hooks_[name] = HookEntry{
        .kind = HookKind::Frame,
        .target = {},
        .scriptCb = {},
        .frameCb = std::move(callback)
    };

    if (pipe_) {
        pipe_->write("hook.registered", nlohmann::json{
            {"name", name}, {"kind", "frame"}});
    }
    return true;
}

bool HookRegistry::registerDetour(const std::string& name, void* source, void* dest,
                                    void** trampoline) {
    if (hooks_.count(name)) return false;
    if (!module_) return false;

    Aurie::AurieStatus status = Aurie::MmCreateHook(
        module_, name, source, dest, trampoline
    );

    if (!Aurie::AurieSuccess(status)) return false;

    hooks_[name] = HookEntry{
        .kind = HookKind::Detour,
        .target = name,
        .scriptCb = {},
        .frameCb = {}
    };

    if (pipe_) {
        pipe_->write("hook.registered", nlohmann::json{
            {"name", name}, {"kind", "detour"}});
    }
    return true;
}

void HookRegistry::removeHook(const std::string& name) {
    auto it = hooks_.find(name);
    if (it == hooks_.end()) return;

    if (it->second.kind == HookKind::Detour && module_) {
        Aurie::MmRemoveHook(module_, name);
    }

    hooks_.erase(it);
}

// NOTE: removeAll() must only be called from the GM main thread. If YYTK fires
// a pending callback between hooks_.clear() and RemoveCallback, dispatchCodeEvent
// sees an empty map and no-ops safely, but the ordering assumption depends on
// single-threaded execution.
void HookRegistry::removeAll() {
    for (auto& [name, entry] : hooks_) {
        if ((entry.kind == HookKind::Detour ||
             entry.kind == HookKind::YycScript) && module_) {
            Aurie::MmRemoveHook(module_, name);
        }
    }
    // Reset YYC slot table so slots can be reused if removeAll is called mid-session.
    for (int i = 0; i < g_yycSlotCount; ++i) {
        g_yycSlots[i] = YycSlot{};
    }
    g_yycSlotCount = 0;
    hooks_.clear();

    if (yytk_ && module_) {
        if (codeCallbackRegistered_) {
            yytk_->RemoveCallback(module_, reinterpret_cast<void*>(&CodeEventHandler));
            codeCallbackRegistered_ = false;
        }
        if (frameCallbackRegistered_) {
            yytk_->RemoveCallback(module_, reinterpret_cast<void*>(&FrameEventHandler));
            frameCallbackRegistered_ = false;
        }
    }
}

bool HookRegistry::isRegistered(const std::string& name) const {
    return hooks_.count(name) > 0;
}

size_t HookRegistry::count() const {
    return hooks_.size();
}

void HookRegistry::dispatchCodeEvent(YYTK::CInstance* self, YYTK::CInstance* other,
                                      YYTK::CCode* code, int argc, YYTK::RValue* args) {
    if (!code) return;
    const char* codeName = code->GetName();
    if (!codeName) return;

    std::string name(codeName);
    for (auto& [hookName, entry] : hooks_) {
        // YycScript hooks fire via MmCreateHook shims, not CODE_EXECUTE.
        if (entry.kind == HookKind::Script && entry.target == name && entry.scriptCb) {
            try {
                entry.scriptCb(self, other, code, argc, args);
            } catch (const std::exception& ex) {
                if (pipe_) {
                    pipe_->write("hook.error", nlohmann::json{
                        {"name", hookName}, {"target", name}, {"error", ex.what()}});
                }
                continue;
            } catch (...) {
                if (pipe_) {
                    pipe_->write("hook.error", nlohmann::json{
                        {"name", hookName}, {"target", name}, {"error", "unknown exception"}});
                }
                continue;
            }

            if (pipe_) {
                pipe_->write("hook.fired", nlohmann::json{
                    {"name", hookName}, {"target", name}});
            }
        }
    }
}

void HookRegistry::dispatchFrameEvent() {
    ++frameCount_;
    for (auto& [hookName, entry] : hooks_) {
        if (entry.kind == HookKind::Frame && entry.frameCb) {
            try {
                entry.frameCb();
            } catch (const std::exception& ex) {
                if (pipe_) {
                    pipe_->write("hook.error", nlohmann::json{
                        {"name", hookName}, {"error", ex.what()}});
                }
            } catch (...) {
                if (pipe_) {
                    pipe_->write("hook.error", nlohmann::json{
                        {"name", hookName}, {"error", "unknown exception"}});
                }
            }
        }
    }

    // Emit throttled hook.fired for frame hooks so TUI has visibility (every 60 frames)
    if (pipe_ && (frameCount_ % 60 == 0)) {
        size_t frameHookCount = 0;
        for (const auto& [hookName, entry] : hooks_) {
            if (entry.kind == HookKind::Frame) ++frameHookCount;
        }
        pipe_->write("hook.fired", nlohmann::json{
            {"name", "frame_hooks"}, {"kind", "frame"},
            {"count", frameHookCount}, {"frameNumber", frameCount_}});
    }
}

#endif // EFL_STUB_SDK

} // namespace efl::bridge
