// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/DateTime.h
//              and iModelCore/Bentley/src/DateTime.cpp
// DanQing dqBase — DateTime 实现
#include "dqBase/DateTime.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// 私有构造函数 + 验证
// Ported from: imodel-native DateTime.h:146-163
// ---------------------------------------------------------------------------

DateTime::DateTime(Info const& info, int16_t year, uint8_t month, uint8_t day,
                   uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond)
    : m_info(info), m_year(year), m_month(month), m_day(day)
    , m_hour(hour), m_minute(minute), m_second(second), m_millisecond(millisecond)
{
    if (!IsValidDateTime(info, year, month, day, hour, minute, second, millisecond)) {
        DqAssert(false && "Invalid parameters to DateTime ctor");
        m_info = Info(); // 使 DateTime 无效
    }
}

bool DateTime::IsValidDateTime(Info const& info, int16_t year, uint8_t month, uint8_t day,
                               uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond) {
    if (!info.IsValid() || month < 1 || month > 12 || day < 1 || day > GetMaxDay(static_cast<uint16_t>(year), month) ||
        hour > 24 || minute > 59 || second > 59 || millisecond > 999)
        return false;
    return hour < 24 || (minute == 0 && second == 0 && millisecond == 0);
}

// ---------------------------------------------------------------------------
// 静态工厂
// ---------------------------------------------------------------------------

DateTime DateTime::CreateTimeOfDay(uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond) {
    return DateTime(Info::CreateForTimeOfDay(), s_timeOfDayDummyYear, s_timeOfDayDummyMonth, s_timeOfDayDummyDay,
                    hour, minute, second, millisecond);
}

DateTime DateTime::GetCurrentTimeUtc() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm utc;
#if defined(_WIN32)
    gmtime_s(&utc, &time_t_now);
#else
    gmtime_r(&time_t_now, &utc);
#endif
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    return DateTime(Kind::Utc,
                    static_cast<int16_t>(utc.tm_year + 1900),
                    static_cast<uint8_t>(utc.tm_mon + 1),
                    static_cast<uint8_t>(utc.tm_mday),
                    static_cast<uint8_t>(utc.tm_hour),
                    static_cast<uint8_t>(utc.tm_min),
                    static_cast<uint8_t>(utc.tm_sec),
                    static_cast<uint16_t>(ms.count()));
}

DateTime DateTime::GetCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm local;
#if defined(_WIN32)
    localtime_s(&local, &time_t_now);
#else
    localtime_r(&time_t_now, &local);
#endif
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    return DateTime(Kind::Local,
                    static_cast<int16_t>(local.tm_year + 1900),
                    static_cast<uint8_t>(local.tm_mon + 1),
                    static_cast<uint8_t>(local.tm_mday),
                    static_cast<uint8_t>(local.tm_hour),
                    static_cast<uint8_t>(local.tm_min),
                    static_cast<uint8_t>(local.tm_sec),
                    static_cast<uint16_t>(ms.count()));
}

DateTime DateTime::FromUnixMilliseconds(uint64_t millis, Kind kind) {
    auto time_t_val = static_cast<time_t>(millis / 1000);
    struct tm t;
#if defined(_WIN32)
    gmtime_s(&t, &time_t_val);
#else
    gmtime_r(&time_t_val, &t);
#endif
    return DateTime(kind,
                    static_cast<int16_t>(t.tm_year + 1900),
                    static_cast<uint8_t>(t.tm_mon + 1),
                    static_cast<uint8_t>(t.tm_mday),
                    static_cast<uint8_t>(t.tm_hour),
                    static_cast<uint8_t>(t.tm_min),
                    static_cast<uint8_t>(t.tm_sec),
                    static_cast<uint16_t>(millis % 1000));
}

// ---------------------------------------------------------------------------
// 查询
// ---------------------------------------------------------------------------

DateTime::DayOfWeek DateTime::GetDayOfWeek() const {
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = m_year - 1900;
    t.tm_mon = m_month - 1;
    t.tm_mday = m_day;
    mktime(&t);
    return static_cast<DayOfWeek>(t.tm_wday);
}

