// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/Cone.ts
// DanQing dqGeom — Cone / frustum (circular or elliptical cross-sections along an axis)
//
// 保真依据：逐位移植 Cone.ts 的类结构（localToWorld + radiusA/radiusB/maxRadius + 工厂 +
// 访问器 + capped/IsClosedVolume/clone/dispatch）。createAxisPoints/createDgnCone 需
// Matrix3d::createRigidHeadsUp/createRigidFromColumns（未移植，phase）；constantVSection/UV
// (uvFractionToPointAndTangents) 与 getConstructiveFrame(CloneRigid) 亦 phase
// （addSolidPrimitive 不依赖——只用 localToWorld/radii/vectorX/Y 直接 tessellate）。
#pragma once

#include "GeometryHandler.h"
#include "GeometryQuery.h"
#include "Point2d.h"
#include "Point3d.h"
#include "Range3d.h"
#include "SolidPrimitive.h"
#include "Transform.h"
#include "Vector3d.h"

#include <optional>

BEGIN_DQ_GEOM_NAMESPACE

// Cone — solid with circular/elliptical cross-sections at z=0 (radiusA) and z=1 (radiusB) (1:1 Cone.ts).
class DQ_GEOM_EXPORT Cone : public SolidPrimitive, public UVSurface, public UVSurfaceIsoParametricDistance {
public:
    ~Cone() override = default;

    // --- Factories (1:1 Cone ctor / createBaseAndTarget) ---
    static dqBase::RefPtr<Cone> Create(Transform const& map, double radiusA, double radiusB, bool capped);
    static dqBase::RefPtr<Cone> CreateBaseAndTarget(Point3d const& centerA, Point3d const& centerB,
                                                    Vector3d const& vectorX, Vector3d const& vectorY,
                                                    double radiusA, double radiusB, bool capped);

    // --- SolidPrimitive ---
    SolidPrimitiveType GetSolidPrimitiveType() const noexcept override { return SolidPrimitiveType::Cone; }
    // TODO commit-N: strokeConstantVSection + Loop::Create (needs StrokeOptions.defaultCircleStrokes).
    dqBase::RefPtr<CurveCollection> ConstantVSection(double /*vFraction*/) const override { return nullptr; }
    // TODO commit-N: CloneRigid (rigid axis normalization).
    std::optional<Transform> GetConstructiveFrame() const override { return m_localToWorld.clone(); }
    bool IsClosedVolume() const override { return m_capped && m_radiusA != 0.0 && m_radiusB != 0.0; }
    void DispatchToHandler(GeometryHandler& handler) override { handler.HandleCone(*this); }

    // --- Accessors (1:1 Cone.getCenterA/getRadiusA/getVectorX/vFractionToRadius/...) ---
    Point3d GetCenterA() const { return m_localToWorld.MultiplyXYZ(0.0, 0.0, 0.0); }
    Point3d GetCenterB() const { return m_localToWorld.MultiplyXYZ(0.0, 0.0, 1.0); }
    Vector3d GetVectorX() const noexcept { return m_localToWorld.matrix.ColumnX(); }
    Vector3d GetVectorY() const noexcept { return m_localToWorld.matrix.ColumnY(); }
    double GetRadiusA() const noexcept { return m_radiusA; }
    double GetRadiusB() const noexcept { return m_radiusB; }
    double GetMaxRadius() const noexcept { return m_maxRadius; }
    double VFractionToRadius(double v) const noexcept { return m_radiusA + v * (m_radiusB - m_radiusA); }
    Transform const& GetLocalToWorld() const noexcept { return m_localToWorld; }

    // --- UVSurface / UVSurfaceIsoParametricDistance ---
    // Ported from: itwinjs-core core/geometry/src/solid/Cone.ts:275-280 (uvFractionToPoint),
    //              :286-299 (uvFractionToPointAndTangents), :334-346 (maxIsoParametricDistance)
    // v=0 底面 / v=1 顶面；u∈[0,1] 环绕一周。
    Point3d UVFractionToPoint(double uFraction, double vFraction) const override;
    Plane3dByOriginAndVectors UVFractionToPointAndTangents(double uFraction, double vFraction) const override;
    Point2d MaxIsoParametricDistance() const noexcept;

    // --- GeometryQuery ---
    Range3d Range() const override;
    void ExtendRange(Range3d& range) const override;
    bool TryTransformInPlace(Transform const& transform) override;
    dqBase::RefPtr<GeometryQuery> clone() const override;
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const override;
    bool IsSameGeometryClass(GeometryQuery const& other) const noexcept override;
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const override;

private:
    Cone(Transform const& map, double radiusA, double radiusB, bool capped)
        : SolidPrimitive(capped)
        , m_localToWorld(map)
        , m_radiusA(radiusA), m_radiusB(radiusB), m_maxRadius(radiusA > radiusB ? radiusA : radiusB) {}

    Transform m_localToWorld;
    double m_radiusA;
    double m_radiusB;
    double m_maxRadius;
};

using ConePtr = dqBase::RefPtr<Cone>;

END_DQ_GEOM_NAMESPACE
