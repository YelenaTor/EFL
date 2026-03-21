#pragma once

// Layer D: IEflWorldStateService - global world state queries

#include <string>
#include <vector>

namespace efl {

class AreaRegistry;
class QuestRegistry;
class NpcRegistry;
class TriggerService;

class WorldStateService {
public:
    WorldStateService(AreaRegistry& areas, QuestRegistry& quests,
                      NpcRegistry& npcs, TriggerService& triggers);

    std::vector<std::string> unlockedAreaIds() const;
    std::vector<std::string> activeQuestIds() const;
    std::vector<std::string> visibleNpcIds() const;
    bool isTriggerMet(const std::string& triggerId) const;

private:
    AreaRegistry& areas_;
    QuestRegistry& quests_;
    NpcRegistry& npcs_;
    TriggerService& triggers_;
};

} // namespace efl
