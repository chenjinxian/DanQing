// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Frustum implementation
//
// Ported from: itwinjs-core core/common/src/Frustum.ts
#include "dqCommon/Frustum.h"

#include <cmath>

BEGIN_DQ_COMMON_NAMESPACE

using namespace dqGeom;

// Helper: vector from point A to point B (B - A)
static Vector3d VecBetween(const Point3d& a, const Point3d& b) noexcept
{
    return Vector3d::From(b.x - a.x, b.y - a.y, b.z - a.z);
}

// Ported from: itwinjs-core Frustum.initNpc()
Frustum& Frustum::initNpc() noexcept
{
    for (int i = 0; i < kNpcCornerCount; ++i)
        points[i] = kNpcCorners[i];
    return *this;
}

// Ported from: itwinjs-core Frustum.getCenter()
Point3d Frustum::getCenter() const noexcept
{
    return Point3d::FromInterpolate(getCorner(Npc::RightTopFront), 0.5, getCorner(Npc::LeftBottomRear));
}

// Ported from: itwinjs-core Frustum.distance()
double Frustum::distance(int corner1, int corner2) const noexcept
{
    return points[corner1].Distance(points[corner2]);
}

// Ported from: itwinjs-core Frustum.getFraction()
double Frustum::getFraction() const noexcept
{
    const double frontDiag = distance(static_cast<int>(Npc::LeftTopFront), static_cast<int>(Npc::RightBottomFront));
    const double rearDiag = distance(static_cast<int>(Npc::LeftTopRear), static_cast<int>(Npc::RightBottomRear));
    if (rearDiag == 0.0)
        return 0.0;
    return frontDiag / rearDiag;
}

// Ported from: itwinjs-core Frustum.multiply()
void Frustum::multiply(const Transform& trans) noexcept
{
    for (auto& pt : points)
        pt = trans.MultiplyPoint3d(pt);
}

// Ported from: itwinjs-core Frustum.translate()
void Frustum::translate(const Vector3d& offset) noexcept
{
    for (auto& pt : points) {
        pt.x += offset.x;
        pt.y += offset.y;
        pt.z += offset.z;
    }
}

// Ported from: itwinjs-core Frustum.transformBy()
Frustum Frustum::transformBy(const Transform& trans) const
{
    Frustum result;
    for (int i = 0; i < kNpcCornerCount; ++i)
        result.points[i] = trans.MultiplyPoint3d(points[i]);
    return result;
}

// Ported from: itwinjs-core Frustum.toRange()
Range3d Frustum::toRange() const noexcept
{
    auto range = Range3d::CreateNull();
    for (const auto& pt : points)
        range.ExtendPoint(pt);
    return range;
}

// Ported from: itwinjs-core Frustum.setFrom()
void Frustum::setFrom(const Frustum& other) noexcept
{
    for (int i = 0; i < kNpcCornerCount; ++i)
        points[i] = other.points[i];
}

// Ported from: itwinjs-core Frustum.scaleAboutCenter()
void Frustum::scaleAboutCenter(double scale) noexcept
{
    const Frustum orig = *this;
    const double f = 0.5 * (1.0 + scale);
    // Each point is interpolated with its opposite corner
    constexpr int LBR = static_cast<int>(Npc::LeftBottomRear);    // 000
    constexpr int RBR = static_cast<int>(Npc::RightBottomRear);   // 100
    constexpr int LTR = static_cast<int>(Npc::LeftTopRear);       // 010
    constexpr int RTR = static_cast<int>(Npc::RightTopRear);      // 110
    constexpr int LBF = static_cast<int>(Npc::LeftBottomFront);   // 001
    constexpr int RBF = static_cast<int>(Npc::RightBottomFront);  // 101
    constexpr int LTF = static_cast<int>(Npc::LeftTopFront);      // 011
    constexpr int RTF = static_cast<int>(Npc::RightTopFront);     // 111

    points[LBR] = Point3d::FromInterpolate(orig.points[RTF], f, orig.points[LBR]);
    points[RBR] = Point3d::FromInterpolate(orig.points[LTF], f, orig.points[RBR]);
    points[LTR] = Point3d::FromInterpolate(orig.points[RBF], f, orig.points[LTR]);
    points[RTR] = Point3d::FromInterpolate(orig.points[LBF], f, orig.points[RTR]);
    points[LBF] = Point3d::FromInterpolate(orig.points[RTR], f, orig.points[LBF]);
    points[RBF] = Point3d::FromInterpolate(orig.points[LTR], f, orig.points[RBF]);
    points[LTF] = Point3d::FromInterpolate(orig.points[RBR], f, orig.points[LTF]);
    points[RTF] = Point3d::FromInterpolate(orig.points[LBR], f, orig.points[RTF]);
}