uint16_t DateTime::GetDayOfYear() const {
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = m_year - 1900;
    t.tm_mon = m_month - 1;
    t.tm_mday = m_day;
    mktime(&t);
    return static_cast<uint16_t>(t.tm_yday + 1);
}

DateTime DateTime::GetTimeOfDay() const {
    return (IsValid() && m_info.GetComponent() != Component::Date)
        ? CreateTimeOfDay(m_hour, m_minute, m_second, m_millisecond)
        : DateTime();
}

// ---------------------------------------------------------------------------
// 转换
// ---------------------------------------------------------------------------

int64_t DateTime::ToUnixMilliseconds() const {
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = m_year - 1900;
    t.tm_mon = m_month - 1;
    t.tm_mday = m_day;
    t.tm_hour = m_hour;
    t.tm_min = m_minute;
    t.tm_sec = m_second;
    t.tm_isdst = 0;
#if defined(_WIN32)
    time_t seconds = _mkgmtime(&t);
#else
    time_t seconds = timegm(&t);
#endif
    return static_cast<int64_t>(seconds) * 1000 + m_millisecond;
}

bool DateTime::ToString(std::string& result) const {
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02u-%02uT%02u:%02u:%02u.%03uZ",
             m_year, m_month, m_day, m_hour, m_minute, m_second, m_millisecond);
    result = buf;
    return true;
}

bool DateTime::ToTimestampString(std::string& result) const {
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02u-%02u %02u:%02u:%02u.%03u",
             m_year, m_month, m_day, m_hour, m_minute, m_second, m_millisecond);
    result = buf;
    return true;
}

DateTime DateTime::ToUtc() const {
    if (m_info.GetKind() == Kind::Utc) return *this;
    // 简化：使用系统时区转换
    int64_t ms = ToUnixMilliseconds();
    return FromUnixMilliseconds(static_cast<uint64_t>(ms), Kind::Utc);
}

DateTime DateTime::ToLocalTime() const {
    if (m_info.GetKind() == Kind::Local) return *this;
    int64_t ms = ToUnixMilliseconds();
    return FromUnixMilliseconds(static_cast<uint64_t>(ms), Kind::Local);
}

// ---------------------------------------------------------------------------
// Julian-Day 转换
// Ported from: imodel-native DateTime.cpp (Julian Day algorithms)
// ---------------------------------------------------------------------------

// Julian Day Number 从年月日计算（Fliegel-Van Flandern 算法）
static double DateToJulianDay(int16_t year, uint8_t month, uint8_t day) {
    double y = year, m = month, d = day;
    // 调整 1/2 月为前一年的 13/14 月
    if (m <= 2) { y -= 1; m += 12; }
    double A = floor(y / 100.0);
    double B = 2 - A + floor(A / 4.0);
    return floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;
}

static void JulianDayToDate(double jd, int16_t& year, uint8_t& month, uint8_t& day) {
    double Z = floor(jd + 0.5);
    double F = jd + 0.5 - Z;
    double A;
    if (Z < 2299161) {
        A = Z;
    } else {
        double alpha = floor((Z - 1867216.25) / 36524.25);
        A = Z + 1 + alpha - floor(alpha / 4.0);
    }
    double B = A + 1524;
    double C = floor((B - 122.1) / 365.25);
    double D = floor(365.25 * C);
    double E = floor((B - D) / 30.6001);

    day = static_cast<uint8_t>(B - D - floor(30.6001 * E) + F);
    month = static_cast<uint8_t>(E < 14 ? E - 1 : E - 13);
    year = static_cast<int16_t>(month > 2 ? C - 4716 : C - 4715);
}

double DateTime::ToJulianDay() const {
    double jd = DateToJulianDay(m_year, m_month, m_day);
    // 添加时间部分（转换为一天的小数）
    double timeFraction = (m_hour + m_minute / 60.0 + (m_second + m_millisecond / 1000.0) / 3600.0) / 24.0;
    return jd + timeFraction;
}

double DateTime::ToJulianDayDate() const {
    return DateToJulianDay(m_year, m_month, m_day);
}

