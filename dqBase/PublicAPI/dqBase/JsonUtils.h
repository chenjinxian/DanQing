// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/JsonUtils.ts
// DanQing dqBase — JSON 类型安全解析工具
//
// 1:1 对齐 itwinjs-core JsonUtils。
// 函数签名采用值级别（非成员查找）：调用方先取出 JSON 值，再传入转换。
// 使用 const void* 作为 JSON 值的类型擦除占位符（待集成 JSON 库后替换）。
#pragma once

#include "Export.h"
#include "DqTypes.h"

#include <cstdint>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// JsonUtils — JSON 类型安全解析（对齐 itwinjs-core JsonUtils.ts）
// ---------------------------------------------------------------------------
struct JsonUtils {
    /// 获取布尔值（null/undefined 返回默认值）
    /// Ported from: itwinjs-core JsonUtils.ts:18
    static bool AsBool(const void* json, bool defaultVal = false);

    /// 获取整数值（非 number 返回默认值，截断小数）
    /// Ported from: itwinjs-core JsonUtils.ts:26
    static int AsInt(const void* json, int defaultVal = 0);

    /// 获取浮点值（非 number 返回默认值）
    /// Ported from: itwinjs-core JsonUtils.ts:34
    static double AsDouble(const void* json, double defaultVal = 0.0);

    /// 获取字符串值（null/undefined 返回默认值）
    /// Ported from: itwinjs-core JsonUtils.ts:42
    static DqString AsString(const void* json, const DqString& defaultVal = "");

    /// 获取数组（非数组返回 nullptr）
    /// Ported from: itwinjs-core JsonUtils.ts:50
    static const void* AsArray(const void* json);

    /// 获取对象（非对象返回 nullptr）
    /// Ported from: itwinjs-core JsonUtils.ts:58
    static const void* AsObject(const void* json);

    /// 设置或移除数字值（等于默认值则移除键）
    /// Ported from: itwinjs-core JsonUtils.ts:69
    static void SetOrRemoveNumber(void* json, const char* key, double val, double defaultVal);

    /// 设置或移除布尔值（等于默认值则移除键）
    /// Ported from: itwinjs-core JsonUtils.ts:83
    static void SetOrRemoveBoolean(void* json, const char* key, bool val, bool defaultVal);

    /// 检查是否为非 null 对象
    /// Ported from: itwinjs-core JsonUtils.ts:91
    static bool IsObject(const void* json);

    /// 检查是否为空对象（无键）
    /// Ported from: itwinjs-core JsonUtils.ts:99
    static bool IsEmptyObject(const void* json);

    /// 检查是否为 undefined 或空对象
    /// Ported from: itwinjs-core JsonUtils.ts:107
    static bool IsEmptyObjectOrUndefined(const void* json);

    /// 检查是否为非空对象
    /// Ported from: itwinjs-core JsonUtils.ts:119
    static bool IsNonEmptyObject(const void* json);
};

END_DQ_BASE_NAMESPACE
