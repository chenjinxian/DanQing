// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              and itwinjs-core core/bentley/src/test/Time.test.ts
// dqBase tests — DateTime 行为验证
#include <gtest/gtest.h>

#include <dqBase/DateTime.h>

#include <cmath>

using dqBase::DateTime;

// ---------------------------------------------------------------------------
// Equals/Compare 基础测试（已移植）
// ---------------------------------------------------------------------------

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, EqualsIgnoreDateTimeInfoSemanticsMatchReference)
TEST(DateTimeTest, EqualsIgnoreDateTimeInfoSemanticsMatchReference) {
    DateTime utc   (DateTime::Kind::Utc,       2012, 10, 18, 8, 30, 0, 0);
    DateTime local (DateTime::Kind::Local,     2012, 10, 18, 8, 30, 0, 0);
    DateTime unspec(DateTime::Kind::Unspecified, 2012, 10, 18, 8, 30, 0, 0);

    EXPECT_TRUE(utc.Equals(local,  true));
    EXPECT_TRUE(local.Equals(utc,  true));
    EXPECT_TRUE(utc.Equals(unspec, true));
    EXPECT_TRUE(unspec.Equals(utc, true));

    EXPECT_FALSE(utc.Equals(local,  false));
    EXPECT_FALSE(local.Equals(utc,  false));
    EXPECT_FALSE(utc.Equals(unspec, false));
    EXPECT_FALSE(unspec.Equals(utc, false));
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, EqualsWhenKindAndValueBothMatch)
TEST(DateTimeTest, EqualsWhenKindAndValueBothMatch) {
    DateTime utc1(DateTime::Kind::Utc, 2012, 10, 18, 8, 30, 0, 0);
    DateTime utc2(DateTime::Kind::Utc, 2012, 10, 18, 8, 30, 0, 0);
    EXPECT_TRUE(utc1.Equals(utc2, true));
    EXPECT_TRUE(utc1.Equals(utc2, false));
    EXPECT_TRUE(utc1.Equals(utc2));
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, DefaultArgIsFullComparison)
TEST(DateTimeTest, DefaultArgIsFullComparison) {
    DateTime utc   (DateTime::Kind::Utc,        2012, 10, 18, 8, 30);
    DateTime local (DateTime::Kind::Local,      2012, 10, 18, 8, 30);
    DateTime utc2  (DateTime::Kind::Utc,        2012, 10, 18, 8, 30);
    EXPECT_FALSE(utc.Equals(local));
    EXPECT_TRUE(utc.Equals(utc2));
}

// ---------------------------------------------------------------------------
// Component 枚举
// Ported from: imodel-native DateTime.h:40-45
// ---------------------------------------------------------------------------

TEST(DateTimeTest, ComponentEnum) {
    EXPECT_EQ(static_cast<int>(DateTime::Component::Date), 0);
    EXPECT_EQ(static_cast<int>(DateTime::Component::DateAndTime), 1);
    EXPECT_EQ(static_cast<int>(DateTime::Component::TimeOfDay), 2);
}

// ---------------------------------------------------------------------------
// CompareResult 枚举
// Ported from: imodel-native DateTime.h:67-73
// ---------------------------------------------------------------------------

TEST(DateTimeTest, CompareResultEnum) {
    // EarlierThan < Equals < LaterThan < Error 的语义
    EXPECT_NE(static_cast<int>(DateTime::CompareResult::EarlierThan),
              static_cast<int>(DateTime::CompareResult::Equals));
    EXPECT_NE(static_cast<int>(DateTime::CompareResult::Equals),
              static_cast<int>(DateTime::CompareResult::LaterThan));
}

// ---------------------------------------------------------------------------
// Info 结构
// Ported from: imodel-native DateTime.h:79-119
// ---------------------------------------------------------------------------

TEST(DateTimeTest, InfoCreateForDateTime) {
    auto info = DateTime::Info::CreateForDateTime(DateTime::Kind::Utc);
    EXPECT_TRUE(info.IsValid());
    EXPECT_EQ(info.GetKind(), DateTime::Kind::Utc);
    EXPECT_EQ(info.GetComponent(), DateTime::Component::DateAndTime);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, InfoCreateForDate)
