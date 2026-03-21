#pragma once

// Layer B (PRIVATE): Named GML routine lookup and invocation

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

#ifdef EFL_STUB_SDK
#include "efl/bridge/sdk_compat.h"

namespace efl::bridge {

class RoutineInvoker {
public:
    explicit RoutineInvoker(YYTK::YYTKInterface* yytk);

    bool hasRoutine(const std::string& name);
    std::optional<YYTK::RValue> invoke(const std::string& name,
                                        std::vector<YYTK::RValue> args = {});
    void clearCache();

private:
    YYTK::YYTKInterface* yytk_;
    std::unordered_map<std::string, YYTK::RoutinePtr> cache_;

    YYTK::RoutinePtr resolve(const std::string& name);
};

} // namespace efl::bridge

#else // Real SDK

#include <Aurie/shared.hpp>
#include <YYToolkit/YYTK_Shared.hpp>

namespace efl::bridge {

class RoutineInvoker {
public:
    explicit RoutineInvoker(YYTK::YYTKInterface* yytk);

    // Check if a GML builtin or script can be resolved.
    bool hasRoutine(const std::string& name);

    // Call a GML builtin by name.
    YYTK::RValue callBuiltin(const std::string& name, std::vector<YYTK::RValue> args = {});

    // Call a game script (must include "gml_Script_" prefix).
    YYTK::RValue callGameScript(const std::string& name, std::vector<YYTK::RValue> args = {});

    // Resolve a named routine pointer.
    void* getRoutinePointer(const std::string& name);

    void clearCache();

private:
    YYTK::YYTKInterface* yytk_;
    std::unordered_map<std::string, void*> cache_;
};

} // namespace efl::bridge

#endif // EFL_STUB_SDK
