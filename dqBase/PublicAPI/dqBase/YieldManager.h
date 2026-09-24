// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/YieldManager.ts
// DanQing dqBase — 让步管理器
//
// 1:1 对齐 itwinjs-core YieldManager。
// 在紧密循环中定期让出 CPU 时间。
#pragma once

#include "Export.h"

#include <cstdint>
#include <thread>
#include <chrono>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// YieldManagerOptions — 让步选项
// Ported from: itwinjs-core YieldManager.ts:12-17
// ---------------------------------------------------------------------------
// ref:
//   export interface YieldManagerOptions {
//     iterationsBeforeYield?: number;  // Default: 1000.
//   };
struct YieldManagerOptions {
    /// 触发实际让步前 [[YieldManager::AllowYield]] 必须被调用的次数。默认 1000。
    /// Ported from: itwinjs-core YieldManager.ts:15 iterationsBeforeYield
    uint32_t iterationsBeforeYield = 1000;
};

// ---------------------------------------------------------------------------
// YieldManager — 让步管理器
// Ported from: itwinjs-core YieldManager.ts:31-54
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT YieldManager {
public:
    /// 控制让步行行为的只读选项（对齐 ref 的 `public readonly options`）。
    const YieldManagerOptions options;

    /// 构造函数。@param options 自定义让步行行为；缺省字段使用默认值。
    /// Ported from: itwinjs-core YieldManager.ts:39-41 constructor
    explicit YieldManager(YieldManagerOptions options = YieldManagerOptions())
        : options(options) {}

    /// 增加迭代计数，若超过 options.iterationsBeforeYield 则让出 CPU 并重置计数器。
    /// Ported from: itwinjs-core YieldManager.ts:44-49 allowYield
    void AllowYield() {
        m_counter = (m_counter + 1) % options.iterationsBeforeYield;
        if (m_counter == 0) {
            actualYield();
        }
    }

private:
    /// ref:51-53 — 实际让步实现。
    void actualYield() {
        std::this_thread::sleep_for(std::chrono::milliseconds(0));
    }

    uint32_t m_counter = 0;
};

END_DQ_BASE_NAMESPACE
