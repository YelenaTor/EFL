#pragma once

// Layer B (PRIVATE): Hook registration and management

#include <string>
#include <functional>
#include <unordered_map>
#include <cstdint>

#ifdef EFL_STUB_SDK
#include "efl/bridge/sdk_compat.h"

namespace efl::bridge {

class HookRegistry {
public:
    explicit HookRegistry(YYTK::YYTKInterface* yytk);
    ~HookRegistry();

    bool registerHook(const std::string& name, const std::string& target,
                      YYTK::CodeCallback callback);
    void removeHook(const std::string& name);
    void removeAll();

    bool isRegistered(const std::string& name) const;
    size_t count() const;

private:
    YYTK::YYTKInterface* yytk_;

    struct HookEntry {
        std::string target;
        YYTK::HookHandle handle;
    };

    std::unordered_map<std::string, HookEntry> hooks_;
};

} // namespace efl::bridge

#else // Real SDK

#include <Aurie/shared.hpp>
#include <YYToolkit/YYTK_Shared.hpp>

namespace efl { class PipeWriter; }

namespace efl::bridge {

// Callback for object/script code execution events.
// v1: observation only — suppression is not wired through to YYTK's Event.Call().
using CodeEventCallback = std::function<void(
    YYTK::CInstance*, YYTK::CInstance*, YYTK::CCode*, int, YYTK::RValue*)>;

// Callback for frame events (called each Present).
using FrameCallback = std::function<void()>;

class HookRegistry {
public:
    HookRegistry(Aurie::AurieModule* module, YYTK::YYTKInterface* yytk);
    ~HookRegistry();

    void setPipeWriter(efl::PipeWriter* pipe);

    // Register a callback on script/object code execution (EVENT_OBJECT_CALL).
    bool registerScriptHook(const std::string& name, const std::string& target,
                            CodeEventCallback callback);

    // Register a per-frame callback (EVENT_FRAME).
    bool registerFrameCallback(const std::string& name, FrameCallback callback);

    // Register an inline function detour via MmCreateHook.
    bool registerDetour(const std::string& name, void* source, void* dest, void** trampoline);

    void removeHook(const std::string& name);
    void removeAll();

    bool isRegistered(const std::string& name) const;
    size_t count() const;

    // Dispatch events from the global callback (called internally).
    void dispatchCodeEvent(YYTK::CInstance* self, YYTK::CInstance* other,
                           YYTK::CCode* code, int argc, YYTK::RValue* args);
    void dispatchFrameEvent();

private:
    Aurie::AurieModule* module_;
    YYTK::YYTKInterface* yytk_;
    efl::PipeWriter* pipe_ = nullptr;

    enum class HookKind { Script, Frame, Detour };

    struct HookEntry {
        HookKind kind;
        std::string target;
        CodeEventCallback scriptCb;
        FrameCallback frameCb;
    };

    std::unordered_map<std::string, HookEntry> hooks_;

    bool codeCallbackRegistered_ = false;
    bool frameCallbackRegistered_ = false;
    uint64_t frameCount_ = 0;
};

} // namespace efl::bridge

#endif // EFL_STUB_SDK
