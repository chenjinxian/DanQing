// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeDebugLog.h
// DanQing dqBase — 调试日志实现
#include "dqBase/BeDebugLog.h"

#include <cstdio>

BEGIN_DQ_BASE_NAMESPACE

void BeDebugLogFunctions::PerformBeDebugLog(const char* message, const char* fileName, unsigned lineNumber) {
    fprintf(stderr, "[DEBUG] %s (%s:%u)\n", message ? message : "", fileName ? fileName : "", lineNumber);
}

END_DQ_BASE_NAMESPACE
