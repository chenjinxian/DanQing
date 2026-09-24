// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeAssert.h
// DanQing dqBase — 断言基础设施
//
// 1:1 对齐 imodel-native BeAssertFunctions (ref BeAssert.h:13-30):
//   - AssertType 枚举值（Normal=0, Data=1, Sigabrt=2, TypeCount=3, All=99）
//   - T_BeAssertHandler typedef 名称
//   - SetBeTestAssertHandler / SetBeAssertHandler / PerformBeAssert / PerformBeDataAssert
//     方法名与分裂（Normal vs Data 两个独立入口）
//   - DefaultAssertionFailureHandler 三参签名（无 atype）
//
// DanQing adaptation（文档化）: imodel-native 的 T_BeAssertHandler 使用 wchar_t
// (Windows-centric)。DanQing 全栈为 UTF-8/char-based（DqString=std::string），因此
// typedef 形参为 const char* 而非 wchar_t const*。这是 §7.2 SDK 独立性约束的
// 必然结果——保持 char-based 与 DanQing 其余公开头一致。函数名/参数顺序/AssertType
// 全部 1:1 对齐参考实现。
#pragma once

#include "Export.h"

#include <functional>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeAssertFunctions — 断言基础设施
// Ported from: imodel-native BeAssert.h:13 BeAssertFunctions
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT BeAssertFunctions {
    // Ported from: imodel-native BeAssert.h:15-22
    enum class AssertType : int {
        Normal    = 0,
        Data      = 1,
        Sigabrt   = 2,
        TypeCount = 3,
        All       = 99,  // ref value 99 (NOT 3) — §5 数值精度对齐
    };

    // Ported from: imodel-native BeAssert.h:23
    //   typedef void T_BeAssertHandler (wchar_t const*, wchar_t const*, unsigned, AssertType);
    // DanQing adaptation: char-based (UTF-8) — see file header.
    using T_BeAssertHandler = void(const char* expr, const char* file, unsigned line, AssertType atype);

    /// 设置测试断言处理器（测试框架用）。
    /// Ported from: imodel-native BeAssert.h:24 SetBeTestAssertHandler
    static void SetBeTestAssertHandler(T_BeAssertHandler* handler);

    /// 设置运行时断言处理器。
    /// Ported from: imodel-native BeAssert.h:25 SetBeAssertHandler
    static void SetBeAssertHandler(T_BeAssertHandler* handler);

    /// 执行 Normal 类型断言失败处理（AssertType::Normal）。
    /// Ported from: imodel-native BeAssert.h:26 PerformBeAssert
    static void PerformBeAssert(const char* expr, const char* file, unsigned line);

    /// 执行 Data 类型断言失败处理（AssertType::Data）。
    /// Ported from: imodel-native BeAssert.h:27 PerformBeDataAssert
    static void PerformBeDataAssert(const char* expr, const char* file, unsigned line);

    /// 默认断言失败处理（输出到 stderr）。
    /// Ported from: imodel-native BeAssert.h:28 DefaultAssertionFailureHandler（三参，无 atype）
    static void DefaultAssertionFailureHandler(const char* expr, const char* file, unsigned line);
};

END_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// 断言宏
// Ported from: imodel-native BeAssert.h:35-73 BeAssert / BeDataAssert / BeAssertOnce / BeDataAssertOnce
// imodel-native 将宏命名为 BeAssert（基于 wchar_t 的 _CRT_WIDE）。DanQing 命名为 DqAssert
// (char-based)，宏展开形态 1:1 对齐参考语义：失败时短路调用对应 PerformBe*。
// ---------------------------------------------------------------------------

#ifdef NDEBUG
    // Release 模式：断言编译为空（ref BeAssert.h:35-44）
    #define DqAssert(_Expression)             ((void)0)
    #define DqDataAssert(_Expression)         ((void)0)
    #define DqAssertOnce(_Expression)         ((void)0)
    #define DqDataAssertOnce(_Expression)     ((void)0)
#else
    // Debug 模式：失败时调用 PerformBeAssert / PerformBeDataAssert
    // 对齐 ref BeAssert.h:48-50 (BeAssert/BeDataAssert) 的短路求值形态。
    #define DqAssert(_Expression) \
        (void)( (!!(_Expression)) || \
                (::dqBase::BeAssertFunctions::PerformBeAssert(#_Expression, __FILE__, __LINE__), 0) )

    #define DqDataAssert(_Expression) \
        (void)( (!!(_Expression)) || \
                (::dqBase::BeAssertFunctions::PerformBeDataAssert(#_Expression, __FILE__, __LINE__), 0) )

    // 对齐 ref BeAssert.h:52-60 BeAssertOnce — static once guard。
    #define DqAssertOnce(_Expression)                 \
        do {                                          \
            static int st_bAssertedOnce = 0;          \
            if (!st_bAssertedOnce) {                  \
                (!!(_Expression)) ||                  \
                    (::dqBase::BeAssertFunctions::PerformBeAssert(#_Expression, __FILE__, __LINE__), 0); \
                st_bAssertedOnce = 1;                 \
            }                                         \
        } while (0)

    // 对齐 ref BeAssert.h:62-70 BeDataAssertOnce。
    #define DqDataAssertOnce(_Expression)             \
        do {                                          \
            static int st_bAssertedOnce = 0;          \
            if (!st_bAssertedOnce) {                  \
                (!!(_Expression)) ||                  \
                    (::dqBase::BeAssertFunctions::PerformBeDataAssert(#_Expression, __FILE__, __LINE__), 0); \
                st_bAssertedOnce = 1;                 \
            }                                         \
        } while (0)
#endif