// Ported from: itwinjs-core Frustum.frontCenter
Point3d Frustum::getFrontCenter() const noexcept
{
    return Point3d::FromInterpolate(getCorner(Npc::LeftBottomFront), 0.5, getCorner(Npc::RightTopFront));
}

// Ported from: itwinjs-core Frustum.rearCenter
Point3d Frustum::getRearCenter() const noexcept
{
    return Point3d::FromInterpolate(getCorner(Npc::LeftBottomRear), 0.5, getCorner(Npc::RightTopRear));
}

// Ported from: itwinjs-core Frustum.scaleXYAboutCenter()
void Frustum::scaleXYAboutCenter(double scale) noexcept
{
    auto interpPt = [scale](const Point3d& center, const Point3d& corner) -> Point3d {
        return Point3d::FromInterpolate(center, scale, corner);
    };

    const auto fc = getFrontCenter();
    const auto rc = getRearCenter();

    points[static_cast<int>(Npc::LeftTopFront)] = interpPt(fc, points[static_cast<int>(Npc::LeftTopFront)]);
    points[static_cast<int>(Npc::RightTopFront)] = interpPt(fc, points[static_cast<int>(Npc::RightTopFront)]);
    points[static_cast<int>(Npc::LeftBottomFront)] = interpPt(fc, points[static_cast<int>(Npc::LeftBottomFront)]);
    points[static_cast<int>(Npc::RightBottomFront)] = interpPt(fc, points[static_cast<int>(Npc::RightBottomFront)]);

    points[static_cast<int>(Npc::LeftTopRear)] = interpPt(rc, points[static_cast<int>(Npc::LeftTopRear)]);
    points[static_cast<int>(Npc::RightTopRear)] = interpPt(rc, points[static_cast<int>(Npc::RightTopRear)]);
    points[static_cast<int>(Npc::LeftBottomRear)] = interpPt(rc, points[static_cast<int>(Npc::LeftBottomRear)]);
    points[static_cast<int>(Npc::RightBottomRear)] = interpPt(rc, points[static_cast<int>(Npc::RightBottomRear)]);
}

// Ported from: itwinjs-core Frustum.getRotation()
std::optional<Matrix3d> Frustum::getRotation() const noexcept
{
    const auto& org = getCorner(Npc::LeftBottomRear);
    const auto xVec = VecBetween(org, getCorner(Npc::RightBottomRear));
    const auto yVec = VecBetween(org, getCorner(Npc::LeftTopRear));
    // createRigidFromColumns NORMALIZES the axes — a rotation must be orthonormal.
    // Ported from: itwinjs-core Frustum.getRotation → Matrix3d.createRigidFromColumns.
    // (CreateColumns on raw edge vectors produced a matrix scaled by the frustum
    // dimensions, which inflated depth computations downstream.)
    auto matrix = Matrix3d::CreateRigidFromColumns(xVec, yVec, AxisOrder::XYZ);
    if (matrix)
        matrix->TransposeInPlace();
    return matrix;
}

// Ported from: itwinjs-core Frustum.getEyePoint()
std::optional<Point3d> Frustum::getEyePoint() const noexcept
{
    const double fraction = getFraction();
    if (std::abs(fraction - 1.0) < 1e-8)
        return std::nullopt;  // Parallel projection

    const auto& org = getCorner(Npc::LeftBottomRear);
    const auto zVec = VecBetween(org, getCorner(Npc::LeftBottomFront));
    const double s = 1.0 / (1.0 - fraction);
    return Point3d::From(org.x + zVec.x * s, org.y + zVec.y * s, org.z + zVec.z * s);
}

// Ported from: itwinjs-core Frustum.invalidate()
void Frustum::invalidate() noexcept
{
    for (auto& pt : points)
        pt = Point3d::FromZero();
}

// Ported from: itwinjs-core Frustum.equals()
bool Frustum::equals(const Frustum& rhs) const noexcept
{
    for (int i = 0; i < kNpcCornerCount; ++i) {
        if (!points[i].IsEqual(rhs.points[i]))
            return false;
    }
    return true;
}

// Ported from: itwinjs-core Frustum.isSame()
bool Frustum::isSame(const Frustum& other) const noexcept
{
    for (int i = 0; i < kNpcCornerCount; ++i) {
        if (!points[i].AlmostEqual(other.points[i]))
            return false;
    }
    return true;
}

