// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/Time.ts
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeTimeUtilities.h (struct BeClock, lines 293-313)
// DanQing dqBase — 可注入虚拟时钟（测试可控）
//
// 替代 Qt QDateTime。1:1 对齐 imodel-native BeClock。
//
// DqClock 提供注入点：测试时塞入 FakeClock 控制 Tile 过期、事务超时等时间行为。
// DanQing 扩展（保留）：CurrentMillis() / SetInstance(DqClock*) 用于测试注入；
//   参考实现 BeClock 通过子类化直接覆盖 GetSteadyTime/GetSystemTime，DanQing 额外
//   提供 SetInstance 以支持现有的 UtilityTest/Tile/txn 超时测试。
#pragma once

#include "DqBase.h"
#include "DqTime.h"

#include <chrono>
#include <cstdint>

BEGIN_DQ_BASE_NAMESPACE

class DateTime;

// ---------------------------------------------------------------------------
// DqClock — 可注入虚拟时钟（对齐 imodel-native BeClock，ref:293-313）
// Ported from: imodel-native BeTimeUtilities.h struct BeClock
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT DqClock {
public:
    virtual ~DqClock() = default;

    //! 单例（注入优先，否则系统时钟）。对齐 ref:296。
    static DqClock& Get();

    // --- DanQing 扩展（保留，用于测试注入）---
    //! DanQing extension: test injection; ref tests subclass BeClock directly.
    //! 注入时钟（传 nullptr 恢复系统时钟）
    static void SetInstance(DqClock* clock);
    //! DanQing extension: 毫秒级 Unix 时间（Logger 等使用）
    virtual int64_t CurrentMillis() const noexcept;

    //! DEPRECATED，被 GetSteadyTime() 取代（ref:300）。
    //! 返回当前时间点。
    virtual DqTimePoint Now() const noexcept { return GetSteadyTime(); }

    //! 单调时钟的当前时间点，适合测量时间间隔（ref:305）。
    //! @remarks 返回的 DqTimePoint 仅在创建它的进程内有效。
    virtual DqTimePoint GetSteadyTime() const noexcept { return DqTimePoint::Now(); }

    //! 系统时钟的当前 UTC 时间（ref:310）。
    //! @remarks 系统时钟不是单调的，可能被用户调整。
    //! @note 返回的 DateTime 适合持久化存储。
    virtual DateTime GetSystemTime() const;
};

// 系统时钟实现
class DQ_BASE_EXPORT DqSystemClock : public DqClock {
public:
    int64_t CurrentMillis() const noexcept override;
    DqTimePoint Now() const noexcept override;
    DqTimePoint GetSteadyTime() const noexcept override;
    DateTime GetSystemTime() const override;
};

// ref-name 别名（使引用 imodel-native 命名的代码可直接编译）
using BeClock = DqClock;

END_DQ_BASE_NAMESPACE
