// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Pascal triangle coefficients for Bezier basis evaluation
// Ported from: itwinjs-core core/geometry/src/numerics/PascalCoefficients.ts
//
// 子集（M-W W-b2，AspectRatioSkew 消费面）：getRow / getBezierBasisValues /
// getBezierBasisDerivatives——UnivariateBezier 求值链的全部依赖。全文件余量
// TODO（BezierPolynomials.h 同注记——随消费面扩展）。
#pragma once

#include "DqGeom.h"

#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// PascalCoefficients — 帕斯卡三角行表 + Bezier 基函数值/导数生成
// Ported from: PascalCoefficients.ts
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT PascalCoefficients {
public:
    // Ported from: PascalCoefficients.getRow (:17-42)——行 0..7 预填，超出按
    // 递推扩展（静态表进程内共享，仅追加不改写——只读访问安全）。
    static std::vector<double> const& getRow(size_t row)
    {
        std::vector<std::vector<double>>& allRows = allRowsMutable();
        if (allRows.empty()) {
            allRows.push_back({1});
            allRows.push_back({1, 1});
            allRows.push_back({1, 2, 1});
            allRows.push_back({1, 3, 3, 1});
            allRows.push_back({1, 4, 6, 4, 1});
            allRows.push_back({1, 5, 10, 10, 5, 1});
            allRows.push_back({1, 6, 15, 20, 15, 6, 1});
            allRows.push_back({1, 7, 21, 35, 35, 21, 7, 1});
        }
        while (allRows.size() <= row) {
            size_t const k = allRows.size();
            auto const& oldRow = allRows[k - 1];
            std::vector<double> newRow(k + 1);
            newRow[0] = 1.0;
            for (size_t i = 1; i < k; ++i)
                newRow[i] = oldRow[i - 1] + oldRow[i];
            newRow[k] = 1.0;
            allRows.push_back(std::move(newRow));
        }
        return allRows[row];
    }

    // Ported from: PascalCoefficients.getBezierBasisValues (:49-68)——帕斯卡行 ×
    // u 升幂（自左）× (1−u) 升幂（自右）。result 复用承载参考的可选目标数组
    // （UnivariateBezier 的 _basisValues 成员缓冲复用语义）。
    static void getBezierBasisValues(size_t order, double u, std::vector<double>& result)
    {
        size_t const row = order - 1;
        auto const& pascalRow = getRow(row);
        if (result.size() < order)
            result.assign(order, 0.0);
        for (size_t i = 0; i < order; ++i)
            result[i] = pascalRow[i];
        // multiply by increasing powers of u ...
        double p = u;
        for (size_t i = 1; i < order; ++i, p *= u)
            result[i] *= p;
        // multiply by powers of (1-u), working from right
        double const v = 1.0 - u;
        p = v;
        for (size_t i = order - 1; i-- > 0; p *= v)
            result[i] *= p;
    }

    // Ported from: PascalCoefficients.getBezierBasisDerivatives (:76-89)——
    // 低一阶基值的就地差分（自右向左覆写）。
    static void getBezierBasisDerivatives(size_t order, double u, std::vector<double>& result)
    {
        if (result.size() < order)
            result.assign(order, 0.0);
        // 参考 `result = this.getBezierBasisValues(order - 1, u, result)` 的目标
        // 就地覆写——低一阶值先写满前 order-1 槽再自右向左差分（读旧写新同
        // 数组，序向保证不串）。
        getBezierBasisValues(order - 1, u, result);
        // derivative is df/du = (order-1) * sum (q[i+1] - q[i]) summed on 0 <= i < order - 1.
        double const f = static_cast<double>(order) - 1.0;
        result[order - 1] = f * result[order - 2];
        for (size_t k = order - 2; k > 0; --k)
            result[k] = f * (result[k - 1] - result[k]);
        result[0] = -f * result[0];
    }

private:
    static std::vector<std::vector<double>>& allRowsMutable()
    {
        static std::vector<std::vector<double>> s_allRows;
        return s_allRows;
    }
};

END_DQ_GEOM_NAMESPACE
