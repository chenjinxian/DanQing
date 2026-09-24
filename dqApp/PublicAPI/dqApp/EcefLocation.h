// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ECEF location (Earth-Centered Earth-Fixed)
// Ported from: itwinjs-core core/common/src/geometry/EcefLocation.ts
#pragma once

#include "Export.h"

#include <dqCommon/Cartographic.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>
#include <dqGeom/Matrix3d.h>

namespace dqApp {

// ---------------------------------------------------------------------------
// EcefLocationProps — JSON representation of an EcefLocation
// Ported from: itwinjs-core EcefLocationProps
// ---------------------------------------------------------------------------
struct EcefLocationProps {
    dqGeom::Point3d origin = {0, 0, 0};
    dqGeom::Vector3d orientation = {0, 0, 0};  // yaw, pitch, roll in degrees
    double distance = 0.0;
};

// ---------------------------------------------------------------------------
// EcefLocation — position and orientation of the iModel on Earth
// Ported from: itwinjs-core core/common/src/geometry/EcefLocation.ts
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT EcefLocation {
public:
    /// Origin in ECEF coordinates (meters).
    dqGeom::Point3d origin = {0, 0, 0};

    /// Transform from ECEF to iModel coordinates.
    dqGeom::Matrix3d orientation;

    /// Create from a Cartographic origin point.
    /// Ported from: itwinjs-core EcefLocation.createFromCartographicOrigin
    static EcefLocation CreateFromCartographicOrigin(const dqCommon::Cartographic& origin);

    /// Create from EcefLocationProps.
    static EcefLocation FromProps(const EcefLocationProps& props);

    /// Convert to EcefLocationProps.
    EcefLocationProps ToProps() const;

    /// Default constructor.
    EcefLocation() = default;

    /// Construct with origin and orientation.
    EcefLocation(const dqGeom::Point3d& origin_, const dqGeom::Matrix3d& orientation_)
        : origin(origin_), orientation(orientation_) {}
};

}  // namespace dqApp
