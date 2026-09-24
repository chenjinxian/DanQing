// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeConsole.h
//              Sink contract: Printf routes through the logging system (ref uses
//              NativeLogging::Logging::LogMessageV("BeConsole", LOG_DEBUG, ...)),
//              NOT raw stdout. DanQing's available sink is Logger::LogTrace.
//
//              No reference BeConsole unit test exists in imodel-native
//              (find ... -iname "BeConsole*test*" -> none), so the assertions
//              below are Authored against the documented sink contract above.
#include <gtest/gtest.h>

#include <dqBase/BeConsole.h>
#include <dqBase/Logger.h>

#include <cstring>
#include <string>

// Authored: no reference test exists in imodel-native for BeConsole.
// Verifies the routing contract from BeConsole.h: Printf must dispatch through
// the Logger (category "BeConsole"), not raw stdout. Today it goes to stdout,
// so the capture callback never fires -> RED until the sink is fixed.
TEST(BeConsoleTest, PrintfRoutesThroughLoggerNotStdout) {
    using dqBase::BeConsole;
    using dqBase::Logger;
    using dqBase::LogLevel;

    Logger::Initialize();

    std::string capturedCategory;
    std::string capturedMessage;
    bool invoked = false;

    Logger::SetLogFunction(
        [&](const char* category, const char* message) {
            capturedCategory = category ? category : "";
            capturedMessage = message ? message : "";
            invoked = true;
        });

    // Trace(0) >= minLevel only when minLevel is Trace; default minLevel is None(4).
    // Ref LOG_DEBUG corresponds to DanQing Trace, so enable Trace for "BeConsole".
    Logger::SetLevel("BeConsole", LogLevel::Trace);

    BeConsole::Printf("test %d", 42);

    EXPECT_TRUE(invoked) << "Printf must route through Logger (BeConsole category)";
    EXPECT_EQ(capturedCategory, "BeConsole");
    EXPECT_NE(capturedMessage.find("test 42"), std::string::npos)
        << "formatted message must reach the logger; got: \"" << capturedMessage << "\"";

    // Teardown: restore Logger to a clean state so other tests are unaffected.
    Logger::SetLevel("BeConsole", LogLevel::None);
    Logger::SetLogFunction(nullptr);
}

// Authored: no reference test exists in imodel-native for BeConsole.
// Ref BeConsole::Printf returns int (returns 1). VPrintf is a DanQing-only
// convenience; once routed through Logger it must also dispatch to the callback.
TEST(BeConsoleTest, VPrintfAlsoRoutesThroughLogger) {
    using dqBase::BeConsole;
    using dqBase::Logger;
    using dqBase::LogLevel;

    Logger::Initialize();

    std::string capturedCategory;
    std::string capturedMessage;
    bool invoked = false;

    Logger::SetLogFunction(
        [&](const char* category, const char* message) {
            capturedCategory = category ? category : "";
            capturedMessage = message ? message : "";
            invoked = true;
        });
    Logger::SetLevel("BeConsole", LogLevel::Trace);

    // Exercise VPrintf via a variadic wrapper.
    auto callVPrintf = [](const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        BeConsole::VPrintf(fmt, args);
        va_end(args);
    };
    callVPrintf("hello %s", "world");

    EXPECT_TRUE(invoked) << "VPrintf must route through Logger (BeConsole category)";
    EXPECT_EQ(capturedCategory, "BeConsole");
    EXPECT_NE(capturedMessage.find("hello world"), std::string::npos)
        << "formatted message must reach the logger; got: \"" << capturedMessage << "\"";

    Logger::SetLevel("BeConsole", LogLevel::None);
    Logger::SetLogFunction(nullptr);
}
