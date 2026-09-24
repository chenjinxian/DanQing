// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Camera (eyepoint, lens, focus)
//
// Ported from: itwinjs-core core/common/src/Camera.ts
// The position, lens angle, and focus distance of a camera.
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <dqGeom/Point3d.h>

#include <cmath>
#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

static constexpr double kPi = 3.14159265358979323846;

// JSON representation of a Camera.
// Ported from: itwinjs-core CameraProps
struct CameraProps {
    double lensRadians = kPi / 2.0;  // lens angle in radians
    double focusDist = 1.0;           // focus distance
    double eyeX = 0.0;               // eyepoint
    double eyeY = 0.0;
    double eyeZ = 0.0;
};

// The position (eyepoint), lens angle, and focus distance of a camera.
// Ported from: itwinjs-core core/common/src/Camera.ts
class DQ_COMMON_EXPORT Camera {
public:
    double lensRadians;
    double focusDist;
    dqGeom::Point3d eye;

    // Default constructor: eye at origin, 90 degree lens, 1 meter focus.
    // Ported from: itwinjs-core Camera constructor (no args)
    Camera() noexcept : lensRadians(kPi / 2.0), focusDist(1.0), eye(dqGeom::Point3d::FromZero()) {}

    // Construct from props.
    // Ported from: itwinjs-core Camera constructor (with props)
    explicit Camera(const CameraProps& props) noexcept
        : lensRadians(props.lensRadians)
        , focusDist(props.focusDist)
        , eye(dqGeom::Point3d::From(props.eyeX, props.eyeY, props.eyeZ))
    {
    }

    // Check if lens angle is valid (between PI/8 and PI).
    // Ported from: itwinjs-core Camera.isValidLensAngle()
    static bool isValidLensAngle(double radians) noexcept
    {
        return radians > (kPi / 8.0) && radians < kPi;
    }

    // Validate lens angle (reset to PI/2 if invalid).
    // Ported from: itwinjs-core Camera.validateLensAngle()
    static double validateLensAngle(double radians) noexcept
    {
        return isValidLensAngle(radians) ? radians : (kPi / 2.0);
    }

    // invalidate focus.
    // Ported from: itwinjs-core Camera.invalidateFocus()
    void invalidateFocus() noexcept { focusDist = 0.0; }

    // Whether focus is valid.
    // Ported from: itwinjs-core Camera.isFocusValid
    bool isFocusValid() const noexcept { return focusDist > 0.0 && focusDist < 1.0e14; }

    // Get/set focus distance.
    double getFocusDistance() const noexcept { return focusDist; }
    void setFocusDistance(double dist) noexcept { focusDist = dist; }

    // Whether lens is valid.
    // Ported from: itwinjs-core Camera.isLensValid
    bool isLensValid() const noexcept { return isValidLensAngle(lensRadians); }

    // Validate lens.
    void validateLens() noexcept { lensRadians = validateLensAngle(lensRadians); }

    // Get/set lens angle.
    double getLensRadians() const noexcept { return lensRadians; }
    void setLensRadians(double radians) noexcept { lensRadians = radians; }

    // Get/set eye point.
    const dqGeom::Point3d& getEyePoint() const noexcept { return eye; }
    void setEyePoint(const dqGeom::Point3d& pt) noexcept { eye = pt; }

    // Whether camera is valid.
    // Ported from: itwinjs-core Camera.isValid
    bool isValid() const noexcept { return isLensValid() && isFocusValid(); }

    // Equality.
    // Ported from: itwinjs-core Camera.equals()
    bool equals(const Camera& other) const noexcept
    {
        return std::abs(lensRadians - other.lensRadians) < 0.01 &&
               std::abs(focusDist - other.focusDist) < 0.1 &&
               eye.AlmostEqual(other.eye);
    }

    // clone.
    Camera clone() const { return *this; }

    // Convert to JSON props.
    CameraProps toJSON() const noexcept
    {
        return {lensRadians, focusDist, eye.x, eye.y, eye.z};
    }

    // Create from JSON props.
    static Camera fromJSON(const CameraProps& props) noexcept { return Camera(props); }
};

END_DQ_COMMON_NAMESPACE
