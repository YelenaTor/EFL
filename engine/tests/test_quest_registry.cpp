#include <gtest/gtest.h>
#include "efl/registries/quest_registry.h"
#include <fstream>
#include <nlohmann/json.hpp>

static nlohmann::json loadFixture(const std::string& name) {
    std::ifstream f("fixtures/" + name);
    return nlohmann::json::parse(f);
}

TEST(QuestRegistry, RegisterAndStart) {
    efl::QuestRegistry reg;
    auto quest = efl::QuestDef::fromJson(loadFixture("sample_quest.json"));
    ASSERT_TRUE(quest.has_value());
    reg.registerQuest(*quest);
    reg.startQuest("find_crystals");
    EXPECT_EQ(reg.getQuestState("find_crystals"), efl::QuestState::Active);
    EXPECT_EQ(reg.getCurrentStage("find_crystals"), "gather");
}

TEST(QuestRegistry, AdvanceStage) {
    efl::QuestRegistry reg;
    auto quest = efl::QuestDef::fromJson(loadFixture("sample_quest.json"));
    reg.registerQuest(*quest);
    reg.startQuest("find_crystals");
    reg.completeStage("find_crystals", "gather");
    EXPECT_EQ(reg.getCurrentStage("find_crystals"), "return");
}

TEST(QuestRegistry, CompleteQuest) {
    efl::QuestRegistry reg;
    auto quest = efl::QuestDef::fromJson(loadFixture("sample_quest.json"));
    reg.registerQuest(*quest);
    reg.startQuest("find_crystals");
    reg.completeStage("find_crystals", "gather");
    reg.completeStage("find_crystals", "return");
    EXPECT_EQ(reg.getQuestState("find_crystals"), efl::QuestState::Completed);
}
