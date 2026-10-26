// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Matrix3d out-of-line members
// Ported from: itwinjs-core core/geometry/src/geometry3d/Matrix3d.ts
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Transform.h>

namespace dqGeom {

// Ported from: itwinjs-core Matrix3d.multiplyMatrixTransform (Matrix3d.ts:2209-2213)
//   return Transform.createRefs(this.multiplyXYZ(other.origin), this.multiplyMatrixMatrix(other.matrix));
dqGeom::Transform Matrix3d::multiplyMatrixTransform(dqGeom::Transform const& other) const
{
    Vector3d const org = MultiplyVector(Vector3d::From(other.origin));
    return dqGeom::Transform::CreateOriginAndMatrix(
        Point3d::From(org.x, org.y, org.z),
        MultiplyMatrix(other.matrix));
}

}  // namespace dqGeom
