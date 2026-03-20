#include <gtest/gtest.h>
#include "efl/core/save_service.h"

TEST(SaveService, SetAndGet) {
    efl::SaveService svc;
    svc.set("com.test.mod", "areas", "cave_01", {{"explored", true}});
    auto val = svc.get("com.test.mod", "areas", "cave_01");
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(val->at("explored").get<bool>());
}

TEST(SaveService, NamespaceIsolation) {
    efl::SaveService svc;
    svc.set("mod_a", "areas", "room1", {{"val", 1}});
    svc.set("mod_b", "areas", "room1", {{"val", 2}});
    EXPECT_EQ(svc.get("mod_a", "areas", "room1")->at("val"), 1);
    EXPECT_EQ(svc.get("mod_b", "areas", "room1")->at("val"), 2);
}

TEST(SaveService, SerializeDeserialize) {
    efl::SaveService svc;
    svc.set("mod", "quests", "q1", {{"stage", 3}});
    auto json = svc.serialize();
    efl::SaveService svc2;
    svc2.deserialize(json);
    EXPECT_EQ(svc2.get("mod", "quests", "q1")->at("stage"), 3);
}

TEST(SaveService, GetMissingReturnsNullopt) {
    efl::SaveService svc;
    auto val = svc.get("no_mod", "areas", "nope");
    EXPECT_FALSE(val.has_value());
}

TEST(SaveService, Remove) {
    efl::SaveService svc;
    svc.set("mod", "areas", "zone1", {{"active", true}});
    ASSERT_TRUE(svc.get("mod", "areas", "zone1").has_value());
    svc.remove("mod", "areas", "zone1");
    EXPECT_FALSE(svc.get("mod", "areas", "zone1").has_value());
}

TEST(SaveService, Clear) {
    efl::SaveService svc;
    svc.set("mod_a", "feature", "id1", {{"x", 10}});
    svc.set("mod_b", "feature", "id2", {{"y", 20}});
    svc.clear();
    EXPECT_FALSE(svc.get("mod_a", "feature", "id1").has_value());
    EXPECT_FALSE(svc.get("mod_b", "feature", "id2").has_value());
}

TEST(SaveService, OverwriteExistingEntry) {
    efl::SaveService svc;
    svc.set("mod", "save", "slot1", {{"hp", 100}});
    svc.set("mod", "save", "slot1", {{"hp", 50}});
    EXPECT_EQ(svc.get("mod", "save", "slot1")->at("hp"), 50);
}

TEST(SaveService, SerializeProducesNestedStructure) {
    efl::SaveService svc;
    svc.set("mymod", "areas", "dungeon", {{"cleared", false}});
    auto j = svc.serialize();
    EXPECT_TRUE(j.contains("mymod"));
    EXPECT_TRUE(j["mymod"].contains("areas"));
    EXPECT_TRUE(j["mymod"]["areas"].contains("dungeon"));
}
