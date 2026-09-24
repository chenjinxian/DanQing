// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/RotationalSweep.ts
// DanQing dqGeom — RotationalSweep (contour rotated around an axis)
//
// 保真依据：移植类结构。参考持 SweepContour + normalizedAxis: Ray3d；DanQing 暂存 CurveCollection
// + axis(origin/direction) + sweepAngle 直接（SweepContour phase）。GetConstructiveFrame 与
// tessellation 为 Phase-N。addSolidPrimitive RotationalSweep case 现 no-op TODO。
#pragma once

#include "Angle.h"
#include "CurveCollection.h"
#include "GeometryHandler.h"
#include "GeometryQuery.h"
#include "Point3d.h"
#include "Range3d.h"
#include "SolidPrimitive.h"
#include "Transform.h"
#include "Vector3d.h"

#include <optional>

BEGIN_DQ_GEOM_NAMESPACE

// RotationalSweep — contour rotated about an axis through sweepAngle (1:1 RotationalSweep.ts).
class DQ_GEOM_EXPORT RotationalSweep : public SolidPrimitive {
public:
    ~RotationalSweep() override = default;

    static dqBase::RefPtr<RotationalSweep> Create(dqBase::RefPtr<CurveCollection> const& curves,
                                                  Point3d const& axisOrigin, Vector3d const& axisDirection,
                                                  Angle const& sweepAngle, bool capped);

    SolidPrimitiveType GetSolidPrimitiveType() const noexcept override { return SolidPrimitiveType::RotationalSweep; }
    dqBase::RefPtr<CurveCollection> ConstantVSection(double /*vFraction*/) const override { return nullptr; }
    std::optional<Transform> GetConstructiveFrame() const override { return std::nullopt; }
    bool IsClosedVolume() const override { return m_capped || m_sweepAngle.IsFullCircle(); }
    void DispatchToHandler(GeometryHandler& handler) override { handler.HandleRotationalSweep(*this); }

    dqBase::RefPtr<CurveCollection> const& GetCurves() const noexcept { return m_curves; }
    Point3d GetAxisOrigin() const noexcept { return m_axisOrigin; }
    Vector3d GetAxisDirection() const noexcept { return m_axisDirection; }
    Angle GetSweepAngle() const noexcept { return m_sweepAngle; }

    Range3d Range() const override;
    void ExtendRange(Range3d& range) const override;
    bool TryTransformInPlace(Transform const& transform) override;
    dqBase::RefPtr<GeometryQuery> clone() const override;
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const override;
    bool IsSameGeometryClass(GeometryQuery const& other) const noexcept override;
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const override;

private:
    RotationalSweep(dqBase::RefPtr<CurveCollection> const& curves, Point3d const& axisOrigin,
                    Vector3d const& axisDirection, Angle const& sweepAngle, bool capped)
        : SolidPrimitive(capped)
        , m_curves(curves), m_axisOrigin(axisOrigin), m_axisDirection(axisDirection), m_sweepAngle(sweepAngle) {}

    dqBase::RefPtr<CurveCollection> m_curves;
    Point3d m_axisOrigin;
    Vector3d m_axisDirection;
    Angle m_sweepAngle;
};

using RotationalSweepPtr = dqBase::RefPtr<RotationalSweep>;

END_DQ_GEOM_NAMESPACE
