// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Logging.h
//              SEVERITY enum (ref:18-27), ConsoleLogger (ref:36-48),
//              Logging static facade (ref:53-89), CategoryLogger (ref:91-246).
//
// No dedicated NativeLogging test exists in imodel-native Bentley/ test dirs
// (only iModelJsNodeAddon TS Logging.test.ts and unrelated folly/LoggingTest).
// Authored: no reference test exists in imodel-native for NativeLogging.
// Focused tests per Batch K brief §TDD:
//   - SEVERITY enum values exact
//   - ConsoleLogger SetSeverity / isSeverityEnabled round-trip (via Logging facade)
//   - Logging::LogMessage routes to Logger sink (test subclass)
//   - CategoryLogger.fatal/error/warning/info/debug/trace dispatch correct SEVERITY
//
// Note: ConsoleLogger::LogMessage/IsSeverityEnabled are PROTECTED (ref:41-42).
// Tests exercise ConsoleLogger through the public Logging facade (SetLogger
// installs it; isSeverityEnabled/LogMessage dispatch to it) and a public
// CaptureLogger subclass for routing assertions — mirroring how ref consumers
// use these classes.
#include <gtest/gtest.h>

#include <dqBase/Logging.h>

using namespace dqBase::NativeLogging;

namespace {
// RAII helper: install a logger, restore the previous one on destruction.
class ScopedLogger {
    Logger* m_prev;
public:
    explicit ScopedLogger(Logger* next) : m_prev(&Logging::GetLogger()) { Logging::SetLogger(next); }
    ~ScopedLogger() { Logging::SetLogger(m_prev); }
};

// Test Logger subclass that captures the (category, sev, msg) triple and
// (optionally) gates severity. Used to assert Logging routing.
class CaptureLogger : public Logger {
public:
    Utf8String lastCategory;
    SEVERITY   lastSeverity = LOG_NEVER;
    Utf8String lastMsg;
    int        callCount = 0;
    bool       gateAll = true; // enable everything except LOG_NEVER when true
    // Test harness: enable everything except LOG_NEVER
    bool IsSeverityEnabled(Utf8CP, SEVERITY sev) override { return gateAll && (sev != LOG_NEVER); }
    void LogMessage(Utf8CP category, SEVERITY sev, Utf8CP msg) override {
        lastCategory = category ? category : "";
        lastSeverity = sev;
        lastMsg = msg ? msg : "";
        ++callCount;
    }
};
} // namespace

// Ported from: imodel-native Logging.h:18-27 — SEVERITY values EXACT
TEST(NativeLoggingTest, SeverityValuesMatchReferenceExactly) {
    // ref: LOG_NEVER=1, LOG_FATAL=0, LOG_ERROR=-1, LOG_WARNING=-2,
    //      LOG_INFO=-3, LOG_DEBUG=-4, LOG_TRACE=-4 (alias of debug)
    EXPECT_EQ(static_cast<int>(LOG_NEVER),    1);
    EXPECT_EQ(static_cast<int>(LOG_FATAL),    0);
    EXPECT_EQ(static_cast<int>(LOG_ERROR),   -1);
    EXPECT_EQ(static_cast<int>(LOG_WARNING), -2);
    EXPECT_EQ(static_cast<int>(LOG_INFO),    -3);
    EXPECT_EQ(static_cast<int>(LOG_DEBUG),   -4);
    EXPECT_EQ(static_cast<int>(LOG_TRACE),   -4);  // alias
    EXPECT_EQ(static_cast<int>(LOG_TRACE), static_cast<int>(LOG_DEBUG));
}

// Ported from: imodel-native Logging.h:36-48 — ConsoleLogger default severity
TEST(NativeLoggingTest, ConsoleLoggerDefaultSeverityIsError) {
    // ref:45 — `SEVERITY m_defaultSeverity = LOG_ERROR;`
    EXPECT_EQ(ConsoleLogger::GetLogger().m_defaultSeverity, LOG_ERROR);
}

