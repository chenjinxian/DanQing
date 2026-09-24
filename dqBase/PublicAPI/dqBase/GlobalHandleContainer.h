// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/GlobalHandleContainer.h
// DanQing dqBase — 全局句柄容器
//
// 1:1 对齐 imodel-native GlobalHandleContainer。
// 将指针映射为唯一 32-bit 句柄。
#pragma once

#include "Export.h"

#include <cstdint>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// GlobalHandleContainer — 指针到句柄的映射
// Ported from: imodel-native GlobalHandleContainer.h
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT GlobalHandleContainer {
    static constexpr uint32_t InvalidHandle = 0;

    /// 检查句柄是否有效
    static bool IsHandleValid(uint32_t handle) noexcept {
        return handle != InvalidHandle;
    }

    /// 分配句柄
    static uint32_t AllocateHandle(void* ptr);

    /// 释放句柄
    static void ReleaseHandle(uint32_t handle);

    /// 从句柄获取指针
    static void* GetPointer(uint32_t handle);

    /// 销毁容器
    static void Destroy();
};

END_DQ_BASE_NAMESPACE
