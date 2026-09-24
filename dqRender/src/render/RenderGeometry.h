// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Render geometry abstract interface
// Ported from: itwinjs-core core/frontend/src/internal/render/RenderGeometry.ts
//
// Abstract interface for all renderable geometry.
// Implemented by WebGL geometry types.
#pragma once

#include "TechniqueImpl.h"
#include "CachedGeometry.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// RenderGeometry — abstract render geometry interface
// (Ported from: itwinjs-core RenderGeometry.ts)
// ---------------------------------------------------------------------------
class RenderGeometry {
public:
    virtual ~RenderGeometry() = default;

    /// Get the geometry type name.
    virtual char const* getGeometryType() const = 0;

    /// Get the technique ID for this geometry.
    virtual TechniqueId getTechniqueId() const = 0;

    /// Get the render pass for this geometry.
    virtual Pass getPass() const = 0;

    /// Check if this geometry is disposed.
    virtual bool isDisposed() const = 0;

    /// Get the underlying CachedGeometry (if any).
    virtual CachedGeometry* asCachedGeometry() { return nullptr; }
};

END_DQ_RENDER_NAMESPACE
