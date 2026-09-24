// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/Bentley/nonport/BeTimeUtilities.cpp
// DanQing dqBase — DqTimeUtilities 时间转换实现（零 Qt 依赖）
//
// 1:1 对齐 imodel-native BeTimeUtilities.cpp：__unix__ 与 Win32 分支均已移植
//（Win32 分支 FILETIME/QPC/SYSTEMTIME/localtime_s/_gmtime64_s/_mkgmtime64，
//  ref:49-86/111-113/120-146/153-161/225-238/249-258/280-281）。
#include "dqBase/DqTime.h"
#include "dqBase/DqClock.h"
#include "dqBase/DateTime.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <thread>
#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/time.h>
#endif

BEGIN_DQ_BASE_NAMESPACE

// Win32 file time：100ns 间隔、自 1601-01-01（ref:40-47）
#define FILETIME_1_1_1970  116444736000000000LL           // Win32 file time of midnight 1/1/70
#define UMILLIS_TO_FTI     10000LL                        // milliseconds -> 100-nanosecond interval

#if defined(_WIN32)

// ----------------------------------------------------------------------------
// QPC — ref:51-72（进程启动时取 frequency，QueryMillisecondsCounter 用）
// ----------------------------------------------------------------------------
namespace {
struct QPC {
    uint64_t m_ticksPerMillisecond;
    QPC() {
        LARGE_INTEGER f;
        ::QueryPerformanceFrequency(&f);
        m_ticksPerMillisecond = f.QuadPart / 1000;  // ticks/second * seconds/millisecond = ticks/millisecond
    }
    uint64_t GetCountInMillis() const {
        LARGE_INTEGER tm;
        ::QueryPerformanceCounter(&tm);
        return tm.QuadPart / m_ticksPerMillisecond;
    }
};
QPC s_qpc; // grab frequency at program start-up time（对齐 ref:72 static QPC s_qpc）

// ----------------------------------------------------------------------------
// FILETIME 转换辅助 — ref:121-134
// ----------------------------------------------------------------------------
inline uint64_t fileTimeAsUInt64(FILETIME const& f)   {return *(uint64_t*)  &f;}
inline void     uInt64AsFileTime(FILETIME& f, uint64_t i) {f = *(FILETIME*)&i;}

inline uint64_t convertFiletimeToUnixMillis(FILETIME const& ft) {
    uint64_t umillis = fileTimeAsUInt64(ft);
    //TODO: this can result in negative number if FILETIME is before Unix epoch!（ref:130）
    umillis -= FILETIME_1_1_1970;   // re-base
    umillis /= UMILLIS_TO_FTI;      // 100-nanosecond interval -> millisecond
    return umillis;
}
} // namespace

#else

// ----------------------------------------------------------------------------
// 进程启动时的 UnixMilli（对齐 ref:76-82 s_startTime）
// 用于 QueryMillisecondsCounter 计算自启动以来的毫秒数。
// ----------------------------------------------------------------------------
namespace {
struct StartTime {
    uint64_t m_initializationTimeInMillis;
    StartTime() : m_initializationTimeInMillis(DqTimeUtilities::GetCurrentTimeAsUnixMillis()) {}
};
StartTime g_startTime; // 在进程启动时初始化
} // namespace

#endif

// ----------------------------------------------------------------------------
// QueryMillisecondsCounter — ref:91-118（Win32 分支:111-113 / __unix__ 分支:109）
// ----------------------------------------------------------------------------
uint64_t DqTimeUtilities::QueryMillisecondsCounter() {
#if defined(_WIN32)
    // 对齐 ref:113：Win32 用 QPC
    return s_qpc.GetCountInMillis();
#else
    // 对齐 ref:109：返回 (uint32_t)(当前 UnixMilli - 启动 UnixMilli)
    // 注意：ref 在此做了 uint32_t 截断，保留以匹配 ref 行为（QueryMillisecondsCounterUInt32 依赖此）。
    return (uint32_t)(DqTimeUtilities::GetCurrentTimeAsUnixMillis() - g_startTime.m_initializationTimeInMillis);
#endif
}

// ----------------------------------------------------------------------------
// GetCurrentTimeAsUnixMillis — ref:151-171（Win32 分支:153-161 / __unix__ 分支:162-167）
// ----------------------------------------------------------------------------
uint64_t DqTimeUtilities::GetCurrentTimeAsUnixMillis() {
#if defined(_WIN32)
    // 对齐 ref:153-161
    SYSTEMTIME st0;
    FILETIME   ft0;

    GetSystemTime(&st0);
    SystemTimeToFileTime(&st0, &ft0);

    return convertFiletimeToUnixMillis(ft0);
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr); // GMT
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000; // => 毫秒
#endif
}

#if defined(_WIN32)
// ----------------------------------------------------------------------------
// ConvertUnixTimeToFiletime — ref:136-142（Win32-only）
// ----------------------------------------------------------------------------
void DqTimeUtilities::ConvertUnixTimeToFiletime(FILETIME& f, time_t t) {
    uint64_t ll = (uint64_t)t * 1000LL; // Second --> millisecond
    ll *= UMILLIS_TO_FTI;           // millisecond -> 100-nanosecond interval
    ll += FILETIME_1_1_1970;        // re-base
    uInt64AsFileTime(f, ll);
}

// ----------------------------------------------------------------------------
// ConvertFiletimeToUnixMillisDouble — ref:144（Win32-only）
// ----------------------------------------------------------------------------
double DqTimeUtilities::ConvertFiletimeToUnixMillisDouble(FILETIME const& f) {
    return (double)(int64_t)convertFiletimeToUnixMillis(f);
}
#endif

