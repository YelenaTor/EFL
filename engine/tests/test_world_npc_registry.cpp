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

TEST(WorldNpcRegistry, ParsesObjectNameAndScheduleSeconds) {
    nlohmann::json j = {
        {"id", "merchant"},
        {"displayName", "Merchant"},
        {"objectName", "par_NPC"},
        {"defaultAreaId", "town_square"},
        {"defaultAnchorId", "640,320"},
        {"schedule", nlohmann::json::array({
            {{"fromSeconds", 21600}, {"toSeconds", 43200}, {"areaId", "town_square"}, {"anchorId", "640,320"}},
            {{"fromSeconds", 46800}, {"toSeconds", 64800}, {"areaId", "market"},      {"anchorId", "800,400"}}
        })}
    };

    auto def = efl::WorldNpcDef::fromJson(j);
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->objectName, "par_NPC");
    EXPECT_EQ(def->defaultAreaId, "town_square");
    EXPECT_EQ(def->defaultAnchorId, "640,320");
    ASSERT_EQ(def->schedule.size(), 2u);
    EXPECT_EQ(def->schedule[0].fromSeconds, 21600);
    EXPECT_EQ(def->schedule[0].toSeconds,   43200);
    EXPECT_EQ(def->schedule[0].areaId,      "town_square");
    EXPECT_EQ(def->schedule[0].anchorId,    "640,320");
    EXPECT_EQ(def->schedule[1].areaId,      "market");
}

// ---------------------------------------------------------------------------
// WorldNpcRegistry — schedule evaluation
// ---------------------------------------------------------------------------

namespace {
efl::WorldNpcDef makeMerchant() {
    nlohmann::json j = {
        {"id", "merchant"},
        {"displayName", "Merchant"},
        {"objectName", "par_NPC"},
        {"defaultAreaId", "home"},
        {"defaultAnchorId", "100,200"},
        {"schedule", nlohmann::json::array({
            {{"fromSeconds", 21600}, {"toSeconds", 43200}, {"areaId", "town_square"}, {"anchorId", "640,320"}},
            {{"fromSeconds", 46800}, {"toSeconds", 64800}, {"areaId", "market"},      {"anchorId", "800,400"}}
        })}
    };
    return *efl::WorldNpcDef::fromJson(j);
}
} // namespace

TEST(WorldNpcRegistry, ActiveEntryForNpcMatchesTimeWindow) {
    efl::WorldNpcRegistry reg;
    reg.registerWorldNpc(makeMerchant());

    // Inside first window (6AM–12PM)
    const auto* entry = reg.activeEntryForNpc("merchant", 30000); // ~8:20AM
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->areaId,   "town_square");
    EXPECT_EQ(entry->anchorId, "640,320");

    // Between windows (12PM–1PM gap)
    EXPECT_EQ(reg.activeEntryForNpc("merchant", 44000), nullptr);

    // Inside second window (1:00PM–6PM)
    const auto* entry2 = reg.activeEntryForNpc("merchant", 50000);
    ASSERT_NE(entry2, nullptr);
    EXPECT_EQ(entry2->areaId,   "market");

    // After all windows — no match
    EXPECT_EQ(reg.activeEntryForNpc("merchant", 70000), nullptr);
}

TEST(WorldNpcRegistry, ActiveLocationFallsBackToDefault) {
    efl::WorldNpcRegistry reg;
    reg.registerWorldNpc(makeMerchant());

    // Gap between schedule windows → falls back to defaultAreaId/defaultAnchorId
    auto [area, anchor] = reg.activeLocationForNpc("merchant", 44000);
    EXPECT_EQ(area,   "home");
    EXPECT_EQ(anchor, "100,200");
}

TEST(WorldNpcRegistry, WorldNpcsForAreaReturnsCorrectNpcs) {
    efl::WorldNpcRegistry reg;
    reg.registerWorldNpc(makeMerchant());

    // At 8AM, merchant is in town_square
    auto npcs = reg.worldNpcsForArea("town_square", 28800);
    ASSERT_EQ(npcs.size(), 1u);
    EXPECT_EQ(npcs[0]->id, "merchant");

    // At 8AM, nothing in market
    EXPECT_TRUE(reg.worldNpcsForArea("market", 28800).empty());

    // At 3PM (54000s), merchant is in market
    npcs = reg.worldNpcsForArea("market", 54000);
    ASSERT_EQ(npcs.size(), 1u);
    EXPECT_EQ(npcs[0]->id, "merchant");
}

TEST(WorldNpcRegistry, TickScheduleFiresCallbackOnBoundaryCrossing) {
    efl::WorldNpcRegistry reg;
    reg.registerWorldNpc(makeMerchant());

    std::vector<std::tuple<std::string, std::string, std::string>> calls;
    reg.onScheduleChange = [&](const std::string& id, const std::string& area, const std::string& anchor) {
        calls.emplace_back(id, area, anchor);
    };

    // First tick at 8AM — lastKnown is empty, fires callback.
    reg.tickSchedule(28800);
    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(std::get<0>(calls[0]), "merchant");
    EXPECT_EQ(std::get<1>(calls[0]), "town_square");

    // Second tick at the same time — no change, no callback.
    reg.tickSchedule(28800);
    EXPECT_EQ(calls.size(), 1u);

    // Cross into second window (3PM).
    reg.tickSchedule(54000);
    ASSERT_EQ(calls.size(), 2u);
    EXPECT_EQ(std::get<1>(calls[1]), "market");
}

TEST(WorldNpcRegistry, SetLastKnownPreventsSpuriousCallback) {
    efl::WorldNpcRegistry reg;
    reg.registerWorldNpc(makeMerchant());

    int callCount = 0;
    reg.onScheduleChange = [&](const std::string&, const std::string&, const std::string&) {
        ++callCount;
    };

    // Simulate bootstrap spawning the NPC and updating lastKnown.
    reg.setLastKnown("merchant", "town_square", "640,320");

    // Tick at 8AM — location matches lastKnown, no callback.
    reg.tickSchedule(28800);
    EXPECT_EQ(callCount, 0);
}
