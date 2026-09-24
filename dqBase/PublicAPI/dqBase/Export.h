// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Bentley.h (EXPORT_ATTRIBUTE)
//              filament libs/utils/include/utils/compiler.h (UTILS_PUBLIC)
// DanQing dqBase — DLL 导出宏 + 命名空间宏
//
// 替代 Qt Q_DECL_EXPORT / Q_DECL_IMPORT，直接用平台原生属性。
// 命名空间宏放在此处（最低层头文件），避免循环依赖。
#pragma once

#if defined(_WIN32) || defined(_WIN64)
    // MSVC / Windows
    #if defined(DQ_BASE_STATIC)
        // 静态库构建：导出宏为空（CMake 对 STATIC 目标 PUBLIC 定义 DQ_BASE_STATIC，
        // dllimport/dllexport 对静态库无意义且触发 C4273/C4251/C4275）
        #define DQ_BASE_EXPORT
    #elif defined(DQ_BASE_BUILDING)
        #define DQ_BASE_EXPORT __declspec(dllexport)
    #else
        #define DQ_BASE_EXPORT __declspec(dllimport)
    #endif
#else
    // GCC / Clang — 对齐 imodel-native EXPORT_ATTRIBUTE
    #define DQ_BASE_EXPORT __attribute__((visibility("default")))
#endif

// 其他模块照此模式声明各自的导出宏：
//   DQ_RENDER_EXPORT / DQ_DATA_EXPORT / DQ_GEOM_EXPORT / DQ_APP_EXPORT

// ---------------------------------------------------------------------------
// 命名空间宏（所有 dqBase 头文件共用）
// ---------------------------------------------------------------------------
#define BEGIN_DQ_BASE_NAMESPACE namespace dqBase {
#define END_DQ_BASE_NAMESPACE }

// ---------------------------------------------------------------------------
// DEFINE_T_SUPER — 在派生类中引用基类的别名，用于显式调用基类成员。
// 对齐 imodel-native Bentley.h:241 — 完全逐字移植（参考代码，非自设计，§3）。
//
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Bentley.h
//              #define DEFINE_T_SUPER(B) private: typedef B T_Super; public:
// ---------------------------------------------------------------------------
#define DEFINE_T_SUPER(B) private: typedef B T_Super; public:
