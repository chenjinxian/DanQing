// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeDebugLog.h
// DanQing dqBase — 调试日志宏
//
// 1:1 对齐 imodel-native BeDebugLog。
#pragma once

#include "Export.h"

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeDebugLogFunctions — 调试日志
// Ported from: imodel-native BeDebugLog.h
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT BeDebugLogFunctions {
    static void PerformBeDebugLog(const char* message, const char* fileName, unsigned lineNumber);
};

END_DQ_BASE_NAMESPACE

// Debug 模式下的调试日志宏
#ifdef NDEBUG
    #define BeDebugLog(message) ((void)0)
#else
    #define BeDebugLog(message) \
        ::dqBase::BeDebugLogFunctions::PerformBeDebugLog(message, __FILE__, __LINE__)
#endif
