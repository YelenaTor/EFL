#pragma once

// Layer B (PRIVATE): Named GML routine lookup and invocation

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
#include <yytk/yytk.h>

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
