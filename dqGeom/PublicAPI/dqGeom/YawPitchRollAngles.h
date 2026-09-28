// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — YawPitchRollAngles
// Ported from: itwinjs-core core/geometry/src/geometry3d/YawPitchRollAngles.ts
//
// 移植面（最小消费面——ViewState3d 构造的 angles→rotation 链，
// ViewState.ts:1502 `YawPitchRollAngles.fromJSON(props.angles).toMatrix3d()`）：
//   - createDegrees / fromJSON（数值度数三字段——YawPitchRollProps 的
//     number 形态，dump/ViewStateProps 实态；AngleProps 对象形 {radians/degrees}
//     登记未移植——无消费面）；
//   - toMatrix3d（:167-203 公式）。
// 未移植（登记——无消费面）：createFromMatrix3d / tryFromTransform /
// createFromRotMatrix / maxAbsDegrees / sumSquaredDegrees / isIdentity /
// freeze 族。
#pragma once

#include "Export.h"

#include "Angle.h"
#include "Matrix3d.h"

BEGIN_DQ_GEOM_NAMESPACE

/// YawPitchRollAngles — three angles (yaw about Z, pitch about Y, roll about X)
/// expanding to a rigid rotation matrix.
/// Ported from: itwinjs-core YawPitchRollAngles (YawPitchRollAngles.ts:53).
struct DQ_GEOM_EXPORT YawPitchRollAngles {
    Angle yaw;    // ← YawPitchRollAngles.yaw（绕 Z）
    Angle pitch;  // ← .pitch（绕 Y——toMatrix3d 内取负号，顺时针）
    Angle roll;   // ← .roll（绕 X）

    /// Ported from: YawPitchRollAngles.createDegrees (YawPitchRollAngles.ts:69-75)。
    static YawPitchRollAngles CreateDegrees(double yawDegrees, double pitchDegrees,
                                            double rollDegrees) noexcept
    {
        return YawPitchRollAngles{Angle::FromDegrees(yawDegrees),
                                  Angle::FromDegrees(pitchDegrees),
                                  Angle::FromDegrees(rollDegrees)};
    }

    /// Ported from: YawPitchRollAngles.fromJSON (YawPitchRollAngles.ts:105-111) —
    /// 数值度数形态（Angle.fromJSON 的 number=degrees 分支，Angle.ts）；
    /// 缺省字段 = 0（:106 json??{} + Angle.fromJSON(undefined)=0 语义由调用侧
    /// 默认实参承担）。
    static YawPitchRollAngles FromJsonDegrees(double yawDegrees = 0.0,
                                              double pitchDegrees = 0.0,
                                              double rollDegrees = 0.0) noexcept
    {
        return CreateDegrees(yawDegrees, pitchDegrees, rollDegrees);
    }

    /// Ported from: YawPitchRollAngles.toMatrix3d (YawPitchRollAngles.ts:167-203) —
    /// axis order XYZ (RPY)，rZ*rY*rX，pitch 取负（y 旋转顺时针，:194-196 注释）。
    [[nodiscard]] Matrix3d ToMatrix3d() const
    {
        double const cz = std::cos(yaw.Radians());
        double const sz = std::sin(yaw.Radians());
        double const cy = std::cos(pitch.Radians());
        double const sy = std::sin(pitch.Radians());
        double const cx = std::cos(roll.Radians());
        double const sx = std::sin(roll.Radians());
        // :197-203 createRowValues（逐行 1:1）。
        return Matrix3d::CreateRowValues(
            cz * cy, -(sz * cx + cz * sy * sx), (sz * sx - cz * sy * cx),
            sz * cy, (cz * cx - sz * sy * sx), -(cz * sx + sz * sy * cx),
            sy, cy * sx, cy * cx);
    }
};

END_DQ_GEOM_NAMESPACE
