// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/RuledSweep.ts
// DanQing dqGeom — RuledSweep (loft between a sequence of contours)
//
// 保真依据：移植类结构。参考持 SweepContour[]；DanQing 暂存 vector<CurveCollection> 直接
// （SweepContour phase）。GetConstructiveFrame 与 tessellation 为 Phase-N。addSolidPrimitive
// RuledSweep case 现 no-op TODO。
#pragma once

#include "CurveCollection.h"
#include "GeometryHandler.h"
#include "GeometryQuery.h"
#include "Range3d.h"
#include "SolidPrimitive.h"
#include "Transform.h"

#include <optional>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// RuledSweep — ruled surface between a sequence of contours (1:1 RuledSweep.ts).
class DQ_GEOM_EXPORT RuledSweep : public SolidPrimitive {
public:
    ~RuledSweep() override = default;

    static dqBase::RefPtr<RuledSweep> Create(std::vector<dqBase::RefPtr<CurveCollection>> const& contours, bool capped);

    SolidPrimitiveType GetSolidPrimitiveType() const noexcept override { return SolidPrimitiveType::RuledSweep; }
    dqBase::RefPtr<CurveCollection> ConstantVSection(double /*vFraction*/) const override { return nullptr; }
    std::optional<Transform> GetConstructiveFrame() const override { return std::nullopt; }
    bool IsClosedVolume() const override { return m_curves.size() > 1 && m_capped; } // phase contour[0]==[n-1] compare
    void DispatchToHandler(GeometryHandler& handler) override { handler.HandleRuledSweep(*this); }

    std::vector<dqBase::RefPtr<CurveCollection>> const& GetCurves() const noexcept { return m_curves; }

    Range3d Range() const override;
    void ExtendRange(Range3d& range) const override;
    bool TryTransformInPlace(Transform const& transform) override;
    dqBase::RefPtr<GeometryQuery> clone() const override;
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const override;
    bool IsSameGeometryClass(GeometryQuery const& other) const noexcept override;
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const override;

private:
    explicit RuledSweep(std::vector<dqBase::RefPtr<CurveCollection>> const& contours, bool capped)
        : SolidPrimitive(capped), m_curves(contours) {}

    std::vector<dqBase::RefPtr<CurveCollection>> m_curves;
};

using RuledSweepPtr = dqBase::RefPtr<RuledSweep>;

END_DQ_GEOM_NAMESPACE
