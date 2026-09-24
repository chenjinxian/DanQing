// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/LinearSweep.ts
// DanQing dqGeom — LinearSweep (contour swept along a vector)
//
// 保真依据：移植 LinearSweep 类结构。参考持 SweepContour（curves + localToWorld frame + cached
// facets）；DanQing 暂存 CurveCollection 直接（SweepContour + FrameBuilder + StrokeCountSection 待
// port），故 GetConstructiveFrame（CloneRigid over contour frame）与 tessellation 为 Phase-N。
// addSolidPrimitive LinearSweep case 现 no-op TODO（sweep tessellation 需 SweepContour.emitFacets
// + addBetweenStrokeSetsWithRuledNormals + region parity）。
#pragma once

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

// LinearSweep — contour swept the full length of a vector (1:1 LinearSweep.ts).
class DQ_GEOM_EXPORT LinearSweep : public SolidPrimitive {
public:
    ~LinearSweep() override = default;

    static dqBase::RefPtr<LinearSweep> Create(dqBase::RefPtr<CurveCollection> const& curves,
                                              Vector3d const& direction, bool capped);

    SolidPrimitiveType GetSolidPrimitiveType() const noexcept override { return SolidPrimitiveType::LinearSweep; }
    // TODO commit-N: needs SweepContour.localToWorld frame.
    dqBase::RefPtr<CurveCollection> ConstantVSection(double /*vFraction*/) const override { return nullptr; }
    std::optional<Transform> GetConstructiveFrame() const override { return std::nullopt; }
    bool IsClosedVolume() const override; // capped && curves is a region (Loop)
    void DispatchToHandler(GeometryHandler& handler) override { handler.HandleLinearSweep(*this); }

    dqBase::RefPtr<CurveCollection> const& GetCurves() const noexcept { return m_curves; }
    Vector3d CloneSweepVector() const noexcept { return m_direction; }

    Range3d Range() const override;
    void ExtendRange(Range3d& range) const override;
    bool TryTransformInPlace(Transform const& transform) override;
    dqBase::RefPtr<GeometryQuery> clone() const override;
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const override;
    bool IsSameGeometryClass(GeometryQuery const& other) const noexcept override;
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const override;

private:
    LinearSweep(dqBase::RefPtr<CurveCollection> const& curves, Vector3d const& direction, bool capped)
        : SolidPrimitive(capped), m_curves(curves), m_direction(direction) {}

    dqBase::RefPtr<CurveCollection> m_curves;
    Vector3d m_direction;
};

using LinearSweepPtr = dqBase::RefPtr<LinearSweep>;

END_DQ_GEOM_NAMESPACE
