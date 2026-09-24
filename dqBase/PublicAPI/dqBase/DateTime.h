// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/DateTime.h
//              itwinjs-core core/bentley/src/Time.ts
// DanQing dqBase — 日期时间
//
// 1:1 对齐 imodel-native DateTime 类。
// 补全: Component/CompareResult 枚举、Info 元数据结构、Julian-Day 转换家族。
#pragma once

#include "Export.h"
#include "BeAssert.h"

#include <cstdint>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// DateTime — 日期时间（对齐 imodel-native DateTime）
// Ported from: imodel-native DateTime.h
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT DateTime {
public:
    /// 日期时间类型（Kind）
    /// Ported from: imodel-native DateTime.h:28-33
    enum class Kind : int {
        Unspecified = 0,
        Utc         = 1,
        Local       = 2,
    };

    /// 日期时间组件类型
    /// Ported from: imodel-native DateTime.h:40-45
    enum class Component : int {
        Date        = 0,  // 仅日期（无时间）
        DateAndTime = 1,  // 日期 + 时间
        TimeOfDay   = 2,  // 仅时间（无日期）
    };

    /// 星期
    /// Ported from: imodel-native DateTime.h:51-60
    enum class DayOfWeek : int {
        Sunday    = 0,
        Monday    = 1,
        Tuesday   = 2,
        Wednesday = 3,
        Thursday  = 4,
        Friday    = 5,
        Saturday  = 6,
    };

    /// 比较结果
    /// Ported from: imodel-native DateTime.h:67-73
    enum class CompareResult : int {
        EarlierThan,  // lhs 早于 rhs
        Equals,       // 相等
        LaterThan,    // lhs 晚于 rhs
        Error,        // 比较失败
    };

    // ---------------------------------------------------------------------------
    // Info — DateTime 元数据
    // Ported from: imodel-native DateTime.h:79-119
    // ---------------------------------------------------------------------------
    struct Info {
    private:
        Kind m_kind = Kind::Unspecified;
        Component m_component = Component::DateAndTime;
        bool m_isValid = false;

        Info(Kind kind, Component component) : m_kind(kind), m_component(component), m_isValid(true) {}

    public:
        Info() {}

        static Info CreateForDateTime(Kind kind) { return Info(kind, Component::DateAndTime); }
        static Info CreateForDate() { return Info(Kind::Unspecified, Component::Date); }
        static Info CreateForTimeOfDay() { return Info(Kind::Unspecified, Component::TimeOfDay); }

        bool operator==(Info const& rhs) const { return m_isValid == rhs.m_isValid && m_kind == rhs.m_kind && m_component == rhs.m_component; }
        bool operator!=(Info const& rhs) const { return !(*this == rhs); }

        bool IsValid() const { return m_isValid; }
        Kind GetKind() const { return m_kind; }
        Component GetComponent() const { return m_component; }
    };

private:
    // TimeOfDay 使用的虚拟日期
    static constexpr uint16_t s_timeOfDayDummyYear = 2000;
    static constexpr uint8_t s_timeOfDayDummyMonth = 1;
    static constexpr uint8_t s_timeOfDayDummyDay = 1;

    Info m_info;
    int16_t m_year = 0;      // 负值表示 BCE（对齐参考 int16_t）
    uint8_t m_month = 0;
    uint8_t m_day = 0;
    uint8_t m_hour = 0;
    uint8_t m_minute = 0;
    uint8_t m_second = 0;
    uint16_t m_millisecond = 0;

    DateTime(Info const& info, int16_t year, uint8_t month, uint8_t day,
             uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond);

    static bool IsValidDateTime(Info const& info, int16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond);

public:
    // --- 构造 ---
    DateTime() {}

    /// 构造仅日期 DateTime
    /// Ported from: imodel-native DateTime.h:177
    DateTime(int16_t year, uint8_t month, uint8_t day)
        : DateTime(Info::CreateForDate(), year, month, day, 0, 0, 0, 0) {}

    /// 构造日期+时间 DateTime
    /// Ported from: imodel-native DateTime.h:188-189
    DateTime(Kind kind, int16_t year, uint8_t month, uint8_t day,
             uint8_t hour, uint8_t minute, uint8_t second = 0, uint16_t millisecond = 0)
        : DateTime(Info::CreateForDateTime(kind), year, month, day, hour, minute, second, millisecond) {}

    // --- 静态工厂 ---
    /// 创建仅时间 DateTime
    /// Ported from: imodel-native DateTime.h:197
    static DateTime CreateTimeOfDay(uint8_t hour, uint8_t minute, uint8_t second = 0, uint16_t millisecond = 0);

    static DateTime GetCurrentTimeUtc();
    static DateTime GetCurrentTime();
    static DateTime FromUnixMilliseconds(uint64_t millis, Kind kind = Kind::Utc);

    /// 从 Julian Day 创建 DateTime
    /// Ported from: imodel-native DateTime.h (FromJulianDay)
    static DateTime FromJulianDay(double julianDay, Kind kind = Kind::Utc);

    /// 从 Julian Day 创建仅日期 DateTime
    /// Ported from: imodel-native DateTime.h (FromJulianDayDate)
    static DateTime FromJulianDayDate(double julianDayDate);

    // --- 查询 ---
    bool IsValid() const { return m_info.IsValid(); }
    bool IsTimeOfDay() const { return IsValid() && m_info.GetComponent() == Component::TimeOfDay; }
    Info const& GetInfo() const { return m_info; }
    Kind GetKind() const { return m_info.GetKind(); }
    Component GetComponent() const { return m_info.GetComponent(); }

    int16_t GetYear() const { return m_year; }
    uint8_t GetMonth() const { return m_month; }
    uint8_t GetDay() const { return m_day; }
    uint8_t GetHour() const { return m_hour; }
    uint8_t GetMinute() const { return m_minute; }
    uint8_t GetSecond() const { return m_second; }
    uint16_t GetMillisecond() const { return m_millisecond; }
    DayOfWeek GetDayOfWeek() const;
    uint16_t GetDayOfYear() const;

    /// 提取时间部分
    /// Ported from: imodel-native DateTime.h:281
    DateTime GetTimeOfDay() const;

    // --- 转换 ---
    int64_t ToUnixMilliseconds() const;
    bool ToString(std::string& result) const;
    DateTime ToUtc() const;
    DateTime ToLocalTime() const;

    /// 转为 Julian Day
    /// Ported from: imodel-native DateTime.h (ToJulianDay)
    double ToJulianDay() const;

    /// 转为 Julian Day（仅日期部分）
    /// Ported from: imodel-native DateTime.h (ToJulianDayDate)
    double ToJulianDayDate() const;

    /// Julian Day 转 Unix 毫秒
    /// Ported from: imodel-native DateTime.h
    static double JulianDayToUnixMilliseconds(double julianDay);

    /// Unix 毫秒转 Julian Day
    /// Ported from: imodel-native DateTime.h
    static double UnixMillisecondsToJulianDay(double unixMilliseconds);

    /// 从午夜起的毫秒数
    /// Ported from: imodel-native DateTime.h
    int32_t ToMillisecondsSinceMidnight() const;

    /// 毫秒转为 rational day（0.0-1.0）
    /// Ported from: imodel-native DateTime.h
    static double MsecToRationalDay(int32_t msec);

    /// rational day 转为毫秒
    /// Ported from: imodel-native DateTime.h
    static int32_t RationalDayToMsec(double rationalDay);

    /// 计算 UTC 偏移（毫秒）
    /// Ported from: imodel-native DateTime.h
    int32_t ComputeOffsetToUtcInMsec() const;

    /// 格式化为时间戳字符串
    /// Ported from: imodel-native DateTime.h
    bool ToTimestampString(std::string& result) const;

    // --- 比较 ---
    bool Equals(const DateTime& rhs, bool ignoreDateTimeInfo = false) const;

    /// 比较两个 DateTime
    /// Ported from: imodel-native DateTime.h:232
    static CompareResult Compare(const DateTime& lhs, const DateTime& rhs);

    // --- 工具 ---
    static bool IsLeapYear(uint16_t year) noexcept;
    static uint8_t GetMaxDay(uint16_t year, uint8_t month) noexcept;
};

END_DQ_BASE_NAMESPACE
