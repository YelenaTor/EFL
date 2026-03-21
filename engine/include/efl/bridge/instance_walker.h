#pragma once

// Layer B (PRIVATE): Game object instance enumeration

#include <string>
#include <vector>
#include <functional>

#ifdef EFL_STUB_SDK
#include "efl/bridge/sdk_compat.h"

namespace efl::bridge {

class InstanceWalker {
public:
    explicit InstanceWalker(YYTK::YYTKInterface* yytk);

    std::vector<YYTK::InstanceId> getAll(const std::string& objectName);
    std::vector<YYTK::InstanceId> filter(const std::string& objectName,
                                          std::function<bool(YYTK::InstanceId)> predicate);

    YYTK::RValue getVariable(YYTK::InstanceId id, const std::string& varName);
    void setVariable(YYTK::InstanceId id, const std::string& varName, const YYTK::RValue& value);

private:
    YYTK::YYTKInterface* yytk_;
};

} // namespace efl::bridge

#else // Real SDK

#include <Aurie/shared.hpp>
#include <YYToolkit/YYTK_Shared.hpp>

namespace efl::bridge {

class InstanceWalker {
public:
    explicit InstanceWalker(YYTK::YYTKInterface* yytk);

    // Get all CInstance pointers for a given object name.
    std::vector<YYTK::CInstance*> getAll(const std::string& objectName);

    // Filter instances by predicate.
    std::vector<YYTK::CInstance*> filter(const std::string& objectName,
                                          std::function<bool(YYTK::CInstance*)> predicate);

    // Access instance member variables.
    YYTK::RValue getVariable(YYTK::CInstance* instance, const std::string& varName);
    void setVariable(YYTK::CInstance* instance, const std::string& varName, YYTK::RValue value);

private:
    YYTK::YYTKInterface* yytk_;
};

} // namespace efl::bridge

#endif // EFL_STUB_SDK
