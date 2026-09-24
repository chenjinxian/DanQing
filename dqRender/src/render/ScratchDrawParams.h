// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Scratch draw parameter reuse
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ScratchDrawParams.ts
//
// Provides a recycled singleton DrawParams to avoid per-draw-call allocation.
// A TargetImpl requests a DrawParams via GetDrawParams, uses it for one draw
// call, then returns it via FreeDrawParams.  The next call reuses the same
// object.
#pragma once

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class DrawParams;
class TargetImpl;
class CachedGeometry;

/// Get a scratch DrawParams for a single draw call.
/// The returned pointer is valid until FreeDrawParams is called.
/// @param target   The render target requesting the params.
/// @param geometry The cached geometry to draw.
/// @return Pointer to a recycled DrawParams (never null).
DrawParams* GetDrawParams(TargetImpl* target, CachedGeometry* geometry);

/// Return the scratch DrawParams for reuse.
/// Must be called exactly once after each GetDrawParams call.
void FreeDrawParams();

END_DQ_RENDER_NAMESPACE
