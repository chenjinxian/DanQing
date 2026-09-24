// SPDX-License-Identifier: Apache-2.0
// Authored: module bootstrap, no direct reference equivalent
// DanQing dqGeom — 纯数学几何引擎引导（零固体内核依赖，内核归真形/仓外）
//
// 提供命名空间宏、版本、全局初始化。dqGeom 是 dqBase 之上的第一层引擎，
// 被 dqRender 向下依赖（渲染经 PolyfaceQuery 只读接口消费几何），对仓外数据层独立。
// 设计依据：docs/DanQing-图形引擎架构设计.md（模块架构）+ CLAUDE.md §8（依赖方向）
#pragma once

#include <dqBase/DqBase.h> // dqBase 基础（零 Qt，thin wrapper 对齐 imodel-native bvector/Utf8String 模式）

#include "Export.h" // DQ_GEOM_EXPORT 宏（平台原生 __declspec/__attribute__）

// ---------------------------------------------------------------------------
// dqGeom 版本
// ---------------------------------------------------------------------------
#define DQ_GEOM_VERSION_MAJOR 0
#define DQ_GEOM_VERSION_MINOR 2
#define DQ_GEOM_VERSION_PATCH 0
#define DQ_GEOM_VERSION_STRING "0.2.0"

// ---------------------------------------------------------------------------
// 命名空间宏
// ---------------------------------------------------------------------------
#define BEGIN_DQ_GEOM_NAMESPACE namespace dqGeom {
#define END_DQ_GEOM_NAMESPACE }

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// 全局初始化（每进程一次）
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT DqGeomLib {
public:
    // Phase 0：无全局状态需注册（幂等，可多次调用）。
    // Phase 2 在此注册 B-Rep 内核后端、求解器工厂等。
    static void Initialize() noexcept;
    static void Shutdown() noexcept;
};

END_DQ_GEOM_NAMESPACE