// ----------------------------------------------------------------------------
// GetCurrentTimeAsUnixMillisDoubleWithDelay — ref:176-188
// 保证返回的时间戳不同于之前/之后任何调用者（忙等至时钟前进）。
// ----------------------------------------------------------------------------
double DqTimeUtilities::GetCurrentTimeAsUnixMillisDoubleWithDelay() {
    double ts0 = GetCurrentTimeAsUnixMillisDouble();
    double ts1;
    while ((ts1 = GetCurrentTimeAsUnixMillisDouble()) == ts0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(0));
    }

    double ts2;
    while ((ts2 = GetCurrentTimeAsUnixMillisDouble()) == ts1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(0));
    }

    return ts1;
}

// ----------------------------------------------------------------------------
// ConvertUnixMillisToTm — ref:247-272（Win32 分支:249-258 / __unix__ 分支:260-268）
// UTC，无时区转换。
// ----------------------------------------------------------------------------
DqStatus DqTimeUtilities::ConvertUnixMillisToTm(tm& stm, uint64_t umillis) {
#if defined(_WIN32)
    // 对齐 ref:249-258
    __time64_t t = umillis / 1000LL;
    errno_t stat = _gmtime64_s(&stm, &t);
    if (stat != 0) {
        return DqStatus::Error;
    }
    return DqStatus::Success;
#else
    time_t t = (time_t)(umillis / 1000LL);
    // gmtime_r 失败返回 nullptr
    if (nullptr == gmtime_r(&t, &stm)) {
        return DqStatus::Error;
    }
    return DqStatus::Success;
#endif
}

// ----------------------------------------------------------------------------
// ConvertTmToUnixMillis — ref:277-293（Win32 分支:280-281 / __unix__ 分支:282-288）
// UTC，无时区转换。
// ----------------------------------------------------------------------------
uint64_t DqTimeUtilities::ConvertTmToUnixMillis(tm const& timeStructIn) {
    tm timeStruct(timeStructIn);
#if defined(_WIN32)
    // 对齐 ref:280-281
    __time64_t time = _mkgmtime64(&timeStruct);
#else
    time_t time = timegm(&timeStruct);
#endif
    return (uint64_t)time * 1000LL;
}

// ----------------------------------------------------------------------------
// ConvertUnixMillisToLocalTime — ref:216-242（Win32 分支:225-238）
// ----------------------------------------------------------------------------
DqStatus DqTimeUtilities::ConvertUnixMillisToLocalTime(tm& localTime, uint64_t unixMilliseconds) {
    time_t t = (time_t)(unixMilliseconds / 1000LL);
    tm local;
#if defined(_WIN32)
    // 对齐 ref:225-230：MSVC localtime_s（errno_t，tm 在前）
    errno_t stat = localtime_s(&local, &t);
    if (stat != 0) {
        return DqStatus::Error;
    }
#else
    // localtime_r 失败返回 nullptr
    if (nullptr == localtime_r(&t, &local)) {
        return DqStatus::Error;
    }
#endif
    localTime = local;
    return DqStatus::Success;
}

// ----------------------------------------------------------------------------
// AdjustUnixMillisForLocalTime — ref:193-211
// 隐式做 UTC -> local 转换。
// ----------------------------------------------------------------------------
DqStatus DqTimeUtilities::AdjustUnixMillisForLocalTime(uint64_t& millis) {
    // tm 不支持毫秒 -> 单独提取后加回
    uint64_t millisecondComponent = millis % 1000LL;

    tm localTime;
    DqStatus stat = ConvertUnixMillisToLocalTime(localTime, millis);
    if (stat != DqStatus::Success) {
        return DqStatus::Error;
    }

    uint64_t localMillis = ConvertTmToUnixMillis(localTime);
    localMillis += millisecondComponent;

    millis = localMillis;
    return DqStatus::Success;
}

// ----------------------------------------------------------------------------
// ConvertBeTimePointToDateTime — ref:298-312
// steady 时间点近似为系统时钟 DateTime。
// ----------------------------------------------------------------------------
DateTime DqTimeUtilities::ConvertBeTimePointToDateTime(DqTimePoint timePoint, DqClock const* clock) {
    if (!timePoint.IsValid())
        return DateTime();

    if (nullptr == clock)
        clock = &DqClock::Get();

    DateTime systemDateTime = clock->GetSystemTime();
    if (!systemDateTime.IsValid())
        return DateTime();

    int64_t systemTime = systemDateTime.ToUnixMilliseconds();

    using namespace std::chrono;
    int64_t dateTimeUnixMilliseconds = systemTime
        + duration_cast<milliseconds>(timePoint.TimePoint() - clock->GetSteadyTime().TimePoint()).count();
    return DateTime::FromUnixMilliseconds((uint64_t)dateTimeUnixMilliseconds);
}

// ----------------------------------------------------------------------------
// ConvertDateTimeToBeTimePoint — ref:317-331
// 系统时钟 DateTime 近似为 steady 时间点。
// ----------------------------------------------------------------------------
DqTimePoint DqTimeUtilities::ConvertDateTimeToBeTimePoint(DateTime const& dateTime, DqClock const* clock) {
    if (!dateTime.IsValid())
        return DqTimePoint();

    if (nullptr == clock)
        clock = &DqClock::Get();

    int64_t dateTimeUnixMilliseconds = dateTime.ToUnixMilliseconds();

    DateTime currentDateTime = clock->GetSystemTime();
    if (!currentDateTime.IsValid())
        return DqTimePoint();
    int64_t currentUnixMilliseconds = currentDateTime.ToUnixMilliseconds();

    using namespace std::chrono;
    return DqTimePoint(clock->GetSteadyTime().TimePoint()
        + milliseconds(dateTimeUnixMilliseconds - currentUnixMilliseconds));
}

END_DQ_BASE_NAMESPACE
