#include <gtest/gtest.h>
#include "efl/registries/dialogue_service.h"
#include <fstream>
#include <nlohmann/json.hpp>

static nlohmann::json loadFixture(const std::string& name) {
    std::ifstream f("fixtures/" + name);
    return nlohmann::json::parse(f);
}

TEST(DialogueService, RegisterAndRetrieve) {
    efl::DialogueService svc;
    efl::DialogueDef def;
    def.id = "test_dlg";
    def.npc = "test_npc";
    def.entries.push_back({"line1", "Hello there!", "npc_neutral", ""});

    svc.registerDialogue(def);
    auto found = svc.getDialogue("test_dlg");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, "test_dlg");
    EXPECT_EQ(found->npc, "test_npc");
    EXPECT_EQ(found->entries.size(), 1);
    EXPECT_EQ(found->entries[0].text, "Hello there!");
}

TEST(DialogueService, LookupByNpcId) {
    efl::DialogueService svc;
    svc.registerDialogue({.id = "dlg_a", .npc = "npc_1", .entries = {}});
    svc.registerDialogue({.id = "dlg_b", .npc = "npc_2", .entries = {}});

    auto a = svc.getDialogue("dlg_a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->npc, "npc_1");

    auto b = svc.getDialogue("dlg_b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->npc, "npc_2");

    EXPECT_EQ(svc.getDialogue("nonexistent"), nullptr);
}

TEST(DialogueService, ParseFromJson) {
    efl::DialogueService svc;
    auto dlg = efl::DialogueDef::fromJson(loadFixture("sample_dialogue.json"));
    ASSERT_TRUE(dlg.has_value());
    EXPECT_EQ(dlg->id, "flora_intro");
    EXPECT_EQ(dlg->npc, "flora_spirit");
    EXPECT_EQ(dlg->entries.size(), 2);

    svc.registerDialogue(*dlg);
    auto found = svc.getDialogue("flora_intro");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->entries[0].text, "Welcome to the Crystal Cave!");
    EXPECT_EQ(found->entries[1].condition, "flag:met_flora");
}

TEST(DialogueService, ParseInvalidJsonReturnsNullopt) {
    // Missing required "id" field
    nlohmann::json j = {{"npc", "someone"}};
    auto result = efl::DialogueDef::fromJson(j);
    EXPECT_FALSE(result.has_value());

    // Missing required "npc" field
    nlohmann::json j2 = {{"id", "test"}};
    auto result2 = efl::DialogueDef::fromJson(j2);
    EXPECT_FALSE(result2.has_value());
}

TEST(DialogueService, AvailableEntriesFiltering) {
    efl::DialogueService svc;
    auto dlg = efl::DialogueDef::fromJson(loadFixture("sample_dialogue.json"));
    svc.registerDialogue(*dlg);

    // Only unconditional entries when evaluator returns false
    auto unconditional = svc.availableEntries("flora_intro",
        [](const std::string&) { return false; });
    EXPECT_EQ(unconditional.size(), 1);
    EXPECT_EQ(unconditional[0]->id, "greet");

    // All entries when evaluator returns true
    auto all = svc.availableEntries("flora_intro",
        [](const std::string&) { return true; });
    EXPECT_EQ(all.size(), 2);
}

TEST(DialogueService, OverwriteExisting) {
    efl::DialogueService svc;
    svc.registerDialogue({.id = "dlg1", .npc = "npc_a", .entries = {}});

    // Re-register with same id but different npc
    svc.registerDialogue({.id = "dlg1", .npc = "npc_b", .entries = {}});
    auto found = svc.getDialogue("dlg1");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->npc, "npc_b");
}
