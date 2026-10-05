// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderClipVolume abstract base.
//
// Ported from: itwinjs-core core/frontend/src/render/RenderClipVolume.ts
//
// A RenderClipVolume is an opaque representation of a clip volume applied to
// geometry within a Viewport. It is created from a ClipVector and takes
// ownership of that ClipVector (which must not be modified while referenced).
//
// M-P P-B/P-C：dqGeom::ClipVector 已落地（P-A）——原 TODO 占位（本地前向声明 +
// "ClipVector not yet ported"）解除；抽象面持 RefPtr 语义（§3.4：TS GC 引用 →
// RefCounted/RefPtr）。
#pragma once

#include "Export.h"

#include <dqBase/RefCounted.h>
#include <dqGeom/ClipVector.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#endif
#ifndef END_DQ_RENDER_NAMESPACE
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Abstract representation of a clip volume applied to geometry within a Viewport.
// Ported from: itwinjs-core RenderClipVolume (abstract class, RenderClipVolume.ts)
class DQ_RENDER_EXPORT RenderClipVolume : public dqBase::RefCounted<RenderClipVolume> {
public:
    using Ptr = dqBase::RefPtr<RenderClipVolume>;

    virtual ~RenderClipVolume() = default;

    // The ClipVector from which this volume was created. It must not be modified.
    // Ported from: itwinjs-core RenderClipVolume.clipVector
    virtual dqGeom::ClipVector const* clipVector() const noexcept = 0;
};

END_DQ_RENDER_NAMESPACE
