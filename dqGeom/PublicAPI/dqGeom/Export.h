// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — DLL 导出宏
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Bentley.h (EXPORT_ATTRIBUTE)
#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #if defined(DQ_GEOM_STATIC)
        // 静态库构建：导出宏为空（CMake 对 STATIC 目标 PUBLIC 定义 DQ_GEOM_STATIC）
        #define DQ_GEOM_EXPORT
    #elif defined(DQ_GEOM_BUILDING)
        #define DQ_GEOM_EXPORT __declspec(dllexport)
    #else
        #define DQ_GEOM_EXPORT __declspec(dllimport)
    #endif
#else
    #define DQ_GEOM_EXPORT __attribute__((visibility("default")))
#endif
