#include <gtest/gtest.h>
#include "efl/registries/world_npc_registry.h"
#include "efl/core/save_service.h"

// ---------------------------------------------------------------------------
// WorldNpcRegistry — hearts and gifts
// ---------------------------------------------------------------------------

TEST(WorldNpcRegistry, HeartsDefaultZero) {
    efl::SaveService saves;
    efl::WorldNpcRegistry reg;
    reg.setSaveService(&saves);
    EXPECT_EQ(reg.getHearts("com.test.mod", "forest_merchant"), 0);
}

TEST(WorldNpcRegistry, AddHeartsIncrementsCorrectly) {
    efl::SaveService saves;
    efl::WorldNpcRegistry reg;
    reg.setSaveService(&saves);

    reg.addHearts("com.test.mod", "forest_merchant", 3);
    EXPECT_EQ(reg.getHearts("com.test.mod", "forest_merchant"), 3);

    reg.addHearts("com.test.mod", "forest_merchant", 2);
    EXPECT_EQ(reg.getHearts("com.test.mod", "forest_merchant"), 5);
}

TEST(WorldNpcRegistry, HeartsClampedAt10) {
    efl::SaveService saves;
    efl::WorldNpcRegistry reg;
    reg.setSaveService(&saves);

    reg.addHearts("com.test.mod", "forest_merchant", 8);
    reg.addHearts("com.test.mod", "forest_merchant", 5); // would be 13
    EXPECT_EQ(reg.getHearts("com.test.mod", "forest_merchant"), 10);
}

TEST(WorldNpcRegistry, HeartsClampedAtZero) {
    efl::SaveService saves;
    efl::WorldNpcRegistry reg;
    reg.setSaveService(&saves);

    reg.addHearts("com.test.mod", "forest_merchant", -5); // below 0
    EXPECT_EQ(reg.getHearts("com.test.mod", "forest_merchant"), 0);
}

TEST(WorldNpcRegistry, GiftRecordedAndDetectedToday) {
    efl::SaveService saves;
    efl::WorldNpcRegistry reg;
    reg.setSaveService(&saves);

    EXPECT_FALSE(reg.wasGiftedToday("com.test.mod", "forest_merchant", "2026-03-28"));

    reg.recordGift("com.test.mod", "forest_merchant", "crystal_gem", "2026-03-28");

    EXPECT_TRUE(reg.wasGiftedToday("com.test.mod", "forest_merchant", "2026-03-28"));
    EXPECT_FALSE(reg.wasGiftedToday("com.test.mod", "forest_merchant", "2026-03-29"));
}

TEST(WorldNpcRegistry, HeartsIsolatedPerNpc) {
    efl::SaveService saves;
    efl::WorldNpcRegistry reg;
    reg.setSaveService(&saves);

    reg.addHearts("com.test.mod", "npc_a", 5);
    reg.addHearts("com.test.mod", "npc_b", 3);

    EXPECT_EQ(reg.getHearts("com.test.mod", "npc_a"), 5);
    EXPECT_EQ(reg.getHearts("com.test.mod", "npc_b"), 3);
}

// ---------------------------------------------------------------------------
// WorldNpcDef parsing — new fields
// ---------------------------------------------------------------------------

TEST(WorldNpcRegistry, ParsesGiftableItemsAndHeartsPerGift) {
    nlohmann::json j = {
        {"id", "test_npc"},
        {"displayName", "Test NPC"},
        {"giftableItems", {"crystal_gem", "iron_ore"}},
        {"heartsPerGift", 2}
    };

    auto def = efl::WorldNpcDef::fromJson(j);
    ASSERT_TRUE(def.has_value());
    ASSERT_EQ(def->giftableItems.size(), 2u);
    EXPECT_EQ(def->giftableItems[0], "crystal_gem");
    EXPECT_EQ(def->giftableItems[1], "iron_ore");
    EXPECT_EQ(def->heartsPerGift, 2);
}
