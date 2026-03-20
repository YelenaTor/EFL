#include <gtest/gtest.h>
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
