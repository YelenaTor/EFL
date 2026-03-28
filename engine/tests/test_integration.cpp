#include <gtest/gtest.h>
#include <algorithm>
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

TEST(Integration, ScriptHookInjectModeEmitsW002) {
    efl::EflBootstrap bootstrap;
    bootstrap.initialize("fixtures/script_hook_packs");

    const auto& diags = bootstrap.diagnostics().all();
    bool hasW002 = std::any_of(diags.begin(), diags.end(),
        [](const efl::DiagnosticEntry& e) { return e.code == "HOOK-W002"; });
    EXPECT_TRUE(hasW002) << "Expected HOOK-W002 for inject-mode script hook";
}

TEST(Integration, ScriptHookUnknownHandlerEmitsW004) {
    efl::EflBootstrap bootstrap;
    bootstrap.initialize("fixtures/script_hook_packs");

    const auto& diags = bootstrap.diagnostics().all();
    bool hasW004 = std::any_of(diags.begin(), diags.end(),
        [](const efl::DiagnosticEntry& e) { return e.code == "HOOK-W004"; });
    EXPECT_TRUE(hasW004) << "Expected HOOK-W004 for unknown handler name";
}

TEST(Integration, ScriptHookManifestParsed) {
    efl::EflBootstrap bootstrap;
    bootstrap.initialize("fixtures/script_hook_packs");

    ASSERT_FALSE(bootstrap.manifests().empty());
    const auto& hooks = bootstrap.manifests()[0].scriptHooks;
    ASSERT_EQ(hooks.size(), 3u);
    EXPECT_EQ(hooks[0].target,  "gml_Script_hoe_node");
    EXPECT_EQ(hooks[0].handler, "efl_resource_despawn");
    EXPECT_EQ(hooks[0].mode,    "callback");
    EXPECT_EQ(hooks[1].mode,    "inject");
    EXPECT_EQ(hooks[2].handler, "efl_nonexistent_handler");
}
