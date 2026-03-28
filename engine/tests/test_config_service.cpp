#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
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

TEST(ConfigService, MissingFileIsNonFatal) {
    efl::ConfigService cfg;
    cfg.loadFromFile("nonexistent_path/config.json"); // should not throw
    EXPECT_EQ(cfg.getString("mod_a", "key", "fallback"), "fallback");
}

TEST(ConfigService, SaveAndLoadRoundTrip) {
    namespace fs = std::filesystem;
    fs::path tmpDir = fs::temp_directory_path() / "efl_cfg_test";
    fs::create_directories(tmpDir);
    std::string path = (tmpDir / "config.json").string();

    {
        efl::ConfigService cfg;
        cfg.loadFromFile(path);
        cfg.set("com.test.mod", "speed", "42");
        cfg.set("com.test.mod", "debug", "true");
    }

    efl::ConfigService cfg2;
    cfg2.loadFromFile(path);
    EXPECT_EQ(cfg2.getString("com.test.mod", "speed"), "42");
    EXPECT_EQ(cfg2.getInt("com.test.mod", "speed", 0), 42);
    EXPECT_TRUE(cfg2.getBool("com.test.mod", "debug"));

    fs::remove_all(tmpDir);
}

TEST(ConfigService, WriteThrough) {
    namespace fs = std::filesystem;
    fs::path tmpDir = fs::temp_directory_path() / "efl_cfg_wt_test";
    fs::create_directories(tmpDir);
    std::string path = (tmpDir / "config.json").string();

    efl::ConfigService cfg;
    cfg.loadFromFile(path);
    cfg.set("com.test.mod", "key", "value"); // triggers save

    std::string content;
    {
        // Scope ifstream so handle is released before remove_all
        std::ifstream f(path);
        EXPECT_TRUE(f.is_open());
        content = std::string((std::istreambuf_iterator<char>(f)), {});
    }
    EXPECT_NE(content.find("value"), std::string::npos);

    fs::remove_all(tmpDir);
}
