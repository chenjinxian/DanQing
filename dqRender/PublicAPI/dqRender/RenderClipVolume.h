// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderClipVolume abstract base (type surface only).
//
// Ported from: itwinjs-core core/frontend/src/render/RenderClipVolume.ts
//
// A RenderClipVolume is an opaque representation of a clip volume applied to
// geometry within a Viewport. It is created from a ClipVector and takes
// ownership of that ClipVector (which must not be modified while referenced).
#pragma once

#include "Export.h"

#include <dqBase/DqTypes.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#endif
#ifndef END_DQ_RENDER_NAMESPACE
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Forward declaration — ClipVector is part of core-geometry, not yet ported
// into dqGeom. Faithful opaque-string placeholder (§6: no half-impl).
// TODO: ClipVector not yet ported from core-geometry into dqGeom.
class ClipVector;

// Abstract representation of a clip volume applied to geometry within a Viewport.
// Ported from: itwinjs-core RenderClipVolume (abstract class)
class DQ_RENDER_EXPORT RenderClipVolume {
public:
    virtual ~RenderClipVolume() = default;

    // The ClipVector from which this volume was created. It must not be modified.
    // Ported from: itwinjs-core RenderClipVolume.clipVector
    //
    // DanQing note: returns a raw pointer because the RenderClipVolume owns the
    // ClipVector (TODO: bump to ClipVector& once ClipVector lands in dqGeom).
    virtual const ClipVector* clipVector() const noexcept = 0;
};

END_DQ_RENDER_NAMESPACE
