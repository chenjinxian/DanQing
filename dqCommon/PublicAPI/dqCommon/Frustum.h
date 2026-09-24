// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Frustum (view volume)
//
// Ported from: itwinjs-core core/common/src/Frustum.ts
// The region of physical (3d) space that appears in a view.
// Stored as 8 points in Npc order defining a truncated pyramid.
#pragma once

#include "Npc.h"
#include "DqCommon.h"

#include <dqGeom/ClipPlane.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Plane3dByOriginAndUnitNormal.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <cstdint>
#include <optional>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// The region of physical (3d) space that appears in a view.
// Ported from: itwinjs-core core/common/src/Frustum.ts
class DQ_COMMON_EXPORT Frustum {
public:
    // 8 corner points in Npc order.
    dqGeom::Point3d points[kNpcCornerCount];

    // Constructor — initializes to the NPC cube.
    // Ported from: itwinjs-core Frustum constructor
    Frustum() noexcept { initNpc(); }

    // Initialize to the NPC cube.
    // Ported from: itwinjs-core Frustum.initNpc()
    Frustum& initNpc() noexcept;

    // Get a corner point.
    // Ported from: itwinjs-core Frustum.getCorner()
    dqGeom::Point3d& getCorner(int i) noexcept { return points[i]; }
    const dqGeom::Point3d& getCorner(int i) const noexcept { return points[i]; }
    dqGeom::Point3d& getCorner(Npc i) noexcept { return points[static_cast<int>(i)]; }
    const dqGeom::Point3d& getCorner(Npc i) const noexcept { return points[static_cast<int>(i)]; }

    // Get the center point (midpoint between RightTopFront and LeftBottomRear).
    // Ported from: itwinjs-core Frustum.getCenter()
    dqGeom::Point3d getCenter() const noexcept;

    // Get the distance between two corners.
    // Ported from: itwinjs-core Frustum.distance()
    double distance(int corner1, int corner2) const noexcept;

    // Get the ratio of front plane diagonal to back plane diagonal.
    // Ported from: itwinjs-core Frustum.getFraction()
    double getFraction() const noexcept;

    // multiply all points by a Transform, in place.
    // Ported from: itwinjs-core Frustum.multiply()
    void multiply(const dqGeom::Transform& trans) noexcept;

    // Offset all points by a vector.
    // Ported from: itwinjs-core Frustum.translate()
    void translate(const dqGeom::Vector3d& offset) noexcept;

    // Transform all points and return result in another Frustum.
    // Ported from: itwinjs-core Frustum.transformBy()
    Frustum transformBy(const dqGeom::Transform& trans) const;

    // Calculate a bounding range from the 8 points.
    // Ported from: itwinjs-core Frustum.toRange()
    dqGeom::Range3d toRange() const noexcept;

    // clone this Frustum.
    // Ported from: itwinjs-core Frustum.clone()
    Frustum clone() const noexcept { return *this; }

    // Set points from another Frustum.
    // Ported from: itwinjs-core Frustum.setFrom()
    void setFrom(const Frustum& other) noexcept;

    // Scale about center.
    // Ported from: itwinjs-core Frustum.scaleAboutCenter()
    void scaleAboutCenter(double scale) noexcept;

    // Center of front face.
    // Ported from: itwinjs-core Frustum.frontCenter
    dqGeom::Point3d getFrontCenter() const noexcept;

    // Center of rear face.
    // Ported from: itwinjs-core Frustum.rearCenter
    dqGeom::Point3d getRearCenter() const noexcept;

    // Scale XY plane about center.
    // Ported from: itwinjs-core Frustum.scaleXYAboutCenter()
    void scaleXYAboutCenter(double scale) noexcept;

    // Get the rotation matrix to the frame of this frustum.
    // Ported from: itwinjs-core Frustum.getRotation()
    std::optional<dqGeom::Matrix3d> getRotation() const noexcept;

    // Get the eye point (undefined if parallel projection).
    // Ported from: itwinjs-core Frustum.getEyePoint()
    std::optional<dqGeom::Point3d> getEyePoint() const noexcept;

    // invalidate by setting all points to zero.
    // Ported from: itwinjs-core Frustum.invalidate()
    void invalidate() noexcept;

    // Equality (exact).
    // Ported from: itwinjs-core Frustum.equals()
    bool equals(const Frustum& rhs) const noexcept;

    // Equality (approximate).
    // Ported from: itwinjs-core Frustum.isSame()
    bool isSame(const Frustum& other) const noexcept;

    // Initialize from a range.
    // Ported from: itwinjs-core Frustum.initFromRange()
    void initFromRange(const dqGeom::Range3d& range) noexcept;

    // Create from a range.
    // Ported from: itwinjs-core Frustum.fromRange()
    static Frustum fromRange(const dqGeom::Range3d& range) noexcept;

    // True if frustum has mirror (incorrect point order).
    // Ported from: itwinjs-core Frustum.hasMirror
    bool hasMirror() const noexcept;

    // Fix point order if mirrored.
    // Ported from: itwinjs-core Frustum.fixPointOrder()
    void fixPointOrder() noexcept;

    // Set points from an array of 8 corners.
    // Ported from: itwinjs-core Frustum.setFromCorners()
    void setFromCorners(const dqGeom::Point3d* corners) noexcept;

    // Get a convex set of clipping planes bounding the region contained by this Frustum.
    // Ported from: itwinjs-core Frustum.getRangePlanes() (Frustum.ts:287-314)
    dqGeom::ConvexClipPlaneSet GetRangePlanes(bool clipFront, bool clipBack,
                                              double expandPlaneDistance) const;

    // Get a (convex) polygon that represents the intersection of this frustum with a
    // plane, or nullopt if no intersection exists.
    // Ported from: itwinjs-core Frustum.getIntersectionWithPlane() (Frustum.ts:317-329)
    std::optional<std::vector<dqGeom::Point3d>> GetIntersectionWithPlane(
        dqGeom::Plane3dByOriginAndUnitNormal const& plane) const;

    // TODO: Requires dqGeom::Map4d (not yet ported)
    // Ported from: itwinjs-core Frustum.toMap4d()
    // std::optional<Map4d> ToMap4d() const;
};

END_DQ_COMMON_NAMESPACE
