#pragma once

// Layer D: WorldNpcRegistry - global NPC definitions with time-of-day schedules

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>

namespace efl {

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

    static std::optional<WorldNpcDef> fromJson(const nlohmann::json& j);
};

class WorldNpcRegistry {
public:
    void registerWorldNpc(WorldNpcDef def);
    const WorldNpcDef* getWorldNpc(const std::string& id) const;
    std::vector<const WorldNpcDef*> worldNpcsForArea(const std::string& areaId, int timeOfDay) const;
    const std::vector<WorldNpcDef>& allWorldNpcs() const;
    void tickSchedule(int currentTimeOfDay);

private:
    std::vector<WorldNpcDef> npcs_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace efl
