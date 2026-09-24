// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Oct-encoded normal
// Ported from: itwinjs-core core/common/src/OctEncodedNormal.ts
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <dqGeom/Vector3d.h>

#include <cmath>
#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// Oct-encoded normal vector (packs a unit vector into 2x16 bits).
// Ported from: itwinjs-core OctEncodedNormal
class DQ_COMMON_EXPORT OctEncodedNormal {
public:
    uint32_t value = 0;

    OctEncodedNormal() noexcept = default;
    explicit OctEncodedNormal(uint32_t v) noexcept : value(v) {}

    // Encode a unit vector to oct-encoded form.
    // Ported from: itwinjs-core OctEncodedNormal.encode()
    static uint32_t encode(const dqGeom::Vector3d& vec) noexcept
    {
        return encodeXYZ(vec.x, vec.y, vec.z);
    }

    static uint32_t encodeXYZ(double nx, double ny, double nz) noexcept
    {
        // Project onto octahedron
        const double invNorm = 1.0 / (std::abs(nx) + std::abs(ny) + std::abs(nz));
        double ox = nx * invNorm;
        double oy = ny * invNorm;

        // Fold lower hemisphere
        if (nz < 0.0) {
            const double tx = (1.0 - std::abs(oy)) * (ox >= 0.0 ? 1.0 : -1.0);
            const double ty = (1.0 - std::abs(ox)) * (oy >= 0.0 ? 1.0 : -1.0);
            ox = tx;
            oy = ty;
        }

        // Map [-1,1] to [0,65535]
        const auto ix = static_cast<uint32_t>(std::round((ox + 1.0) * 0.5 * 65535.0));
        const auto iy = static_cast<uint32_t>(std::round((oy + 1.0) * 0.5 * 65535.0));

        return ix | (iy << 16);
    }

    // Create from a vector.
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

    static dqGeom::Vector3d decodeValue(uint32_t val) noexcept
    {
        // unpack from [0,65535] to [-1,1]
        const double ix = static_cast<double>(val & 0xFFFF);
        const double iy = static_cast<double>((val >> 16) & 0xFFFF);
        double ox = ix / 65535.0 * 2.0 - 1.0;
        double oy = iy / 65535.0 * 2.0 - 1.0;

        // Unfold lower hemisphere
        double nz = 1.0 - (std::abs(ox) + std::abs(oy));
        if (nz < 0.0) {
            const double tx = (1.0 - std::abs(oy)) * (ox >= 0.0 ? 1.0 : -1.0);
            const double ty = (1.0 - std::abs(ox)) * (oy >= 0.0 ? 1.0 : -1.0);
            ox = tx;
            oy = ty;
        }

        // Normalize
        const double len = std::sqrt(ox * ox + oy * oy + nz * nz);
        if (len > 0.0) {
            return dqGeom::Vector3d::From(ox / len, oy / len, nz / len);
        }
        return dqGeom::Vector3d::From(0, 0, 1);
    }

    bool operator==(const OctEncodedNormal& rhs) const noexcept { return value == rhs.value; }
    bool operator!=(const OctEncodedNormal& rhs) const noexcept { return value != rhs.value; }
};

END_DQ_COMMON_NAMESPACE
