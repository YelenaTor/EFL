#include <gtest/gtest.h>
#include "efl/registries/area_registry.h"
#include <fstream>
#include <nlohmann/json.hpp>

static nlohmann::json loadFixture(const std::string& name) {
    std::ifstream f("fixtures/" + name);
    return nlohmann::json::parse(f);
}

TEST(AreaRegistry, RegisterAndLookup) {
    efl::AreaRegistry registry;
    auto area = efl::AreaDef::fromJson(loadFixture("sample_area.json"));
    ASSERT_TRUE(area.has_value());
    registry.registerArea(*area);
    auto found = registry.getArea("crystal_cave");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->displayName, "Crystal Cave");
    EXPECT_EQ(found->backend, efl::AreaBackend::Hijacked);
    EXPECT_EQ(found->hostRoom, "rm_mine_04");
}

TEST(AreaRegistry, AreaNotFound) {
    efl::AreaRegistry registry;
    EXPECT_EQ(registry.getArea("nonexistent"), nullptr);
}

TEST(AreaRegistry, ListAreas) {
    efl::AreaRegistry registry;
    registry.registerArea({.id = "a", .displayName = "A"});
    registry.registerArea({.id = "b", .displayName = "B"});
    EXPECT_EQ(registry.allAreas().size(), 2);
}

TEST(AreaRegistry, NativeAreaParsed) {
    auto area = efl::AreaDef::fromJson(loadFixture("sample_area_native.json"));
    ASSERT_TRUE(area.has_value());
    EXPECT_EQ(area->id,          "fluffkin_hollow");
    EXPECT_EQ(area->backend,     efl::AreaBackend::Native);
    EXPECT_EQ(area->hostRoom,    "rm_forest_trigger");
    EXPECT_EQ(area->roomWidth,   1280);
    EXPECT_EQ(area->roomHeight,  720);
    EXPECT_EQ(area->music,       "mus_hollow_ambient");
    EXPECT_EQ(area->entryAnchor, "512,360");
}

TEST(AreaRegistry, DefaultRoomDimensions) {
    nlohmann::json j = {{"id", "small_room"}, {"displayName", "Small"}};
    auto area = efl::AreaDef::fromJson(j);
    ASSERT_TRUE(area.has_value());
    EXPECT_EQ(area->roomWidth,  1024);
    EXPECT_EQ(area->roomHeight, 768);
}
