#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "efl/core/log_service.h"

TEST(LogService, InfoWritesToBuffer) {
    efl::LogService log;
    log.info("BOOT", "Starting EFL");
    auto entries = log.recent(10);
    ASSERT_EQ(entries.size(), 1);
    EXPECT_EQ(entries[0].level, efl::LogLevel::Info);
    EXPECT_EQ(entries[0].category, "BOOT");
    EXPECT_EQ(entries[0].message, "Starting EFL");
}

TEST(LogService, WarnAndError) {
    efl::LogService log;
    log.warn("HOOK", "Target may have changed");
    log.error("MANIFEST", "Missing required field");
    auto entries = log.recent(10);
    ASSERT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].level, efl::LogLevel::Warn);
    EXPECT_EQ(entries[1].level, efl::LogLevel::Error);
}

TEST(LogService, BufferRespectsBound) {
    efl::LogService log(5);
    for (int i = 0; i < 10; i++) {
        log.info("TEST", "msg " + std::to_string(i));
    }
    auto entries = log.recent(100);
    EXPECT_EQ(entries.size(), 5);
}

TEST(LogService, FileOutputNotOpenByDefault) {
    efl::LogService log;
    EXPECT_FALSE(log.isFileOutputOpen());
}

TEST(LogService, SetFileOutputOpensFile) {
    namespace fs = std::filesystem;
    fs::path tmpDir = fs::temp_directory_path() / "efl_test_logs";
    fs::create_directories(tmpDir);
    std::string logPath = (tmpDir / "test_efl.log").string();

    std::string content;
    {
        efl::LogService log;
        EXPECT_FALSE(log.isFileOutputOpen());
        log.setFileOutput(logPath);
        EXPECT_TRUE(log.isFileOutputOpen());

        log.info("TEST", "file output test");

        std::ifstream f(logPath);
        ASSERT_TRUE(f.is_open());
        content = std::string((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    } // log and f both close here, releasing file handles

    EXPECT_NE(content.find("file output test"), std::string::npos);
    fs::remove_all(tmpDir);
}
