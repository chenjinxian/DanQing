// SPDX-License-Identifier: Apache-2.0
// Authored: module bootstrap, no direct reference equivalent
// DanQing dqGeom — 全局初始化实现
#include "dqGeom/DqGeom.h"

BEGIN_DQ_GEOM_NAMESPACE

namespace {
bool g_initialized = false;
}

void DqGeomLib::Initialize() noexcept {
    if (g_initialized)
        return;
    g_initialized = true;
    // Phase 0：无全局状态需注册。Phase 2 在此注册 B-Rep 内核后端 / 求解器工厂。
}

void DqGeomLib::Shutdown() noexcept {
    g_initialized = false;
}

END_DQ_GEOM_NAMESPACE
