// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GraphicTemplate abstract interface (type surface only).
//
// Ported from: itwinjs-core core/frontend/src/render/GraphicTemplate.ts
//
// A GraphicTemplate is a reusable representation of a RenderGraphic. It holds
// all the WebGL resources needed to render the graphics, so it can be reused
// many times without allocating additional GPU resources. Its primary use is
// instanced rendering.
#pragma once

#include "Export.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#endif
#ifndef END_DQ_RENDER_NAMESPACE
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Abstract interface for a reusable representation of a RenderGraphic.
// Ported from: itwinjs-core GraphicTemplate (beta interface)
//
// Concrete template nodes/batches/branches are internal to itwinjs
// (GraphicTemplateImpl); DanQing surfaces only the public interface fields:
//   - isInstanceable (public)
//   - internal node/batch/branch accessors are intentionally NOT exposed
//     (they use Symbol-based branding, which has no C++ equivalent; §6 — no
//     invention). Concrete impl in src/ will hold them privately.
class DQ_RENDER_EXPORT GraphicTemplate {
public:
    virtual ~GraphicTemplate() = default;

    // Whether the graphics in this template can be instanced. Non-instanceable
    // graphics include those produced from glTF models that already contain
    // instanced geometry, and view-independent geometry created from a
    // GraphicBuilder. createGraphicFromTemplate will assert/BeAssert if you
    // attempt to instance a non-instanceable template.
    // Ported from: itwinjs-core GraphicTemplate.isInstanceable
    virtual bool isInstanceable() const noexcept = 0;
};

END_DQ_RENDER_NAMESPACE
