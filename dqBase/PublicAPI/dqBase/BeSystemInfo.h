// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeSystemInfo.h
// DanQing dqBase — 系统信息
//
// 1:1 对齐 imodel-native BeSystemInfo。
#pragma once

#include "Export.h"
#include "DqTypes.h"

#include <cstdint>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeSystemInfo — 系统信息
// Ported from: imodel-native BeSystemInfo.h
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT BeSystemInfo {
private:
    BeSystemInfo() = default;

public:
    /// 获取物理内存大小（字节）。成功返回非零值，否则返回 0。
    /// Ported from: imodel-native BeSystemInfo.h GetAmountOfPhysicalMemory
    static uint64_t GetAmountOfPhysicalMemory();

    /// 缓存 Android 平台的设备 ID。
    /// Ported from: imodel-native BeSystemInfo.h CacheAndroidDeviceId
    static void CacheAndroidDeviceId(const DqString& deviceId);

    /// 返回详细的设备型号名称（目前仅在 iOS 上实现）。
    /// Ported from: imodel-native BeSystemInfo.h GetModelName
    static DqString GetModelName();

    /// 获取 CPU 核心数
    /// Ported from: imodel-native BeSystemInfo.h GetNumberOfCpus
    static uint32_t GetNumberOfCpus();

    /// 获取操作系统名称
    /// Ported from: imodel-native BeSystemInfo.h GetOSName
    static DqString GetOSName();

    /// 获取操作系统版本
    /// Ported from: imodel-native BeSystemInfo.h GetOSVersion
    static DqString GetOSVersion();

    /// 获取机器名称
    /// Ported from: imodel-native BeSystemInfo.h GetMachineName
    static DqString GetMachineName();

    /// 获取设备 ID（用于授权等）
    /// Ported from: imodel-native BeSystemInfo.h GetDeviceId
    static DqString GetDeviceId();
};

END_DQ_BASE_NAMESPACE
