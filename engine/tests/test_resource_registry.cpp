#include <gtest/gtest.h>
#include "efl/registries/resource_registry.h"
#include <fstream>
#include <nlohmann/json.hpp>

static nlohmann::json loadFixture(const std::string& name) {
    std::ifstream f("fixtures/" + name);
    return nlohmann::json::parse(f);
}

TEST(ResourceRegistry, RegisterAndLookup) {
    efl::ResourceRegistry reg;
    auto res = efl::ResourceDef::fromJson(loadFixture("sample_resource.json"));
    ASSERT_TRUE(res.has_value());
    reg.registerResource(*res);
    auto found = reg.getResource("mythril_ore");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->kind, "breakable_node");
    EXPECT_EQ(found->yieldTable.size(), 2);
}

TEST(ResourceRegistry, QueryByArea) {
    efl::ResourceRegistry reg;
    auto res = efl::ResourceDef::fromJson(loadFixture("sample_resource.json"));
    reg.registerResource(*res);
    auto inCave = reg.resourcesInArea("crystal_cave");
    EXPECT_EQ(inCave.size(), 1);
    EXPECT_EQ(inCave[0]->id, "mythril_ore");
    auto inFarm = reg.resourcesInArea("farm");
    EXPECT_EQ(inFarm.size(), 0);
}

TEST(ResourceRegistry, QueryByKind) {
    efl::ResourceRegistry reg;
    reg.registerResource({.id = "r1", .kind = "breakable_node"});
    reg.registerResource({.id = "r2", .kind = "forageable"});
    reg.registerResource({.id = "r3", .kind = "breakable_node"});
    auto breakables = reg.resourcesByKind("breakable_node");
    EXPECT_EQ(breakables.size(), 2);
}

TEST(ResourceRegistry, NotFound) {
    efl::ResourceRegistry reg;
    EXPECT_EQ(reg.getResource("nope"), nullptr);
}

TEST(ResourceRegistry, DungeonVotesParsed) {
    auto res = efl::ResourceDef::fromJson(loadFixture("sample_resource_dungeon_votes.json"));
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->spawnRules.dungeonVotes.size(), 2);

    const auto& v0 = res->spawnRules.dungeonVotes[0];
    EXPECT_EQ(v0.biome, "upper_mines");
    EXPECT_EQ(v0.pool, "ore_rock");
    EXPECT_EQ(v0.weight, 5);

    const auto& v1 = res->spawnRules.dungeonVotes[1];
    EXPECT_EQ(v1.biome, "tide_caverns");
    EXPECT_EQ(v1.pool, "ore_rock");  // default
    EXPECT_EQ(v1.weight, 1);          // default
}

TEST(ResourceRegistry, ResourcesWithDungeonVotes) {
    efl::ResourceRegistry reg;
    auto with_votes = efl::ResourceDef::fromJson(loadFixture("sample_resource_dungeon_votes.json"));
    auto no_votes   = efl::ResourceDef::fromJson(loadFixture("sample_resource.json"));
    ASSERT_TRUE(with_votes.has_value());
    ASSERT_TRUE(no_votes.has_value());
    reg.registerResource(*with_votes);
    reg.registerResource(*no_votes);

    auto result = reg.resourcesWithDungeonVotes();
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]->id, "custom_ore");
}

TEST(ResourceRegistry, AnchorAndDungeonVotesCoexist) {
    nlohmann::json j = {
        {"id", "combo_ore"},
        {"kind", "harvestable_node"},
        {"spawnRules", {
            {"anchors", {{"my_cave", "3,7"}}},
            {"dungeonVotes", {{{"biome", "deep_earth"}, {"weight", 2}}}}
        }}
    };
    auto res = efl::ResourceDef::fromJson(j);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->spawnRules.anchors.count("my_cave"), 1);
    ASSERT_EQ(res->spawnRules.dungeonVotes.size(), 1);
    EXPECT_EQ(res->spawnRules.dungeonVotes[0].biome, "deep_earth");
    EXPECT_EQ(res->spawnRules.dungeonVotes[0].weight, 2);
}
