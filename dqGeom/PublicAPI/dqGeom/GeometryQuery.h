// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — GeometryQuery abstract base
//
// Ported from: itwinjs-core core/geometry/src/curve/GeometryQuery.ts
//
// Root of the geometry class hierarchy.  All reference-counted geometry types
// (CurvePrimitive, CurveCollection, SolidPrimitive, Polyface) derive from this.
//
// CLAUDE.md §7.2: RefCounted base, virtual dispatch, no RTTI.
#pragma once

#include "Export.h"
#include "Range3d.h"
#include "Transform.h"

#include <dqBase/RefCounted.h>

BEGIN_DQ_GEOM_NAMESPACE

class GeometryHandler;

// ---------------------------------------------------------------------------
// GeometryCategory — type discriminator
// (Ported from: itwinjs-core GeometryQueryCategory)
// ---------------------------------------------------------------------------
enum class GeometryCategory : int {
    CurvePrimitive,
    CurveCollection,
    Solid,
    Polyface,
    Point,
    PointCollection,
    BSurface,
};

// ---------------------------------------------------------------------------
// GeometryQuery — abstract root of geometry hierarchy
// (Ported from: itwinjs-core GeometryQuery.ts)
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT GeometryQuery : public dqBase::RefCounted<GeometryQuery> {
public:
    ~GeometryQuery() override = default;

    // --- Type discriminator ---
    virtual GeometryCategory Category() const noexcept = 0;

    // --- Pure virtual geometry operations ---
    virtual Range3d Range() const = 0;
    virtual void ExtendRange(Range3d& range) const = 0;
    virtual bool TryTransformInPlace(Transform const& transform) = 0;
    virtual bool TryTranslateInPlace(Vector3d const& v);
    virtual dqBase::RefPtr<GeometryQuery> clone() const = 0;
    virtual dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const = 0;
    virtual bool IsSameGeometryClass(GeometryQuery const& other) const noexcept = 0;
    virtual bool IsAlmostEqual(GeometryQuery const& other, double tol) const = 0;

    // --- Double dispatch (RTTI-free type dispatch) ---
    virtual void DispatchToHandler(GeometryHandler& handler) = 0;
};

using GeometryQueryPtr = dqBase::RefPtr<GeometryQuery>;

END_DQ_GEOM_NAMESPACE
