#include <gtest/gtest.h>
#include "efl/core/manifest.h"

TEST(ManifestParser, ParseValidManifest) {
    auto result = efl::ManifestParser::parseFile("fixtures/valid_manifest.efl");
    ASSERT_TRUE(result.has_value());
    auto& manifest = result.value();
    EXPECT_EQ(manifest.modId, "com.test.example");
    EXPECT_EQ(manifest.name, "Test Mod");
    EXPECT_EQ(manifest.version, "1.0.0");
    EXPECT_TRUE(manifest.features.areas);
    EXPECT_TRUE(manifest.features.warps);
    EXPECT_FALSE(manifest.features.npcs);
    EXPECT_TRUE(manifest.settings.strictMode);
}

TEST(ManifestParser, RejectInvalidManifest) {
    auto result = efl::ManifestParser::parseFile("fixtures/invalid_manifest.efl");
    EXPECT_FALSE(result.has_value());
}

TEST(ManifestParser, ParseFromString) {
    std::string json = R"({
        "schemaVersion": 2,
        "modId": "com.test.inline",
        "name": "Inline Test",
        "version": "0.1.0",
        "eflVersion": "1.0.0"
    })";
    auto result = efl::ManifestParser::parseString(json);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->modId, "com.test.inline");
}

TEST(ManifestParser, VersionCompatibility) {
    efl::Manifest m;
    m.eflVersion = "1.0.0";
    EXPECT_TRUE(efl::ManifestParser::isCompatible(m, "1.0.0"));
    EXPECT_TRUE(efl::ManifestParser::isCompatible(m, "1.1.0"));
    EXPECT_FALSE(efl::ManifestParser::isCompatible(m, "0.9.0"));
}
