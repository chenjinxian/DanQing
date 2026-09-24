// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — DLL export macro
// Ported from: filament libs/utils/include/utils/compiler.h (UTILS_PUBLIC)
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/Bentley.h (EXPORT_ATTRIBUTE)
#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #if defined(DQ_RENDER_STATIC)
        // 静态库构建：导出宏为空（CMake 对 STATIC 目标 PUBLIC 定义 DQ_RENDER_STATIC）
        #define DQ_RENDER_EXPORT
    #elif defined(DQ_RENDER_BUILDING)
        #define DQ_RENDER_EXPORT __declspec(dllexport)
    #else
        #define DQ_RENDER_EXPORT __declspec(dllimport)
    #endif
#else
    #define DQ_RENDER_EXPORT __attribute__((visibility("default")))
#endif
