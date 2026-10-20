// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — BezierCurveBase abstract base
// Ported from: itwinjs-core core/geometry/src/bspline/BezierCurveBase.ts
//
// 子集（M-W W-b2，AspectRatioSkew 消费面）：_polygon 承载、emitStrokes、
// computeStrokeCountForOptions（弦差/转角/边长三容差——参考 :205-252 全量）、
// polygonLength/quickLength、startPoint/endPoint、saturateInPlace/setInterval
// 转发、reverseInPlace、cloneTransformed。
// TODO（随消费面扩展）：copyPolesAsJsonArray、isInPlane、constructOffsetXY、
// clonePartialCurve、projectedParameterRange、emitStrokableParts（handler 面）、
// extendRange 的内部极值精确形（BezierCurve3d.ts:163-212 的 UnivariateBezier
// roots 链——BezierPolynomials 的 roots 未移植；当前 ExtendRange 走
// CurvePrimitive 默认[首末点]——消费面内 BezierCurve3d 仅作 span 描线器）。
#pragma once

#include "DqGeom.h"
#include "Bezier1dNd.h"
#include "CurvePrimitive.h"
#include "LineString3d.h"
#include "StrokeOptions.h"

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// BezierCurveBase — CurvePrimitive + Bezier1dNd polygon 承载
// Ported from: BezierCurveBase.ts:38-295
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT BezierCurveBase : public CurvePrimitive {
public:
    ~BezierCurveBase() override = default;

    // 1:1 degree/order/numPoles（:75-81）。
    int degree() const noexcept { return static_cast<int>(m_polygon.order()) - 1; }
    int order() const noexcept { return static_cast<int>(m_polygon.order()); }
    int numPoles() const noexcept { return static_cast<int>(m_polygon.order()); }

    // 1:1 getPolePoint3d 抽象（:86——3d 恒成、3dH 零权失败[3dH 未移植]）。
    virtual bool getPolePoint3d(size_t i, Point3d& point) const = 0;

    // 1:1 setInterval/fractionToParentFraction 转发（:94-98）。
    void setInterval(double a, double b) { m_polygon.setInterval(a, b); }
    double fractionToParentFraction(double fraction) const
    {
        return m_polygon.fractionToParentFraction(fraction);
    }

    // 1:1 saturateInPlace 转发（:65-73——区间以 span 分位形）。
    bool saturateInPlace(KnotVector const& knotVector, int spanIndex)
    {
        bool const boolStat = m_polygon.saturateInPlace(knotVector, spanIndex);
        if (boolStat) {
            setInterval(knotVector.spanFractionToFraction(spanIndex, 0.0),
                        knotVector.spanFractionToFraction(spanIndex, 1.0));
        }
        return boolStat;
    }

    // 1:1 reverseInPlace（:63）。
    void reverseInPlace() { m_polygon.reverseInPlace(); }

    // 1:1 emitStrokes（:101-109——computeStrokeCountForOptions 驱动均步）。
    void EmitStrokes(LineString3d& dest, StrokeOptions const& options = {}) const override
    {
        int const numPerSpan = computeStrokeCountForOptions(&options);
        double const fractionStep = 1.0 / numPerSpan;
        for (int i = 0; i <= numPerSpan; ++i) {
            double const fraction = i * fractionStep;
            // EQUIVALENCE（§11.10）：参考 dest.appendStrokePoint——DanQing
            // LineString3d 的 AddPoint 同名面（CurvePrimitive::EmitStrokes 默认
            // 实现同面先例）。发散=未发现；验证法=StrokeParabola 锁。
            dest.AddPoint(FractionToPoint(fraction));
        }
    }

    // 1:1 computeStrokeCountForOptions（:205-252——首末边差分/转角累计/三容差
    // 合成；`degree<3 → radians1 ×3` 的二次曲线欠描防护原样）。
    // EQUIVALENCE（§11.10）：参考以 _workPoint0/1 成员作 scratch；DanQing 局部
    // 变量承载（函数可 const——CurvePrimitive::EmitStrokes 的 const 合同）。
    // 发散=未发现（行为面=读极点算计数，scratch 居所不可观测）；验证法=
    // StrokeParabola 锁。
    int computeStrokeCountForOptions(StrokeOptions const* options) const
    {
        int numStrokes = 1;
        Point3d workPoint0, workPoint1;
        bool const hasP0 = getPolePoint3d(0, workPoint0);
        bool const hasP1 = getPolePoint3d(1, workPoint1);
        if (hasP0 && hasP1) {
            double dx0 = workPoint1.x - workPoint0.x;
            double dy0 = workPoint1.y - workPoint0.y;
            double dz0 = workPoint1.z - workPoint0.z;
            double sumRadians = 0.0;
            double thisLength = hypotenuseXYZ(dx0, dy0, dz0);
            workPoint1 = workPoint0;
            double sumLength = thisLength;
            double maxLength = thisLength;
            double maxRadians = 0.0;
            for (size_t i = 2; getPolePoint3d(i, workPoint1); ++i) {
                double const dx1 = workPoint1.x - workPoint0.x;
                double const dy1 = workPoint1.y - workPoint0.y;
                double const dz1 = workPoint1.z - workPoint0.z;
                double const thisRadians =
                    Angle::RadiansBetweenVectorsXYZ(dx0, dy0, dz0, dx1, dy1, dz1);
                sumRadians += thisRadians;
                maxRadians = maxAbsXY(thisRadians, maxRadians);
                thisLength = hypotenuseXYZ(dx1, dy1, dz1);
                sumLength += thisLength;
                maxLength = maxXY(maxLength, thisLength);
                dx0 = dx1;
                dy0 = dy1;
                dz0 = dz1;
                workPoint0 = workPoint1;
            }
            double const length1 = maxLength * degree();      // This may be larger than sumLength
            double const length2 = std::sqrt(length1 * sumLength);  // This is in between
            double radians1 = maxRadians * (degree() - 1);   // As if worst case keeps happening.
            if (degree() < 3)
                radians1 *= 3;  // so quadratics aren't under-stroked
            double const radians2 = std::sqrt(radians1 * sumRadians);
            int const minCount = degree();  // NOTE: 1) a small, nontrivial, straight Bezier is
                                            // over-stroked, and 2) options.minStrokesPerPrimitive
                                            // is ignored
            numStrokes = StrokeOptions::ApplyAngleTol(
                options, StrokeOptions::ApplyMaxEdgeLength(options, minCount, length2),
                radians2, 0.1);
            if (options != nullptr) {
                numStrokes = options->ApplyChordTolToLengthAndRadians(numStrokes, sumLength, radians1);
            }
        }
        return numStrokes;
    }

    // 1:1 polygonLength（:131-141）。
    double polygonLength() const
    {
        Point3d workPoint0, workPoint1;
        if (!getPolePoint3d(0, workPoint0))
            return 0.0;
        size_t i = 0;
        double sum = 0.0;
        while (getPolePoint3d(++i, workPoint1)) {
            sum += workPoint0.Distance(workPoint1);
            workPoint0 = workPoint1;
        }
        return sum;
    }

    // 1:1 startPoint/endPoint（:146-155——极缺失回落零点的参考语义；DanQing
    // 直接返回 Point3d 值，缺失分支在 getter 返回 false 时回落）。
    Point3d StartPoint() const override
    {
        Point3d p;
        return getPolePoint3d(0, p) ? p : Point3d::FromZero();
    }
    Point3d EndPoint() const override
    {
        Point3d p;
        return getPolePoint3d(static_cast<size_t>(order()) - 1, p) ? p : Point3d::FromZero();
    }

    // 1:1 quickLength（:157）。
    double QuickLength() const override { return polygonLength(); }

    // 1:1 cloneTransformed（:258-262）。
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const override
    {
        auto curve1 = clone();
        curve1->TryTransformInPlace(transform);
        return curve1;
    }

protected:
    // 1:1 ctor（:53-61——blockSize + data 承载）。
    BezierCurveBase(size_t blockSize, std::vector<double> data)
        : m_polygon(blockSize, std::move(data))
    {
    }

    Bezier1dNd m_polygon;  // 1:1 _polygon（:43）
};

END_DQ_GEOM_NAMESPACE
