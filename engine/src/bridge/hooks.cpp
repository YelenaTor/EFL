#include "efl/bridge/hooks.h"

namespace efl::bridge {

HookRegistry::HookRegistry(YYTK::YYTKInterface* yytk)
    : yytk_(yytk) {}

bool HookRegistry::registerHook(const std::string& name, const std::string& target,
                                 YYTK::CodeCallback callback) {
    if (!yytk_) return false;
    if (hooks_.count(name)) return false; // already registered under this name

    YYTK::HookHandle handle = yytk_->CreateHook(target, std::move(callback));
    hooks_[name] = HookEntry{target, handle};
    return true;
}

void HookRegistry::removeHook(const std::string& name) {
    auto it = hooks_.find(name);
    if (it == hooks_.end()) return;

    if (yytk_ && it->second.handle) {
        yytk_->RemoveHook(it->second.handle);
    }
    hooks_.erase(it);
}

void HookRegistry::removeAll() {
    if (yytk_) {
        for (auto& [name, entry] : hooks_) {
            if (entry.handle) {
                yytk_->RemoveHook(entry.handle);
            }
        }
    }
    hooks_.clear();
}

bool HookRegistry::isRegistered(const std::string& name) const {
    return hooks_.count(name) > 0;
}

size_t HookRegistry::count() const {
    return hooks_.size();
}

} // namespace efl::bridge