DateTime DateTime::FromJulianDay(double julianDay, Kind kind) {
    double jd = julianDay;
    // 提取时间部分
    double jdFloor = floor(jd + 0.5);
    double timeFrac = jd + 0.5 - jdFloor;
    int totalSec = static_cast<int>(timeFrac * 86400.0 + 0.5);
    uint8_t hour = static_cast<uint8_t>(totalSec / 3600);
    uint8_t minute = static_cast<uint8_t>((totalSec % 3600) / 60);
    uint8_t second = static_cast<uint8_t>(totalSec % 60);

    int16_t year; uint8_t month, day;
    JulianDayToDate(jd, year, month, day);

    return DateTime(kind, year, month, day, hour, minute, second, 0);
}

DateTime DateTime::FromJulianDayDate(double julianDayDate) {
    int16_t year; uint8_t month, day;
    JulianDayToDate(julianDayDate, year, month, day);
    return DateTime(year, month, day);
}

double DateTime::JulianDayToUnixMilliseconds(double julianDay) {
    // Julian Day 2440587.5 = Unix epoch (1970-01-01 00:00:00 UTC)
    return (julianDay - 2440587.5) * 86400000.0;
}

double DateTime::UnixMillisecondsToJulianDay(double unixMilliseconds) {
    return unixMilliseconds / 86400000.0 + 2440587.5;
}

int32_t DateTime::ToMillisecondsSinceMidnight() const {
    return static_cast<int32_t>(m_hour) * 3600000 +
           static_cast<int32_t>(m_minute) * 60000 +
           static_cast<int32_t>(m_second) * 1000 +
           static_cast<int32_t>(m_millisecond);
}

double DateTime::MsecToRationalDay(int32_t msec) {
    return static_cast<double>(msec) / 86400000.0;
}

int32_t DateTime::RationalDayToMsec(double rationalDay) {
    return static_cast<int32_t>(rationalDay * 86400000.0 + 0.5);
}

int32_t DateTime::ComputeOffsetToUtcInMsec() const {
    if (m_info.GetKind() == Kind::Utc) return 0;
    if (m_info.GetKind() == Kind::Unspecified) return 0;
    // Local → 计算本地时区偏移
    time_t rawtime = static_cast<time_t>(ToUnixMilliseconds() / 1000);
    struct tm utc_tm, local_tm;
#if defined(_WIN32)
    gmtime_s(&utc_tm, &rawtime);
    localtime_s(&local_tm, &rawtime);
#else
    gmtime_r(&rawtime, &utc_tm);
    localtime_r(&rawtime, &local_tm);
#endif
    // 计算 UTC 和本地时间的差异
    time_t utc_time = mktime(&utc_tm);
    time_t local_time = mktime(&local_tm);
    return static_cast<int32_t>(difftime(local_time, utc_time) * 1000.0);
}

// ---------------------------------------------------------------------------
// 比较
// ---------------------------------------------------------------------------

bool DateTime::Equals(const DateTime& rhs, bool ignoreDateTimeInfo) const {
    if (!IsValid() || !rhs.IsValid())
        return IsValid() == rhs.IsValid();
    return (ignoreDateTimeInfo || m_info == rhs.m_info) &&
           m_year == rhs.m_year && m_month == rhs.m_month && m_day == rhs.m_day &&
           m_hour == rhs.m_hour && m_minute == rhs.m_minute && m_second == rhs.m_second &&
           m_millisecond == rhs.m_millisecond;
}

// Ported from: imodel-native DateTime.cpp Compare
DateTime::CompareResult DateTime::Compare(const DateTime& lhs, const DateTime& rhs) {
    if (!lhs.IsValid() || !rhs.IsValid())
        return CompareResult::Error;
    // 使用 Julian Day 比较（对齐参考）
    double lhsJd = lhs.ToJulianDay();
    double rhsJd = rhs.ToJulianDay();
    if (lhsJd < rhsJd) return CompareResult::EarlierThan;
    if (lhsJd > rhsJd) return CompareResult::LaterThan;
    return CompareResult::Equals;
}

// ---------------------------------------------------------------------------
// 工具
// ---------------------------------------------------------------------------

bool DateTime::IsLeapYear(uint16_t year) noexcept {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

uint8_t DateTime::GetMaxDay(uint16_t year, uint8_t month) noexcept {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && IsLeapYear(year)) return 29;
    return days[month - 1];
}

END_DQ_BASE_NAMESPACE
