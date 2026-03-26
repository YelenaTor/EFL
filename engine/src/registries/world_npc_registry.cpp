#include "efl/registries/world_npc_registry.h"

namespace efl {

std::optional<WorldNpcDef> WorldNpcDef::fromJson(const nlohmann::json& j) {
    if (!j.contains("id") || !j.contains("displayName"))
        return std::nullopt;

    WorldNpcDef def;
    def.id          = j.at("id").get<std::string>();
    def.displayName = j.at("displayName").get<std::string>();

    if (j.contains("portraitAsset"))   def.portraitAsset   = j.at("portraitAsset").get<std::string>();
    if (j.contains("defaultAreaId"))   def.defaultAreaId   = j.at("defaultAreaId").get<std::string>();
    if (j.contains("defaultAnchorId")) def.defaultAnchorId = j.at("defaultAnchorId").get<std::string>();
    if (j.contains("unlockTrigger"))   def.unlockTrigger   = j.at("unlockTrigger").get<std::string>();

    if (j.contains("schedule") && j.at("schedule").is_array()) {
        for (const auto& item : j.at("schedule")) {
            NpcScheduleEntry entry;
            if (item.contains("timeFrom"))  entry.timeFrom  = item.at("timeFrom").get<int>();
            if (item.contains("timeTo"))    entry.timeTo    = item.at("timeTo").get<int>();
            if (item.contains("areaId"))    entry.areaId    = item.at("areaId").get<std::string>();
            if (item.contains("anchorId"))  entry.anchorId  = item.at("anchorId").get<std::string>();
            def.schedule.push_back(std::move(entry));
        }
    }

    return def;
}

void WorldNpcRegistry::registerWorldNpc(WorldNpcDef def) {
    if (index_.count(def.id)) {
        npcs_[index_[def.id]] = std::move(def);
        return;
    }
    index_[def.id] = npcs_.size();
    npcs_.push_back(std::move(def));
}

const WorldNpcDef* WorldNpcRegistry::getWorldNpc(const std::string& id) const {
    auto it = index_.find(id);
    if (it == index_.end()) return nullptr;
    return &npcs_[it->second];
}

std::vector<const WorldNpcDef*> WorldNpcRegistry::worldNpcsForArea(
        const std::string& areaId, int timeOfDay) const {
    std::vector<const WorldNpcDef*> result;
    for (const auto& npc : npcs_) {
        bool matched = false;
        for (const auto& entry : npc.schedule) {
            if (entry.areaId == areaId &&
                timeOfDay >= entry.timeFrom &&
                timeOfDay <= entry.timeTo) {
                result.push_back(&npc);
                matched = true;
                break;
            }
        }
        // No schedule entries — fall back to defaultAreaId
        if (!matched && npc.schedule.empty() && npc.defaultAreaId == areaId) {
            result.push_back(&npc);
        }
    }
    return result;
}

const std::vector<WorldNpcDef>& WorldNpcRegistry::allWorldNpcs() const {
    return npcs_;
}

void WorldNpcRegistry::tickSchedule(int /*currentTimeOfDay*/) {
    // TODO: teleport WorldNpcs to their scheduled anchor when FoM time hook is wired.
    // For now this is a stub — actual instance movement requires YYTK hooks in Layer B.
}

} // namespace efl
