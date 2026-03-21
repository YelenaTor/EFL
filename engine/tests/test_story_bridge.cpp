#include <gtest/gtest.h>
#include "efl/registries/story_bridge.h"
#include "efl/core/trigger_service.h"
#include <fstream>
#include <nlohmann/json.hpp>

static nlohmann::json loadFixture(const std::string& name) {
    std::ifstream f("fixtures/" + name);
    return nlohmann::json::parse(f);
}

TEST(StoryBridge, LoadEventDefinition) {
    efl::StoryBridge bridge;
    auto event = efl::EventDef::fromJson(loadFixture("sample_event.json"));
    ASSERT_TRUE(event.has_value());
    bridge.registerEvent(*event);
    auto found = bridge.getEvent("crystal_cave_reveal");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->mode, efl::EventMode::NativeBridge);
    EXPECT_EQ(found->commands.size(), 2);
}

TEST(StoryBridge, TriggerGatedEvent) {
    efl::StoryBridge bridge;
    efl::TriggerService triggers;
    bridge.registerEvent({.id = "gated_evt", .trigger = "unlock"});
    triggers.registerTrigger({.id = "unlock", .type = efl::TriggerType::FlagSet,
                              .flagName = "flag"});
    EXPECT_FALSE(bridge.canFire("gated_evt", triggers));
    triggers.setFlag("flag", true);
    EXPECT_TRUE(bridge.canFire("gated_evt", triggers));
}
