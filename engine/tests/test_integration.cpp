#include <gtest/gtest.h>
#include "efl/core/bootstrap.h"
#include "efl/core/registry_service.h"
#include "efl/core/diagnostics.h"

// Integration tests: full content pack loading pipeline
// Working directory for tests is engine/tests/ (set by CMakeLists.txt gtest_discover_tests)

TEST(Integration, LoadContentPack) {
    efl::EflBootstrap bootstrap;
    bool ok = bootstrap.initialize("fixtures/sample_packs");
    EXPECT_TRUE(ok);

    auto& reg = bootstrap.registries();

    // Area loaded
    EXPECT_NE(reg.areas().getArea("crystal_cave"), nullptr);

    // Warp loaded
    EXPECT_NE(reg.warps().getWarp("farm_to_cave"), nullptr);

    // Resource loaded
    EXPECT_NE(reg.resources().getResource("mythril_ore"), nullptr);

    // Quest loaded
    EXPECT_NE(reg.quests().getQuest("find_crystals"), nullptr);

    // NPC loaded
    EXPECT_NE(reg.npcs().getNpc("flora_spirit"), nullptr);

    // WorldNpc loaded
    EXPECT_NE(reg.worldNpcs().getWorldNpc("forest_merchant"), nullptr);

    // Recipe loaded
    EXPECT_NE(reg.crafting().getRecipe("crystal_pickaxe"), nullptr);

    // Dialogue loaded
    EXPECT_NE(reg.dialogue().getDialogue("flora_intro"), nullptr);

    // Event loaded
    EXPECT_NE(reg.story().getEvent("crystal_cave_reveal"), nullptr);
}

TEST(Integration, TriggerRegisteredFromPack) {
    efl::EflBootstrap bootstrap;
    bool ok = bootstrap.initialize("fixtures/sample_packs");
    EXPECT_TRUE(ok);

    // Trigger 'has_cave_key' should be registered (but flag not set, so evaluates false)
    // The important thing is it doesn't throw — it was registered
    auto& triggers = bootstrap.registries().triggers();
    EXPECT_FALSE(triggers.evaluate("has_cave_key")); // flag not set
    triggers.setFlag("player_has_cave_key", true);
    EXPECT_TRUE(triggers.evaluate("has_cave_key"));
}

TEST(Integration, DiagnosticsForInvalidContent) {
    efl::EflBootstrap bootstrap;
    bootstrap.initialize("fixtures/invalid_packs");
    // An invalid manifest (missing required fields) should produce at least one error
    EXPECT_GT(bootstrap.diagnostics().countBySeverity(efl::Severity::Error), 0);
}

TEST(Integration, MissingContentDirIsNonFatal) {
    efl::EflBootstrap bootstrap;
    bool ok = bootstrap.initialize("fixtures/nonexistent_content_pack");
    // Missing directory is a warning, not a fatal error
    EXPECT_TRUE(ok);
    EXPECT_EQ(bootstrap.diagnostics().countBySeverity(efl::Severity::Error), 0);
}
