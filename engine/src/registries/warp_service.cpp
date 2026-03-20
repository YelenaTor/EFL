#include "efl/registries/warp_service.h"
#include "efl/core/trigger_service.h"

namespace efl {

std::optional<WarpDef> WarpDef::fromJson(const nlohmann::json& j) {
    if (!j.contains("id") || !j.contains("sourceArea") || !j.contains("targetArea")) {
        return std::nullopt;
    }
    WarpDef def;
    def.id           = j.at("id").get<std::string>();
    def.sourceArea   = j.at("sourceArea").get<std::string>();
    def.targetArea   = j.at("targetArea").get<std::string>();
    if (j.contains("sourceAnchor"))   def.sourceAnchor   = j.at("sourceAnchor").get<std::string>();
    if (j.contains("targetAnchor"))   def.targetAnchor   = j.at("targetAnchor").get<std::string>();
    if (j.contains("requireTrigger")) def.requireTrigger = j.at("requireTrigger").get<std::string>();
    if (j.contains("failureText"))    def.failureText    = j.at("failureText").get<std::string>();
    return def;
}

void WarpService::registerWarp(const WarpDef& def) {
    index_[def.id] = warps_.size();
    warps_.push_back(def);
}

const WarpDef* WarpService::getWarp(const std::string& id) const {
    auto it = index_.find(id);
    if (it == index_.end()) return nullptr;
    return &warps_[it->second];
}

std::vector<const WarpDef*> WarpService::warpsFrom(const std::string& sourceArea) const {
    std::vector<const WarpDef*> result;
    for (const auto& w : warps_) {
        if (w.sourceArea == sourceArea) result.push_back(&w);
    }
    return result;
}

std::vector<const WarpDef*> WarpService::warpsTo(const std::string& targetArea) const {
    std::vector<const WarpDef*> result;
    for (const auto& w : warps_) {
        if (w.targetArea == targetArea) result.push_back(&w);
    }
    return result;
}

bool WarpService::canWarp(const std::string& warpId, const TriggerService& triggers) const {
    const WarpDef* def = getWarp(warpId);
    if (!def) return false;
    if (def->requireTrigger.empty()) return true;
    return triggers.evaluate(def->requireTrigger);
}

} // namespace efl
