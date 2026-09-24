// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/Time.ts
//              imodel-native iModelCore/Bentley/Bentley/nonport/BeTimeUtilities.cpp (BeClock::Get, ref:336-339)
// DanQing dqBase — DqClock 系统时钟与注入实现（零 Qt 依赖）
#include "dqBase/DqClock.h"
#include "dqBase/DateTime.h"

#include <chrono>

BEGIN_DQ_BASE_NAMESPACE

namespace {
DqClock* g_instance = nullptr; // 注入的时钟（nullptr 表示用系统单例）
DqSystemClock g_systemClock;   // 进程级系统时钟单例
} // namespace

DqClock& DqClock::Get() {
    // 对齐 ref:336-339：注入优先，否则返回系统时钟单例。
    // ref 用 static BeClock s_clock（基类实例，虚函数走默认实现）；
    // DanQing 用 DqSystemClock 单例，行为等价且 CurrentMillis 可用。
    return g_instance ? *g_instance : g_systemClock;
}

// DanQing extension: 测试注入
void DqClock::SetInstance(DqClock* clock) {
    g_instance = clock;
}

// DanQing extension: 默认实现走系统时钟（与 DqSystemClock 一致）
int64_t DqClock::CurrentMillis() const noexcept {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

// 默认实现：返回当前 UTC DateTime（对齐 ref:310）
DateTime DqClock::GetSystemTime() const {
    return DateTime::GetCurrentTimeUtc();
}

// ---------------------------------------------------------------------------
// DqSystemClock — 系统时钟实现
// ---------------------------------------------------------------------------
int64_t DqSystemClock::CurrentMillis() const noexcept {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

DqTimePoint DqSystemClock::Now() const noexcept {
    return DqTimePoint::Now();
}

DqTimePoint DqSystemClock::GetSteadyTime() const noexcept {
    return DqTimePoint::Now();
}

DateTime DqSystemClock::GetSystemTime() const {
    return DateTime::GetCurrentTimeUtc();
}

END_DQ_BASE_NAMESPACE
