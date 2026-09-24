// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/Logger.ts
//              LogLevel enum (lines 21-31), isEnabled() semantics (line 256),
//              setLevel() / getLevel() (lines 167-170, 227-240)
#include <gtest/gtest.h>

#include <dqBase/Logger.h>

// Ported from: itwinjs-core core/bentley/src/Logger.ts LogLevel enum (lines 21-31)
//              enum LogLevel { Trace, Info, Warning, Error, None } — implicit 0..4
TEST(LoggerTest, LogLevelMatchesReferenceNumericValues) {
    EXPECT_EQ(static_cast<int>(dqBase::LogLevel::Trace),   0);
    EXPECT_EQ(static_cast<int>(dqBase::LogLevel::Info),    1);
    EXPECT_EQ(static_cast<int>(dqBase::LogLevel::Warning), 2);
    EXPECT_EQ(static_cast<int>(dqBase::LogLevel::Error),   3);
    EXPECT_EQ(static_cast<int>(dqBase::LogLevel::None),    4);
}

// Ported from: itwinjs-core core/bentley/src/Logger.ts
//              isEnabled() (line 256): level >= minLevel
//              setLevel(category, minLevel) (lines 167-170)
TEST(LoggerTest, IsEnabledRespectsLevelOrdering) {
    dqBase::Logger::InitializeToConsole();
    dqBase::Logger::SetLevel("test-cat", dqBase::LogLevel::Warning); // minLevel = Warning(2)
    EXPECT_FALSE(dqBase::Logger::IsEnabled("test-cat", dqBase::LogLevel::Trace));   // 0 >= 2 false
    EXPECT_FALSE(dqBase::Logger::IsEnabled("test-cat", dqBase::LogLevel::Info));    // 1 >= 2 false
    EXPECT_TRUE (dqBase::Logger::IsEnabled("test-cat", dqBase::LogLevel::Warning)); // 2 >= 2 true
    EXPECT_TRUE (dqBase::Logger::IsEnabled("test-cat", dqBase::LogLevel::Error));   // 3 >= 2 true
}
