// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Solar calculation helpers
// Ported from: itwinjs-core core/common/src/SolarCalculate.ts
//
// M-S 范围面：仅 calculateSolarDirectionFromAngles（DTA ThematicDisplayEditor
// defaultSettings 的太阳向默认所需，ThematicDisplay.ts:21-35）。
// TODO: port SolarCalculate.ts remainder（calculateSolarAngles /
// calculateSolarDirection[date+location] / sunrise-sunset 族）——随
// solar/timeline 里程碑立项（参考源已在案）。
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <dqGeom/Angle.h>
#include <dqGeom/Vector3d.h>

#include <cmath>

BEGIN_DQ_COMMON_NAMESPACE

// Calculate solar direction corresponding to the given azimuth and elevation
// (altitude) angles in degrees.
// Ported from: itwinjs-core calculateSolarDirectionFromAngles
// (SolarCalculate.ts:191-197)。§3.4 适配：参考的 {azimuth,elevation} 对象
// 形参 → 两 double。
inline dqGeom::Vector3d calculateSolarDirectionFromAngles(double azimuth, double elevation)
{
    const double az = dqGeom::Angle::DegreesToRadians(azimuth);
    const double el = dqGeom::Angle::DegreesToRadians(elevation);
    const double cosElevation = std::cos(el);
    const double sinElevation = std::sin(el);
    return dqGeom::Vector3d::From(
        -std::sin(az) * cosElevation, -std::cos(az) * cosElevation, -sinElevation);
}

END_DQ_COMMON_NAMESPACE
