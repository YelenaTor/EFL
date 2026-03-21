#pragma once

// Layer C: Central registry coordination

#include "efl/registries/area_registry.h"
#include "efl/registries/warp_service.h"
#include "efl/registries/resource_registry.h"
#include "efl/registries/quest_registry.h"
#include "efl/registries/npc_registry.h"
#include "efl/registries/crafting_registry.h"
#include "efl/registries/dialogue_service.h"
#include "efl/registries/story_bridge.h"
#include "efl/core/trigger_service.h"

namespace efl {

class RegistryService {
public:
    AreaRegistry& areas() { return areas_; }
    WarpService& warps() { return warps_; }
    ResourceRegistry& resources() { return resources_; }
    QuestRegistry& quests() { return quests_; }
    NpcRegistry& npcs() { return npcs_; }
    CraftingRegistry& crafting() { return crafting_; }
    DialogueService& dialogue() { return dialogue_; }
    StoryBridge& story() { return story_; }
    TriggerService& triggers() { return triggers_; }

    const AreaRegistry& areas() const { return areas_; }
    const WarpService& warps() const { return warps_; }
    const ResourceRegistry& resources() const { return resources_; }
    const QuestRegistry& quests() const { return quests_; }
    const NpcRegistry& npcs() const { return npcs_; }
    const CraftingRegistry& crafting() const { return crafting_; }
    const DialogueService& dialogue() const { return dialogue_; }
    const StoryBridge& story() const { return story_; }
    const TriggerService& triggers() const { return triggers_; }

private:
    AreaRegistry areas_;
    WarpService warps_;
    ResourceRegistry resources_;
    QuestRegistry quests_;
    NpcRegistry npcs_;
    CraftingRegistry crafting_;
    DialogueService dialogue_;
    StoryBridge story_;
    TriggerService triggers_;
};

} // namespace efl
