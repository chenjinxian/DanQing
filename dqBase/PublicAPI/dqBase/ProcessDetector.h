// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/ProcessDetector.ts
// DanQing dqBase — 进程检测
//
// 1:1 对齐 itwinjs-core ProcessDetector。
// C++ 版本仅检测编译目标平台。
#pragma once

#include "Export.h"

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// ProcessDetector — 运行时环境检测
// Ported from: itwinjs-core ProcessDetector.ts
// ---------------------------------------------------------------------------
struct ProcessDetector {
    /// 是否为桌面平台
    static constexpr bool IsDesktopPlatform() noexcept {
#if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
        return true;
#else
        return false;
#endif
    }

    /// 是否为移动平台
    static constexpr bool IsMobilePlatform() noexcept {
#if defined(__ANDROID__) || defined(__APPLE_IOS__)
        return true;
#else
        return false;
#endif
    }

    /// 是否为 Apple 平台
    static constexpr bool IsApplePlatform() noexcept {
#if defined(__APPLE__)
        return true;
#else
        return false;
#endif
    }

    /// 是否为 Windows 平台
    static constexpr bool IsWindowsPlatform() noexcept {
#if defined(_WIN32)
        return true;
#else
        return false;
#endif
    }

    /// 是否为 Linux 平台
    static constexpr bool IsLinuxPlatform() noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
        return true;
#else
        return false;
#endif
    }
};

END_DQ_BASE_NAMESPACE
