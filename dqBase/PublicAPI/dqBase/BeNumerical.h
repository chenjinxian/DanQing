// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeNumerical.h
// dqBase dqBase — 浮点比较工具
//
// 1:1 对齐 imodel-native BeNumerical。
// 算法/签名/返回类型严格对齐参考：
//   - BeNextafter / BeNextafterf: ref lines 21-22, 26-27（使用 std::nextafter/nextafterf）
//   - BeIsnan / BeFinite:        返回 int（ref lines 23-24, 28-29）
//   - NextafterDelta(sv):        fabs(sv) + BeNextafter(v, DBL_MAX) 方向（ref lines 37-41）
//   - ComputeComparisonTolerance: uv<1.0 ? DBL_EPSILON : NextafterDelta(uv)（ref lines 47-51）
//   - Compare(sv1, sv2):         无 tol 参数（ref lines 57-62）
#pragma once

#include "Export.h"

#include <cfloat>
#include <cmath>
#include <algorithm>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeNumerical — 浮点比较工具
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeNumerical.h
// ---------------------------------------------------------------------------
struct BeNumerical {
    // Ported from: imodel-native BeNumerical.h lines 21-22, 26-27
    //   非平台分支: nextafter / nextafterf（ref:26-27）
    static double BeNextafter(double x, double y) noexcept { return std::nextafter(x, y); }
    static float   BeNextafterf(float x, float y) noexcept { return std::nextafterf(x, y); }

    // Ported from: imodel-native BeNumerical.h lines 23-24, 28-29
    //   返回 int（true→非零，false→0），对齐 _isnan/_finite 与 isnan/isfinite 语义
    static int BeIsnan(double v) noexcept { return std::isnan(v) ? 1 : 0; }
    static int BeFinite(double v) noexcept { return std::isfinite(v) ? 1 : 0; }

    //! 获取 nextafter delta：可加到 fabs(sv) 上得到与 fabs(sv) 不等的最小增量。
    //! 用于计算约 sv 大小的数值的比较容差。
    // Ported from: imodel-native BeNumerical.h lines 37-41
    //   double v = fabs(sv); return BeNextafter(v, DBL_MAX) - v;
    static double NextafterDelta(double sv) noexcept {
        double v = std::fabs(sv);
        return BeNextafter(v, DBL_MAX) - v;
    }

    //! 计算用于判定两数是否不等的容差。
    //! @remarks 不要用两数之差作为容差计算依据！
    //! @param[in] sv1  a value
    //! @param[in] sv2  another value
    // Ported from: imodel-native BeNumerical.h lines 47-51
    //   uv = max(|sv1|,|sv2|); (uv<1.0) ? DBL_EPSILON : NextafterDelta(uv)
    static double ComputeComparisonTolerance(double sv1, double sv2) noexcept {
        double uv = std::max<double>(std::fabs(sv1), std::fabs(sv2));
        return (uv < 1.0) ? DBL_EPSILON : NextafterDelta(uv);
    }

    //! 在最近容差下比较两值，返回 0（相等）/ -1（sv1<sv2）/ 1（sv1>sv2）。
    //! @param[in] sv1  a value
    //! @param[in] sv2  another value
    // Ported from: imodel-native BeNumerical.h lines 57-62
    //   无 tol 参数；严格 fabs(sv2-sv1) < ComputeComparisonTolerance(sv1,sv2)
    static int Compare(double sv1, double sv2) noexcept {
        if (std::fabs(sv2 - sv1) < ComputeComparisonTolerance(sv1, sv2))
            return 0;
        return sv1 < sv2 ? -1 : 1;
    }

    //! Checks if value1 is greater than value2 to the closest tolerance possible.
    // Ported from: imodel-native BeNumerical.h line 65
    static bool IsGreater(double value1, double value2) noexcept { return (Compare(value1, value2) == 1); }

    //! Checks if value1 is greater or equal to value2 to the closest tolerance possible.
    // Ported from: imodel-native BeNumerical.h line 68
    static bool IsGreaterOrEqual(double value1, double value2) noexcept { return (Compare(value1, value2) >= 0); }

    //! Checks if value1 is less than value2 to the closest tolerance possible.
    // Ported from: imodel-native BeNumerical.h line 71
    static bool IsLess(double value1, double value2) noexcept { return (Compare(value1, value2) == -1); }

    //! Checks if value1 is less or equal to value2 to the closest tolerance possible.
    // Ported from: imodel-native BeNumerical.h line 74
    static bool IsLessOrEqual(double value1, double value2) noexcept { return (Compare(value1, value2) <= 0); }

    //! Checks if given values are equal to the closest tolerance possible.
    // Ported from: imodel-native BeNumerical.h line 77
    static bool IsEqual(double value1, double value2) noexcept { return (Compare(value1, value2) == 0); }

    //! Checks if value is equal to zero to the closest tolerance possible.
    // Ported from: imodel-native BeNumerical.h line 80
    static bool IsEqualToZero(double value) noexcept { return IsEqual(value, 0.0); }

    //! Checks if value is greater than zero to the closest tolerance possible.
    // Ported from: imodel-native BeNumerical.h line 83
    static bool IsGreaterThanZero(double value) noexcept { return IsGreater(value, 0.0); }

    //! Checks if value is greater or equal to zero to the closest tolerance possible.
    // Ported from: imodel-native BeNumerical.h line 86
    static bool IsGreaterOrEqualToZero(double value) noexcept { return IsGreaterOrEqual(value, 0.0); }

    //! Checks if value is less than zero to the closest tolerance possible.
    // Ported from: imodel-native BeNumerical.h line 89
    static bool IsLessThanZero(double value) noexcept { return IsLess(value, 0.0); }

    //! Checks if value is less or equal to zero to the closest tolerance possible.
    // Ported from: imodel-native BeNumerical.h line 92
    static bool IsLessOrEqualToZero(double value) noexcept { return IsLessOrEqual(value, 0.0); }
};

END_DQ_BASE_NAMESPACE
