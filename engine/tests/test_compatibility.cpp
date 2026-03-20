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
