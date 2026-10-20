// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — BSplineCurve3d
// Ported from: itwinjs-core core/geometry/src/bspline/BSplineCurve.ts
//
// 子集（M-W W-b2，AspectRatioSkew 消费面——直构三极点 2 次 clamped bcurve +
// evaluate/fractionToPoint + EmitStrokes 容差细分 + range + clone）：
// create（经典过钳位/现代两形态识别）/createUniformKnots、degree/order/numSpan/
// numPoles、copyKnots/copyPoints(Float64Array)、getPolePoint3d、
// evaluatePointInSpan/evaluatePointAndDerivativeInSpan、spanFractionToKnot、
// knotToPoint/knotToPointAndDerivative/knotToPointAnd2Derivatives、
// fractionToPoint/fractionToPointAndDerivative、startPoint/endPoint、
// tryTransformInPlace/clone/isAlmostEqual、extendRange（极点面）、
// quickLength、emitStrokes（span→saturated Bezier 链）、
// computeStrokeCountForOptions、getSaturatedBezierSpan3d、
// initializeWorkBezier。
// TODO（随消费面扩展）：BSplineCurve3dBase 抽象基座与 BSplineCurve3dH（加权）
// 两态分裂、createPeriodicUniformKnots、createFromInterpolationCurve3dOptions/
// Akima 链、collectBezierSpans、closestPoint、clonePartialCurve（addKnot 链）、
// appendPlaneIntersectionPoints、constructOffsetXY、projectedParameterRange、
// emitStrokableParts（handler 面）、computeAndAttachRecursiveStrokeCounts、
// definitionData、isInPlane。
#pragma once

#include "DqGeom.h"
#include "BSpline1dNd.h"
#include "BezierCurve3d.h"
#include "CurvePrimitive.h"
#include "KnotVector.h"
#include "LineString3d.h"
#include "Plane3dByOriginAndVectors.h"
#include "PointHelpers.h"
#include "Ray3d.h"

