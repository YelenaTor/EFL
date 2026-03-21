#pragma once

// SDK compatibility shim for test builds (EFL_STUB_SDK).
// Provides the original stub types so that the 83 application-layer tests
// continue to compile without real Aurie/YYToolkit headers.

#ifndef EFL_STUB_SDK
#error "sdk_compat.h must only be included when EFL_STUB_SDK is defined"
#endif

#include <cstdint>
#include <string>
#include <functional>
#include <vector>

// ── Aurie stubs ──────────────────────────────────────────────────────────────

namespace Aurie {

using AurieStatus = int32_t;
constexpr AurieStatus AURIE_SUCCESS = 0;
constexpr AurieStatus AURIE_MODULE_NOT_FOUND = -1;

struct AurieModule {
    std::string name;
    std::string version;
};

using ModuleInitCallback = std::function<AurieStatus()>;
using ModuleUnloadCallback = std::function<void()>;

#define EXPORTED_AURIE_MODULE(init_fn, unload_fn)

} // namespace Aurie

// ── YYTK stubs ───────────────────────────────────────────────────────────────

namespace YYTK {

struct RValue {
    enum Kind { REAL, STRING, UNDEFINED };
    Kind kind = UNDEFINED;
    double real = 0.0;
    std::string str;

    RValue() = default;
    RValue(double v) : kind(REAL), real(v) {}
    RValue(const std::string& s) : kind(STRING), str(s) {}
};

using RoomId = int32_t;
constexpr RoomId ROOM_INVALID = -1;

using InstanceId = int32_t;

using HookHandle = void*;

using ScriptCallback = std::function<void(RValue& result, int argc, RValue* args)>;
using CodeCallback = std::function<void()>;

using RoutinePtr = void(*)(RValue& result, void* self, void* other, int argc, RValue* args);

class YYTKInterface {
public:
    virtual ~YYTKInterface() = default;

    virtual HookHandle CreateHook(const std::string& target, CodeCallback callback) {
        return nullptr;
    }
    virtual void RemoveHook(HookHandle handle) {}

    virtual RoutinePtr GetNamedRoutinePointer(const std::string& name) {
        return nullptr;
    }

    virtual RoomId GetCurrentRoom() { return ROOM_INVALID; }
    virtual std::string GetRoomName(RoomId id) { return ""; }

    virtual std::vector<InstanceId> GetInstances(const std::string& objectName) {
        return {};
    }

    virtual RValue GetInstanceVariable(InstanceId id, const std::string& varName) {
        return {};
    }
    virtual void SetInstanceVariable(InstanceId id, const std::string& varName, const RValue& value) {}
};

} // namespace YYTK
