// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/TorusPipe.ts
// DanQing dqGeom — TorusPipe (minor circle swept along a major circular arc)
//
// 保真依据：逐位移植 TorusPipe.ts 类结构（localToWorld + radiusA/radiusB + sweep + isReversed
// + 工厂 + 访问器 + capped/IsClosedVolume/clone/dispatch）。UV (uvFractionToPointAndTangents) 与
// getConstructiveFrame(CloneRigid)、createInFrame 的 mirror/negative-sweep 分支（需 Matrix3d::
// scaleColumnsInPlace）、createDgnTorusPipe/createAlongArc 均 phase（addSolidPrimitive 不依赖——
// 经 localToWorld.MultiplyXYZ((R+r·cosφ)cosθ, (R+r·cosφ)sinθ, r·sinφ) 直接 tessellate）。
#pragma once

#include "Angle.h"
#include "GeometryHandler.h"
#include "GeometryQuery.h"
#include "Point3d.h"
#include "Range3d.h"
#include "SolidPrimitive.h"
#include "Transform.h"
#include "Vector3d.h"

#include <optional>

BEGIN_DQ_GEOM_NAMESPACE

// TorusPipe — minor circle (xz-plane, radiusB) swept along a major circular arc (xy-plane, radiusA) (1:1 TorusPipe.ts).
class DQ_GEOM_EXPORT TorusPipe : public SolidPrimitive {
public:
    ~TorusPipe() override = default;

    // --- Factories (1:1 TorusPipe ctor / createInFrame) ---
    static dqBase::RefPtr<TorusPipe> Create(Transform const& map, double radiusA, double radiusB,
                                            Angle const& sweep, bool capped);
    static dqBase::RefPtr<TorusPipe> CreateInFrame(Transform const& frame, double majorRadius, double minorRadius,
                                                   Angle const& sweep, bool capped);

    // --- SolidPrimitive ---
    SolidPrimitiveType GetSolidPrimitiveType() const noexcept override { return SolidPrimitiveType::TorusPipe; }
    // TODO commit-N: strokeConstantVSection + Loop::Create.
    dqBase::RefPtr<CurveCollection> ConstantVSection(double /*vFraction*/) const override { return nullptr; }
    // TODO commit-N: CloneRigid.
    std::optional<Transform> GetConstructiveFrame() const override { return m_localToWorld.clone(); }
    bool IsClosedVolume() const override { return m_capped || m_sweep.IsFullCircle(); }
    void DispatchToHandler(GeometryHandler& handler) override { handler.HandleTorusPipe(*this); }

    // --- Accessors (1:1 TorusPipe.getRadiusA/getMajorRadius/getSweepAngle/getThetaFraction/...) ---
    // Local (stored) radii — used for tessellation via localToWorld.MultiplyXYZ.
    double GetRadiusA() const noexcept { return m_radiusA; }
    double GetRadiusB() const noexcept { return m_radiusB; }
    // World-scaled radii (1:1 TorusPipe.getMajorRadius/getMinorRadius).
    double GetMajorRadius() const noexcept { return m_radiusA * m_localToWorld.matrix.ColumnXMagnitude(); }
    double GetMinorRadius() const noexcept { return m_radiusB * m_localToWorld.matrix.ColumnZMagnitude(); }
    Angle GetSweepAngle() const noexcept { return m_sweep; }
    double GetSweepRadians() const noexcept { return m_sweep.Radians(); }
    double GetThetaFraction() const noexcept { return m_sweep.Radians() / Angle::k2Pi; }
    bool GetIsReversed() const noexcept { return m_isReversed; }
    Transform const& GetLocalToWorld() const noexcept { return m_localToWorld; }

    // --- GeometryQuery ---
    Range3d Range() const override;
    void ExtendRange(Range3d& range) const override;
    bool TryTransformInPlace(Transform const& transform) override;
    dqBase::RefPtr<GeometryQuery> clone() const override;
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const override;
    bool IsSameGeometryClass(GeometryQuery const& other) const noexcept override;
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const override;

private:
    TorusPipe(Transform const& map, double radiusA, double radiusB, Angle const& sweep, bool capped)
        : SolidPrimitive(capped)
        , m_localToWorld(map), m_radiusA(radiusA), m_radiusB(radiusB), m_sweep(sweep), m_isReversed(false) {}

    Transform m_localToWorld;
    double m_radiusA;   // major hoop radius (local)
    double m_radiusB;   // minor hoop radius (local)
    Angle m_sweep;      // sweep of the major arc
    bool m_isReversed;
};

using TorusPipePtr = dqBase::RefPtr<TorusPipe>;

END_DQ_GEOM_NAMESPACE
