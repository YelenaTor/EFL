#pragma once

// Layer B (PRIVATE): Hook registration and trampoline storage

#include <string>
#include <functional>
#include <unordered_map>
#include <yytk/yytk.h>

namespace efl::bridge {

class HookRegistry {
public:
    explicit HookRegistry(YYTK::YYTKInterface* yytk);

    bool registerHook(const std::string& name, const std::string& target,
                      YYTK::CodeCallback callback);
    void removeHook(const std::string& name);
    void removeAll();

    bool isRegistered(const std::string& name) const;
    size_t count() const;

private:
    YYTK::YYTKInterface* yytk_;

    struct HookEntry {
        std::string target;
        YYTK::HookHandle handle;
    };

    std::unordered_map<std::string, HookEntry> hooks_;
};

} // namespace efl::bridge
