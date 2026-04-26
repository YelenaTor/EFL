#include <gtest/gtest.h>
#include "efl/registries/story_bridge.h"
#include "efl/core/trigger_service.h"
#include <fstream>
#include <nlohmann/json.hpp>

static nlohmann::json loadFixture(const std::string& name) {
    std::ifstream f("fixtures/" + name);
    return nlohmann::json::parse(f);
}

TEST(StoryBridge, LoadCutsceneDefinition) {
    efl::StoryBridge bridge;
    auto def = efl::CutsceneDef::fromJson(loadFixture("sample_event.json"));
    ASSERT_TRUE(def.has_value());
    bridge.registerCutscene(*def);
    auto found = bridge.getCutscene("crystal_cave_reveal");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->trigger, "has_cave_key");
    EXPECT_TRUE(found->once);
    ASSERT_EQ(found->onFire.setFlags.size(), 1u);
    EXPECT_EQ(found->onFire.setFlags[0], "cave_revealed");
    EXPECT_EQ(found->onFire.startQuest, "find_crystals");
}

TEST(StoryBridge, EligibilityGatedByTrigger) {
    efl::StoryBridge bridge;
    efl::TriggerService triggers;

    efl::CutsceneDef def;
    def.id      = "cave_cutscene";
    def.trigger = "unlock";
    def.once    = false; // allow re-evaluation for this test
    bridge.registerCutscene(def);

    triggers.registerTrigger({.id = "unlock", .type = efl::TriggerType::FlagSet,
                              .flagName = "flag"});

    EXPECT_FALSE(bridge.evaluateEligibility("cave_cutscene", triggers));
    triggers.setFlag("flag", true);
    EXPECT_TRUE(bridge.evaluateEligibility("cave_cutscene", triggers));
}

TEST(StoryBridge, OnceFlagPreventsReplay) {
    efl::StoryBridge bridge;
    efl::TriggerService triggers;

    efl::CutsceneDef def;
    def.id      = "one_shot";
    def.trigger = "";   // always eligible
    def.once    = true;
    bridge.registerCutscene(def);

    EXPECT_TRUE(bridge.evaluateEligibility("one_shot", triggers));
    EXPECT_FALSE(bridge.evaluateEligibility("one_shot", triggers)); // seen, blocked
}

TEST(StoryBridge, UnknownKeyReturnsFalse) {
    efl::StoryBridge bridge;
    efl::TriggerService triggers;
    EXPECT_FALSE(bridge.evaluateEligibility("no_such_cutscene", triggers));
}

TEST(StoryBridge, FireEffectsSetsFlagsAndQuest) {
    efl::StoryBridge bridge;
    efl::TriggerService triggers;

    efl::CutsceneDef def;
    def.id = "area_entry";
    def.onFire.setFlags = {"entered_cave"};
    def.onFire.startQuest = "cave_quest";
    bridge.registerCutscene(def);

    std::string startedQuest;
    bridge.onQuestStart = [&](const std::string& q) { startedQuest = q; };

    bridge.fireEffects("area_entry", triggers);
    EXPECT_TRUE(triggers.getFlag("entered_cave"));
    EXPECT_EQ(startedQuest, "cave_quest");
}
