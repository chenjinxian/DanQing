// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewPose (immutable snapshot of view state)
//
// Ported from: itwinjs-core core/frontend/src/ViewPose.ts
// An immutable snapshot of a view's origin, extents, rotation, and camera.
#pragma once

#include "Export.h"

#include <dqBase/DqTime.h>
#include <dqCommon/Camera.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <cmath>
#include <memory>
#include <optional>

namespace dqApp {

class ViewState;
class ViewState3d;
class ViewState2d;

// View pose type.
enum class ViewPoseType : uint8_t {
    View3d = 0,
    View2d = 1,
};

// Abstract base class for view poses.
// Ported from: itwinjs-core ViewPose
class DQ_APP_EXPORT ViewPose {
public:
    virtual ~ViewPose() = default;

    // ← Viewport.ts:3670 — pose.undoTime（0.5s 防抖窗口的比较基准，BeTimePoint）。
    dqBase::DqTimePoint undoTime;

    // Type identification (replaces dynamic_cast).
    virtual ViewPoseType getType() const noexcept = 0;

    // The origin of the view in world coordinates.
    virtual dqGeom::Point3d GetOrigin() const = 0;

    // The extents of the view in world coordinates.
    virtual dqGeom::Vector3d GetExtents() const = 0;

    // The 3x3 ortho-normal rotation matrix of the view.
    virtual dqGeom::Matrix3d getRotation() const = 0;

    // Whether the camera is enabled.
    bool IsCameraOn() const noexcept { return m_cameraOn; }

    // Compute the center of the viewed volume.
    // Ported from: itwinjs-core ViewPose.center
    dqGeom::Point3d getCenter() const
    {
        const auto ext = GetExtents();
        const auto rot = getRotation();
        // center = origin + rotation^T * extents * 0.5
        const auto& coffs = rot.coffs;
        const double dx = coffs[0] * ext.x + coffs[3] * ext.y + coffs[6] * ext.z;
        const double dy = coffs[1] * ext.x + coffs[4] * ext.y + coffs[7] * ext.z;
        const double dz = coffs[2] * ext.x + coffs[5] * ext.y + coffs[8] * ext.z;
        const auto o = GetOrigin();
        return dqGeom::Point3d::From(o.x + dx * 0.5, o.y + dy * 0.5, o.z + dz * 0.5);
    }

    // Returns the target point of the view.
    // Ported from: itwinjs-core ViewPose.target
    virtual dqGeom::Point3d GetTarget() const { return getCenter(); }

    // Compute the Z vector of the rotation matrix.
    // Ported from: itwinjs-core ViewPose.zVec
    dqGeom::Vector3d GetZVec() const
    {
        const auto& coffs = getRotation().coffs;
        return dqGeom::Vector3d::From(coffs[6], coffs[7], coffs[8]);
    }

    // Equality check.
    virtual bool equals(const ViewPose& other) const = 0;

    // Returns true if this pose is equivalent to the pose represented by the
    // specified ViewState. Ported from: itwinjs-core ViewPose.equalState
    // (ViewPose.ts:27). 3d 实现见 ViewPose.cpp（ViewPose.ts:108-117）。
    virtual bool equalState(const ViewState& view) const = 0;

protected:
    explicit ViewPose(bool cameraOn) noexcept : m_cameraOn(cameraOn) {}

private:
    bool m_cameraOn;
};

// 3D view pose with camera support.
// Ported from: itwinjs-core ViewPose3d
class DQ_APP_EXPORT ViewPose3d : public ViewPose {
public:
    dqGeom::Point3d origin;
    dqGeom::Vector3d extents;
    dqGeom::Matrix3d rotation;
    dqCommon::Camera camera;

