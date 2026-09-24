// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderGraphic abstract base class
//
// Ported from: itwinjs-core core/frontend/src/render/RenderGraphic.ts
// Abstract representation of an object which can be rendered.
#pragma once

#include "Export.h"
#include "RenderMemory.h"

#include <dqGeom/Range3d.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

#include <cstdint>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

// Abstract representation of a renderable object.
// Ported from: itwinjs-core RenderGraphic
class DQ_RENDER_EXPORT RenderGraphic {
public:
    virtual ~RenderGraphic() = default;

    // Extend range to include this graphic's bounding box.
    virtual void unionRange(dqGeom::Range3d& range) const = 0;

    // Record the graphics memory consumed by this graphic.
    // Ported from: itwinjs-core RenderGraphic.collectStatistics
    // (RenderGraphic.ts:52 — abstract; concrete subclasses add their
    // texture/buffer bytes). Default no-op: subclasses without byte
    // accounting report nothing (same as an uninstrumented reference graphic).
    virtual void collectStatistics(RenderMemory::Statistics& stats) const { (void)stats; }

    // Type identification for RenderGraphicAdapter (no RTTI).
    // Returns true if this is a GraphicBranch (public API scene graph node).
    // Internal Graphics (Primitive, CachedGeometry, etc.) return false.
    // Ported from: itwinjs-core uses instanceof checks; we use virtual dispatch.
    virtual bool isBranch() const noexcept { return false; }
};

// An array of RenderGraphics.
// Ported from: itwinjs-core GraphicList
using GraphicList = std::vector<RenderGraphic*>;

// A graphic that owns another graphic (prevents auto-disposal).
// Ported from: itwinjs-core RenderGraphicOwner
//
// When the Viewport changes decorations, old graphics are disposed.
// A RenderGraphicOwner wraps a graphic so it survives decoration changes.
// The caller must explicitly call disposeGraphic() to release the owned graphic.
class DQ_RENDER_EXPORT RenderGraphicOwner : public RenderGraphic {
public:
    explicit RenderGraphicOwner(RenderGraphic* owned) : m_graphic(owned) {}

    // The owned graphic.
    RenderGraphic* graphic() const noexcept { return m_graphic; }

    // Does nothing on destruction — caller must dispose manually via disposeGraphic().
    // ← itwinjs-core: dispose() is a no-op
    ~RenderGraphicOwner() override = default;

    // Dispose the owned graphic.
    // ← itwinjs-core: disposeGraphic() calls this.graphic[Symbol.dispose]()
    void disposeGraphic()
    {
        delete m_graphic;
        m_graphic = nullptr;
    }

    // Does nothing — caller must dispose manually.
    void unionRange(dqGeom::Range3d& range) const override
    {
        if (m_graphic)
            m_graphic->unionRange(range);
    }

private:
    RenderGraphic* m_graphic;
};

END_DQ_RENDER_NAMESPACE
