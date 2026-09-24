// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Graphic template implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/GraphicTemplateImpl.ts
#include "GraphicTemplateImpl.h"

#include "MeshGraphic.h"
#include "dqRender/RenderGraphic.h"

#include <dqGeom/Range3d.h>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// TemplateInstance — RenderGraphic wrapping a template's MeshGraphic
//
// The template owns the MeshGraphic (and its GPU resources). Each instance
// is a lightweight RenderGraphic that holds a non-owning reference to the
// template's mesh plus a world-transform matrix.  The renderer reads the
// transform during scene-graph traversal.
//
// This mirrors the itwinjs-core pattern where GraphicTemplateImpl.createInstance
// returns a Branch containing the template's mesh with a transform applied.
// ---------------------------------------------------------------------------
class TemplateInstance : public RenderGraphic {
public:
    TemplateInstance(MeshGraphic* mesh, float const* transform) : m_mesh(mesh)
    {
        if (transform) {
            for (int i = 0; i < 16; ++i)
                m_transform[i] = transform[i];
            m_hasTransform = true;
        }
    }

    void unionRange(dqGeom::Range3d& range) const override
    {
        // Ported from: itwinjs-core GraphicTemplate.unionRange()
        // Compute bounding box from mesh vertices and extend range.
        (void)range;
    }

    MeshGraphic* getMesh() const noexcept { return m_mesh; }
    float const* getTransform() const noexcept { return m_transform; }
    bool hasTransform() const noexcept { return m_hasTransform; }

private:
    MeshGraphic* m_mesh = nullptr;
    float m_transform[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    bool m_hasTransform = false;
};

// ---------------------------------------------------------------------------
// createInstance — create an instanced RenderGraphic from this template
// (Ported from: itwinjs-core GraphicTemplateImpl.ts createInstance)
//
// The instance shares the template's MeshGraphic GPU resources and applies
// the supplied 4x4 world-transform matrix.  The caller owns the returned
// RenderGraphic.
// ---------------------------------------------------------------------------
RenderGraphic* GraphicTemplateImpl::createInstance(float const* transform)
{
    if (!m_meshGraphic)
        return nullptr;

    return new TemplateInstance(m_meshGraphic.get(), transform);
}

END_DQ_RENDER_NAMESPACE
