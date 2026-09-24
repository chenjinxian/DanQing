// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Planar texture projection implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/PlanarTextureProjection.ts
#include "PlanarTextureProjection.h"

#include <cmath>
#include <cstring>
#include <algorithm>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// computePlanarTextureProjection
// (Ported from: itwinjs-core PlanarTextureProjection.ts
//   computePlanarTextureProjection)
// ---------------------------------------------------------------------------
PlanarTextureProjection::ProjectionResult
PlanarTextureProjection::computePlanarTextureProjection(
    float const* texturePlaneNormal,
    float const* viewRotation,
    float const* viewFrustumCorners,
    uint32_t textureWidth,
    uint32_t textureHeight)
{
    ProjectionResult result{};

    // Default identity projection.
    std::memset(result.projectionMatrix, 0, sizeof(result.projectionMatrix));
    result.projectionMatrix[0] = 1.0f;
    result.projectionMatrix[5] = 1.0f;
    result.projectionMatrix[10] = 1.0f;
    result.projectionMatrix[15] = 1.0f;

    if (!texturePlaneNormal || !viewRotation || !viewFrustumCorners ||
        textureWidth == 0 || textureHeight == 0)
    {
        return result;
    }

    // Project frustum corners onto the texture plane.
    // For each corner, compute the signed distance to the plane and
    // project onto the plane's local coordinate frame.
    float minU = 1.0e30f;
    float maxU = -1.0e30f;
    float minV = 1.0e30f;
    float maxV = -1.0e30f;
    float minW = 1.0e30f;
    float maxW = -1.0e30f;

    // Derive two tangent vectors from the plane normal.
    // Use the view rotation to define the "right" and "up" directions in the
    // projection plane.
    float const n0 = texturePlaneNormal[0];
    float const n1 = texturePlaneNormal[1];
    float const n2 = texturePlaneNormal[2];

    // Right vector = first row of the view rotation, projected onto the plane.
    float r0 = viewRotation[0];
    float r1 = viewRotation[1];
    float r2 = viewRotation[2];
    float rDotN = r0 * n0 + r1 * n1 + r2 * n2;
    r0 -= rDotN * n0;
    r1 -= rDotN * n1;
    r2 -= rDotN * n2;
    float rLen = std::sqrt(r0 * r0 + r1 * r1 + r2 * r2);
    if (rLen > 1.0e-10f) {
        r0 /= rLen;
        r1 /= rLen;
        r2 /= rLen;
    }

    // Up vector = second row of the view rotation, projected onto the plane.
    float u0 = viewRotation[3];
    float u1 = viewRotation[4];
    float u2 = viewRotation[5];
    float uDotN = u0 * n0 + u1 * n1 + u2 * n2;
    u0 -= uDotN * n0;
    u1 -= uDotN * n1;
    u2 -= uDotN * n2;
    float uLen = std::sqrt(u0 * u0 + u1 * u1 + u2 * u2);
    if (uLen > 1.0e-10f) {
        u0 /= uLen;
        u1 /= uLen;
        u2 /= uLen;
    }

    for (uint32_t i = 0; i < 8; ++i) {
        float const* corner = viewFrustumCorners + i * 3;

        float projU = corner[0] * r0 + corner[1] * r1 + corner[2] * r2;
        float projV = corner[0] * u0 + corner[1] * u1 + corner[2] * u2;
        float projW = corner[0] * n0 + corner[1] * n1 + corner[2] * n2;

        minU = std::min(minU, projU);
        maxU = std::max(maxU, projU);
        minV = std::min(minV, projV);
        maxV = std::max(maxV, projV);
        minW = std::min(minW, projW);
        maxW = std::max(maxW, projW);
    }

    // Frustum bounds.
    result.left = minU;
    result.right = maxU;
    result.bottom = minV;
    result.top = maxV;
    result.near = minW;
    result.far = maxW;

    // Build orthographic projection matrix mapping [minU,maxU] x [minV,maxV]
    // x [minW,maxW] to [-1,1] x [-1,1] x [-1,1].
    float const rangeU = maxU - minU;
    float const rangeV = maxV - minV;
    float const rangeW = maxW - minW;

    float* m = result.projectionMatrix;
    std::memset(m, 0, 16 * sizeof(float));

    if (rangeU > 1.0e-10f && rangeV > 1.0e-10f && rangeW > 1.0e-10f) {
        m[0] = 2.0f / rangeU;
        m[5] = 2.0f / rangeV;
        m[10] = -2.0f / rangeW;  // reverse Z for standard depth convention
        m[12] = -(maxU + minU) / rangeU;
        m[13] = -(maxV + minV) / rangeV;
        m[14] = -(maxW + minW) / rangeW;
        m[15] = 1.0f;
    }

    return result;
}

// ---------------------------------------------------------------------------
// isTileRangeInBounds
// (Ported from: itwinjs-core PlanarTextureProjection.ts
//   isTileRangeInBounds)
// ---------------------------------------------------------------------------
bool PlanarTextureProjection::isTileRangeInBounds(float const* tileRange,
                                                   float const* drapeRange)
{
    if (!tileRange || !drapeRange) return false;

    // tileRange: [minX, minY, minZ, maxX, maxY, maxZ]
    // drapeRange: [minX, minY, minZ, maxX, maxY, maxZ]
    // Overlap test: ranges overlap on all three axes.
    bool overlapX = tileRange[3] >= drapeRange[0] && tileRange[0] <= drapeRange[3];
    bool overlapY = tileRange[4] >= drapeRange[1] && tileRange[1] <= drapeRange[4];
    bool overlapZ = tileRange[5] >= drapeRange[2] && tileRange[2] <= drapeRange[5];

    return overlapX && overlapY && overlapZ;
}

END_DQ_RENDER_NAMESPACE
