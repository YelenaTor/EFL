#include <gtest/gtest.h>
#include "efl/registries/npc_registry.h"
#include "efl/registries/dialogue_service.h"
#include <fstream>
#include <nlohmann/json.hpp>

static nlohmann::json loadFixture(const std::string& name) {
    std::ifstream f("fixtures/" + name);
    return nlohmann::json::parse(f);
}

TEST(NpcRegistry, RegisterAndLookup) {
    efl::NpcRegistry reg;
    auto npc = efl::NpcDef::fromJson(loadFixture("sample_npc.json"));
    ASSERT_TRUE(npc.has_value());
    reg.registerNpc(*npc);
    auto found = reg.getNpc("flora_spirit");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->displayName, "Flora");
    EXPECT_EQ(found->kind, "local");
}

TEST(NpcRegistry, NpcsInArea) {
    efl::NpcRegistry reg;
    reg.registerNpc({.id = "npc1", .displayName = "N1", .kind = "local", .defaultArea = "cave"});
    reg.registerNpc({.id = "npc2", .displayName = "N2", .kind = "local", .defaultArea = "farm"});
    reg.registerNpc({.id = "npc3", .displayName = "N3", .kind = "local", .defaultArea = "cave"});
    auto inCave = reg.npcsInArea("cave");
    EXPECT_EQ(inCave.size(), 2);
}

TEST(NpcRegistry, NotFound) {
    efl::NpcRegistry reg;
    EXPECT_EQ(reg.getNpc("nobody"), nullptr);
}

TEST(DialogueService, LoadAndGetEntries) {
    efl::DialogueService svc;
    auto dlg = efl::DialogueDef::fromJson(loadFixture("sample_dialogue.json"));
    ASSERT_TRUE(dlg.has_value());
    svc.registerDialogue(*dlg);
    auto found = svc.getDialogue("flora_intro");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->entries.size(), 2);
    EXPECT_EQ(found->entries[0].text, "Welcome to the Crystal Cave!");
}

TEST(DialogueService, ConditionalEntries) {
    efl::DialogueService svc;
    auto dlg = efl::DialogueDef::fromJson(loadFixture("sample_dialogue.json"));
    svc.registerDialogue(*dlg);
    // Without conditions met, only unconditional entries
    auto unconditional = svc.availableEntries("flora_intro", [](const std::string&) { return false; });
    EXPECT_EQ(unconditional.size(), 1);
    EXPECT_EQ(unconditional[0]->id, "greet");
    // With conditions met, all entries
    auto all = svc.availableEntries("flora_intro", [](const std::string&) { return true; });
    EXPECT_EQ(all.size(), 2);
}
