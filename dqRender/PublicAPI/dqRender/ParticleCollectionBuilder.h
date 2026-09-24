// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Particle collection builder
//
// Ported from: itwinjs-core core/frontend/src/render/ParticleCollectionBuilder.ts
// Builder for creating particle effect graphics from individual particle definitions.
#pragma once

#include "Export.h"
#include "RenderGraphic.h"
#include "rhi/Handle.h"

#include <cstdint>

BEGIN_DQ_RENDER_NAMESPACE

// Properties for a single particle.
// Ported from: itwinjs-core ParticleProps
struct DQ_RENDER_EXPORT ParticleProps {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float width = 1.0f;
    float height = 1.0f;
    float transparency = 0.0f;
    float rotationMatrix[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
};

// Parameters for creating a ParticleCollectionBuilder.
// Ported from: itwinjs-core ParticleCollectionBuilder constructor params
struct DQ_RENDER_EXPORT ParticleCollectionBuilderParams {
    rhi::TextureHandle texture;
    float width = 1.0f;
    float height = 1.0f;
    float transparency = 0.0f;
};

// Builder for creating particle effect graphics.
// Ported from: itwinjs-core ParticleCollectionBuilder
class DQ_RENDER_EXPORT ParticleCollectionBuilder {
public:
    virtual ~ParticleCollectionBuilder() = default;

    // add a single particle to the collection.
    virtual void addParticle(ParticleProps const& particle) = 0;

    // finish building and return the resulting graphic. Caller owns the returned pointer.
    virtual RenderGraphic* finish() = 0;

    // Create a new builder with the given parameters.
    static ParticleCollectionBuilder* create(ParticleCollectionBuilderParams const& params);
};

END_DQ_RENDER_NAMESPACE
