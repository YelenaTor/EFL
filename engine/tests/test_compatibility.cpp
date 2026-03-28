#include <gtest/gtest.h>
#include "efl/core/compatibility_service.h"

TEST(CompatibilityService, ExactMatch) {
    EXPECT_TRUE(efl::CompatibilityService::isCompatible("0.2.0", "0.2.0"));
}

TEST(CompatibilityService, NewerEflSatisfiesOlderRequirement) {
    EXPECT_TRUE(efl::CompatibilityService::isCompatible("0.3.0", "0.2.0"));
}

TEST(CompatibilityService, OlderEflFailsNewerRequirement) {
    EXPECT_FALSE(efl::CompatibilityService::isCompatible("0.1.0", "0.2.0"));
}

TEST(CompatibilityService, MajorVersionMismatch) {
    EXPECT_FALSE(efl::CompatibilityService::isCompatible("2.0.0", "1.0.0"));
}

// In EFL_STUB_SDK builds (no live Aurie runtime), all external deps are absent.
TEST(CompatibilityService, ExternalModAbsentInStubMode) {
    EXPECT_FALSE(efl::CompatibilityService::isExternalModLoaded("fields-together"));
    EXPECT_FALSE(efl::CompatibilityService::isExternalModLoaded("any-mod-id"));
}
