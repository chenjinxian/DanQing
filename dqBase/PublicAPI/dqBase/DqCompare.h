// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/Compare.ts
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeNumerical.h
// DanQing dqBase — 比较工具函数
//
// 1:1 对齐 itwinjs-core Compare.ts 的所有导出函数。
// SortedArray/Dictionary/dqGeom 使用这些比较器。
#pragma once

#include "Export.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// OrderedComparator — 有序比较器类型（对齐 itwinjs-core OrderedComparator）
// Ported from: itwinjs-core Compare.ts OrderedComparator
// ---------------------------------------------------------------------------
template<typename T, typename U = T>
using OrderedComparator = std::function<int(const T&, const U&)>;

// ---------------------------------------------------------------------------
// 基础比较函数
// Ported from: itwinjs-core Compare.ts
// ---------------------------------------------------------------------------

/// 数值比较：返回 -1, 0, 或 1
inline int compareNumbers(double a, double b) noexcept {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

/// 带容差的数值比较
/// Ported from: itwinjs-core Compare.ts compareWithTolerance
inline int compareWithTolerance(double a, double b, double tolerance = 0.1) noexcept {
    double diff = a - b;
    if (std::abs(diff) <= tolerance) return 0;
    return diff < 0 ? -1 : 1;
}

/// 布尔比较：false < true
/// Ported from: itwinjs-core Compare.ts compareBooleans
inline int compareBooleans(bool a, bool b) noexcept {
    if (a == b) return 0;
    return a ? 1 : -1;
}

/// 字符串比较（字典序）
/// Ported from: itwinjs-core Compare.ts compareStrings
inline int compareStrings(const std::string& a, const std::string& b) noexcept {
    return a.compare(b);
}

// ---------------------------------------------------------------------------
// 可选值比较（处理 undefined/null 情况）
// Ported from: itwinjs-core Compare.ts
// ---------------------------------------------------------------------------

/// 比较两个可能未定义的值。已定义值用 compareDefined 比较；
/// 两个都未定义返回 0；未定义的排在已定义的前面。
template<typename T>
int comparePossiblyUndefined(
    std::function<int(const T&, const T&)> compareDefined,
    const T* lhs, const T* rhs)
{
    if (!lhs && !rhs) return 0;
    if (!lhs) return -1;
    if (!rhs) return 1;
    return compareDefined(*lhs, *rhs);
}

/// 比较两个可能为空的字符串
inline int compareStringsOrUndefined(const std::string* lhs, const std::string* rhs) {
    if (!lhs && !rhs) return 0;
    if (!lhs) return -1;
    if (!rhs) return 1;
    return lhs->compare(*rhs);
}

/// 比较两个可能为零的数值
inline int compareNumbersOrUndefined(const double* lhs, const double* rhs) {
    if (!lhs && !rhs) return 0;
    if (!lhs) return -1;
    if (!rhs) return 1;
    return compareNumbers(*lhs, *rhs);
}

/// 比较两个可能为零的布尔值
inline int compareBooleansOrUndefined(const bool* lhs, const bool* rhs) {
    if (!lhs && !rhs) return 0;
    if (!lhs) return -1;
    if (!rhs) return 1;
    return compareBooleans(*lhs, *rhs);
}

// ---------------------------------------------------------------------------
// 相等性检查
// Ported from: itwinjs-core Compare.ts areEqualPossiblyUndefined
// ---------------------------------------------------------------------------

template<typename T, typename U>
bool areEqualPossiblyUndefined(const T* t, const U* u,
    std::function<bool(const T&, const U&)> areEqual)
{
    if (!t && !u) return true;
    if (!t || !u) return false;
    return areEqual(*t, *u);
}

// ---------------------------------------------------------------------------
// 数组比较
// Ported from: itwinjs-core Compare.ts compareArrays
// ---------------------------------------------------------------------------

template<typename T>
int compareArrays(const std::vector<T>& lhs, const std::vector<T>& rhs,
    std::function<int(const T&, const T&)> compare)
{
    size_t minSize = lhs.size() < rhs.size() ? lhs.size() : rhs.size();
    for (size_t i = 0; i < minSize; ++i) {
        int c = compare(lhs[i], rhs[i]);
        if (c != 0) return c;
    }
    if (lhs.size() < rhs.size()) return -1;
    if (lhs.size() > rhs.size()) return 1;
    return 0;
}

// ---------------------------------------------------------------------------
// Simple types comparison (number | string | boolean)
// Ported from: itwinjs-core Compare.ts compareSimpleTypes (ref:100) +
//               SimpleTypesArray (ref:124) + compareSimpleArrays (ref:131)
// ---------------------------------------------------------------------------

/// @brief Variant 覆盖 number/string/boolean 三种简单类型。
///        对齐 itwinjs-core SimpleTypesArray = number[] | string[] | boolean[]。
using SimpleVariant = std::variant<double, std::string, bool>;

/// @brief 简单类型的数组变体（对齐 itwinjs-core SimpleTypesArray）。
///        ref:124 — `export type SimpleTypesArray = number[] | string[] | boolean[];`
using SimpleTypesArray = std::variant<std::vector<double>, std::vector<std::string>, std::vector<bool>>;

namespace detail {
    // 返回 variant index 对应的 TS 类型名字符串（对齐 ref:104 `typeof lhs`）。
    inline const char* simpleTypeName(const SimpleVariant& v) {
        switch (v.index()) {
            case 0: return "number";
            case 1: return "string";
            case 2: return "boolean";
            default: return "undefined";
        }
    }
} // namespace detail

/// @brief 比较两个简单类型 (number | string | boolean)。
///        对齐 itwinjs-core Compare.ts:100 compareSimpleTypes。
///        ref 语义：先比较类型名 (typeof)，不同类型按类型名字典序返回；
///        相同类型则用对应 compareNumbers/compareStrings/compareBooleans。
inline int compareSimpleTypes(const SimpleVariant& lhs, const SimpleVariant& rhs) {
    // Make sure the types are the same (ref:104-107)
    int typeCmp = compareStrings(detail::simpleTypeName(lhs), detail::simpleTypeName(rhs));
    if (typeCmp != 0) {
        return typeCmp;
    }
    // Compare actual values (ref:110-117)
    switch (lhs.index()) {
        case 0: return compareNumbers(std::get<0>(lhs), std::get<0>(rhs)); // number
        case 1: return compareStrings(std::get<1>(lhs), std::get<1>(rhs)); // string
        case 2: return compareBooleans(std::get<2>(lhs), std::get<2>(rhs)); // boolean
        default: return 0;
    }
}

/// @brief 比较两个简单类型数组 (number[] | string[] | boolean[])。
///        对齐 itwinjs-core Compare.ts:131 compareSimpleArrays。
///        ref 语义：任一为空/undefined 优先处理；长度不等返回长度差；
///        长度相等则逐元素用 compareSimpleTypes 比较，首个非零结果即返回。
/// @note 当两数组 variant index 不同时（类型不同），按 variant index 比较类型，
///       与 ref 在 TS 层通过 typeof 推断一致。
inline int compareSimpleArrays(const SimpleTypesArray* lhs, const SimpleTypesArray* rhs) {
    // ref:132-135 — undefined handling
    if (lhs == nullptr) return (rhs == nullptr) ? 0 : -1;
    if (rhs == nullptr) return 1;

    // 类型不同时按 variant index 比较（等价于 ref 中 typeof 不同）。
    if (lhs->index() != rhs->index()) {
        return lhs->index() < rhs->index() ? -1 : 1;
    }

    // 长度比较 (ref:136-140)
    auto lengthsAsLong = [](const auto& vec) -> long {
        return static_cast<long>(vec.size());
    };

    // 同类型分别处理三种情况
    switch (lhs->index()) {
        case 0: { // number[]
            const auto& la = std::get<0>(*lhs);
            const auto& ra = std::get<0>(*rhs);
            if (la.empty() && ra.empty()) return 0; // ref:136-138
            if (la.size() != ra.size()) return static_cast<int>(lengthsAsLong(la) - lengthsAsLong(ra)); // ref:138-139
            for (size_t i = 0; i < la.size(); ++i) { // ref:142-148
                int c = compareSimpleTypes(SimpleVariant(la[i]), SimpleVariant(ra[i]));
                if (c != 0) return c;
            }
            return 0;
        }
        case 1: { // string[]
            const auto& la = std::get<1>(*lhs);
            const auto& ra = std::get<1>(*rhs);
            if (la.empty() && ra.empty()) return 0;
            if (la.size() != ra.size()) return static_cast<int>(lengthsAsLong(la) - lengthsAsLong(ra));
            for (size_t i = 0; i < la.size(); ++i) {
                int c = compareSimpleTypes(SimpleVariant(la[i]), SimpleVariant(ra[i]));
                if (c != 0) return c;
            }
            return 0;
        }
        case 2: { // boolean[]
            const auto& la = std::get<2>(*lhs);
            const auto& ra = std::get<2>(*rhs);
            if (la.empty() && ra.empty()) return 0;
            if (la.size() != ra.size()) return static_cast<int>(lengthsAsLong(la) - lengthsAsLong(ra));
            for (size_t i = 0; i < la.size(); ++i) {
                // vector<bool> 返回代理引用，需显式转 bool
                int c = compareSimpleTypes(SimpleVariant(static_cast<bool>(la[i])),
                                           SimpleVariant(static_cast<bool>(ra[i])));
                if (c != 0) return c;
            }
            return 0;
        }
        default:
            return 0;
    }
}

END_DQ_BASE_NAMESPACE
