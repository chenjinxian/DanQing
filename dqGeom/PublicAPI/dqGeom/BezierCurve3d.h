// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — BezierCurve3d
// Ported from: itwinjs-core core/geometry/src/bspline/BezierCurve3d.ts
//
// 子集（M-W W-b2，AspectRatioSkew/BSplineCurve3d span 描线消费面）：create
// （Point3d 数组）/createOrder/loadSpanPoles/getPolePoint3d/fractionToPoint/
// fractionToPointAndDerivative/tryTransformInPlace/clone/isAlmostEqual/
// dispatchToGeometryHandler。TODO（随消费面扩展）：Point2d 形态 create、
// fractionToPointAnd2Derivatives 的 epsilon 差分形（BezierCurve3d.ts:135-147）、
// copyPointsAsLineString、extendRange 精确极值形（见 BezierCurveBase.h 注记）。
#pragma once

#include "DqGeom.h"
#include "BezierCurveBase.h"
#include "Ray3d.h"

#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// BezierCurve3d — 3d Bezier 曲线（极点 xyz 三分块）
// Ported from: BezierCurve3d.ts:29-213
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT BezierCurve3d : public BezierCurveBase {
public:
    // 1:1 create（:82-104——Point3d 数组形态；Point2d 臂未移植[消费面缺席]）。
    static dqBase::RefPtr<BezierCurve3d> create(std::vector<Point3d> const& data)
    {
        if (data.empty())
            return nullptr;
        std::vector<double> polygon;
        polygon.reserve(data.size() * 3);
        for (auto const& p : data) {
            polygon.push_back(p.x);
            polygon.push_back(p.y);
            polygon.push_back(p.z);
        }
        return dqBase::RefPtr<BezierCurve3d>(new BezierCurve3d(std::move(polygon)));
    }

    // 1:1 createOrder（:106-109——全零初始化）。
    static dqBase::RefPtr<BezierCurve3d> createOrder(size_t order)
    {
        return dqBase::RefPtr<BezierCurve3d>(
            new BezierCurve3d(std::vector<double>(order * 3, 0.0)));
    }

    // 1:1 loadSpanPoles（:111-113）。
    void loadSpanPoles(std::vector<double> const& data, size_t spanIndex)
    {
        m_polygon.loadSpanPoles(data, spanIndex);
    }

    // 1:1 getPolePoint3d（:45-50）。
    bool getPolePoint3d(size_t i, Point3d& point) const override
    {
        std::vector<double> workData;
        if (!m_polygon.getPolygonPoint(i, workData))
            return false;
        point = Point3d::From(workData[0], workData[1], workData[2]);
        return true;
    }

    // 1:1 fractionToPoint（:119-122）。
    // EQUIVALENCE（§11.10）：参考以 _workData0/1 成员缓冲复用；DanQing 局部
    // vector 承载（const 合同——CurvePrimitive::FractionToPoint 为 const）。
    // 发散=未发现（行为面=同序求值）；验证法=StrokeParabola/HelloWorld 锁。
    Point3d FractionToPoint(double fraction) const override
    {
        std::vector<double> workData;
        m_polygon.evaluate(fraction, workData);
        return Point3d::From(workData[0], workData[1], workData[2]);
    }

    // 1:1 fractionToPointAndDerivative（:124-128）。
    Point3d FractionToPointAndDerivative(double fraction, Vector3d& derivative) const override
    {
        std::vector<double> workData0;
        std::vector<double> workData1;
        m_polygon.evaluate(fraction, workData0);
        m_polygon.evaluateDerivative(fraction, workData1);
        derivative = Vector3d::From(workData1[0], workData1[1], workData1[2]);
        return Point3d::From(workData0[0], workData0[1], workData0[2]);
    }

    // 1:1 tryTransformInPlace（:33-41——逐极变换）。
    bool TryTransformInPlace(Transform const& transform) override
    {
        std::vector<double> data;
        for (size_t i = 0; i < m_polygon.order(); ++i) {
            if (!m_polygon.getPolygonPoint(i, data))
                continue;
            Point3d const p = transform.MultiplyXYZ(data[0], data[1], data[2]);
            data[0] = p.x;
            data[1] = p.y;
            data[2] = p.z;
            m_polygon.setPolygonPoint(i, data);
        }
        return true;
    }

    // extendRange（BezierCurve3d.ts:163-212 的精确极值形[UnivariateBezier roots]
    // 未移植——BezierPolynomials 子集面）：**极点保守超集**承载（与参考
    // BSplineCurve3d.extendRange[:860-870] 的极点面同形——保守范围上界）。
    // TODO（随消费面扩展）：roots 链归位精确极值。
    void ExtendRange(Range3d& rangeToExtend) const override
    {
        for (size_t i = 0; i < m_polygon.order(); ++i) {
            Point3d p;
            if (getPolePoint3d(i, p))
                rangeToExtend.ExtendPoint(p);
        }
    }

    // 1:1 clone（:115-117）。
    dqBase::RefPtr<GeometryQuery> clone() const override
    {
        return dqBase::RefPtr<BezierCurve3d>(
            new BezierCurve3d(m_polygon.clonePolygon()));
    }

    // 1:1 isAlmostEqual（:149-154——经 _polygon 逐坐标）。
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const override;

    // 1:1 isSameGeometryClass（:31——DanQing 无 RTTI：CurveType 判别面）。
    bool IsSameGeometryClass(GeometryQuery const& other) const noexcept override
    {
        if (other.Category() != GeometryCategory::CurvePrimitive)
            return false;
        return static_cast<CurvePrimitive const&>(other).GetCurveType()
            == CurveType::BezierCurve;
    }

    // 1:1 GetCurveType（CurvePrimitive 合同）。
    CurveType GetCurveType() const noexcept override { return CurveType::BezierCurve; }

    // 1:1 dispatchToGeometryHandler（:156-158）。
    void DispatchToHandler(GeometryHandler& handler) override;

private:
    // 1:1 ctor（:62-66——blockSize 3）。
    explicit BezierCurve3d(std::vector<double> polygon)
        : BezierCurveBase(3, std::move(polygon))
    {
    }
};

END_DQ_GEOM_NAMESPACE
