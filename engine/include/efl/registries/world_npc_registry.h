#pragma once

// Layer D: WorldNpcRegistry - global NPC definitions with time-of-day schedules

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

class SaveService;

struct NpcScheduleEntry {
    int timeFrom = 0;   // game time (0-2359)
    int timeTo = 0;
    std::string areaId;
    std::string anchorId;
};

struct WorldNpcDef {
    std::string id;
    std::string displayName;
    std::string portraitAsset;
    std::string defaultAreaId;
    std::string defaultAnchorId;
    std::optional<std::string> unlockTrigger;
    std::vector<NpcScheduleEntry> schedule;
    std::vector<std::string> giftableItems; // item ids accepted as gifts
    int heartsPerGift = 1;                  // hearts awarded per valid gift

    static std::optional<WorldNpcDef> fromJson(const nlohmann::json& j);
};

class WorldNpcRegistry {
public:
    void setSaveService(SaveService* saves);

    void registerWorldNpc(WorldNpcDef def);
    const WorldNpcDef* getWorldNpc(const std::string& id) const;
    std::vector<const WorldNpcDef*> worldNpcsForArea(const std::string& areaId, int timeOfDay) const;
    const std::vector<WorldNpcDef>& allWorldNpcs() const;
    void tickSchedule(int currentTimeOfDay);

    // Hearts/gifts — backed by SaveService under EFL/<modId>/npc/<npcId>/hearts|lastGiftDate
    int  getHearts(const std::string& modId, const std::string& npcId) const;
    void addHearts(const std::string& modId, const std::string& npcId, int delta);
    void recordGift(const std::string& modId, const std::string& npcId,
                    const std::string& itemId, const std::string& date);
    bool wasGiftedToday(const std::string& modId, const std::string& npcId,
                        const std::string& today) const;

private:
    SaveService* saves_ = nullptr;
    std::vector<WorldNpcDef> npcs_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace efl
