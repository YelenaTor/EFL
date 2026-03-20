#include <gtest/gtest.h>
#include "efl/core/config_service.h"

TEST(ConfigService, SetAndGet) {
    efl::ConfigService cfg;
    cfg.set("mod_a", "debug", "true");
    EXPECT_EQ(cfg.getString("mod_a", "debug"), "true");
    EXPECT_TRUE(cfg.getBool("mod_a", "debug"));
}

TEST(ConfigService, DefaultValues) {
    efl::ConfigService cfg;
    EXPECT_EQ(cfg.getString("mod_a", "missing", "default"), "default");
    EXPECT_FALSE(cfg.getBool("mod_a", "missing"));
}
