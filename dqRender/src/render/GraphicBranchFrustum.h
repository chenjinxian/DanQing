// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Graphic branch frustum utilities
// Ported from: itwinjs-core core/frontend/src/internal/render/GraphicBranchFrustum.ts
//
// Frustum-related utilities for graphic branches.
#pragma once

#include "Matrix.h"

#include <array>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GraphicBranchFrustum — frustum utilities for branches
// (Ported from: itwinjs-core GraphicBranchFrustum.ts)
// ---------------------------------------------------------------------------
class GraphicBranchFrustum {
public:
    /// Compute the frustum scale from a branch transform.
    /// The frustum scale is used to adjust line weights and point sizes
    /// when rendering through a branch with a non-identity transform.
    static void computeFrustumScale(float const* branchTransform, float& scaleX, float& scaleY)
    {
        // Extract the scale factors from the transform
        // The scale is the length of the first two column vectors
        float cx = branchTransform[0];
        float cy = branchTransform[1];
        float cz = branchTransform[2];
        scaleX = std::sqrt(cx * cx + cy * cy + cz * cz);

        cx = branchTransform[4];
        cy = branchTransform[5];
        cz = branchTransform[6];
        scaleY = std::sqrt(cx * cx + cy * cy + cz * cz);
    }

    /// multiply two transforms (result = a * b).
    static void multiplyTransforms(float const* a, float const* b, float* result)
    {
        auto r = Matrix4::multiply(
            *reinterpret_cast<Matrix4 const*>(a),
            *reinterpret_cast<Matrix4 const*>(b));
        std::memcpy(result, r.ptr(), 16 * sizeof(float));
    }
};

END_DQ_RENDER_NAMESPACE
