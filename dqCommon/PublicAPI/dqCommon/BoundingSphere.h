// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Bounding sphere
//
// Ported from: itwinjs-core core/common/src/geometry/BoundingSphere.ts
// Spherical bounding volume for quick intersection/containment tests.
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>

#include <algorithm>
#include <cmath>

BEGIN_DQ_COMMON_NAMESPACE

// Describes a spherical volume of space as an approximation of a more complex shape.
// Ported from: itwinjs-core BoundingSphere
class DQ_COMMON_EXPORT BoundingSphere {
public:
    dqGeom::Point3d center;
    double radius = 0.0;

    BoundingSphere() noexcept = default;
    BoundingSphere(const dqGeom::Point3d& center_, double radius_) noexcept
        : center(center_), radius(radius_)
    {
    }

    // Change center and radius.
    // Ported from: itwinjs-core BoundingSphere.init()
    void init(const dqGeom::Point3d& c, double r) noexcept
    {
        center = c;
        radius = r;
    }

    // Apply transform, producing new bounding sphere.
    // Ported from: itwinjs-core BoundingSphere.transformBy()
    BoundingSphere transformBy(const dqGeom::Transform& transform) const noexcept
    {
        BoundingSphere result;
        result.center = transform.MultiplyPoint3d(center);
        const auto& m = transform.GetMatrix();
        const double maxScale = (std::max)({m.ColumnXMagnitude(), m.ColumnYMagnitude(), m.ColumnZMagnitude()});
        result.radius = radius * maxScale;
        return result;
    }

    // Apply transform in place.
    // Ported from: itwinjs-core BoundingSphere.transformInPlace()
    void transformInPlace(const dqGeom::Transform& transform) noexcept
    {
        *this = transformBy(transform);
    }
};

END_DQ_COMMON_NAMESPACE
