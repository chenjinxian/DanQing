// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/Box.ts
// DanQing dqGeom — Box (solid defined by local frame + base/top x/y sizes)
//
// 保真依据：逐位移植 Box.ts。Box : SolidPrimitive，持 _localToWorld (Transform) + baseX/baseY/
// topX/topY。8 角点 x-fastest then y then z 序。getConstructiveFrame 的 CloneRigid 与 extendRange
// 的 transform 变体（需 Matrix3d::IsSingular/ScaleColumnsInPlace + Range3d::extendTransformedXYZ）
// 为 Phase-N（tryTransformInPlace mirror 分支亦 phase）。
#pragma once

#include "GeometryHandler.h"
#include "GeometryQuery.h"
#include "LineString3d.h"
#include "Loop.h"
#include "Point3d.h"
#include "Range3d.h"
#include "SolidPrimitive.h"
#include "Transform.h"
#include "Vector3d.h"

#include <array>

BEGIN_DQ_GEOM_NAMESPACE

// Box — solid defined by a local coordinate frame + base/top rectangle sizes (1:1 Box.ts).
class DQ_GEOM_EXPORT Box : public SolidPrimitive {
public:
    ~Box() override = default;

    // --- Factories (1:1 Box.createDgnBox / createRange / ctor) ---
    static dqBase::RefPtr<Box> Create(Transform const& map, double baseX, double baseY,
                                      double topX, double topY, bool capped);
    static dqBase::RefPtr<Box> CreateDgnBox(Point3d const& origin, Vector3d const& vectorX,
                                            Vector3d const& vectorY, Point3d const& topOrigin,
                                            double baseX, double baseY, double topX, double topY, bool capped);
    static dqBase::RefPtr<Box> CreateRange(Range3d const& range, bool capped);

    // --- SolidPrimitive ---
    SolidPrimitiveType GetSolidPrimitiveType() const noexcept override { return SolidPrimitiveType::Box; }
    dqBase::RefPtr<CurveCollection> ConstantVSection(double zFraction) const override;
    std::optional<Transform> GetConstructiveFrame() const override;
    bool IsClosedVolume() const override { return m_capped; }
    void DispatchToHandler(GeometryHandler& handler) override { handler.HandleBox(*this); }

    // --- Accessors (1:1 Box.getBaseX/getBaseOrigin/getVectorX/...) ---
    double GetBaseX() const noexcept { return m_baseX; }
    double GetBaseY() const noexcept { return m_baseY; }
    double GetTopX() const noexcept { return m_topX; }
    double GetTopY() const noexcept { return m_topY; }
    Point3d GetBaseOrigin() const { return m_localToWorld.MultiplyXYZ(0.0, 0.0, 0.0); }
    Point3d GetTopOrigin() const { return m_localToWorld.MultiplyXYZ(0.0, 0.0, 1.0); }
    Vector3d GetVectorX() const noexcept { return m_localToWorld.matrix.ColumnX(); }
    Vector3d GetVectorY() const noexcept { return m_localToWorld.matrix.ColumnY(); }
    Vector3d GetVectorZ() const noexcept { return m_localToWorld.matrix.ColumnZ(); }
    Transform const& GetLocalToWorld() const noexcept { return m_localToWorld; }

    // 8 corners, x-fastest then y then z (1:1 Box.getCorners).
    std::array<Point3d, 8> GetCorners() const;

    // --- GeometryQuery ---
    Range3d Range() const override;
    void ExtendRange(Range3d& range) const override;
    bool TryTransformInPlace(Transform const& transform) override;
    dqBase::RefPtr<GeometryQuery> clone() const override;
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const override;
    bool IsSameGeometryClass(GeometryQuery const& other) const noexcept override;
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const override;

private:
    Box(Transform const& map, double baseX, double baseY, double topX, double topY, bool capped)
        : SolidPrimitive(capped)
        , m_localToWorld(map)
        , m_baseX(baseX), m_baseY(baseY), m_topX(topX), m_topY(topY) {}

    Transform m_localToWorld;
    double m_baseX;
    double m_baseY;
    double m_topX;
    double m_topY;
};

using BoxPtr = dqBase::RefPtr<Box>;

END_DQ_GEOM_NAMESPACE
