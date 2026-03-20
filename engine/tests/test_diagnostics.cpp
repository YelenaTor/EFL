#include <gtest/gtest.h>
#include "efl/core/diagnostics.h"

TEST(Diagnostics, EmitAndCollect) {
    efl::DiagnosticEmitter emitter;
    emitter.emit("MANIFEST-E001", efl::Severity::Error, "MANIFEST",
                 "Missing required field 'version'");
    auto all = emitter.all();
    ASSERT_EQ(all.size(), 1);
    EXPECT_EQ(all[0].code, "MANIFEST-E001");
    EXPECT_EQ(all[0].severity, efl::Severity::Error);
}

TEST(Diagnostics, CountBySeverity) {
    efl::DiagnosticEmitter emitter;
    emitter.emit("BOOT-E001", efl::Severity::Error, "BOOT", "Fatal");
    emitter.emit("HOOK-W001", efl::Severity::Warning, "HOOK", "Degraded");
    emitter.emit("AREA-H001", efl::Severity::Hazard, "AREA", "Future risk");
    EXPECT_EQ(emitter.countBySeverity(efl::Severity::Error), 1);
    EXPECT_EQ(emitter.countBySeverity(efl::Severity::Warning), 1);
    EXPECT_EQ(emitter.countBySeverity(efl::Severity::Hazard), 1);
}

TEST(Diagnostics, SeverityToWireName) {
    EXPECT_EQ(efl::severityWireName(efl::Severity::Error), "error");
    EXPECT_EQ(efl::severityWireName(efl::Severity::Warning), "warning");
    EXPECT_EQ(efl::severityWireName(efl::Severity::Hazard), "hazard");
}
