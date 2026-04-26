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

    if (j.contains("objectName"))      def.objectName      = j.at("objectName").get<std::string>();
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
            if (item.contains("fromSeconds")) entry.fromSeconds = item.at("fromSeconds").get<int>();
            if (item.contains("toSeconds"))   entry.toSeconds   = item.at("toSeconds").get<int>();
            if (item.contains("areaId"))      entry.areaId      = item.at("areaId").get<std::string>();
            if (item.contains("anchorId"))    entry.anchorId    = item.at("anchorId").get<std::string>();
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
        auto [activeArea, activeAnchor] = activeLocationForNpc(npc.id, timeOfDay);
        if (activeArea == areaId)
            result.push_back(&npc);
    }
    return result;
}

const NpcScheduleEntry* WorldNpcRegistry::activeEntryForNpc(
        const std::string& id, int timeOfDay) const {
    auto it = index_.find(id);
    if (it == index_.end()) return nullptr;
    const WorldNpcDef& npc = npcs_[it->second];
    for (const auto& entry : npc.schedule) {
        if (timeOfDay >= entry.fromSeconds && timeOfDay < entry.toSeconds)
            return &entry;
    }
    return nullptr;
}

std::pair<std::string, std::string> WorldNpcRegistry::activeLocationForNpc(
        const std::string& id, int timeOfDay) const {
    auto it = index_.find(id);
    if (it == index_.end()) return {};
    const WorldNpcDef& npc = npcs_[it->second];
    const NpcScheduleEntry* entry = activeEntryForNpc(id, timeOfDay);
    if (entry) return {entry->areaId, entry->anchorId};
    return {npc.defaultAreaId, npc.defaultAnchorId};
}

void WorldNpcRegistry::setLastKnown(const std::string& npcId,
                                     const std::string& areaId,
                                     const std::string& anchorId) {
    lastKnownArea_[npcId]   = areaId;
    lastKnownAnchor_[npcId] = anchorId;
}

const std::vector<WorldNpcDef>& WorldNpcRegistry::allWorldNpcs() const {
    return npcs_;
}

void WorldNpcRegistry::tickSchedule(int currentTimeOfDay) {
    if (!onScheduleChange) return;
    for (const auto& npc : npcs_) {
        auto [areaId, anchorId] = activeLocationForNpc(npc.id, currentTimeOfDay);
        auto& lastArea   = lastKnownArea_[npc.id];
        auto& lastAnchor = lastKnownAnchor_[npc.id];
        if (areaId != lastArea || anchorId != lastAnchor) {
            lastArea   = areaId;
            lastAnchor = anchorId;
            onScheduleChange(npc.id, areaId, anchorId);
        }
    }
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
