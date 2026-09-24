// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeTimeUtilities.h
//              itwinjs-core core/bentley/src/Time.ts
// DanQing dqBase — 时间持续 / 时间点 / 秒表 / 时间工具
//
// 1:1 对齐 imodel-native BeDuration / BeTimePoint / StopWatch / BeTimeUtilities。
// 基于 std::chrono，零外部依赖。
//
// 别名（ref-name 兼容）：BeDuration / BeTimePoint / StopWatch / BeTimeUtilities
// 均映射到 DanQing 名称，使引用 imodel-native 命名的代码可直接编译。
#pragma once

#include "DqBase.h"
#include "NonCopyable.h"
#include "DqStatus.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <functional>
#include <thread>

#if defined(_WIN32)
// FILETIME 前向声明（完整定义在 windows.h）。
// 对齐参考 BeTimeUtilities.h 的做法：形参用 _FILETIME，头文件无需引入 windows.h。
struct _FILETIME;
#endif

// struct tm 在 <ctime> 中提供；BeTimeUtilities 的 tm 转换函数使用它。
// DateTime.h 提供 DateTime 类型（GetSystemTime 等返回值）。

BEGIN_DQ_BASE_NAMESPACE

// 前向声明（避免 DqTime.h 与 DqClock.h 循环 include）
class DqClock;
class DateTime;

// ---------------------------------------------------------------------------
// DqDuration — 时间持续（对齐 imodel-native BeDuration，ref:150-195）
// Ported from: imodel-native BeTimeUtilities.h BeDuration
//
// 与 ref 一致：允许 Hours/Minutes/Seconds/Milliseconds/Nanoseconds 隐式构造，
// 显式删除 int/double 构造以强制指定单位（ref:166-167）。
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT DqDuration {
public:
    using SteadyClock   = std::chrono::steady_clock;
    using Nanoseconds   = std::chrono::nanoseconds;
    using Milliseconds  = std::chrono::milliseconds;
    using Seconds       = std::chrono::seconds;
    using Minutes       = std::chrono::minutes;
    using Hours         = std::chrono::hours;

    // --- 构造（对齐 ref:159-167，允许 chrono 单位隐式转换）---
    constexpr DqDuration() noexcept : m_duration(Milliseconds::zero()) {}
    constexpr DqDuration(Hours val) noexcept : m_duration(val) {}
    constexpr DqDuration(Minutes val) noexcept : m_duration(val) {}
    constexpr DqDuration(Seconds val) noexcept : m_duration(val) {}
    constexpr DqDuration(Milliseconds val) noexcept : m_duration(val) {}
    constexpr DqDuration(Nanoseconds val) noexcept : m_duration(val) {}
    constexpr DqDuration(std::chrono::duration<double, SteadyClock::period> val) noexcept
        : m_duration(std::chrono::duration_cast<SteadyClock::duration>(val)) {}

    // 禁止裸 int/double 构造——必须指定单位（ref:166-167）
    DqDuration(int) = delete;       // Note: you must specify units!
    DqDuration(double) = delete;    // Note: you must specify units!

    // --- 静态工厂 ---
    constexpr static DqDuration FromSeconds(double val) noexcept {
        return DqDuration(std::chrono::duration_cast<SteadyClock::duration>(
            std::chrono::duration<double>(val)));
    }
    constexpr static DqDuration FromMilliseconds(int64_t val) noexcept {
        return DqDuration(Milliseconds(val));
    }
    constexpr static DqDuration Zero() noexcept {
        return DqDuration(Milliseconds::zero());
    }

    // --- 转换操作符（对齐 ref:176-182）---
    //! 转为 double 秒数（不是纳秒！）
    constexpr operator double() const noexcept {
        return std::chrono::duration_cast<std::chrono::duration<double>>(m_duration).count();
    }
    constexpr operator Milliseconds() const noexcept {
        return std::chrono::duration_cast<Milliseconds>(m_duration);
    }
    constexpr operator Seconds() const noexcept {
        return std::chrono::duration_cast<Seconds>(m_duration);
    }

    // --- 查询 ---
    //! 转为秒数（与 operator double() 等价，ref:179）
    constexpr double ToSeconds() const noexcept { return (double)(*this); }
    constexpr int64_t ToMilliseconds() const noexcept {
        return std::chrono::duration_cast<Milliseconds>(m_duration).count();
    }
    constexpr bool IsZero() const noexcept {
        return m_duration.count() == 0;
    }
    constexpr bool IsTowardsFuture() const noexcept {
        return m_duration.count() > 0;
    }
    constexpr bool IsTowardsPast() const noexcept {
        return m_duration.count() < 0;
    }

    // --- 内部 duration 访问 ---
    constexpr SteadyClock::duration Duration() const noexcept { return m_duration; }

    // --- 算术运算 ---
    constexpr DqDuration operator+(DqDuration rhs) const noexcept {
        return FromRawDuration(m_duration + rhs.m_duration);
    }
    constexpr DqDuration operator-(DqDuration rhs) const noexcept {
        return FromRawDuration(m_duration - rhs.m_duration);
    }
    constexpr bool operator==(DqDuration rhs) const noexcept { return m_duration == rhs.m_duration; }
    constexpr bool operator!=(DqDuration rhs) const noexcept { return m_duration != rhs.m_duration; }
    constexpr bool operator<(DqDuration rhs) const noexcept  { return m_duration < rhs.m_duration; }
    constexpr bool operator<=(DqDuration rhs) const noexcept { return m_duration <= rhs.m_duration; }
    constexpr bool operator>(DqDuration rhs) const noexcept  { return m_duration > rhs.m_duration; }
    constexpr bool operator>=(DqDuration rhs) const noexcept { return m_duration >= rhs.m_duration; }

    // --- 动作 ---
    //! 挂起当前线程持续时长（ref:194）
    void Sleep() const {
        if (IsTowardsFuture())
            std::this_thread::sleep_for(m_duration);
    }

    // 内部工厂：从底层 steady duration 构造。Nanoseconds 与 SteadyClock::duration
    // 是同一类型，无法再用 Nanoseconds ctor 重载，故提供此命名工厂供算术运算使用。
    static constexpr DqDuration FromRawDuration(SteadyClock::duration d) noexcept {
        DqDuration result;
        result.m_duration = d;
        return result;
    }

private:
    SteadyClock::duration m_duration;
};

