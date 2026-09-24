// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Screen-space post-processing effect builder
//
// Ported from: itwinjs-core core/frontend/src/render/ScreenSpaceEffectBuilder.ts
// Builder for creating custom screen-space (post-processing) shader effects.
#pragma once

#include "Export.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Type of a uniform variable in a screen-space effect.
// Ported from: itwinjs-core UniformType
enum class UniformType : uint8_t {
    Bool,
    Int,
    Float,
    Vec2,
    Vec3,
    Vec4,
};

// Type of a varying variable in a screen-space effect.
// Ported from: itwinjs-core VaryingType
enum class VaryingType : uint8_t {
    Float,
    Vec2,
    Vec3,
    Vec4,
};

// GLSL source code for a screen-space effect.
// Ported from: itwinjs-core ScreenSpaceEffectSource
struct DQ_RENDER_EXPORT ScreenSpaceEffectSource {
    const char* vertex = nullptr;
    const char* fragment = nullptr;
    const char* sampleSourcePixel = nullptr;
};

// Parameters for creating a ScreenSpaceEffectBuilder.
// Ported from: itwinjs-core ScreenSpaceEffectBuilder constructor params
struct DQ_RENDER_EXPORT ScreenSpaceEffectBuilderParams {
    const char* name = nullptr;
    bool textureCoordFromPosition = false;
    ScreenSpaceEffectSource source;
};

// Builder for creating custom screen-space post-processing effects.
// Ported from: itwinjs-core ScreenSpaceEffectBuilder
class DQ_RENDER_EXPORT ScreenSpaceEffectBuilder {
public:
    virtual ~ScreenSpaceEffectBuilder() = default;

    // add a uniform variable to the effect.
    virtual void addUniform(const char* name, UniformType type) = 0;

    // add a varying variable to the effect.
    virtual void addVarying(const char* name, VaryingType type) = 0;

    // Finish building the effect. Must be called after all uniforms/varyings are added.
    virtual void finish() = 0;

    // Create a new builder with the given parameters.
    static ScreenSpaceEffectBuilder* create(ScreenSpaceEffectBuilderParams const& params);
};

END_DQ_RENDER_NAMESPACE
