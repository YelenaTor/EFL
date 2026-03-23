#include <gtest/gtest.h>
#include "efl/core/trigger_service.h"

TEST(TriggerService, SimpleFlag) {
    efl::TriggerService svc;
    svc.registerTrigger({.id = "has_key", .type = efl::TriggerType::FlagSet,
                         .flagName = "player_has_cave_key"});
    EXPECT_FALSE(svc.evaluate("has_key"));
    svc.setFlag("player_has_cave_key", true);
    EXPECT_TRUE(svc.evaluate("has_key"));
}

TEST(TriggerService, AllOf) {
    efl::TriggerService svc;
    svc.setFlag("a", true);
    svc.setFlag("b", false);
    svc.registerTrigger({.id = "both", .type = efl::TriggerType::AllOf,
                         .conditions = {"flag:a", "flag:b"}});
    EXPECT_FALSE(svc.evaluate("both"));
    svc.setFlag("b", true);
    EXPECT_TRUE(svc.evaluate("both"));
}

TEST(TriggerService, AnyOf) {
    efl::TriggerService svc;
    svc.setFlag("x", false);
    svc.setFlag("y", true);
    svc.registerTrigger({.id = "either", .type = efl::TriggerType::AnyOf,
                         .conditions = {"flag:x", "flag:y"}});
    EXPECT_TRUE(svc.evaluate("either"));
}

TEST(TriggerService, QuestComplete) {
    efl::TriggerService svc;
    svc.registerTrigger({.id = "quest_done", .type = efl::TriggerType::QuestComplete,
                         .questId = "find_crystals"});
    EXPECT_FALSE(svc.evaluate("quest_done"));
    svc.markQuestComplete("find_crystals");
    EXPECT_TRUE(svc.evaluate("quest_done"));
}

TEST(TriggerService, SelfReferentialCycleRejected) {
    efl::TriggerService svc;
    // A trigger that references itself via allOf
    svc.registerTrigger({.id = "loop", .type = efl::TriggerType::AllOf,
                         .conditions = {"flag:ok", "loop"}});
    // Should have been rejected — evaluate returns false (not registered)
    EXPECT_FALSE(svc.evaluate("loop"));
}

TEST(TriggerService, MutualCycleRejected) {
    efl::TriggerService svc;
    // Register A first (references B which doesn't exist yet — no cycle)
    svc.registerTrigger({.id = "a", .type = efl::TriggerType::AllOf,
                         .conditions = {"b"}});
    // Register B referencing A — creates a cycle, B is rejected
    svc.registerTrigger({.id = "b", .type = efl::TriggerType::AnyOf,
                         .conditions = {"a"}});
    // B should not exist
    EXPECT_FALSE(svc.evaluate("b"));
    // A still exists but references nonexistent B, so evaluates false
    EXPECT_FALSE(svc.evaluate("a"));
}

TEST(TriggerService, NoCycleAllowed) {
    efl::TriggerService svc;
    svc.registerTrigger({.id = "leaf", .type = efl::TriggerType::FlagSet,
                         .flagName = "done"});
    svc.registerTrigger({.id = "parent", .type = efl::TriggerType::AllOf,
                         .conditions = {"leaf", "flag:extra"}});
    // No cycle — both should be registered
    svc.setFlag("done", true);
    svc.setFlag("extra", true);
    EXPECT_TRUE(svc.evaluate("parent"));
}
