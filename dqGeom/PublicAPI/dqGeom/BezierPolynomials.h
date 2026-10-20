// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — UnivariateBezier（Bezier 系数服务）
// Ported from: itwinjs-core core/geometry/src/numerics/BezierPolynomials.ts
//
// 子集（M-W W-b2，AspectRatioSkew 消费面）：BezierCoffs 最小基座（coffs/order/
// zero）+ UnivariateBezier 的 ctor(order)/sumBasisFunctions/sumBasisFunctionDerivatives
// ——Bezier1dNd.evaluate/evaluateDerivative 的全部依赖。全文件余量（roots/
// deflate/elevate/combine/Order2-4 特化类等）TODO——随消费面扩展。
#pragma once

#include "DqGeom.h"
#include "PascalCoefficients.h"

#include <algorithm>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// BezierCoffs — 系数基座（子集：coffs/order/zero）
// Ported from: BezierPolynomials.ts:25-70 BezierCoffs
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT BezierCoffs {
public:
    explicit BezierCoffs(size_t order)
        : m_coffs(order, 0.0)
    {
    }
    virtual ~BezierCoffs() = default;

    // 1:1 coffs 字段（系数数组，length = order 承载）。
    std::vector<double>& coffs() noexcept { return m_coffs; }
    std::vector<double> const& coffs() const noexcept { return m_coffs; }
    // 1:1 order 属性。
    size_t order() const noexcept { return m_coffs.size(); }
    // 1:1 zero()。
    void zero() noexcept { std::fill(m_coffs.begin(), m_coffs.end(), 0.0); }

private:
    std::vector<double> m_coffs;
};

// ---------------------------------------------------------------------------
// UnivariateBezier — 单变量 Bezier（子集：order 构造 + 两个和函数）
// Ported from: BezierPolynomials.ts:359-556 UnivariateBezier
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT UnivariateBezier : public BezierCoffs {
public:
    // Ported from: UnivariateBezier ctor (:363-366——number 形 = order)。
    explicit UnivariateBezier(size_t order)
        : BezierCoffs(order)
    {
    }

    // Ported from: UnivariateBezier.sumBasisFunctions (:522-528)——
    // 基函数值 × 分块控制点加权和（sumWeightedBlocks :501-513）。
    // const 承载（Bezier1dNd::evaluate 的 const 合同——m_basisValues 求值
    // 缓冲 mutable 复用）。
    void sumBasisFunctions(double u, std::vector<double> const& polygon, size_t blockSize,
                           std::vector<double>& result) const
    {
        if (result.size() < blockSize)
            result.assign(blockSize, 0.0);
        PascalCoefficients::getBezierBasisValues(order(), u, m_basisValues);
        sumWeightedBlocks(m_basisValues, order(), polygon, blockSize, result);
    }

    // Ported from: UnivariateBezier.sumBasisFunctionDerivatives (:538-544)。
    void sumBasisFunctionDerivatives(double u, std::vector<double> const& polygon,
                                     size_t blockSize, std::vector<double>& result) const
    {
        if (result.size() < blockSize)
            result.assign(blockSize, 0.0);
        PascalCoefficients::getBezierBasisDerivatives(order(), u, m_basisValues);
        sumWeightedBlocks(m_basisValues, order(), polygon, blockSize, result);
    }

private:
    // Ported from: sumWeightedBlocks (:501-513)——result 先清零再加权累加。
    static void sumWeightedBlocks(std::vector<double> const& weights, size_t numWeights,
                                  std::vector<double> const& data, size_t numPerBlock,
                                  std::vector<double>& result)
    {
        for (size_t k0 = 0; k0 < numPerBlock; ++k0)
            result[k0] = 0.0;
        size_t k = 0;
        for (size_t iWeight = 0; iWeight < numWeights; ++iWeight) {
            double const w = weights[iWeight];
            for (size_t i = 0; i < numPerBlock; ++i)
                result[i] += w * data[k++];
        }
    }

    mutable std::vector<double> m_basisValues;  // 1:1 _basisValues（复用缓冲——const 求值链）
};

END_DQ_GEOM_NAMESPACE
