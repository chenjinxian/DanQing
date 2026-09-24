// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — DLL export macro
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Bentley.h (EXPORT_ATTRIBUTE)
// 注：dqApp 是应用层，允许使用 Qt，但 Export.h 保持平台原生宏以统一风格。
#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #if defined(DQ_APP_STATIC)
        // 静态库构建：导出宏为空（CMake 对 STATIC 目标 PUBLIC 定义 DQ_APP_STATIC）
        #define DQ_APP_EXPORT
    #elif defined(DQ_APP_BUILDING)
        #define DQ_APP_EXPORT __declspec(dllexport)
    #else
        #define DQ_APP_EXPORT __declspec(dllimport)
    #endif
#else
    #define DQ_APP_EXPORT __attribute__((visibility("default")))
#endif
