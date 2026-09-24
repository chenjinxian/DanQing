// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/JsonUtils.ts
// DanQing dqBase — JSON 工具实现
//
// 注意：当前为桩实现。JSON 值使用 const void* 类型擦除。
// 实际使用时需集成 JSON 库（如 nlohmann/json），将 void* 替换为具体类型。
#include "dqBase/JsonUtils.h"

BEGIN_DQ_BASE_NAMESPACE

// Ported from: itwinjs-core JsonUtils.ts:18 — asBool
bool JsonUtils::AsBool(const void* json, bool defaultVal) {
    if (json == nullptr) return defaultVal;
    // 桩：非 null 视为 true（待集成 JSON 库后实现完整类型检查）
    return true;
}

// Ported from: itwinjs-core JsonUtils.ts:26 — asInt
int JsonUtils::AsInt(const void* json, int defaultVal) {
    if (json == nullptr) return defaultVal;
    // 桩：待集成 JSON 库后实现 typeof json === "number" ? Math.trunc(json) : defaultVal
    return defaultVal;
}

// Ported from: itwinjs-core JsonUtils.ts:34 — asDouble
double JsonUtils::AsDouble(const void* json, double defaultVal) {
    if (json == nullptr) return defaultVal;
    return defaultVal;
}

// Ported from: itwinjs-core JsonUtils.ts:42 — asString
DqString JsonUtils::AsString(const void* json, const DqString& defaultVal) {
    if (json == nullptr) return defaultVal;
    return defaultVal;
}

// Ported from: itwinjs-core JsonUtils.ts:50 — asArray
const void* JsonUtils::AsArray(const void* /*json*/) {
    // 桩：待集成 JSON 库后实现 Array.isArray(json) ? json : undefined
    return nullptr;
}

// Ported from: itwinjs-core JsonUtils.ts:58 — asObject
const void* JsonUtils::AsObject(const void* /*json*/) {
    // 桩：待集成 JSON 库后实现 typeof json === "object" ? json : undefined
    return nullptr;
}

// Ported from: itwinjs-core JsonUtils.ts:69 — setOrRemoveNumber
void JsonUtils::SetOrRemoveNumber(void* /*json*/, const char* /*key*/, double /*val*/, double /*defaultVal*/) {
    // 桩：待集成 JSON 库后实现
}

// Ported from: itwinjs-core JsonUtils.ts:83 — setOrRemoveBoolean
void JsonUtils::SetOrRemoveBoolean(void* /*json*/, const char* /*key*/, bool /*val*/, bool /*defaultVal*/) {
    // 桩：待集成 JSON 库后实现
}

// Ported from: itwinjs-core JsonUtils.ts:91 — isObject
bool JsonUtils::IsObject(const void* json) {
    return json != nullptr;
}

// Ported from: itwinjs-core JsonUtils.ts:99 — isEmptyObject
bool JsonUtils::IsEmptyObject(const void* json) {
    // 桩：待集成 JSON 库后实现 Object.keys(json).length === 0
    return json == nullptr;
}

// Ported from: itwinjs-core JsonUtils.ts:107 — isEmptyObjectOrUndefined
bool JsonUtils::IsEmptyObjectOrUndefined(const void* json) {
    return json == nullptr || IsEmptyObject(json);
}

// Ported from: itwinjs-core JsonUtils.ts:119 — isNonEmptyObject
bool JsonUtils::IsNonEmptyObject(const void* json) {
    return !IsEmptyObjectOrUndefined(json);
}

END_DQ_BASE_NAMESPACE
