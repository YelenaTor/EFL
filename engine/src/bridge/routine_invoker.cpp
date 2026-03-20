#include "efl/bridge/routine_invoker.h"

namespace efl::bridge {

RoutineInvoker::RoutineInvoker(YYTK::YYTKInterface* yytk)
    : yytk_(yytk) {}

YYTK::RoutinePtr RoutineInvoker::resolve(const std::string& name) {
    auto it = cache_.find(name);
    if (it != cache_.end()) return it->second;

    YYTK::RoutinePtr ptr = nullptr;
    if (yytk_) {
        ptr = yytk_->GetNamedRoutinePointer(name);
    }
    cache_[name] = ptr;
    return ptr;
}

bool RoutineInvoker::hasRoutine(const std::string& name) {
    return resolve(name) != nullptr;
}

std::optional<YYTK::RValue> RoutineInvoker::invoke(const std::string& name,
                                                     std::vector<YYTK::RValue> args) {
    YYTK::RoutinePtr ptr = resolve(name);
    if (!ptr) return std::nullopt;

    YYTK::RValue result;
    ptr(result, nullptr, nullptr, static_cast<int>(args.size()),
        args.empty() ? nullptr : args.data());
    return result;
}

void RoutineInvoker::clearCache() {
    cache_.clear();
}

} // namespace efl::bridge
