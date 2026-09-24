// SPDX-License-Identifier: Apache-2.0
// Authored: module bootstrap, no direct reference equivalent
// DanQing dqBase — 共享基础层引导
//
// 提供版本、全局初始化。零 Qt 依赖，仅用 C++ 标准库。
// 命名空间宏和导出宏在 Export.h 中定义。
#pragma once

#include "Export.h"

#include <cstdint>

// ---------------------------------------------------------------------------
// dqBase 版本
// ---------------------------------------------------------------------------
#define DQ_BASE_VERSION_MAJOR 0
#define DQ_BASE_VERSION_MINOR 1
#define DQ_BASE_VERSION_PATCH 0
#define DQ_BASE_VERSION_STRING "0.1.0"

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// 全局初始化（每进程一次）
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT DqBaseLib {
public:
    // 注册 dqBase 日志分类等。幂等，可多次调用。
    static void Initialize() noexcept;
    static void Shutdown() noexcept;
};

END_DQ_BASE_NAMESPACE