TEST(DateTimeTest, InfoCreateForDate) {
    auto info = DateTime::Info::CreateForDate();
    EXPECT_TRUE(info.IsValid());
    EXPECT_EQ(info.GetKind(), DateTime::Kind::Unspecified);
    EXPECT_EQ(info.GetComponent(), DateTime::Component::Date);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, InfoCreateForTimeOfDay)
TEST(DateTimeTest, InfoCreateForTimeOfDay) {
    auto info = DateTime::Info::CreateForTimeOfDay();
    EXPECT_TRUE(info.IsValid());
    EXPECT_EQ(info.GetKind(), DateTime::Kind::Unspecified);
    EXPECT_EQ(info.GetComponent(), DateTime::Component::TimeOfDay);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, InfoEquality)
TEST(DateTimeTest, InfoEquality) {
    auto info1 = DateTime::Info::CreateForDateTime(DateTime::Kind::Utc);
    auto info2 = DateTime::Info::CreateForDateTime(DateTime::Kind::Utc);
    auto info3 = DateTime::Info::CreateForDate();
    EXPECT_EQ(info1, info2);
    EXPECT_NE(info1, info3);
}

// ---------------------------------------------------------------------------
// 构造函数 + IsValid
// Ported from: imodel-native DateTime.h:168-197
// ---------------------------------------------------------------------------

TEST(DateTimeTest, DefaultCtorIsInvalid) {
    DateTime dt;
    EXPECT_FALSE(dt.IsValid());
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, DateOnlyCtor)
TEST(DateTimeTest, DateOnlyCtor) {
    DateTime dt(2024, 6, 15);
    EXPECT_TRUE(dt.IsValid());
    EXPECT_EQ(dt.GetYear(), 2024);
    EXPECT_EQ(dt.GetMonth(), 6);
    EXPECT_EQ(dt.GetDay(), 15);
    EXPECT_EQ(dt.GetComponent(), DateTime::Component::Date);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, DateTimeKindCtor)
TEST(DateTimeTest, DateTimeKindCtor) {
    DateTime dt(DateTime::Kind::Utc, 2024, 6, 15, 10, 30, 45, 123);
    EXPECT_TRUE(dt.IsValid());
    EXPECT_EQ(dt.GetKind(), DateTime::Kind::Utc);
    EXPECT_EQ(dt.GetComponent(), DateTime::Component::DateAndTime);
    EXPECT_EQ(dt.GetHour(), 10);
    EXPECT_EQ(dt.GetMinute(), 30);
    EXPECT_EQ(dt.GetSecond(), 45);
    EXPECT_EQ(dt.GetMillisecond(), 123);
}

// ---------------------------------------------------------------------------
// CreateTimeOfDay
// Ported from: imodel-native DateTime.h:197
// ---------------------------------------------------------------------------

TEST(DateTimeTest, CreateTimeOfDay) {
    auto tod = DateTime::CreateTimeOfDay(14, 30, 0, 0);
    EXPECT_TRUE(tod.IsValid());
    EXPECT_TRUE(tod.IsTimeOfDay());
    EXPECT_EQ(tod.GetHour(), 14);
    EXPECT_EQ(tod.GetMinute(), 30);
}

// ---------------------------------------------------------------------------
// GetTimeOfDay
// Ported from: imodel-native DateTime.h:281
// ---------------------------------------------------------------------------

TEST(DateTimeTest, GetTimeOfDay) {
    DateTime dt(DateTime::Kind::Utc, 2024, 6, 15, 14, 30, 45, 123);
    auto tod = dt.GetTimeOfDay();
    EXPECT_TRUE(tod.IsTimeOfDay());
    EXPECT_EQ(tod.GetHour(), 14);
    EXPECT_EQ(tod.GetMinute(), 30);
    EXPECT_EQ(tod.GetSecond(), 45);
    EXPECT_EQ(tod.GetMillisecond(), 123);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, GetTimeOfDayFromDateOnlyReturnsInvalid)
TEST(DateTimeTest, GetTimeOfDayFromDateOnlyReturnsInvalid) {
    DateTime dt(2024, 6, 15);
    auto tod = dt.GetTimeOfDay();
    EXPECT_FALSE(tod.IsValid());
}

