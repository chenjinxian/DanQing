// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Animation parameters
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Animation.ts
//
// Parameters for vertex animation displacement.
// Animation data is stored in textures and applied in the vertex shader.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// AnimationParams — vertex animation displacement parameters
// ---------------------------------------------------------------------------
struct AnimationParams {
    // Displacement texture parameters
    float dispParams[4] = {0.0f, 0.0f, 0.0f, 0.0f};   // (offset, scale, ...)
    float normalParams[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // normal displacement
    float scalarParams[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // scalar displacement
    float scalarQParams[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // quantized scalar

    bool isActive() const noexcept
    {
        return dispParams[0] != 0.0f || dispParams[1] != 0.0f ||
               normalParams[0] != 0.0f || normalParams[1] != 0.0f;
    }
};

END_DQ_RENDER_NAMESPACE
