#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include <vector>

// Minimal YYTK stubs for EFL compilation
// Replace with real YYToolkit SDK when vendoring

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
