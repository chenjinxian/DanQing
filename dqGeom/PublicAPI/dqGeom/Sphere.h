// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/Sphere.ts
// DanQing dqGeom — Sphere / ellipsoid (unit sphere mapped by localToWorld + latitude sweep)
//
// 保真依据：逐位移植 Sphere.ts 的类结构（localToWorld + latitudeSweep + 工厂 + 访问器 +
// capped/IsClosedVolume/clone/dispatch）。createDgnSphere/createFromAxesAndScales 需
// Matrix3d::createRigidFromColumns/scaleColumnsInPlace（scaleColumnsInPlace 未移植，phase）；
// constantVSection/UV 与 getConstructiveFrame(CloneRigid) 亦 phase（addSolidPrimitive 不依赖——
// 经 localToWorld.MultiplyXYZ(cosφ cosθ, cosφ sinθ, sinφ) 直接 tessellate）。
#pragma once

#include "Angle.h"
#include "AngleSweep.h"
#include "GeometryHandler.h"
#include "GeometryQuery.h"
#include "Matrix3d.h"
#include "Point2d.h"
#include "Point3d.h"
#include "Range3d.h"
#include "SolidPrimitive.h"
#include "Transform.h"
#include "Vector3d.h"

#include <optional>

BEGIN_DQ_GEOM_NAMESPACE

// Sphere — unit sphere mapped to world by localToWorld, with optional restricted latitude (1:1 Sphere.ts).
class DQ_GEOM_EXPORT Sphere : public SolidPrimitive, public UVSurface, public UVSurfaceIsoParametricDistance {
public:
    ~Sphere() override = default;

    // Full latitude sweep -π/2..π/2 (1:1 AngleSweep.createFullLatitude).
    static AngleSweep FullLatitudeSweep() noexcept { return AngleSweep::FromStartEndRadians(-Angle::kPi / 2.0, Angle::kPi / 2.0); }

    // --- Factories (1:1 Sphere ctor / createCenterRadius / createEllipsoid) ---
    static dqBase::RefPtr<Sphere> Create(Transform const& localToWorld, AngleSweep const& latitudeSweep, bool capped);
    static dqBase::RefPtr<Sphere> CreateCenterRadius(Point3d const& center, double radius,
                                                     AngleSweep const& latitudeSweep, bool capped);
    static dqBase::RefPtr<Sphere> CreateEllipsoid(Transform const& localToWorld, AngleSweep const& latitudeSweep, bool capped);

    // --- SolidPrimitive ---
    SolidPrimitiveType GetSolidPrimitiveType() const noexcept override { return SolidPrimitiveType::Sphere; }
    // TODO commit-N: strokeConstantVSection + Loop::Create.
    dqBase::RefPtr<CurveCollection> ConstantVSection(double /*vFraction*/) const override { return nullptr; }
    // TODO commit-N: CloneRigid.
    std::optional<Transform> GetConstructiveFrame() const override { return m_localToWorld.clone(); }
    bool IsClosedVolume() const override; // capped || full-latitude sweep
    void DispatchToHandler(GeometryHandler& handler) override { handler.HandleSphere(*this); }

    // --- Accessors (1:1 Sphere.cloneCenter/cloneVectorX/Y/Z/cloneLatitudeSweep/vFractionToRadians/...) ---
    Point3d CloneCenter() const noexcept { return m_localToWorld.origin; }
    Vector3d CloneVectorX() const noexcept { return m_localToWorld.matrix.ColumnX(); }
    Vector3d CloneVectorY() const noexcept { return m_localToWorld.matrix.ColumnY(); }
    Vector3d CloneVectorZ() const noexcept { return m_localToWorld.matrix.ColumnZ(); }
    AngleSweep CloneLatitudeSweep() const noexcept { return m_latitudeSweep; }
    double VFractionToRadians(double v) const noexcept { return m_latitudeSweep.FractionToRadians(v); }
    static double UFractionToRadians(double u) noexcept { return u * Angle::k2Pi; }
    double MaxAxisRadius() const noexcept;
    Transform const& GetLocalToWorld() const noexcept { return m_localToWorld; }

    // --- UVSurface / UVSurfaceIsoParametricDistance ---
    // Ported from: itwinjs-core core/geometry/src/solid/Sphere.ts:294-308 (uvFractionToPoint),
    //              :312-327 (uvFractionToPointAndTangents), :345-356 (maxIsoParametricDistance)
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
    Sphere(Transform const& localToWorld, AngleSweep const& latitudeSweep, bool capped)
        : SolidPrimitive(capped), m_localToWorld(localToWorld), m_latitudeSweep(latitudeSweep) {}

    Transform m_localToWorld;
    AngleSweep m_latitudeSweep;
};

using SpherePtr = dqBase::RefPtr<Sphere>;

END_DQ_GEOM_NAMESPACE
