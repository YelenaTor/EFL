#include <gtest/gtest.h>
#include "efl/registries/world_state_service.h"
#include "efl/registries/area_registry.h"
#include "efl/registries/quest_registry.h"
#include "efl/registries/npc_registry.h"
#include "efl/core/trigger_service.h"

TEST(WorldStateService, AggregatesUnlockedAreas) {
    efl::AreaRegistry areas;
    efl::QuestRegistry quests;
    efl::NpcRegistry npcs;
    efl::TriggerService triggers;
    areas.registerArea({.id = "cave", .displayName = "Cave"});
    areas.registerArea({.id = "locked", .displayName = "Locked", .unlockTrigger = "key"});
    triggers.registerTrigger({.id = "key", .type = efl::TriggerType::FlagSet, .flagName = "f"});

    efl::WorldStateService ws(areas, quests, npcs, triggers);
    auto unlocked = ws.unlockedAreaIds();
    EXPECT_EQ(unlocked.size(), 1u); // only "cave" (no trigger)
    triggers.setFlag("f", true);
    unlocked = ws.unlockedAreaIds();
    EXPECT_EQ(unlocked.size(), 2u); // both unlocked
}

TEST(WorldStateService, ActiveQuests) {
    efl::AreaRegistry areas;
    efl::QuestRegistry quests;
    efl::NpcRegistry npcs;
    efl::TriggerService triggers;
    quests.registerQuest({.id = "q1", .title = "Quest 1", .stages = {{.id = "s1"}}});
    quests.registerQuest({.id = "q2", .title = "Quest 2", .stages = {{.id = "s1"}}});
    quests.startQuest("q1");

    efl::WorldStateService ws(areas, quests, npcs, triggers);
    EXPECT_EQ(ws.activeQuestIds().size(), 1u);
    EXPECT_EQ(ws.activeQuestIds()[0], "q1");
}

TEST(WorldStateService, VisibleNpcs) {
    efl::AreaRegistry areas;
    efl::QuestRegistry quests;
    efl::NpcRegistry npcs;
    efl::TriggerService triggers;
    npcs.registerNpc({.id = "npc1", .displayName = "N1"});
    npcs.registerNpc({.id = "npc2", .displayName = "N2", .unlockTrigger = "flag1"});
    triggers.registerTrigger({.id = "flag1", .type = efl::TriggerType::FlagSet, .flagName = "f1"});

    efl::WorldStateService ws(areas, quests, npcs, triggers);
    EXPECT_EQ(ws.visibleNpcIds().size(), 1u); // only npc1 (no trigger)
    triggers.setFlag("f1", true);
    EXPECT_EQ(ws.visibleNpcIds().size(), 2u); // both visible
}

TEST(WorldStateService, IsTriggerMet) {
    efl::AreaRegistry areas;
    efl::QuestRegistry quests;
    efl::NpcRegistry npcs;
    efl::TriggerService triggers;
    triggers.registerTrigger({.id = "t1", .type = efl::TriggerType::FlagSet, .flagName = "myFlag"});

    efl::WorldStateService ws(areas, quests, npcs, triggers);
    EXPECT_FALSE(ws.isTriggerMet("t1"));
    triggers.setFlag("myFlag", true);
    EXPECT_TRUE(ws.isTriggerMet("t1"));
}
