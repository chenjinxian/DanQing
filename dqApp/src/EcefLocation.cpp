// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — EcefLocation implementation
// Ported from: itwinjs-core core/common/src/geometry/EcefLocation.ts
#include "dqApp/EcefLocation.h"

namespace dqApp {

EcefLocation EcefLocation::CreateFromCartographicOrigin(const dqCommon::Cartographic& origin)
{
    // Ported from: itwinjs-core EcefLocation.createFromCartographicOrigin
    //              (core/common/src/IModel.ts:299-320, angle/point 可选参数未移植——
    //               无调用方使用；TODO 随调用方出现再补)。
    // 采样构造（替代旧解析切平面公式）：在东西/南北各 10 m 采样 ECEF 差分并
    // 刚化（createRigidFromColumns）——参考的确定性微小倾斜是 BackgroundMapGeometry
    // 椭球剪影平面朝向判定（BackgroundMapGeometry.ts:360-363）稳定符号的来源。
    dqGeom::Point3d ecefOrigin = origin.toEcef();

    // Constant.earthRadiusWGS84.polar (core/geometry/src/Constant.ts:28-31)
    constexpr double kEarthRadiusWGS84Polar = 6356752.3142;
    double const deltaRadians = 10.0 / kEarthRadiusWGS84Polar;
    auto const northCarto = dqCommon::Cartographic::fromRadians(
        origin.longitude, origin.latitude + deltaRadians, origin.height);
    auto const eastCarto = dqCommon::Cartographic::fromRadians(
        origin.longitude + deltaRadians, origin.latitude, origin.height);
    dqGeom::Point3d const ecefNorth = northCarto.toEcef();
    dqGeom::Point3d const ecefEast = eastCarto.toEcef();

    // expectDefined(Vector3d.createStartEnd(ecefOrigin, ecefEast).normalize())
    dqGeom::Vector3d xVector = dqGeom::Vector3d::FromStartEnd(ecefOrigin, ecefEast);
    xVector.Normalize();
    dqGeom::Vector3d yVector = dqGeom::Vector3d::FromStartEnd(ecefOrigin, ecefNorth);
    yVector.Normalize();

    // expectDefined(Matrix3d.createRigidFromColumns(xVector, yVector, AxisOrder.XYZ))
    auto matrix = dqGeom::Matrix3d::CreateRigidFromColumns(xVector, yVector, dqGeom::AxisOrder::XYZ);
    return EcefLocation(ecefOrigin, matrix.value_or(dqGeom::Matrix3d::CreateIdentity()));
}

EcefLocation EcefLocation::FromProps(const EcefLocationProps& props)
{
    EcefLocation loc;
    loc.origin = props.origin;
    // TODO: compute orientation from yaw/pitch/distance when needed
    return loc;
}

EcefLocationProps EcefLocation::ToProps() const
{
    EcefLocationProps props;
    props.origin = origin;
    return props;
}

}  // namespace dqApp
