// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — GeometryQuery default implementations
// Ported from: itwinjs-core core/geometry/src/curve/GeometryQuery.ts
#include "dqGeom/GeometryQuery.h"
#include "dqGeom/Transform.h"

BEGIN_DQ_GEOM_NAMESPACE

bool GeometryQuery::TryTranslateInPlace(Vector3d const& v)
{
    Transform t = Transform::CreateTranslation(v);
    return TryTransformInPlace(t);
}

END_DQ_GEOM_NAMESPACE