// ---------------------------------------------------------------------------
// DqTimePoint — 时间点（对齐 imodel-native BeTimePoint，ref:202-233）
// Ported from: imodel-native BeTimeUtilities.h BeTimePoint
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT DqTimePoint {
public:
    using SteadyClock = std::chrono::steady_clock;

    DqTimePoint() = default;
    constexpr explicit DqTimePoint(SteadyClock::time_point tp) noexcept : m_tp(tp) {}
    //! 从 duration 构造（自 epoch 起的时间点）。对齐 ref:208 BeTimePoint(T_Super const&)，
    //! 其中 T_Super = steady_clock::time_point 可从 steady_clock::duration 隐式构造。
    //! 允许 `DqTimePoint(DqDuration::Hours(2))` 这样的用法。
    constexpr DqTimePoint(DqDuration d) noexcept : m_tp(d.Duration()) {}

    // --- 静态工厂 ---
    static DqTimePoint Now() noexcept {
        return DqTimePoint(SteadyClock::now());
    }
    static DqTimePoint FromNow(DqDuration d) noexcept {
        return DqTimePoint(SteadyClock::now() + d.Duration());
    }
    static DqTimePoint BeforeNow(DqDuration d) noexcept {
        return DqTimePoint(SteadyClock::now() - d.Duration());
    }

    // --- 查询 ---
    SteadyClock::rep GetTicks() const noexcept {
        return m_tp.time_since_epoch().count();
    }
    bool IsValid() const noexcept {
        return GetTicks() != 0;
    }
    //! 若此时间点是有效且在未来的时间点，返回 true（ref:228）
    //! @note 无效时间点直接返回 false，不会调用 Now()
    bool IsInFuture() const noexcept {
        return IsValid() && (Now() < *this);
    }
    //! 若此时间点是有效且已过去的时间点，返回 true（ref:232）
    //! @note 无效时间点直接返回 false，不会调用 Now()
    bool IsInPast() const noexcept {
        return IsValid() && (Now() > *this);
    }

    // --- 内部 time_point 访问 ---
    SteadyClock::time_point TimePoint() const noexcept { return m_tp; }

    // --- 运算 ---
    DqDuration operator-(DqTimePoint rhs) const noexcept {
        return DqDuration::FromRawDuration(m_tp - rhs.m_tp);
    }
    DqTimePoint operator+(DqDuration d) const noexcept {
        return DqTimePoint(m_tp + d.Duration());
    }
    DqTimePoint operator-(DqDuration d) const noexcept {
        return DqTimePoint(m_tp - d.Duration());
    }
    bool operator==(DqTimePoint rhs) const noexcept { return m_tp == rhs.m_tp; }
    bool operator!=(DqTimePoint rhs) const noexcept { return m_tp != rhs.m_tp; }
    bool operator<(DqTimePoint rhs) const noexcept  { return m_tp < rhs.m_tp; }
    bool operator<=(DqTimePoint rhs) const noexcept { return m_tp <= rhs.m_tp; }
    bool operator>(DqTimePoint rhs) const noexcept  { return m_tp > rhs.m_tp; }
    bool operator>=(DqTimePoint rhs) const noexcept { return m_tp >= rhs.m_tp; }

private:
    SteadyClock::time_point m_tp{};
};

