#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

class TriggerService; // forward declare

struct WarpDef {
    std::string id;
    std::string sourceArea;
    std::string sourceAnchor;
    std::string targetArea;
    std::string targetAnchor;
    std::string requireTrigger; // empty = always available
    std::string failureText;

    static std::optional<WarpDef> fromJson(const nlohmann::json& j);
};

class WarpService {
public:
    void registerWarp(const WarpDef& def);
    const WarpDef* getWarp(const std::string& id) const;
    std::vector<const WarpDef*> warpsFrom(const std::string& sourceArea) const;
    std::vector<const WarpDef*> warpsTo(const std::string& targetArea) const;
    bool canWarp(const std::string& warpId, const TriggerService& triggers) const;

private:
    std::vector<WarpDef> warps_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace efl
