// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — TextString implementation
// Ported from: itwinjs-core core/common/src/geometry/TextString.ts
#include <dqCommon/TextString.h>

namespace dqCommon {

// Ported from: itwinjs-core TextString.toJSON (TextString.ts:88-105)
TextStringProps TextString::toJSON() const
{
    TextStringProps props;
    props.text = text;
    props.font = font;
    props.height = height;
    props.widthFactor = widthFactor;
    props.bold = bold;
    props.italic = italic;
    props.underline = underline;

    // if (!this.origin.isAlmostZero) props.origin = this.origin.toJSON();
    // isAlmostZero = Geometry.smallMetricDistance 距原点判定（Point3d.ts）。
    if (!origin.AlmostEqual(dqGeom::Point3d::FromZero(), 1.0e-6))
        props.origin = origin;

    // if (!this.rotation.isIdentity()) props.rotation = this.rotation.toJSON();
    if (!rotation.isIdentity())
        props.rotation = rotation;

    return props;
}

// Ported from: itwinjs-core TextString.transformInPlace (TextString.ts:107-126)
bool TextString::transformInPlace(dqGeom::Transform const& transform)
{
    dqGeom::Point3d const newOrigin = transform.MultiplyPoint3d(origin);
    dqGeom::Transform const newTransform =
        rotation.ToMatrix3d().multiplyMatrixTransform(transform);
    // newTransform.matrix.normalizeColumnsInPlace(scales)——矩阵按值取副本变换
    // （GetMatrix() 返回 const&；TS 原地语义等价）。
    dqGeom::Matrix3d mtx = newTransform.GetMatrix();
    dqGeom::Vector3d scales;
    if (!mtx.normalizeColumnsInPlace(&scales))
        return false;
    std::optional<dqGeom::YawPitchRollAngles> const newRotation =
        dqGeom::YawPitchRollAngles::CreateFromMatrix3d(mtx);
    if (!newRotation)
        return false;
    double const newHeight = height * scales.y;
    double const newWidth = width() * scales.x;
    if (newHeight < 1.0e-10 || newWidth < 1.0e-10)
        return false;
    origin = newOrigin;
    rotation = *newRotation;
    height = newHeight;
    // this.widthFactor = (newHeight === newWidth ? undefined : (newWidth / newHeight));
    widthFactor = (newHeight == newWidth) ? std::optional<double>()
                                           : std::optional<double>(newWidth / newHeight);
    return true;
}

}  // namespace dqCommon