// ---------------------------------------------------------------------------
// DqStopWatch — 秒表计时器（对齐 imodel-native StopWatch，ref:239-285）
// Ported from: imodel-native BeTimeUtilities.h StopWatch
//
// 与 ref 一致：ctor 接受 description 参数，GetDescription() 返回名称。
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT DqStopWatch : public DqNonCopyable {
public:
    //! 创建命名秒表并可选择是否立即启动（ref:256）
    explicit DqStopWatch(const char* description = nullptr, bool startImmediately = false)
        : m_description(description) {
        if (startImmediately) Start();
    }

    //! 创建未命名秒表并可选择是否立即启动（ref:259）
    explicit DqStopWatch(bool startImmediately) : m_description(nullptr) {
        if (startImmediately) Start();
    }

    //! 获取 ctor 中提供的描述（ref:262）
    const char* GetDescription() const noexcept { return m_description; }

    void Start() noexcept {
        m_running = true;
        m_start = DqTimePoint::Now();
    }

    void Stop() noexcept {
        if (m_running) {
            m_elapsed = m_elapsed + (DqTimePoint::Now() - m_start);
            m_running = false;
        }
    }

    void Reset() noexcept {
        m_elapsed = DqDuration::Zero();
        m_running = false;
    }

    //! 从 Start() 起到当前的经过时长（ref:271）
    DqDuration GetCurrent() const noexcept {
        if (m_running)
            return m_elapsed + (DqTimePoint::Now() - m_start);
        return m_elapsed;
    }
    double GetCurrentSeconds() const noexcept {
        return GetCurrent().ToSeconds();
    }

    //! Start() 与 Stop() 之间的经过时长（ref:275）
    DqDuration GetElapsed() const noexcept { return GetCurrent(); }
    double GetElapsedSeconds() const noexcept { return GetCurrentSeconds(); }

    // --- 静态工具 ---
    //! 测量一个方法的耗时（ref:279）
    static DqDuration Measure(std::function<void()> func) {
        DqStopWatch sw(true);
        func();
        sw.Stop();
        return sw.GetElapsed();
    }

private:
    const char* m_description = nullptr;
    DqTimePoint m_start;
    DqDuration m_elapsed{DqDuration::Zero()};
    bool m_running = false;
};

// ===========================================================================
// DqTimeUtilities — 时间转换工具（对齐 imodel-native BeTimeUtilities，ref:37-143）
// Ported from: imodel-native BeTimeUtilities.h struct BeTimeUtilities
//
// 提供：当前时间获取、UnixMilli 时间转换、tm 转换、本地时间调整、
//       BeTimePoint <-> DateTime 互转、Win32 FILETIME 转换（_WIN32 守卫）。
// ===========================================================================
struct DQ_BASE_EXPORT DqTimeUtilities {
    // --- 计数器（ref:45-58）---
    //! 自未指定起点以来的毫秒数（deprecated，用 DqTimePoint::Now() 代替）
    static uint64_t QueryMillisecondsCounter();
    //! 自未指定起点以来的秒数（deprecated）
    static double QuerySecondsCounter() { return QueryMillisecondsCounter() / 1000.0; }
    //! 32 位毫秒计数器（49 天后回绕，ref:58）
    static uint32_t QueryMillisecondsCounterUInt32() {
        return (uint32_t)((0xffffffff & QueryMillisecondsCounter()));
    }