    // Construct from a ViewState3d.
    // Ported from: itwinjs-core ViewPose3d constructor
    ViewPose3d(const dqGeom::Point3d& origin_, const dqGeom::Vector3d& extents_,
               const dqGeom::Matrix3d& rotation_, const dqCommon::Camera& camera_, bool cameraOn)
        : ViewPose(cameraOn), origin(origin_), extents(extents_), rotation(rotation_), camera(camera_)
    {
    }

    ViewPoseType getType() const noexcept override { return ViewPoseType::View3d; }

    // Accessors
    dqGeom::Point3d GetOrigin() const override { return origin; }
    dqGeom::Vector3d GetExtents() const override { return extents; }
    dqGeom::Matrix3d getRotation() const override { return rotation; }

    // Target point (accounts for camera).
    // Ported from: itwinjs-core ViewPose3d.target
    dqGeom::Point3d GetTarget() const override
    {
        if (!IsCameraOn())
            return getCenter();
        // target = eye + rotation.getRow(2) * -focusDist
        const auto& coffs = rotation.coffs;
        const auto& eye = camera.eye;
        const double fd = camera.focusDist;
        return dqGeom::Point3d::From(
            eye.x + coffs[6] * (-fd),
            eye.y + coffs[7] * (-fd),
            eye.z + coffs[8] * (-fd));
    }

    // Equality.
    // Ported from: itwinjs-core ViewPose3d.equal()
    bool equals(const ViewPose& other) const override
    {
        if (other.getType() != ViewPoseType::View3d)
            return false;
        const auto& o3d = static_cast<const ViewPose3d&>(other);
        return IsCameraOn() == o3d.IsCameraOn() &&
               origin.AlmostEqual(o3d.origin) &&
               extents.AlmostEqual(o3d.extents) &&
               rotation.IsAlmostEqual(o3d.rotation) &&
               (!IsCameraOn() || camera.equals(o3d.camera));
    }

    // Ported from: itwinjs-core ViewPose3d.equalState (ViewPose.ts:108-117).
    // 实现见 ViewPose.cpp。
    bool equalState(const ViewState& view) const override;
};

// 2D view pose.
// Ported from: itwinjs-core ViewPose2d
class DQ_APP_EXPORT ViewPose2d : public ViewPose {
public:
    dqGeom::Point3d origin2d;
    dqGeom::Vector3d delta;
    double angleDegrees = 0.0;

    // Construct from 2D view parameters.
    ViewPose2d(const dqGeom::Point3d& origin_, const dqGeom::Vector3d& delta_, double angleDeg)
        : ViewPose(false), origin2d(origin_), delta(delta_), angleDegrees(angleDeg)
    {
    }

    ViewPoseType getType() const noexcept override { return ViewPoseType::View2d; }

    // Accessors
    dqGeom::Point3d GetOrigin() const override { return origin2d; }
    dqGeom::Vector3d GetExtents() const override { return delta; }

    // Rotation from angle.
    // Ported from: itwinjs-core ViewPose2d.rotation
    dqGeom::Matrix3d getRotation() const override
    {
        const double rad = angleDegrees * M_PI / 180.0;
        const double c = std::cos(rad);
        const double s = std::sin(rad);
        return dqGeom::Matrix3d::CreateRowValues(
            c, -s, 0.0,
            s, c, 0.0,
            0.0, 0.0, 1.0);
    }

    // Equality.
    bool equals(const ViewPose& other) const override
    {
        if (other.getType() != ViewPoseType::View2d)
            return false;
        const auto& o2d = static_cast<const ViewPose2d&>(other);
        return origin2d.AlmostEqual(o2d.origin2d) &&
               delta.AlmostEqual(o2d.delta) &&
               std::abs(angleDegrees - o2d.angleDegrees) < 1e-8;
    }

    // Ported from: itwinjs-core ViewPose2d.equalState (ViewPose.ts:157-164).
    // 实现见 ViewPose.cpp（DanQing 无 2D 视图类，恒 false）。
    bool equalState(const ViewState& view) const override;
};

}  // namespace dqApp
