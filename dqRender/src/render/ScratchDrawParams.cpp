// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Scratch draw parameter reuse implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ScratchDrawParams.ts
#include "ScratchDrawParams.h"

#include "DrawParams.h"
#include "CachedGeometry.h"
#include "TargetImpl.h"

BEGIN_DQ_RENDER_NAMESPACE

namespace {

// Recycled singleton DrawParams.
DrawParams sScratchDrawParams;
bool sScratchInUse = false;

}  // namespace

// ---------------------------------------------------------------------------
// GetDrawParams
// (Ported from: itwinjs-core ScratchDrawParams.ts getDrawParams)
// ---------------------------------------------------------------------------
DrawParams* GetDrawParams(TargetImpl* target, CachedGeometry* geometry)
{
    // If the scratch params are already in use (reentrant draw), allocate a
    // fresh one.  In practice this should not happen, but we guard against it.
    if (sScratchInUse) {
        // Return a heap-allocated fallback; caller must manage lifetime.
        auto* fallback = new DrawParams();
        fallback->init(nullptr, geometry);
        fallback->setTarget(target);
        return fallback;
    }

    sScratchInUse = true;
    sScratchDrawParams.reset();
    sScratchDrawParams.init(nullptr, geometry);
    sScratchDrawParams.setTarget(target);
    return &sScratchDrawParams;
}

// ---------------------------------------------------------------------------
// FreeDrawParams
// (Ported from: itwinjs-core ScratchDrawParams.ts freeDrawParams)
// ---------------------------------------------------------------------------
void FreeDrawParams()
{
    if (sScratchInUse) {
        sScratchDrawParams.reset();
        sScratchInUse = false;
    }
}

END_DQ_RENDER_NAMESPACE
