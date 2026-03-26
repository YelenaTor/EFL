#include "efl/bridge/instance_walker.h"

namespace efl::bridge {

#ifdef EFL_STUB_SDK

// ── Stub implementation (for tests) ─────────────────────────────────────────

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

void InstanceWalker::destroyInstance(YYTK::InstanceId /*id*/) {
    // Stub: no-op
}

#else // Real SDK

// ── Real implementation ─────────────────────────────────────────────────────

InstanceWalker::InstanceWalker(YYTK::YYTKInterface* yytk)
    : yytk_(yytk) {}

std::vector<YYTK::CInstance*> InstanceWalker::getAll(const std::string& objectName) {
    if (!yytk_) return {};

    std::vector<YYTK::CInstance*> result;

    // asset_get_index(objectName) → object index
    YYTK::RValue objIndex = yytk_->CallBuiltin(
        "asset_get_index", {YYTK::RValue(objectName)});
    if (objIndex.m_Kind != YYTK::VALUE_REAL || objIndex.m_Real < 0) return result;

    // instance_number(objIndex) → count
    YYTK::RValue count = yytk_->CallBuiltin("instance_number", {objIndex});
    if (count.m_Kind != YYTK::VALUE_REAL) return result;

    int n = static_cast<int>(count.m_Real);
    result.reserve(n);

    for (int i = 0; i < n; ++i) {
        // instance_find(objIndex, i) → instance id
        YYTK::RValue inst = yytk_->CallBuiltin(
            "instance_find", {objIndex, YYTK::RValue(static_cast<double>(i))});

        YYTK::CInstance* ptr = nullptr;
        if (Aurie::AurieSuccess(yytk_->GetInstanceObject(inst.ToInt32(), ptr)) && ptr) {
            result.push_back(ptr);
        }
    }

    return result;
}

std::vector<YYTK::CInstance*> InstanceWalker::filter(
    const std::string& objectName,
    std::function<bool(YYTK::CInstance*)> predicate) {

    auto all = getAll(objectName);
    std::vector<YYTK::CInstance*> result;
    result.reserve(all.size());
    for (auto* inst : all) {
        if (predicate(inst)) {
            result.push_back(inst);
        }
    }
    return result;
}

YYTK::RValue InstanceWalker::getVariable(YYTK::CInstance* instance, const std::string& varName) {
    if (!yytk_ || !instance) return {};

    YYTK::RValue* member = nullptr;
    YYTK::RValue instVal = instance->ToRValue();
    if (Aurie::AurieSuccess(yytk_->GetInstanceMember(instVal, varName.c_str(), member)) && member) {
        return *member;
    }
    return {};
}

void InstanceWalker::setVariable(YYTK::CInstance* instance, const std::string& varName,
                                  YYTK::RValue value) {
    if (!yytk_ || !instance) return;

    YYTK::RValue* member = nullptr;
    YYTK::RValue instVal = instance->ToRValue();
    if (Aurie::AurieSuccess(yytk_->GetInstanceMember(instVal, varName.c_str(), member)) && member) {
        *member = value;
    }
}

void InstanceWalker::destroyInstance(YYTK::CInstance* inst) {
    if (!yytk_ || !inst) return;
    YYTK::RValue instVal = inst->ToRValue();
    yytk_->CallBuiltin("instance_destroy", {instVal});
}

#endif // EFL_STUB_SDK

} // namespace efl::bridge
