#include "efl/registries/world_state_service.h"
#include "efl/registries/area_registry.h"
#include "efl/registries/quest_registry.h"
#include "efl/registries/npc_registry.h"
#include "efl/core/trigger_service.h"

namespace efl {

WorldStateService::WorldStateService(AreaRegistry& areas, QuestRegistry& quests,
                                     NpcRegistry& npcs, TriggerService& triggers)
    : areas_(areas), quests_(quests), npcs_(npcs), triggers_(triggers) {}

std::vector<std::string> WorldStateService::unlockedAreaIds() const {
    std::vector<std::string> result;
    for (const auto& area : areas_.allAreas()) {
        if (area.unlockTrigger.empty() || triggers_.evaluate(area.unlockTrigger)) {
            result.push_back(area.id);
        }
    }
    return result;
}

std::vector<std::string> WorldStateService::activeQuestIds() const {
    return quests_.activeQuestIds();
}

std::vector<std::string> WorldStateService::visibleNpcIds() const {
    std::vector<std::string> result;
    for (const auto& npc : npcs_.allNpcs()) {
        if (npc.unlockTrigger.empty() || triggers_.evaluate(npc.unlockTrigger)) {
            result.push_back(npc.id);
        }
    }
    return result;
}

bool WorldStateService::isTriggerMet(const std::string& triggerId) const {
    return triggers_.evaluate(triggerId);
}

} // namespace efl