#include <optional>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// BSplineCurve3d — 极点为 Point3d 的 B 样条曲线
// Ported from: BSplineCurve.ts:430-871 BSplineCurve3d（+ :98-424 基座面——
// 子集直接并入本类，3dH 分裂未移植）。
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT BSplineCurve3d : public CurvePrimitive {
public:
    // 1:1 构造经工厂；ctor 私有（:437-439）。
    ~BSplineCurve3d() override = default;

    // --- 1:1 度/阶/span/极数（BSplineCurve3dBase :114-132）---
    int degree() const noexcept { return m_bcurve.degree(); }
    int order() const noexcept { return m_bcurve.order(); }
    int numSpan() const noexcept { return m_bcurve.numSpan(); }
    int numPoles() const noexcept { return static_cast<int>(m_bcurve.numPoles()); }

    // 1:1 polesRef/knotsRef（:133-140——活引用语义的 DanQing const 读面）。
    std::vector<double> const& polesRef() const noexcept { return m_bcurve.packedData; }
    std::vector<double> const& knotsRef() const noexcept { return m_bcurve.knots.knots; }

    // 1:1 poleDimension（:146-148——3 = 常规 xyz）。
    int poleDimension() const noexcept { return static_cast<int>(m_bcurve.poleLength); }

    // 1:1 copyKnots（:483-485）。
    std::vector<double> copyKnots(bool includeExtraEndKnot) const
    {
        return m_bcurve.knots.copyKnots(includeExtraEndKnot);
    }
    // 1:1 copyPointsFloat64Array（:479-481）。
    std::vector<double> copyPointsFloat64Array() const { return m_bcurve.packedData; }
    // 1:1 copyPoints（:475-477）。
    std::vector<std::vector<double>> copyPoints() const
    {
        return Point3dArray::unpackNumbersToNestedArrays(m_bcurve.packedData, 3);
    }

    // 1:1 getPolePoint3d（:450-457——越界 false，参考 undefined）。
    bool getPolePoint3d(int poleIndex, Point3d& result) const
    {
        auto const k = poleIndexToDataIndex(poleIndex);
        if (!k.has_value())
            return false;
        auto const& data = m_bcurve.packedData;
        result = Point3d::From(data[*k], data[*k + 1], data[*k + 2]);
        return true;
    }

    // 1:1 poleIndexToDataIndex（:262-266）。
    std::optional<size_t> poleIndexToDataIndex(int poleIndex) const noexcept
    {
        if (poleIndex >= 0 && poleIndex < numPoles())
            return static_cast<size_t>(poleIndex) * m_bcurve.poleLength;
        return std::nullopt;
    }

    // 1:1 spanFractionToKnot（:471-473）。
    double spanFractionToKnot(int spanIndex, double spanFraction) const
    {
        return m_bcurve.spanFractionToKnot(spanIndex, spanFraction);
    }

    // --- 求值（:644-700）---
    // 1:1 evaluatePointInSpan（:644-647）。
    Point3d evaluatePointInSpan(int spanIndex, double spanFraction) const
    {
        auto& bc = const_cast<BSpline1dNd&>(m_bcurve);  // 缓冲求值的内部可变面
        bc.evaluateBuffersInSpan(spanIndex, spanFraction);
        return Point3d::From(bc.poleBuffer[0], bc.poleBuffer[1], bc.poleBuffer[2]);
    }
    // 1:1 evaluatePointAndDerivativeInSpan（:652-658）。
    Ray3d evaluatePointAndDerivativeInSpan(int spanIndex, double spanFraction) const
    {
        auto& bc = const_cast<BSpline1dNd&>(m_bcurve);
        bc.evaluateBuffersInSpan1(spanIndex, spanFraction);
        return Ray3d::create(
            Point3d::From(bc.poleBuffer[0], bc.poleBuffer[1], bc.poleBuffer[2]),
            Vector3d::From(bc.poleBuffer1[0], bc.poleBuffer1[1], bc.poleBuffer1[2]));
    }
    // 1:1 knotToPoint（:665-668）。
    Point3d knotToPoint(double u) const
    {
        auto& bc = const_cast<BSpline1dNd&>(m_bcurve);
        bc.evaluateBuffersAtKnot(u);
        return Point3d::From(bc.poleBuffer[0], bc.poleBuffer[1], bc.poleBuffer[2]);
    }
    // 1:1 knotToPointAndDerivative（:675-685）。
    Ray3d knotToPointAndDerivative(double u) const
    {
        auto& bc = const_cast<BSpline1dNd&>(m_bcurve);
        bc.evaluateBuffersAtKnot(u, 1);
        return Ray3d::create(
            Point3d::From(bc.poleBuffer[0], bc.poleBuffer[1], bc.poleBuffer[2]),
            Vector3d::From(bc.poleBuffer1[0], bc.poleBuffer1[1], bc.poleBuffer1[2]));
    }
    // 1:1 knotToPointAnd2Derivatives（:692-700）。
    Plane3dByOriginAndVectors knotToPointAnd2Derivatives(double u) const
    {
        auto& bc = const_cast<BSpline1dNd&>(m_bcurve);
        bc.evaluateBuffersAtKnot(u, 2);
        return Plane3dByOriginAndVectors::createOriginAndVectorsXYZ(
            bc.poleBuffer[0], bc.poleBuffer[1], bc.poleBuffer[2],
            bc.poleBuffer1[0], bc.poleBuffer1[1], bc.poleBuffer1[2],
            bc.poleBuffer2[0], bc.poleBuffer2[1], bc.poleBuffer2[2]);
    }

    // --- 1:1 fraction 求值（:189-207——fraction→knot 换算 + knotLength01 导数缩放）---
    Point3d FractionToPoint(double fraction) const override
    {
        return knotToPoint(m_bcurve.knots.fractionToKnot(fraction));
    }
    Point3d FractionToPointAndDerivative(double fraction, Vector3d& derivative) const override
    {
        double const knot = m_bcurve.knots.fractionToKnot(fraction);
        Ray3d const result = knotToPointAndDerivative(knot);
        derivative = result.direction;
        derivative.Scale(m_bcurve.knots.knotLength01());
        return result.origin;
    }
    // 1:1 fractionToPointAnd2Derivatives（:200-207——vectorU ×a、vectorV ×a²）。
    Plane3dByOriginAndVectors fractionToPointAnd2Derivatives(double fraction) const
    {
        double const knot = m_bcurve.knots.fractionToKnot(fraction);
        auto result = knotToPointAnd2Derivatives(knot);
        double const a = m_bcurve.knots.knotLength01();
        result.vectorU.Scale(a);
        result.vectorV.Scale(a * a);
        return result;
    }

    // 1:1 startPoint/endPoint（:209-215）。
    Point3d StartPoint() const override { return evaluatePointInSpan(0, 0.0); }
    Point3d EndPoint() const override { return evaluatePointInSpan(numSpan() - 1, 1.0); }

    // --- CurvePrimitive/GeometryQuery 合同 ---
    CurveType GetCurveType() const noexcept override { return CurveType::BSplineCurve; }

    // 1:1 tryTransformInPlace（:445-448——逐极变换）。
    bool TryTransformInPlace(Transform const& transform) override
    {
        Point3dArray::multiplyInPlace(transform, m_bcurve.packedData);
        return true;
    }

    // 1:1 clone（:637-642——深拷）。
    dqBase::RefPtr<GeometryQuery> clone() const override
    {
        return dqBase::RefPtr<GeometryQuery>(cloneCurve());
    }

    // clone 的本类型面（内部用——cloneTransformed 链）。
    dqBase::RefPtr<BSplineCurve3d> cloneCurve() const
    {
        auto curve = dqBase::RefPtr<BSplineCurve3d>(
            new BSplineCurve3d(static_cast<size_t>(numPoles()), order(), m_bcurve.knots.clone()));
        curve->m_bcurve.packedData = m_bcurve.packedData;
        return curve;
    }

    // 1:1 cloneTransformed（:303-307）。
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const override
    {
        auto curve = cloneCurve();
        curve->TryTransformInPlace(transform);
        return curve;
    }

    // 1:1 isSameGeometryClass（:441-443——DanQing 无 RTTI：CurveType 判别面）。
    bool IsSameGeometryClass(GeometryQuery const& other) const noexcept override
    {
        if (other.Category() != GeometryCategory::CurvePrimitive)
            return false;
        return static_cast<CurvePrimitive const&>(other).GetCurveType()
            == CurveType::BSplineCurve;
    }

    // 1:1 isAlmostEqual（:702-708——结 + 极点双通道）。
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const override;

    // 1:1 extendRange（:860-870——极点面[非曲线本身——保守超集语义原文]）。
    void ExtendRange(Range3d& rangeToExtend) const override
    {
        auto const& buffer = m_bcurve.packedData;
        size_t const n = buffer.size() >= 2 ? buffer.size() - 2 : 0;
        for (size_t i0 = 0; i0 < n; i0 += 3)
            rangeToExtend.ExtendPoint(Point3d::From(buffer[i0], buffer[i0 + 1], buffer[i0 + 2]));
    }

    // 1:1 quickLength（:717-719——控制多边形边长和）。
    double QuickLength() const override
    {
        return Point3dArray::sumEdgeLengths(m_bcurve.packedData);
    }

    // 1:1 emitStrokes（:786-794——span 饱和 Bezier 链）。
    void EmitStrokes(LineString3d& dest, StrokeOptions const& options = {}) const override;

    // 1:1 computeStrokeCountForOptions（:754-764——逐 span 饱和 Bezier 计数累加）。
    int computeStrokeCountForOptions(StrokeOptions const* options) const;

    // 1:1 getSaturatedBezierSpan3d（:820-831——装载 span 极点 + saturate）。
    // 参考 result 复用槽→DanQing 以 workBezier 自持（initializeWorkBezier
    // :432-435 惰性面）。
    dqBase::RefPtr<BezierCurve3d> getSaturatedBezierSpan3d(int spanIndex) const;

    // 1:1 dispatchToGeometryHandler（:850-852）。
    void DispatchToHandler(GeometryHandler& handler) override;

    // --- 工厂 ---
    // 1:1 createUniformKnots（:487-509——均匀钳位结）。
    static dqBase::RefPtr<BSplineCurve3d> createUniformKnots(std::vector<Point3d> const& poles,
                                                             int order);
    // 1:1 create（:601-635——经典过钳位[numPoles+order==numKnots]与现代
    // [numPoles+order==numKnots+2]两形态识别；Point3d 数组 + 打包 doubles 两形态）。
    static dqBase::RefPtr<BSplineCurve3d> create(std::vector<Point3d> const& poleArray,
                                                 std::vector<double> const& knotArray, int order);
    static dqBase::RefPtr<BSplineCurve3d> create(std::vector<double> const& poleArray,
                                                 std::vector<double> const& knotArray, int order);

private:
    // 1:1 私有 ctor（:437-439）+ 基座 :110-113。
    BSplineCurve3d(size_t numPoles, int order, KnotVector const& knots)
        : m_bcurve(BSpline1dNd::create(numPoles, 3, order, knots))
    {
    }

    // 两计数形态识别核（create :612-616——共享于 Point3d/打包 doubles 两工厂）。
    static dqBase::RefPtr<BSplineCurve3d> createImpl(size_t numPoles,
                                                     std::vector<double> const& knotArray,
                                                     int order);

    BSpline1dNd m_bcurve;  // 1:1 _bcurve（:102）
};

END_DQ_GEOM_NAMESPACE