// ---------------------------------------------------------------------------
// IsLeapYear / GetMaxDay
// ---------------------------------------------------------------------------

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, IsLeapYear)
TEST(DateTimeTest, IsLeapYear) {
    EXPECT_FALSE(DateTime::IsLeapYear(2023));
    EXPECT_TRUE(DateTime::IsLeapYear(2024));
    EXPECT_FALSE(DateTime::IsLeapYear(1900));
    EXPECT_TRUE(DateTime::IsLeapYear(2000));
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, GetMaxDay)
TEST(DateTimeTest, GetMaxDay) {
    EXPECT_EQ(DateTime::GetMaxDay(2024, 1), 31);
    EXPECT_EQ(DateTime::GetMaxDay(2024, 2), 29);  // leap year
    EXPECT_EQ(DateTime::GetMaxDay(2023, 2), 28);  // non-leap
    EXPECT_EQ(DateTime::GetMaxDay(2024, 4), 30);
}

// ---------------------------------------------------------------------------
// Julian-Day 转换
// Ported from: imodel-native DateTime.cpp (Julian Day algorithms)
// ---------------------------------------------------------------------------

TEST(DateTimeTest, JulianDayRoundtrip) {
    DateTime dt(DateTime::Kind::Utc, 2024, 6, 15, 12, 0, 0, 0);
    double jd = dt.ToJulianDay();
    // Julian Day for 2024-06-15 12:00:00 UTC
    EXPECT_NEAR(jd, 2460477.0, 0.5);

    DateTime dt2 = DateTime::FromJulianDay(jd, DateTime::Kind::Utc);
    EXPECT_EQ(dt2.GetYear(), 2024);
    EXPECT_EQ(dt2.GetMonth(), 6);
    EXPECT_EQ(dt2.GetDay(), 15);
    EXPECT_EQ(dt2.GetHour(), 12);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, JulianDayDateOnly)
TEST(DateTimeTest, JulianDayDateOnly) {
    DateTime dt(2024, 6, 15);
    double jd = dt.ToJulianDayDate();
    EXPECT_NEAR(jd, 2460476.5, 0.5);  // Date-only JD
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, JulianDayEpoch)
TEST(DateTimeTest, JulianDayEpoch) {
    // Unix epoch: 1970-01-01 00:00:00 UTC = JD 2440587.5
    DateTime epoch(DateTime::Kind::Utc, 1970, 1, 1, 0, 0, 0, 0);
    EXPECT_NEAR(epoch.ToJulianDay(), 2440587.5, 0.001);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, JulianDayToUnixMilliseconds)
TEST(DateTimeTest, JulianDayToUnixMilliseconds) {
    // JD 2440587.5 = Unix epoch = 0 ms
    EXPECT_NEAR(DateTime::JulianDayToUnixMilliseconds(2440587.5), 0.0, 1.0);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, UnixMillisecondsToJulianDay)
TEST(DateTimeTest, UnixMillisecondsToJulianDay) {
    EXPECT_NEAR(DateTime::UnixMillisecondsToJulianDay(0), 2440587.5, 0.001);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, JulianDayRoundtripDate)
TEST(DateTimeTest, JulianDayRoundtripDate) {
    DateTime dt(2000, 1, 1);
    double jd = dt.ToJulianDayDate();
    DateTime dt2 = DateTime::FromJulianDayDate(jd);
    EXPECT_EQ(dt2.GetYear(), 2000);
    EXPECT_EQ(dt2.GetMonth(), 1);
    EXPECT_EQ(dt2.GetDay(), 1);
}

// ---------------------------------------------------------------------------
// ToMillisecondsSinceMidnight / MsecToRationalDay / RationalDayToMsec
// Ported from: imodel-native DateTime.h
// ---------------------------------------------------------------------------

TEST(DateTimeTest, ToMillisecondsSinceMidnight) {
    DateTime dt(DateTime::Kind::Utc, 2024, 1, 1, 1, 0, 0, 0);  // 1:00:00.000
    EXPECT_EQ(dt.ToMillisecondsSinceMidnight(), 3600000);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, MsecToRationalDay)
TEST(DateTimeTest, MsecToRationalDay) {
    // 43200000 ms = 12:00:00 = 0.5 day
    EXPECT_DOUBLE_EQ(DateTime::MsecToRationalDay(43200000), 0.5);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, RationalDayToMsec)
TEST(DateTimeTest, RationalDayToMsec) {
    EXPECT_EQ(DateTime::RationalDayToMsec(0.5), 43200000);
}

