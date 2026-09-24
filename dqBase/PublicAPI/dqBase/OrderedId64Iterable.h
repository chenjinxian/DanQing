// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/OrderedId64Iterable.ts
// DanQing dqBase — 有序 ID 迭代工具
//
// 1:1 对齐 itwinjs-core OrderedId64Iterable。
// 提供有序 ID 集合的集合运算（union, intersection, difference）。
#pragma once

#include "Export.h"
#include "DqId.h"
#include "DqTypes.h"

#include <algorithm>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// OrderedId64Iterable — 有序 ID 迭代工具
// Ported from: itwinjs-core OrderedId64Iterable.ts
// ---------------------------------------------------------------------------
struct OrderedId64Iterable {
    /// 比较 ID 的 hex 字符串形式：先比较长度，等长再字典序。
    /// Ported from: itwinjs-core OrderedId64Iterable.ts compare (ref:32-42)
    ///   if (lhs.length !== rhs.length)
    ///     return lhs.length < rhs.length ? -1 : 1;
    ///   if (lhs !== rhs)
    ///     return lhs < rhs ? -1 : 1;
    ///   return 0;
    ///
    /// 此为参考实现的精确移植：作用于字符串表面（Id64String），长度优先于字典序。
    /// 对 canonical（无前导零）hex 输入等价于数值序；对非 canonical 输入（前导零、
    /// 混合长度）与"裸数值 <"分歧——这正是 P1 #10 审计行的分歧面。
    static int CompareString(DqStringView lhs, DqStringView rhs) noexcept {
        if (lhs.size() != rhs.size())
            return lhs.size() < rhs.size() ? -1 : 1;
        if (lhs != rhs) {
            // std::string_view::operator< 已是字典序（unsigned char 比较）
            return lhs < rhs ? -1 : 1;
        }
        return 0;
    }

    /// 比较 DqId：使用 canonical hex 字符串形式委托给 CompareString。
    /// Ported from: itwinjs-core OrderedId64Iterable.compare
    ///
    /// 注：DqId::ToString() 总是产生 canonical 形式（无前导零），故此处与原数值
    /// `<` 行为等价；保留委托形式以 1:1 对齐参考算法。
    static int Compare(DqId a, DqId b) {
        return CompareString(a.ToString(), b.ToString());
    }

    /// 原地排序 ID 数组（按 Compare 序）
    /// Ported from: itwinjs-core OrderedId64Iterable.sortArray (ref:51-54)
    static void SortArray(DqIdVector& ids) {
        std::sort(ids.begin(), ids.end(), [](DqId x, DqId y) {
            return Compare(x, y) < 0;
        });
    }

    /// 检查两个有序集合是否相等（参考 uniqueIterator 流式去重比对）
    /// Ported from: itwinjs-core OrderedId64Iterable.areEqualSets (ref:59-79)
    ///
    /// 注：参考使用 uniqueIterator 消除重复后再逐元素比对，因此
    /// `[[1,1,1],[1]]` 应判为相等。原实现 `operator==` 不去重，对带重复输入
    /// 会给出错误结果（审计行 41）。
    static bool AreEqualSets(const DqIdVector& a, const DqIdVector& b) {
        size_t i = 0, j = 0;
        const size_t na = a.size(), nb = b.size();
        // 跳过连续重复（ref uniqueIterator 的 prev 去重逻辑 ref:102-115）
        auto skipDupA = [&]() {
            while (i + 1 < na && a[i + 1] == a[i]) ++i;
        };
        auto skipDupB = [&]() {
            while (j + 1 < nb && b[j + 1] == b[j]) ++j;
        };
        while (i < na && j < nb) {
            if (Compare(a[i], b[j]) != 0) return false;
            skipDupA();
            skipDupB();
            ++i; ++j;
        }
        return i == na && j == nb;
    }

    /// 检查有序集合是否为空
    /// Ported from: itwinjs-core OrderedId64Iterable.isEmptySet
    static bool IsEmptySet(const DqIdVector& ids) {
        return ids.empty();
    }

    /// 计算两个有序集合的并集
    /// Ported from: itwinjs-core OrderedId64Iterable.union
    static DqIdVector Union(const DqIdVector& a, const DqIdVector& b) {
        DqIdVector result;
        result.reserve(a.size() + b.size());
        std::set_union(a.begin(), a.end(), b.begin(), b.end(),
                       std::back_inserter(result),
                       [](DqId x, DqId y) { return Compare(x, y) < 0; });
        return result;
    }

    /// 计算两个有序集合的交集
    /// Ported from: itwinjs-core OrderedId64Iterable.intersection
    static DqIdVector Intersection(const DqIdVector& a, const DqIdVector& b) {
        DqIdVector result;
        result.reserve(std::min(a.size(), b.size()));
        std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                              std::back_inserter(result),
                              [](DqId x, DqId y) { return Compare(x, y) < 0; });
        return result;
    }

    /// 计算两个有序集合的差集 (a - b)
    /// Ported from: itwinjs-core OrderedId64Iterable.difference
    static DqIdVector Difference(const DqIdVector& a, const DqIdVector& b) {
        DqIdVector result;
        result.reserve(a.size());
        std::set_difference(a.begin(), a.end(), b.begin(), b.end(),
                            std::back_inserter(result),
                            [](DqId x, DqId y) { return Compare(x, y) < 0; });
        return result;
    }

    /// 去重有序集合（仅消除连续重复，不重排——参考 unique 要求输入已有序）
    /// Ported from: itwinjs-core OrderedId64Iterable.unique (ref:95-97) +
    ///               uniqueIterator (ref:102-115)
    ///
    /// 注：参考 unique 仅基于"前一个元素"做连续去重，假定输入已按 Compare 序排列。
    /// 原实现先 std::sort 再 std::unique 违反参考语义（会改变无序输入的相对顺序，
    /// 而参考明确不要求排序）。此处改为只去连续重复。
    static DqIdVector Unique(const DqIdVector& ids) {
        DqIdVector out;
        out.reserve(ids.size());
        for (size_t i = 0; i < ids.size(); ++i) {
            if (!out.empty() && out.back() == ids[i]) continue;
            out.push_back(ids[i]);
        }
        return out;
    }
};

END_DQ_BASE_NAMESPACE
