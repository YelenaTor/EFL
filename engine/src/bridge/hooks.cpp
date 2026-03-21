#include "efl/bridge/hooks.h"

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

#include "efl/ipc/pipe_writer.h"

// ── Real implementation ─────────────────────────────────────────────────────

// Global pointer for static callback dispatch
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

    // Ensure the global CODE callback is registered with YYTK
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

void HookRegistry::removeAll() {
    for (auto& [name, entry] : hooks_) {
        if (entry.kind == HookKind::Detour && module_) {
            Aurie::MmRemoveHook(module_, name);
        }
    }
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
        if (entry.kind == HookKind::Script && entry.target == name && entry.scriptCb) {
            entry.scriptCb(self, other, code, argc, args);

            if (pipe_) {
                pipe_->write("hook.fired", nlohmann::json{
                    {"name", hookName}, {"target", name}});
            }
        }
    }
}

void HookRegistry::dispatchFrameEvent() {
    for (auto& [hookName, entry] : hooks_) {
        if (entry.kind == HookKind::Frame && entry.frameCb) {
            entry.frameCb();
        }
    }
}

#endif // EFL_STUB_SDK

} // namespace efl::bridge
