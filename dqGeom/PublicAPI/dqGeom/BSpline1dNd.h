// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — BSpline1dNd（R→Rⁿ B 样条求值引擎）
// Ported from: itwinjs-core core/geometry/src/bspline/BSpline1dNd.ts
//
// 子集（M-W W-b2，AspectRatioSkew/BSplineCurve3d 消费面）：构造/缓冲/求值
// （evaluateBasisFunctionsInSpan + evaluateBuffers(InSpan/InSpan1/AtKnot) +
// sumPoleBuffer 族）+ reverseInPlace。TODO（随消费面扩展）：addKnot（
// clonePartialCurve 链）、testClosablePolygon、getPoint3dPole。
#pragma once

#include "DqGeom.h"
#include "KnotVector.h"

#include <algorithm>
#include <utility>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// BSpline1dNd — 分块极点 + 结向量的 B 样条函数
// Ported from: BSpline1dNd.ts:21-311
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT BSpline1dNd {
public:
    // 1:1 公开字段（:22-27）。
    KnotVector knots;
    std::vector<double> packedData;  // 极点按 poleLength 分块
    size_t poleLength = 0;

    // 求值缓冲（1:1 :55-98——构造期预分配）。
    std::vector<double> basisBuffer;
    std::vector<double> basisBuffer1;
    std::vector<double> basisBuffer2;
    std::vector<double> poleBuffer;
    std::vector<double> poleBuffer1;
    std::vector<double> poleBuffer2;

    BSpline1dNd() = default;

    // 1:1 create（:108-110——knots 值语义拷贝承载参考的 capture）。
    static BSpline1dNd create(size_t numPoles, size_t poleLength, int order, KnotVector const& knots)
    {
        BSpline1dNd out;
        out.knots = knots.clone();
        out.packedData.assign(numPoles * poleLength, 0.0);
        out.poleLength = poleLength;
        out.basisBuffer.assign(static_cast<size_t>(order), 0.0);
        out.basisBuffer1.assign(static_cast<size_t>(order), 0.0);
        out.basisBuffer2.assign(static_cast<size_t>(order), 0.0);
        out.poleBuffer.assign(poleLength, 0.0);
        out.poleBuffer1.assign(poleLength, 0.0);
        out.poleBuffer2.assign(poleLength, 0.0);
        return out;
    }

    // 1:1 degree/order/numSpan/numPoles（:29-43）。
    int degree() const noexcept { return knots.degree; }
    int order() const noexcept { return knots.degree + 1; }
    int numSpan() const noexcept { return static_cast<int>(numPoles()) - knots.degree; }
    size_t numPoles() const noexcept { return poleLength != 0 ? packedData.size() / poleLength : 0; }

    // 1:1 spanFractionToKnot（:112-114）。
    double spanFractionToKnot(int spanIndex, double spanFraction) const
    {
        return knots.spanFractionToKnot(spanIndex, spanFraction);
    }

    // 1:1 evaluateBasisFunctionsInSpan（:120-132——span 钳位 + 全局结换算）。
    bool evaluateBasisFunctionsInSpan(int spanIndex, double spanFraction, std::vector<double>& f,
                                      std::vector<double>* df = nullptr,
                                      std::vector<double>* ddf = nullptr)
    {
        if (spanIndex < 0)
            spanIndex = 0;
        if (spanIndex >= numSpan())
            spanIndex = numSpan() - 1;
        int const knotIndex0 = spanIndex + degree() - 1;
        double const globalKnot = knots.baseKnotFractionToKnot(knotIndex0, spanFraction);
        return df != nullptr
            ? knots.evaluateBasisFunctions1(knotIndex0, globalKnot, f, *df, ddf)
            : knots.evaluateBasisFunctions(knotIndex0, globalKnot, f);
    }

    // 1:1 sumPoleBufferForSpan（:137-143）。
    void sumPoleBufferForSpan(int spanIndex)
    {
        std::fill(poleBuffer.begin(), poleBuffer.end(), 0.0);
        size_t k = static_cast<size_t>(spanIndex) * poleLength;
        for (double const f : basisBuffer) {
            for (size_t j = 0; j < poleLength; ++j)
                poleBuffer[j] += f * packedData[k++];
        }
    }

    // 1:1 sumPoleBuffer1ForSpan（:148-154）。
    void sumPoleBuffer1ForSpan(int spanIndex)
    {
        std::fill(poleBuffer1.begin(), poleBuffer1.end(), 0.0);
        size_t k = static_cast<size_t>(spanIndex) * poleLength;
        for (double const f : basisBuffer1) {
            for (size_t j = 0; j < poleLength; ++j)
                poleBuffer1[j] += f * packedData[k++];
        }
    }

    // 1:1 sumPoleBuffer2ForSpan（:159-166）。
    void sumPoleBuffer2ForSpan(int spanIndex)
    {
        std::fill(poleBuffer2.begin(), poleBuffer2.end(), 0.0);
        size_t k = static_cast<size_t>(spanIndex) * poleLength;
        for (double const f : basisBuffer2) {
            for (size_t j = 0; j < poleLength; ++j)
                poleBuffer2[j] += f * packedData[k++];
        }
    }

    // 1:1 evaluateBuffersInSpan（:173-176）。
    void evaluateBuffersInSpan(int spanIndex, double spanFraction)
    {
        evaluateBasisFunctionsInSpan(spanIndex, spanFraction, basisBuffer);
        sumPoleBufferForSpan(spanIndex);
    }

    // 1:1 evaluateBuffersInSpan1（:183-187）。
    void evaluateBuffersInSpan1(int spanIndex, double spanFraction)
    {
        evaluateBasisFunctionsInSpan(spanIndex, spanFraction, basisBuffer, &basisBuffer1);
        sumPoleBufferForSpan(spanIndex);
        sumPoleBuffer1ForSpan(spanIndex);
    }

    // 1:1 evaluateBuffersAtKnot（:192-207——numDerivative 三档）。
    void evaluateBuffersAtKnot(double u, int numDerivative = 0)
    {
        int const knotIndex0 = knots.knotToLeftKnotIndex(u);
        if (numDerivative < 1) {
            knots.evaluateBasisFunctions(knotIndex0, u, basisBuffer);
            sumPoleBufferForSpan(knotIndex0 - degree() + 1);
        } else if (numDerivative == 1) {
            knots.evaluateBasisFunctions1(knotIndex0, u, basisBuffer, basisBuffer1);
            sumPoleBufferForSpan(knotIndex0 - degree() + 1);
            sumPoleBuffer1ForSpan(knotIndex0 - degree() + 1);
        } else {
            knots.evaluateBasisFunctions1(knotIndex0, u, basisBuffer, basisBuffer1, &basisBuffer2);
            sumPoleBufferForSpan(knotIndex0 - degree() + 1);
            sumPoleBuffer1ForSpan(knotIndex0 - degree() + 1);
            sumPoleBuffer2ForSpan(knotIndex0 - degree() + 1);
        }
    }

    // 1:1 reverseInPlace（:209-217——极点块反转 + 结镜像）。
    void reverseInPlace()
    {
        size_t const pLen = poleLength;
        auto& data = packedData;
        for (size_t i0 = 0, j0 = pLen * (numPoles() - 1); i0 < j0; i0 += pLen, j0 -= pLen) {
            for (size_t i = 0; i < pLen; ++i)
                std::swap(data[i0 + i], data[j0 + i]);
        }
        knots.reflectKnots();
    }
};

END_DQ_GEOM_NAMESPACE
