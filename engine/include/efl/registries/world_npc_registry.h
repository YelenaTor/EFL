#pragma once

// Layer D: WorldNpcRegistry - global NPC definitions with time-of-day schedules

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <utility>
#include <nlohmann/json.hpp>

namespace efl {

class SaveService;

struct NpcScheduleEntry {
    int fromSeconds = 0;   // seconds since midnight (0–86399); e.g. 21600 = 6AM
    int toSeconds   = 0;
    std::string areaId;
    std::string anchorId;  // "x,y" pixel coordinates
};

struct WorldNpcDef {
    std::string id;
    std::string displayName;
    std::string objectName;      // GML object to spawn, e.g. "par_NPC"
    std::string portraitAsset;
    std::string defaultAreaId;
    std::string defaultAnchorId; // "x,y" pixel coordinates; used when no schedule entry matches
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

    // Returns all WorldNpcs whose active location (at timeOfDay) is areaId.
    std::vector<const WorldNpcDef*> worldNpcsForArea(const std::string& areaId, int timeOfDay) const;

    // Returns the schedule entry active at timeOfDay for this NPC, or nullptr if none match.
    const NpcScheduleEntry* activeEntryForNpc(const std::string& id, int timeOfDay) const;

    // Returns {areaId, anchorId} for where an NPC should be at timeOfDay.
    // Falls back to {defaultAreaId, defaultAnchorId} when no schedule entry covers this time.
    std::pair<std::string, std::string> activeLocationForNpc(const std::string& id, int timeOfDay) const;

    const std::vector<WorldNpcDef>& allWorldNpcs() const;

    // Called every frame. Detects schedule boundary crossings and fires onScheduleChange.
    void tickSchedule(int currentTimeOfDay);

    // Update the registry's last-known location for an NPC without firing onScheduleChange.
    // Call this after spawning a WorldNpc on room entry to prevent the next tick from
    // treating the freshly-spawned NPC as a location change.
    void setLastKnown(const std::string& npcId,
                      const std::string& areaId,
                      const std::string& anchorId);

    // Fired when a WorldNpc's active area or anchor changes mid-day.
    // Args: (npcId, newAreaId, newAnchorId)
    std::function<void(const std::string&, const std::string&, const std::string&)> onScheduleChange;

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

    // Per-NPC last-known location; updated by tickSchedule and setLastKnown.
    std::unordered_map<std::string, std::string> lastKnownArea_;
    std::unordered_map<std::string, std::string> lastKnownAnchor_;
};

} // namespace efl