// Ported from: itwinjs-core Frustum.initFromRange()
void Frustum::initFromRange(const Range3d& range) noexcept
{
    const auto& low = range.low;
    const auto& high = range.high;
    auto& pts = points;
    pts[0].x = pts[2].x = pts[4].x = pts[6].x = low.x;
    pts[1].x = pts[3].x = pts[5].x = pts[7].x = high.x;
    pts[0].y = pts[1].y = pts[4].y = pts[5].y = low.y;
    pts[2].y = pts[3].y = pts[6].y = pts[7].y = high.y;
    pts[0].z = pts[1].z = pts[2].z = pts[3].z = low.z;
    pts[4].z = pts[5].z = pts[6].z = pts[7].z = high.z;
}

// Ported from: itwinjs-core Frustum.fromRange()
Frustum Frustum::fromRange(const Range3d& range) noexcept
{
    Frustum frustum;
    frustum.initFromRange(range);
    return frustum;
}

// Ported from: itwinjs-core Frustum.hasMirror
// TS: u = pts[0].vectorTo(pts[_001])  _001 = 4 = LeftBottomFront
//     v = pts[0].vectorTo(pts[_010])  _010 = 2 = LeftTopRear
//     w = pts[0].vectorTo(pts[_100])  _100 = 1 = RightBottomRear
bool Frustum::hasMirror() const noexcept
{
    constexpr int LBR = static_cast<int>(Npc::LeftBottomRear);
    const auto u = VecBetween(points[LBR], points[static_cast<int>(Npc::LeftBottomFront)]);
    const auto v = VecBetween(points[LBR], points[static_cast<int>(Npc::LeftTopRear)]);
    const auto w = VecBetween(points[LBR], points[static_cast<int>(Npc::RightBottomRear)]);
    return u.TripleProduct(v, w) > 0.0;
}

// Ported from: itwinjs-core Frustum.fixPointOrder()
void Frustum::fixPointOrder() noexcept
{
    if (!hasMirror())
        return;
    // Swap pairs of points
    for (int i = 0; i < 8; i += 2) {
        const auto tmp = points[i];
        points[i] = points[i + 1];
        points[i + 1] = tmp;
    }
}

// Ported from: itwinjs-core Frustum.setFromCorners()
void Frustum::setFromCorners(const Point3d* corners) noexcept
{
    for (int i = 0; i < kNpcCornerCount; ++i)
        points[i] = corners[i];
}

// Ported from: itwinjs-core Frustum.getRangePlanes() (Frustum.ts:287-314)
ConvexClipPlaneSet Frustum::GetRangePlanes(bool clipFront, bool clipBack,
                                           double expandPlaneDistance) const
{
    ConvexClipPlaneSet set = ConvexClipPlaneSet::createEmpty();
    // createCrossProductToPoints(origin, A, B) = cross(A-origin, B-origin); the resulting
    // normal is the inward normal of the face, distance = normal·pointOnPlane.
    auto addFace = [&](Point3d const& origin, Point3d const& A, Point3d const& B,
                       Point3d const& pointOnPlane) {
        Vector3d n = Vector3d::FromCrossProduct(
            A.x - origin.x, A.y - origin.y, A.z - origin.z,
            B.x - origin.x, B.y - origin.y, B.z - origin.z);
        if (n.Normalize() > 1.0e-15) {
            double const d = n.DotProduct(pointOnPlane) - expandPlaneDistance;
            auto cp = ClipPlane::createNormalAndDistance(n, d);
            if (cp)
                set.addPlaneToConvexSet(*cp);
        }
    };
    addFace(points[5], points[3], points[1], points[1]);
    addFace(points[2], points[4], points[0], points[0]);
    addFace(points[3], points[6], points[2], points[2]);
    addFace(points[4], points[1], points[0], points[0]);
    if (clipBack)
        addFace(points[1], points[2], points[0], points[0]);
    if (clipFront)
        addFace(points[6], points[5], points[4], points[4]);
    return set;
}

// Ported from: itwinjs-core Frustum.getIntersectionWithPlane() (Frustum.ts:317-329)
std::optional<std::vector<Point3d>> Frustum::GetIntersectionWithPlane(
    Plane3dByOriginAndUnitNormal const& plane) const
{
    ClipPlane const clipPlane = ClipPlane::createPlane(plane);
    auto loopPoints = clipPlane.intersectRange(toRange(), true);
    if (!loopPoints)
        return std::nullopt;
    ConvexClipPlaneSet const convexSet = GetRangePlanes(false, false, 0.0);
    if (!convexSet.polygonClip(*loopPoints))
        return std::nullopt;
    if (loopPoints->size() < 4)
        return std::nullopt;
    return loopPoints;
}

END_DQ_COMMON_NAMESPACE
