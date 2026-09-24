// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeGetProcAddress.h
// DanQing dqBase — 动态库加载
//
// 1:1 对齐 imodel-native BeGetProcAddress。
#pragma once

#include "Export.h"
#include "DqTypes.h"

#include <cstdint>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeGetProcAddress — 动态库加载
// Ported from: imodel-native BeGetProcAddress.h
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT BeGetProcAddress {
    /// 加载动态库
    static void* LoadLibrary(const DqString& path);

    /// 卸载动态库
    static void UnloadLibrary(void* handle);

    /// 获取函数地址
    static void* GetProcAddress(void* handle, const char* procName);

    /// 设置库搜索路径
    static void SetLibrarySearchPath(const DqString& path);
};

END_DQ_BASE_NAMESPACE
