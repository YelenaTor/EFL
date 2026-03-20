#include <gtest/gtest.h>
#include "efl/registries/warp_service.h"
#include "efl/core/trigger_service.h"
#include <fstream>
#include <nlohmann/json.hpp>

static nlohmann::json loadFixture(const std::string& name) {
    std::ifstream f("fixtures/" + name);
    return nlohmann::json::parse(f);
}

TEST(WarpService, RegisterAndResolve) {
    efl::WarpService svc;
    auto warp = efl::WarpDef::fromJson(loadFixture("sample_warp.json"));
    ASSERT_TRUE(warp.has_value());
    svc.registerWarp(*warp);
    auto warps = svc.warpsFrom("farm");
    ASSERT_EQ(warps.size(), 1);
    EXPECT_EQ(warps[0]->targetArea, "crystal_cave");
}

TEST(WarpService, TriggerGated) {
    efl::WarpService svc;
    efl::TriggerService triggers;
    svc.registerWarp({.id = "gated", .sourceArea = "a", .targetArea = "b",
                      .requireTrigger = "unlock_b"});
    triggers.registerTrigger({.id = "unlock_b", .type = efl::TriggerType::FlagSet,
                              .flagName = "key_b"});

    EXPECT_FALSE(svc.canWarp("gated", triggers));
    triggers.setFlag("key_b", true);
    EXPECT_TRUE(svc.canWarp("gated", triggers));
}
