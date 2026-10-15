// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Oct-encoded normal
// Ported from: itwinjs-core core/common/src/OctEncodedNormal.ts
//
// 格式 = 参考 16 位（2×8bit：`clampUint8(ry) << 8 | clampUint8(rx)`，OctEncodedNormal.ts:62）。
// M-W W-a3 前置归位（主会话裁决 2026-10-15）：本类型一度分叉为 32 位（2×16bit，
// [0,65535]/分量）——与 DanQing 自家 GLSL octDecodeNormal（/255.0 参考语义，
// SurfaceNormalShaders.h:24）及 imdl 瓦内法线字节格式（参考 16 位）双失配；
// 首个真实消费者 = AuxChannelTable.addNormals（AuxChannelTable.ts:272 setUint16）。
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <dqGeom/Vector3d.h>

#include <cmath>
#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

namespace detail {
// Ported from: OctEncodedNormal.ts:26-28 signNotZero。
inline double octSignNotZero(double val) noexcept
{
    return val < 0.0 ? -1.0 : 1.0;
}
// Ported from: OctEncodedNormal.ts:17-19 clampUint8（经 :13-15 clamp + :21-24
// roundUint16 的 Uint16Array 截断语义——C++ static_cast<uint16_t> 截断同构）。
inline uint32_t octClampUint8(double val) noexcept
{
    double const clamped = val < -1.0 ? -1.0 : (val > 1.0 ? 1.0 : val);
    return static_cast<uint16_t>(0.5 + (clamped * 0.5 + 0.5) * 255.0);
}
}  // namespace detail

// Oct-encoded normal vector (packs a unit vector into 16 bits——参考 :30-33
// "compressed into a single 16-bit integer using oct-encoding")。
// Ported from: itwinjs-core OctEncodedNormal
class DQ_COMMON_EXPORT OctEncodedNormal {
public:
    // The encoded normal（16 位值；uint32_t 承载——参考 value 为 JS number）。
    uint32_t value = 0;

    OctEncodedNormal() noexcept = default;
    // Ported from: OctEncodedNormal.ts:42-44（参考构造经 roundUint16——截断 mod 2^16；
    // 合法编码恒 < 65536，直通同构）。
    explicit OctEncodedNormal(uint32_t v) noexcept : value(v) {}

    // Compute the encoded 16-bit value of the supplied normalized vector.
    // Ported from: itwinjs-core OctEncodedNormal.encode()
    static uint32_t encode(const dqGeom::Vector3d& vec) noexcept
    {
        return encodeXYZ(vec.x, vec.y, vec.z);
    }

    // Ported from: OctEncodedNormal.ts:52-63 encodeXYZ——L1 投影（denom=|x|+|y|+|z|）+
    // nz<0 折叠（折叠用折叠前原值：参考 :57-60 先存 x=rx/y=ry 再覆写）+ clampUint8 打包。
    static uint32_t encodeXYZ(double nx, double ny, double nz) noexcept
    {
        const double denom = std::abs(nx) + std::abs(ny) + std::abs(nz);
        double rx = nx / denom;
        double ry = ny / denom;
        if (nz < 0.0) {
            const double x = rx;
            const double y = ry;
            rx = (1.0 - std::abs(y)) * detail::octSignNotZero(x);
            ry = (1.0 - std::abs(x)) * detail::octSignNotZero(y);
        }
        return detail::octClampUint8(ry) << 8 | detail::octClampUint8(rx);
    }

    // Create an OctEncodedNormal from a normalized vector.
    // Ported from: itwinjs-core OctEncodedNormal.fromVector()
    static OctEncodedNormal fromVector(const dqGeom::Vector3d& vec) noexcept
    {
        return OctEncodedNormal(encode(vec));
    }

    // Decode back to a unit vector.
    // Ported from: itwinjs-core OctEncodedNormal.decode()
    dqGeom::Vector3d decode() const noexcept
    {
        return decodeValue(value);
    }

    // Ported from: OctEncodedNormal.ts:76-101 decodeValue——ex=val&0xff、ey=val>>8、
    // /255 映射、ez=1-(|ex|+|ey|)、z<0 折叠（折叠用折叠前原值）、normalizeInPlace。
    static dqGeom::Vector3d decodeValue(uint32_t val) noexcept
    {
        double const ex = static_cast<double>(val & 0xFF) / 255.0 * 2.0 - 1.0;
        double const ey = static_cast<double>((val >> 8) & 0xFF) / 255.0 * 2.0 - 1.0;
        double const ez = 1.0 - (std::abs(ex) + std::abs(ey));

        double nx = ex;
        double ny = ey;
        double nz = ez;
        if (nz < 0.0) {
            const double x = nx;
            const double y = ny;
            nx = (1.0 - std::abs(y)) * detail::octSignNotZero(x);
            ny = (1.0 - std::abs(x)) * detail::octSignNotZero(y);
        }

        // normalizeInPlace（Vector3d.ts——零向量回落 (0,0,1) 与 DanQing Normalize 同族；
        // 合法编码解码后恒非零）。
        auto v = dqGeom::Vector3d::From(nx, ny, nz);
        v.Normalize();
        return v;
    }

    bool operator==(const OctEncodedNormal& rhs) const noexcept { return value == rhs.value; }
    bool operator!=(const OctEncodedNormal& rhs) const noexcept { return value != rhs.value; }
};

END_DQ_COMMON_NAMESPACE