// Ported from: imodel-native Logging.h:36-48 — ConsoleLogger SetSeverity round-trip.
// ConsoleLogger is installed as the active Logger; the public Logging facade
// dispatches isSeverityEnabled to it. SetSeverity overrides per-category
// threshold; unknown categories fall back to m_defaultSeverity.
TEST(NativeLoggingTest, ConsoleLoggerSetSeverityRoundTrip) {
    ConsoleLogger& cl = ConsoleLogger::GetLogger();
    ScopedLogger scoped(&cl);
    cl.m_defaultSeverity = LOG_WARNING; // baseline for this test
    cl.SetSeverity("roundtrip-cat", LOG_INFO);

    // Set category explicitly enabled at INFO → INFO/WARNING/ERROR/FATAL enabled
    EXPECT_TRUE(Logging::isSeverityEnabled("roundtrip-cat", LOG_INFO));
    EXPECT_TRUE(Logging::isSeverityEnabled("roundtrip-cat", LOG_WARNING));
    EXPECT_TRUE(Logging::isSeverityEnabled("roundtrip-cat", LOG_ERROR));
    EXPECT_TRUE(Logging::isSeverityEnabled("roundtrip-cat", LOG_FATAL));
    // DEBUG/TRACE (more verbose than INFO) disabled
    EXPECT_FALSE(Logging::isSeverityEnabled("roundtrip-cat", LOG_DEBUG));
    EXPECT_FALSE(Logging::isSeverityEnabled("roundtrip-cat", LOG_TRACE));
    // LOG_NEVER can never be enabled (ref:20)
    EXPECT_FALSE(Logging::isSeverityEnabled("roundtrip-cat", LOG_NEVER));
}

// Ported from: imodel-native Logging.h:36-48 — fallback to m_defaultSeverity
TEST(NativeLoggingTest, ConsoleLoggerFallsBackToDefaultSeverityForUnknownCategory) {
    ConsoleLogger& cl = ConsoleLogger::GetLogger();
    ScopedLogger scoped(&cl);
    cl.m_defaultSeverity = LOG_ERROR;
    // Unknown category → uses m_defaultSeverity=LOG_ERROR
    EXPECT_TRUE(Logging::isSeverityEnabled("unknown-cat-xyz", LOG_ERROR));
    EXPECT_TRUE(Logging::isSeverityEnabled("unknown-cat-xyz", LOG_FATAL));
    EXPECT_FALSE(Logging::isSeverityEnabled("unknown-cat-xyz", LOG_WARNING));
    EXPECT_FALSE(Logging::isSeverityEnabled("unknown-cat-xyz", LOG_INFO));
    EXPECT_FALSE(Logging::isSeverityEnabled("unknown-cat-xyz", LOG_DEBUG));
}

// Ported from: imodel-native Logging.h:53-64 — Logging routes through Logger*
TEST(NativeLoggingTest, LoggingLogMessageRoutesToInstalledLogger) {
    CaptureLogger cap;
    ScopedLogger scoped(&cap);

    Logging::LogMessage("route-cat", LOG_WARNING, "hello route");
    ASSERT_EQ(cap.callCount, 1);
    EXPECT_EQ(cap.lastCategory, "route-cat");
    EXPECT_EQ(cap.lastSeverity, LOG_WARNING);
    EXPECT_EQ(cap.lastMsg, "hello route");
}

// Ported from: imodel-native Logging.h:69-78 — LogMessageV/Va formatting
TEST(NativeLoggingTest, LoggingLogMessageVFormatsArgs) {
    CaptureLogger cap;
    ScopedLogger scoped(&cap);

    Logging::LogMessageV("fmt-cat", LOG_INFO, "val=%d s=%s", 42, "ok");
    ASSERT_EQ(cap.callCount, 1);
    EXPECT_EQ(cap.lastSeverity, LOG_INFO);
    EXPECT_EQ(cap.lastMsg, "val=42 s=ok");
}

// Ported from: imodel-native Logging.h:61-64 — disabled severity does NOT route
TEST(NativeLoggingTest, LoggingLogMessageSuppressedWhenSeverityDisabled) {
    CaptureLogger cap;
    cap.gateAll = false; // disable everything
    ScopedLogger scoped(&cap);

    Logging::LogMessage("suppressed-cat", LOG_ERROR, "should not arrive");
    EXPECT_EQ(cap.callCount, 0);
}

