// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — AuxData implementation
// Ported from: itwinjs-core core/geometry/src/polyface/AuxData.ts
#include "dqGeom/AuxData.h"

#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

namespace {

// Ported from: AuxData.ts:295-304 transformPoints（文件局部自由函数）——
// 逐 (x,y,z) 三元组读出为 Point3d、经 transform 回调改写、写回。
template<typename F>
void transformPoints(std::vector<double>& coords, F&& transform)
{
    Point3d point;
    for (size_t i = 0; i + 2 < coords.size(); i += 3) {
        point.x = coords[i];
        point.y = coords[i + 1];
        point.z = coords[i + 2];
        transform(point);
        coords[i] = point.x;
        coords[i + 1] = point.y;
        coords[i + 2] = point.z;
    }
}

// Ported from: PointHelpers.ts NumberArray.isAlmostEqual (:72-85) 的 number[] 形态——
// 长度相等 + 逐分量 |a-b| <= tolerance。
bool numberArrayIsAlmostEqual(std::vector<double> const& dataA, std::vector<double> const& dataB,
                              double tolerance)
{
    if (dataA.size() != dataB.size())
        return false;
    for (size_t i = 0; i < dataA.size(); i++)
        if (std::fabs(dataA[i] - dataB[i]) > tolerance)
            return false;
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// AuxChannelData
// ---------------------------------------------------------------------------

// Ported from: AuxData.ts:60-63 copyValues.
void AuxChannelData::copyValues(AuxChannelData const& other, size_t thisIndex, size_t otherIndex,
                                size_t blockSize)
{
    for (size_t i = 0; i < blockSize; i++)
        values[thisIndex * blockSize + i] = other.values[otherIndex * blockSize + i];
}

// Ported from: AuxData.ts:73-76 isAlmostEqual（默认容差 1.0e-8；
// `tol ? tol : 1.0E-8` 的 truthiness 怪癖原样——tol==0 时回落默认）。
bool AuxChannelData::isAlmostEqual(AuxChannelData const& other, double tol) const
{
    double const tolerance = tol != 0.0 ? tol : 1.0E-8;
    return std::fabs(input - other.input) < tolerance
        && numberArrayIsAlmostEqual(values, other.values, tolerance);
}

// ---------------------------------------------------------------------------
// AuxChannel
// ---------------------------------------------------------------------------

// Ported from: AuxData.ts:108-120 isAlmostEqual.
bool AuxChannel::isAlmostEqual(AuxChannel const& other, double tol) const
{
    if (dataType != other.dataType || name != other.name || inputName != other.inputName
        || data.size() != other.data.size())
        return false;

    for (size_t i = 0; i < data.size(); i++)
        if (!data[i].isAlmostEqual(other.data[i], tol))
            return false;

    return true;
}

// Ported from: AuxData.ts:148-157 scalarRange（非 scalar → nullopt；
// Range1d.createNull + 逐 data.values extendArray）。
std::optional<Range1d> AuxChannel::scalarRange() const
{
    if (!isScalar())
        return std::nullopt;

    Range1d range = Range1d::CreateNull();
    for (auto const& d : data)
        for (double v : d.values)
            range.Extend(v);

    return range;
}

// Ported from: AuxData.ts:164-176 computeDisplacementRange.
Range3d AuxChannel::computeDisplacementRange(double scale, Range3d* result) const
{
    Range3d local;
    Range3d& r = result != nullptr ? *result : local;
    r.SetNull();

    if (AuxChannelDataType::Vector == dataType) {
        for (auto const& d : data) {
            auto const& v = d.values;
            for (size_t i = 0; i + 2 < v.size(); i += 3)
                r.ExtendPoint(Point3d::From(v[i] * scale, v[i + 1] * scale, v[i + 2] * scale));
        }
    }

    return r;
}

// ---------------------------------------------------------------------------
// PolyfaceAuxData
// ---------------------------------------------------------------------------

// Ported from: AuxData.ts:212-221 isAlmostEqual（indices 精确相等 + channels 容差）。
bool PolyfaceAuxData::isAlmostEqual(PolyfaceAuxData const& other, double tol) const
{
    if (indices != other.indices || channels.size() != other.channels.size())
        return false;

    for (size_t i = 0; i < channels.size(); i++)
        if (!channels[i].isAlmostEqual(other.channels[i], tol))
            return false;

    return true;
}

// Ported from: AuxData.ts:233-245 createForVisitor（值数组留空骨架）。
PolyfaceAuxData PolyfaceAuxData::createForVisitor() const
{
    std::vector<AuxChannel> visitorChannels;

    for (auto const& parentChannel : channels) {
        std::vector<AuxChannelData> visitorChannelData;
        for (auto const& parentChannelData : parentChannel.data)
            visitorChannelData.emplace_back(parentChannelData.input, std::vector<double>{});

        visitorChannels.emplace_back(std::move(visitorChannelData), parentChannel.dataType,
                                     parentChannel.name, parentChannel.inputName);
    }

    return PolyfaceAuxData(std::move(visitorChannels), {});
}

// Ported from: AuxData.ts:252-292 tryTransformInPlace。
// scale = Math.pow(Math.abs(det), 1/3) * (det >= 0 ? 1 : -1)（:256 原样 pow 非 cbrt）；
// Normal 型：inverseRot 懒建（:268）、multiplyTransposeVectorInPlace（:273）、
// 重规范化 tol 1.0e-15 怪癖（:275-279——仅当 magnitude 非近 0 且非近 1 才规范化）；
// 不可逆 → return false（:269-270）。
bool PolyfaceAuxData::tryTransformInPlace(Transform const& transform)
{
    std::optional<Matrix3d> inverseRot;
    Matrix3d const& rot = transform.GetMatrix();
    double const det = rot.Determinant();
    double const scale = std::pow(std::fabs(det), 1.0 / 3.0) * (det >= 0 ? 1.0 : -1.0);
    for (auto& channel : channels) {
        for (auto& data : channel.data) {
            switch (channel.dataType) {
                case AuxChannelDataType::Scalar:
                    continue;
                case AuxChannelDataType::Distance: {
                    for (size_t i = 0; i < data.values.size(); i++)
                        data.values[i] *= scale;
                    break;
                }
                case AuxChannelDataType::Normal: {
                    if (!inverseRot.has_value()) {
                        Matrix3d inv;
                        if (!rot.Inverse(inv))
                            return false;
                        inverseRot = inv;
                    }
                    transformPoints(data.values, [&inverseRot](Point3d& point) {
                        // Point3d↔Vector3d 分量直通（参考 XYZ 基类同构——AuxData.ts:273-279）。
                        Vector3d const v = inverseRot->MultiplyTransposeVector(Vector3d::From(point));
                        point.x = v.x;
                        point.y = v.y;
                        point.z = v.z;
                        double const dot = point.MagnitudeSquared();
                        double const tol = 1.0e-15;  // cf. GrowableXYZArray.multiplyAndRenormalizeMatrix3dInverseTransposeInPlace
                        if (dot > tol && std::fabs(dot - 1.0) > tol) {  // only renormalize if magnitude is not near 0 or 1
                            double const mag = 1.0 / std::sqrt(dot);
                            point.x *= mag;
                            point.y *= mag;
                            point.z *= mag;
                        }
                    });
                    break;
                }
                case AuxChannelDataType::Vector: {
                    transformPoints(data.values, [&rot](Point3d& point) {
                        Vector3d const v = rot.MultiplyVector(Vector3d::From(point));
                        point.x = v.x;
                        point.y = v.y;
                        point.z = v.z;
                    });
                    break;
                }
            }
        }
    }

    return true;
}

END_DQ_GEOM_NAMESPACE
