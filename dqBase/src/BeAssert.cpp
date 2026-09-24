// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeAssert.h
// DanQing dqBase — 断言基础设施实现
//
// 实现 BeAssert.h 中 BeAssertFunctions 的静态成员。SetBeAssertHandler/
// SetBeTestAssertHandler/PerformBeAssert/PerformBeDataAssert 名称 1:1 对齐参考。
#include "dqBase/BeAssert.h"

#include <cstdio>

BEGIN_DQ_BASE_NAMESPACE

namespace {
// 注：ref 使用裸函数指针（T_BeAssertHandler*）；DanQing 内部用 std::function
// 存储以便 lambda 闭包（测试框架需求），对外签名仍为 T_BeAssertHandler*。
BeAssertFunctions::T_BeAssertHandler* g_testHandler = nullptr;
BeAssertFunctions::T_BeAssertHandler* g_runtimeHandler = nullptr;
bool g_inAssert = false;
} // namespace

void BeAssertFunctions::SetBeTestAssertHandler(T_BeAssertHandler* handler) {
    g_testHandler = handler;
}

void BeAssertFunctions::SetBeAssertHandler(T_BeAssertHandler* handler) {
    g_runtimeHandler = handler;
}

void BeAssertFunctions::PerformBeAssert(const char* expr, const char* file, unsigned line) {
    if (g_inAssert) return; // 防止递归
    g_inAssert = true;

    if (g_testHandler) {
        g_testHandler(expr, file, line, AssertType::Normal);
    } else if (g_runtimeHandler) {
        g_runtimeHandler(expr, file, line, AssertType::Normal);
    } else {
        DefaultAssertionFailureHandler(expr, file, line);
    }

    g_inAssert = false;
}

void BeAssertFunctions::PerformBeDataAssert(const char* expr, const char* file, unsigned line) {
    if (g_inAssert) return; // 防止递归
    g_inAssert = true;

    if (g_testHandler) {
        g_testHandler(expr, file, line, AssertType::Data);
    } else if (g_runtimeHandler) {
        g_runtimeHandler(expr, file, line, AssertType::Data);
    } else {
        DefaultAssertionFailureHandler(expr, file, line);
    }

    g_inAssert = false;
}

void BeAssertFunctions::DefaultAssertionFailureHandler(const char* expr, const char* file, unsigned line) {
    fprintf(stderr, "ASSERTION FAILED: %s\n  at %s:%u\n", expr, file, line);
}

END_DQ_BASE_NAMESPACE
