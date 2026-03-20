#include "efl/bridge/instance_walker.h"

namespace efl::bridge {

InstanceWalker::InstanceWalker(YYTK::YYTKInterface* yytk)
    : yytk_(yytk) {}

std::vector<YYTK::InstanceId> InstanceWalker::getAll(const std::string& objectName) {
    if (!yytk_) return {};
    return yytk_->GetInstances(objectName);
}

std::vector<YYTK::InstanceId> InstanceWalker::filter(
    const std::string& objectName,
    std::function<bool(YYTK::InstanceId)> predicate) {

    auto all = getAll(objectName);
    std::vector<YYTK::InstanceId> result;
    result.reserve(all.size());
    for (auto id : all) {
        if (predicate(id)) {
            result.push_back(id);
        }
    }
    return result;
}

YYTK::RValue InstanceWalker::getVariable(YYTK::InstanceId id, const std::string& varName) {
    if (!yytk_) return {};
    return yytk_->GetInstanceVariable(id, varName);
}

void InstanceWalker::setVariable(YYTK::InstanceId id, const std::string& varName,
                                  const YYTK::RValue& value) {
    if (!yytk_) return;
    yytk_->SetInstanceVariable(id, varName, value);
}

} // namespace efl::bridge