// Ported from: imodel-native Logging.h:91-246 — CategoryLogger severity dispatch
TEST(NativeLoggingTest, CategoryLoggerDispatchesCorrectSeverity) {
    CaptureLogger cap;
    ScopedLogger scoped(&cap);

    CategoryLogger cat("dispatch-cat");
    cap.callCount = 0;

    cat.fatal("f");   ASSERT_EQ(cap.lastSeverity, LOG_FATAL);   EXPECT_EQ(cap.lastMsg, "f");
    cat.error("e");   ASSERT_EQ(cap.lastSeverity, LOG_ERROR);   EXPECT_EQ(cap.lastMsg, "e");
    cat.warning("w"); ASSERT_EQ(cap.lastSeverity, LOG_WARNING); EXPECT_EQ(cap.lastMsg, "w");
    cat.info("i");    ASSERT_EQ(cap.lastSeverity, LOG_INFO);    EXPECT_EQ(cap.lastMsg, "i");
    cat.debug("d");   ASSERT_EQ(cap.lastSeverity, LOG_DEBUG);   EXPECT_EQ(cap.lastMsg, "d");
    cat.trace("t");   ASSERT_EQ(cap.lastSeverity, LOG_TRACE);   EXPECT_EQ(cap.lastMsg, "t");

    // Every dispatch used m_category
    EXPECT_EQ(cap.lastCategory, "dispatch-cat");
    EXPECT_EQ(cap.callCount, 6);
}

// Ported from: imodel-native Logging.h:148-153, 167-172, ... — *v variants
TEST(NativeLoggingTest, CategoryLoggerVVariantsFormatAndDispatch) {
    CaptureLogger cap;
    ScopedLogger scoped(&cap);

    CategoryLogger cat("vcat");
    cat.fatalv("n=%d", 1);   ASSERT_EQ(cap.lastSeverity, LOG_FATAL);   EXPECT_EQ(cap.lastMsg, "n=1");
    cat.errorv("n=%d", 2);   ASSERT_EQ(cap.lastSeverity, LOG_ERROR);   EXPECT_EQ(cap.lastMsg, "n=2");
    cat.warningv("n=%d", 3); ASSERT_EQ(cap.lastSeverity, LOG_WARNING); EXPECT_EQ(cap.lastMsg, "n=3");
    cat.infov("n=%d", 4);    ASSERT_EQ(cap.lastSeverity, LOG_INFO);    EXPECT_EQ(cap.lastMsg, "n=4");
    cat.debugv("n=%d", 5);   ASSERT_EQ(cap.lastSeverity, LOG_DEBUG);   EXPECT_EQ(cap.lastMsg, "n=5");
    cat.tracev("n=%d", 6);   ASSERT_EQ(cap.lastSeverity, LOG_TRACE);   EXPECT_EQ(cap.lastMsg, "n=6");
}

// Ported from: imodel-native Logging.h:120-134 — CategoryLogger.message/messagev
TEST(NativeLoggingTest, CategoryLoggerMessageVariantsRoute) {
    CaptureLogger cap;
    ScopedLogger scoped(&cap);

    CategoryLogger cat("msg-cat");
    cat.message(LOG_ERROR, "plain");
    ASSERT_EQ(cap.callCount, 1);
    EXPECT_EQ(cap.lastSeverity, LOG_ERROR);
    EXPECT_EQ(cap.lastMsg, "plain");

    cat.messagev(LOG_WARNING, "n=%d", 7);
    ASSERT_EQ(cap.callCount, 2);
    EXPECT_EQ(cap.lastSeverity, LOG_WARNING);
    EXPECT_EQ(cap.lastMsg, "n=7");
}

// Ported from: imodel-native Logging.h:114-116 — isSeverityEnabled passthrough
TEST(NativeLoggingTest, CategoryLoggerIsSeverityEnabledPassthrough) {
    ConsoleLogger& cl = ConsoleLogger::GetLogger();
    ScopedLogger scoped(&cl);
    cl.m_defaultSeverity = LOG_INFO;
    cl.SetSeverity("enabled-cat", LOG_DEBUG);

    CategoryLogger cat("enabled-cat");
    // category set to DEBUG → DEBUG enabled
    EXPECT_TRUE(cat.isSeverityEnabled(LOG_DEBUG));
    EXPECT_TRUE(cat.isSeverityEnabled(LOG_INFO));
    EXPECT_TRUE(cat.isSeverityEnabled(LOG_ERROR));

    CategoryLogger cat2("other-cat-default-info");
    // default INFO → DEBUG disabled, INFO enabled
    EXPECT_FALSE(cat2.isSeverityEnabled(LOG_DEBUG));
    EXPECT_TRUE(cat2.isSeverityEnabled(LOG_INFO));
}

// Ported from: imodel-native Logging.h:12 — USING_NAMESPACE_DQ_BASE_LOGGING macro
TEST(NativeLoggingTest, UsingNamespaceMacroExpands) {
    USING_NAMESPACE_DQ_BASE_LOGGING
    // If the macro is correct, SEVERITY resolves unqualified inside this block.
    SEVERITY s = LOG_INFO;
    EXPECT_EQ(static_cast<int>(s), -3);
}
