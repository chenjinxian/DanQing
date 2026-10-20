// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Bezier1dNd（Bezier 曲线共享极点容器）
// Ported from: itwinjs-core core/geometry/src/bspline/Bezier1dNd.ts
//
// 子集（M-W W-b2，AspectRatioSkew/BezierCurve3d 消费面）：ctor(blockSize,
// polygon)/clonePolygon/order/packedData/evaluate/evaluateDerivative/
// getPolygonPoint/setPolygonPoint/loadSpanPoles/isAlmostEqual/reverseInPlace/
// interpolatePoleInPlace/saturateInPlace/setInterval/fractionToParentFraction。
// TODO（随消费面扩展）：create（Point2d/3d/4d 工厂面）、loadSpanPolesWithWeight、
// unpackToJsonArrays、saturate1dInPlace（平面相交链）、subdivide*（
// clonePartialCurve 链）。
#pragma once

#include "DqGeom.h"
#include "BezierPolynomials.h"
#include "KnotVector.h"
#include "Segment1d.h"

#include <algorithm>
#include <optional>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// Bezier1dNd — 分块极点的 Bezier 曲线核
// Ported from: Bezier1dNd.ts:24-364
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT Bezier1dNd {
public:
    // 1:1 ctor（:30-35——CAPTURES polygon；DanQing 值语义移动承载）。
    Bezier1dNd(size_t blockSize, std::vector<double> polygon)
        : m_packedData(std::move(polygon))
        , m_order(m_packedData.size() / blockSize)
        , m_blockSize(blockSize)
        , m_basis(m_order)
    {
    }

    // 1:1 clonePolygon（:40-48——无目标参形态=复制）。
    std::vector<double> clonePolygon() const { return m_packedData; }

    // 1:1 order/packedData（:50-52）。
    size_t order() const noexcept { return m_order; }
    std::vector<double>& packedData() noexcept { return m_packedData; }
    std::vector<double> const& packedData() const noexcept { return m_packedData; }

    // 1:1 evaluate（:93-95——参考非 const 方法；DanQing const 承载以适配
    // CurvePrimitive::FractionToPoint 的 const 合同——m_basis 的求值缓冲经
    // mutable 复用）。
    void evaluate(double s, std::vector<double>& buffer) const
    {
        m_basis.sumBasisFunctions(s, m_packedData, m_blockSize, buffer);
    }

    // 1:1 evaluateDerivative（:102-104——const 承载同上）。
    void evaluateDerivative(double s, std::vector<double>& buffer) const
    {
        m_basis.sumBasisFunctionDerivatives(s, m_packedData, m_blockSize, buffer);
    }

    // 1:1 getPolygonPoint（:111-121——越界 false，参考 undefined）。
    bool getPolygonPoint(size_t i, std::vector<double>& buffer) const
    {
        if (buffer.size() < m_blockSize)
            buffer.assign(m_blockSize, 0.0);
        if (i < m_order) {
            size_t const k0 = m_blockSize * i;
            for (size_t k = 0; k < m_blockSize; ++k)
                buffer[k] = m_packedData[k0 + k];
            return true;
        }
        return false;
    }

    // 1:1 setPolygonPoint（:123-129）。
    void setPolygonPoint(size_t i, std::vector<double> const& buffer)
    {
        if (i < m_order) {
            size_t const k0 = m_blockSize * i;
            for (size_t k = 0; k < m_blockSize; ++k)
                m_packedData[k0 + k] = buffer[k];
        }
    }

    // 1:1 loadSpanPoles（:134-138——从 data[blockSize*spanIndex] 起装载整阶）。
    void loadSpanPoles(std::vector<double> const& data, size_t spanIndex)
    {
        size_t k = spanIndex * m_blockSize;
        for (size_t i = 0; i < m_packedData.size(); ++i)
            m_packedData[i] = data[k++];
    }

    // 1:1 isAlmostEqual（:160-175——blockSize/order/逐坐标 isSameCoordinate）。
    bool isAlmostEqual(Bezier1dNd const& other) const
    {
        if (m_blockSize != other.m_blockSize)
            return false;
        if (m_order != other.m_order)
            return false;
        if (m_packedData.size() != other.m_packedData.size())
            return false;
        for (size_t i = 0; i < m_packedData.size(); ++i) {
            if (!isSameCoordinate(m_packedData[i], other.m_packedData[i]))
                return false;
        }
        return true;
    }

    // 1:1 reverseInPlace（:177-189——块反转）。
    void reverseInPlace()
    {
        size_t const m = m_blockSize;
        size_t const n = m_order;
        for (size_t i = 0, j = (n - 1) * m; i < j; i += m, j -= m) {
            for (size_t k = 0; k < m; ++k)
                std::swap(m_packedData[i + k], m_packedData[j + k]);
        }
    }

    // 1:1 interpolatePoleInPlace（:198-205）。
    void interpolatePoleInPlace(size_t poleIndexA, double fraction, size_t poleIndexB)
    {
        size_t i0 = poleIndexA * m_blockSize;
        size_t i1 = poleIndexB * m_blockSize;
        auto& data = m_packedData;
        for (size_t i = 0; i < m_blockSize; ++i, ++i0, ++i1)
            data[i0] += fraction * (data[i1] - data[i0]);
    }

    // 1:1 saturateInPlace（:214-251——重结插入使 span 达 Bezier 饱和形；
    // 参考 setInterval(knotA, knotB) 存结值——DanQing Segment1d 同参）。
    bool saturateInPlace(KnotVector const& knotVector, int spanIndex)
    {
        int const degree = knotVector.degree;
        int const kA = spanIndex + degree - 1;  // left knot index of the active span
        int const kB = kA + 1;
        if (spanIndex < 0 || spanIndex >= knotVector.numSpans())
            return false;
        auto const& knotArray = knotVector.knots;
        double const knotA = knotArray[static_cast<size_t>(kA)];
        double const knotB = knotArray[static_cast<size_t>(kB)];
        setInterval(knotA, knotB);
        if (knotB <= knotA + KnotVector::knotTolerance)
            return false;
        for (int numInsert = degree - 1; numInsert > 0; --numInsert) {
            //  left numInsert poles are pulled forward
            int k0 = kA - numInsert;
            if (knotArray[static_cast<size_t>(k0)] < knotA) {
                int k1 = kB;
                for (int i = 0; i < numInsert; ++i, ++k0, ++k1) {
                    double const knot0 = knotArray[static_cast<size_t>(k0)];
                    double const knot1 = knotArray[static_cast<size_t>(k1)];
                    double const fraction = (knotA - knot0) / (knot1 - knot0);
                    interpolatePoleInPlace(static_cast<size_t>(i), fraction, static_cast<size_t>(i) + 1);
                }
            }
        }
        for (int numInsert = degree - 1; numInsert > 0; --numInsert) {
            int k2 = kB + numInsert;
            if (knotArray[static_cast<size_t>(k2)] > knotB) {
                for (int i = 0; i < numInsert; ++i, --k2) {
                    double const knot2 = knotArray[static_cast<size_t>(k2)];  // right side of moving window
                    // left side of window is always the (previously saturated) knotA
                    double const fraction = (knotB - knot2) / (knotA - knot2);
                    interpolatePoleInPlace(static_cast<size_t>(degree - i), fraction,
                                           static_cast<size_t>(degree - i - 1));
                }
            }
        }
        return true;
    }

    // 1:1 setInterval（:359-361——DanQing Segment1d 承载）。
    void setInterval(double a, double b) { m_interval = Segment1d::Create(a, b); }
    // 1:1 fractionToParentFraction（:363——interval 缺席原样返回）。
    double fractionToParentFraction(double fraction) const
    {
        return m_interval.has_value() ? m_interval->FractionToPoint(fraction) : fraction;
    }

private:
    std::vector<double> m_packedData;  // 1:1 _packedData
    size_t m_order = 0;                // 1:1 _order
    size_t m_blockSize = 0;            // 1:1 _blockSize
    UnivariateBezier m_basis;          // 1:1 _basis
    std::optional<Segment1d> m_interval;  // 1:1 interval（:357 可选字段）
};

END_DQ_GEOM_NAMESPACE