    // --- 当前时间（ref:66-73）---
    //! 当前 Unix 毫秒时间（自 1970-01-01 UTC）
    static uint64_t GetCurrentTimeAsUnixMillis();
    //! 当前 Unix 毫秒时间（double）
    static double GetCurrentTimeAsUnixMillisDouble() {
        return (double)(int64_t)GetCurrentTimeAsUnixMillis();
    }
    //! 保证与之前/之后任何调用者返回的时间戳不同的时间戳（ref:73）
    static double GetCurrentTimeAsUnixMillisDoubleWithDelay();

    // --- tm 转换（ref:98-111，POSIX gmtime_r/timegm）---
    //! tm -> UnixMilli（UTC，无时区转换），ref:98
    static uint64_t ConvertTmToUnixMillis(tm const& t);
    //! UnixMilli -> tm（UTC，无时区转换），ref:105
    //! @return Success 成功 / Error 失败
    static DqStatus ConvertUnixMillisToTm(tm& t, uint64_t umillis);
    //! tm -> UnixMilli（double），ref:111
    static double ConvertTmToUnixMillisDouble(tm const& t) {
        return (double)(int64_t)ConvertTmToUnixMillis(t);
    }

    // --- 本地时间（ref:117-123，POSIX localtime_r）---
    //! 按 user 本地时间调整 UnixMilli（UTC->local），ref:117
    //! @param[in,out] u 待调整的 UnixMilli，返回本地时间对应的 UnixMilli
    //! @return Success 成功 / Error 失败
    static DqStatus AdjustUnixMillisForLocalTime(uint64_t& u);
    //! UnixMilli(UTC) -> 本地时间 tm，ref:123
    static DqStatus ConvertUnixMillisToLocalTime(tm& localTime, uint64_t unixMilliseconds);

    // --- BeTimePoint <-> DateTime（ref:132-141）---
    //! steady 时间点近似为系统时钟 DateTime，ref:132
    //! @param[in] timePoint steady 时间点
    //! @param[in] clock 用于获取参考时间点的 BeClock，nullptr 表示用 BeClock::Get()
    //! @returns 转换成功返回 DateTime，失败返回无效 DateTime
    static DateTime ConvertBeTimePointToDateTime(DqTimePoint timePoint, DqClock const* clock = nullptr);
    //! 系统时钟 DateTime 近似为 steady 时间点，ref:141
    //! @param[in] dateTime 系统时钟 DateTime
    //! @param[in] clock 用于获取参考时间点的 BeClock，nullptr 表示用 BeClock::Get()
    //! @returns 转换成功返回 BeTimePoint，失败返回无效 BeTimePoint
    static DqTimePoint ConvertDateTimeToBeTimePoint(DateTime const& dateTime, DqClock const* clock = nullptr);

    // --- Win32-only FILETIME 转换（ref:82-91，_WIN32 守卫对齐参考）---
#if defined(_WIN32)
    //! FILETIME -> UnixMilli（double），ref:85
    static double ConvertFiletimeToUnixMillisDouble(_FILETIME const& f);
    //! UnixTime(time_t) -> FILETIME，ref:90
    static void ConvertUnixTimeToFiletime(_FILETIME& f, time_t t);
#endif
};

// ---------------------------------------------------------------------------
// ref-name 别名（使引用 imodel-native 命名的代码可直接编译）
// ---------------------------------------------------------------------------
using BeDuration     = DqDuration;
using BeTimePoint    = DqTimePoint;
using StopWatch      = DqStopWatch;
using BeTimeUtilities = DqTimeUtilities;

END_DQ_BASE_NAMESPACE
