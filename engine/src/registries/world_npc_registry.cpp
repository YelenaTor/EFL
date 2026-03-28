#include "efl/registries/world_npc_registry.h"
#include "efl/core/save_service.h"
#include <algorithm>

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
    if (j.contains("heartsPerGift"))   def.heartsPerGift   = j.at("heartsPerGift").get<int>();
    if (j.contains("giftableItems") && j.at("giftableItems").is_array()) {
        for (const auto& item : j.at("giftableItems"))
            def.giftableItems.push_back(item.get<std::string>());
    }

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

void WorldNpcRegistry::setSaveService(SaveService* saves) {
    saves_ = saves;
}

int WorldNpcRegistry::getHearts(const std::string& modId, const std::string& npcId) const {
    if (!saves_) return 0;
    auto val = saves_->get(modId, "npc", npcId + "/hearts");
    if (val && val->is_number_integer())
        return val->get<int>();
    return 0;
}

void WorldNpcRegistry::addHearts(const std::string& modId, const std::string& npcId, int delta) {
    if (!saves_) return;
    int current = getHearts(modId, npcId);
    int next = std::clamp(current + delta, 0, 10);
    saves_->set(modId, "npc", npcId + "/hearts", next);
}

void WorldNpcRegistry::recordGift(const std::string& modId, const std::string& npcId,
                                   const std::string& /*itemId*/, const std::string& date) {
    if (!saves_) return;
    saves_->set(modId, "npc", npcId + "/lastGiftDate", date);
}

bool WorldNpcRegistry::wasGiftedToday(const std::string& modId, const std::string& npcId,
                                       const std::string& today) const {
    if (!saves_) return false;
    auto val = saves_->get(modId, "npc", npcId + "/lastGiftDate");
    if (val && val->is_string())
        return val->get<std::string>() == today;
    return false;
}

} // namespace efl
