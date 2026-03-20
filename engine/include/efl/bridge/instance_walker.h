#pragma once

// Layer B (PRIVATE): Game object instance enumeration

#include <string>
#include <vector>
#include <functional>
#include <yytk/yytk.h>

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
