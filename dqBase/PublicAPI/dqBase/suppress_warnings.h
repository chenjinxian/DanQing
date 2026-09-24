// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/suppress_warnings.h
//              AND iModelCore/Bentley/PublicAPI/Bentley/Bentley.h:342-401
//                  PUSH_MSVC_IGNORE / POP_MSVC_IGNORE            (Bentley.h:343-355, 368-369)
//                  PUSH_MSVC_IGNORE_ANALYZE / POP_MSVC_IGNORE_ANALYZE (Bentley.h:351-352, 370)
//                  PUSH_DISABLE_DEPRECATION_WARNINGS / POP_...   (Bentley.h:354-355, 391-392)
//                  PUSH_CLANG_IGNORE / POP_CLANG_IGNORE          (Bentley.h:385-389, 399-400)
//                  CLANG_DIAG_* helpers                          (Bentley.h:380-383)
// DanQing dqBase — 编译器警告抑制
//
// 1:1 对齐 imodel-native：
//   - UNREACHABLE_CODE(stmt) 默认展开为 stmt（ref suppress_warnings.h:34-39）。
//     反转了此前 DanQing 的 __builtin_unreachable()/__assume(false) 实现——那与
//     参考语义相反（参考明确在注释中说明该宏用于"emit a return statement"）。
//   - PUSH_CLANG_IGNORE(warnName) 内部拼接 -W 前缀（ref Bentley.h:385-388 via
//     CLANG_DIAG_JOINSTR(-W,x)），调用方传入已剥离 -W 的警告名。
//   - MSVC 全局警告禁用（4100/4706/4127/4245/4389/4201）1:1 对齐 ref suppress_warnings.h:8-41。
//   - BENTLEY_WARNINGS_HIGHEST_LEVEL 条件（ref:35-39）映射为 DanQing 前缀
//     DQ_BASE_WARNINGS_HIGHEST_LEVEL。
#pragma once
/// @cond DQ_BASE_SDK_Internal

#if defined(_MSC_VER)

// --- MSVC 全局警告禁用 (ref suppress_warnings.h:8-32) ---
#pragma warning(disable:4100) // unreferenced formal parameter
#pragma warning(disable:4706) // assignment within conditional expression
#pragma warning(disable:4127) // conditional expression is constant

// ref suppress_warnings.h:14-21 — signed 常量赋值给 unsigned 变量（位赋值，安全）。
#pragma warning(disable:4245) // 'initializing': conversion from 'int' to 'unsigned ...', signed/unsigned mismatch

// ref suppress_warnings.h:24-26 — 同尺寸 signed/unsigned 等值比较（如 bit-field vs bool）。
#pragma warning(disable:4389) // '==' and '!=': signed/unsigned mismatch

// ref suppress_warnings.h:32 — nameless struct (GCC 也支持)。
#pragma warning(disable:4201) // nonstandard extension used: nameless struct

#endif // _MSC_VER

// --- UNREACHABLE_CODE(stmt) (ref suppress_warnings.h:34-39) ---
// 参考注释: "Use the UNREACHABLE_CODE macro to mark code that you know is unreachable.
//  This may be necessary when adding a return statement to avoid a compiler warning."
// 默认（未定义 DQ_BASE_WARNINGS_HIGHEST_LEVEL）展开为 stmt —— 语句被实际发射。
// 仅当定义 DQ_BASE_WARNINGS_HIGHEST_LEVEL 时才丢弃 stmt。
#if !defined(DQ_BASE_WARNINGS_HIGHEST_LEVEL)
    #define UNREACHABLE_CODE(stmt)  stmt
#else
    #define UNREACHABLE_CODE(stmt)
#endif

// --- PUSH_MSVC_IGNORE / POP_MSVC_IGNORE (ref Bentley.h:342-355, 368-369) ---
#if defined(_MSC_VER) && !defined(DQ_BASE_RESOURCE_COMPILER)
    #define PUSH_MSVC_IGNORE(ERRORS_TO_IGNORE) \
        __pragma(warning(push))                \
        __pragma(warning(disable: ERRORS_TO_IGNORE))

    #define POP_MSVC_IGNORE __pragma(warning(pop))

    #define PUSH_DISABLE_DEPRECATION_WARNINGS PUSH_MSVC_IGNORE(4996)
    #define POP_DISABLE_DEPRECATION_WARNINGS  POP_MSVC_IGNORE
#else
    #define PUSH_MSVC_IGNORE(ERRORS_TO_IGNORE)
    #define POP_MSVC_IGNORE

    #define PUSH_DISABLE_DEPRECATION_WARNINGS \
        _Pragma("GCC diagnostic push")                                   \
        _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
    #define POP_DISABLE_DEPRECATION_WARNINGS _Pragma("GCC diagnostic pop")
#endif

// --- PUSH_CLANG_IGNORE / POP_CLANG_IGNORE (ref Bentley.h:377-389, 398-400) ---
// ref 在内部拼接 -W 前缀（CLANG_DIAG_JOINSTR(-W,x)），调用方传入已剥离 -W 的警告名。
#ifdef __clang__
    // ref Bentley.h:380-383 helpers:
    #define DQ_BASE_CLANG_DIAG_STR(s)         #s
    #define DQ_BASE_CLANG_DIAG_JOINSTR(x, y)  DQ_BASE_CLANG_DIAG_STR(x ## y)
    #define DQ_BASE_CLANG_DIAG_DO_PRAGMA(x)   _Pragma(#x)
    #define DQ_BASE_CLANG_DIAG_PRAGMA(x)      DQ_BASE_CLANG_DIAG_DO_PRAGMA(clang diagnostic x)

    // ref Bentley.h:385-387: x 是不带 -W 的警告名；宏内部补 -W 前缀。
    #define PUSH_CLANG_IGNORE(x) \
        DQ_BASE_CLANG_DIAG_PRAGMA(push) \
        DQ_BASE_CLANG_DIAG_PRAGMA(ignored DQ_BASE_CLANG_DIAG_JOINSTR(-W, x))

    // ref Bentley.h:388-389
    #define POP_CLANG_IGNORE DQ_BASE_CLANG_DIAG_PRAGMA(pop)
#else
    #define PUSH_CLANG_IGNORE(x)
    #define POP_CLANG_IGNORE
#endif

/// @endcond DQ_BASE_SDK_Internal
