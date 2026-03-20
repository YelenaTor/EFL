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
