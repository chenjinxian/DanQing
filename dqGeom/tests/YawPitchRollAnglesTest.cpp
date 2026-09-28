// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — YawPitchRollAngles tests
// Ported from: itwinjs-core core/geometry/src/test/geometry3d/YawPitchRollAngles.test.ts

#include <gtest/gtest.h>

#include <dqGeom/YawPitchRollAngles.h>

#include <cmath>

namespace {

constexpr double kTol = 1.0e-14;

void expectMatrixAlmostEqual(dqGeom::Matrix3d const& actual,
                             dqGeom::Matrix3d const& expected, double tol = kTol)
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            EXPECT_NEAR(actual.coffs[static_cast<size_t>(i * 3 + j)],
                        expected.coffs[static_cast<size_t>(i * 3 + j)], tol)
                << "matrix differs at (" << i << "," << j << ")";
}

}  // namespace

// Ported from: YawPitchRollAngles.test.ts "Degrees" (YawPitchRollAngles.test.ts:34-72
//              — the toMatrix3d half: matrix == rZ(yaw) * rY(-pitch) * rX(roll)
//              composed from createRotationAroundAxisIndex 2/1/0, pitch negated).
TEST(YawPitchRollAnglesTest, ToMatrix3dMatchesAxisProduct)
{
    // 参考用例的度数三元组子集（Vector3d x=roll, y=pitch, z=yaw——:36-40 表）。
    struct Case {
        double roll, pitch, yaw;
    };
    const Case cases[] = {
        {10.0, 0.0, 0.0},
        {0.0, 10.0, 0.0},
        {0.0, 0.0, 10.0},
        {0.0, 10.0, 20.0},
        {10.0, 20.0, 30.0},
        {0.0, 0.0, 90.0},
    };
    for (auto const& c : cases) {
        auto const ypr = dqGeom::YawPitchRollAngles::CreateDegrees(c.yaw, c.pitch, c.roll);
        // 参考对拍基准（:42-46）：yawMatrix = axis2(yaw)、pitchMatrix = axis1(-pitch)、
        // rollMatrix = axis0(roll)，product = yaw * pitch * roll。
        auto const yawMatrix =
            dqGeom::Matrix3d::CreateRotationAroundZ(dqGeom::Angle::DegreesToRadians(c.yaw));
        auto const pitchMatrix =
            dqGeom::Matrix3d::CreateRotationAroundY(dqGeom::Angle::DegreesToRadians(-c.pitch));
        auto const rollMatrix =
            dqGeom::Matrix3d::CreateRotationAroundX(dqGeom::Angle::DegreesToRadians(c.roll));
        auto const product = yawMatrix.MultiplyMatrix(pitchMatrix.MultiplyMatrix(rollMatrix));

        expectMatrixAlmostEqual(ypr.ToMatrix3d(), product);
    }
}

// Ported from: YawPitchRollAngles.test.ts "json" (:99-113 — fromJSON of the numeric
//              YawPitchRollProps reproduces createDegrees; setFromJSON equal too).
TEST(YawPitchRollAnglesTest, FromJsonDegreesMatchesCreateDegrees)
{
    auto const ypr0 = dqGeom::YawPitchRollAngles::CreateDegrees(10.0, 20.0, 30.0);
    auto const ypr1 = dqGeom::YawPitchRollAngles::FromJsonDegrees(10.0, 20.0, 30.0);
    expectMatrixAlmostEqual(ypr0.ToMatrix3d(), ypr1.ToMatrix3d());
}

// ---------------------------------------------------------------------------
// dump 钉值（Authored 补充——无参考对应：两模型 saved ViewState 的 angles 段
// （imodel.json viewDefinitionProps.angles——采集 RPC 载荷原样）经
// YawPitchRollAngles → rotation 矩阵的逐分量钉死；ViewState3d 构造的
// `rotation = YawPitchRollAngles.fromJSON(props.angles).toMatrix3d()`
// （ViewState.ts:1502）输入面。矩阵期望值 = 参考 toMatrix3d 公式的离线复算
// （YawPitchRollAngles.ts:197-203）。
// ---------------------------------------------------------------------------
// Authored: no reference test exists for these exact captured angle triples.
TEST(YawPitchRollAnglesTest, Instances60SavedAnglesProduceCapturedRotation)
{
    // instances60-imodel-v1/imodel.json defaultViewState.viewDefinitionProps.angles。
    auto const ypr = dqGeom::YawPitchRollAngles::FromJsonDegrees(
        /*yaw=*/-5.836131458448762, /*pitch=*/-160.98686924400624,
        /*roll=*/-107.4190033527506);
    auto const m = ypr.ToMatrix3d();
    auto const expected = dqGeom::Matrix3d::CreateRowValues(
        -0.94054349819232985, -0.33967326654909852, 0.0,
        0.096136201592062182, -0.26619780905027829, 0.95911237985977538,
        -0.32578483505464967, 0.90208691291288379, 0.28302551616368093);
    expectMatrixAlmostEqual(m, expected);
}

// Authored: 同上（joeshouse-v1/imodel.json——标准等轴测：分量 = ±√2/2 / ±1/√6 / ±1/√3）。
TEST(YawPitchRollAnglesTest, JoesHouseSavedAnglesProduceIsometricRotation)
{
    auto const ypr = dqGeom::YawPitchRollAngles::FromJsonDegrees(
        /*yaw=*/29.999999999999932, /*pitch=*/-35.264389682754675,
        /*roll=*/-45.00000000000011);
    auto const m = ypr.ToMatrix3d();
    auto const expected = dqGeom::Matrix3d::CreateRowValues(
        0.70710678118654779, -0.70710678118654724, -1.1102230246251565e-16,
        0.40824829046386207, 0.40824829046386224, 0.81649658092772692,
        -0.57735026918962606, -0.57735026918962662, 0.57735026918962451);
    expectMatrixAlmostEqual(m, expected);
}