// ---------------------------------------------------------------------------
// Compare
// Ported from: imodel-native DateTime.h:232
// ---------------------------------------------------------------------------

TEST(DateTimeTest, CompareEarlier) {
    DateTime a(DateTime::Kind::Utc, 2024, 1, 1, 0, 0);
    DateTime b(DateTime::Kind::Utc, 2024, 6, 1, 0, 0);
    EXPECT_EQ(DateTime::Compare(a, b), DateTime::CompareResult::EarlierThan);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, CompareLater)
TEST(DateTimeTest, CompareLater) {
    DateTime a(DateTime::Kind::Utc, 2024, 6, 1, 0, 0);
    DateTime b(DateTime::Kind::Utc, 2024, 1, 1, 0, 0);
    EXPECT_EQ(DateTime::Compare(a, b), DateTime::CompareResult::LaterThan);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, CompareEqual)
TEST(DateTimeTest, CompareEqual) {
    DateTime a(DateTime::Kind::Utc, 2024, 6, 15, 12, 0, 0, 0);
    DateTime b(DateTime::Kind::Utc, 2024, 6, 15, 12, 0, 0, 0);
    EXPECT_EQ(DateTime::Compare(a, b), DateTime::CompareResult::Equals);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, CompareInvalidReturnsError)
TEST(DateTimeTest, CompareInvalidReturnsError) {
    DateTime valid(DateTime::Kind::Utc, 2024, 1, 1, 0, 0);
    DateTime invalid;
    EXPECT_EQ(DateTime::Compare(valid, invalid), DateTime::CompareResult::Error);
}

// ---------------------------------------------------------------------------
// ToString / ToTimestampString
// ---------------------------------------------------------------------------

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, ToString)
TEST(DateTimeTest, ToString) {
    DateTime dt(DateTime::Kind::Utc, 2024, 6, 15, 12, 30, 45, 123);
    std::string s;
    EXPECT_TRUE(dt.ToString(s));
    EXPECT_EQ(s, "2024-06-15T12:30:45.123Z");
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, ToTimestampString)
TEST(DateTimeTest, ToTimestampString) {
    DateTime dt(DateTime::Kind::Utc, 2024, 6, 15, 12, 30, 45, 123);
    std::string s;
    EXPECT_TRUE(dt.ToTimestampString(s));
    EXPECT_EQ(s, "2024-06-15 12:30:45.123");
}

// ---------------------------------------------------------------------------
// FromUnixMilliseconds / ToUnixMilliseconds
// ---------------------------------------------------------------------------

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/DateTime_Tests.cpp
//              TEST(DateTimeTest, UnixMillisecondsRoundtrip)
TEST(DateTimeTest, UnixMillisecondsRoundtrip) {
    // 使用已知的 Unix 时间戳验证往返一致性
    // 2024-01-01 00:00:00 UTC = 1704067200000 ms
    DateTime dt = DateTime::FromUnixMilliseconds(1704067200000ULL, DateTime::Kind::Utc);
    EXPECT_EQ(dt.GetYear(), 2024);
    EXPECT_EQ(dt.GetMonth(), 1);
    EXPECT_EQ(dt.GetDay(), 1);
    EXPECT_EQ(dt.GetHour(), 0);
    int64_t ms = dt.ToUnixMilliseconds();
    // 往返一致性（mktime 可能受本地时区影响，但 FromUnixMilliseconds 用 gmtime_r）
    DateTime dt2 = DateTime::FromUnixMilliseconds(static_cast<uint64_t>(ms), DateTime::Kind::Utc);
    EXPECT_EQ(dt2.GetYear(), 2024);
    EXPECT_EQ(dt2.GetMonth(), 1);
    EXPECT_EQ(dt2.GetDay(), 1);
}

// ---------------------------------------------------------------------------
// m_year int16_t (BCE support)
// Ported from: imodel-native DateTime.h:123
// ---------------------------------------------------------------------------

TEST(DateTimeTest, NegativeYearIsBCE) {
    // -4713 is the earliest supported year in the Julian Day system
    DateTime dt(DateTime::Kind::Utc, -4713, 11, 24, 12, 0, 0, 0);
    EXPECT_TRUE(dt.IsValid());
    EXPECT_EQ(dt.GetYear(), -4713);
}
