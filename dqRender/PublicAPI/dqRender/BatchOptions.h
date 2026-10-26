// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — batch creation options
// Ported from: itwinjs-core core/frontend/src/common/render/BatchOptions.ts
//
// Options used when constructing a Batch — a RenderGraphic with an associated
// FeatureTable describing individual Features within the graphic.
#pragma once

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// BatchOptions — createBatch options
// Ported from: itwinjs-core BatchOptions (BatchOptions.ts:20-34)
// ---------------------------------------------------------------------------
struct BatchOptions {
    /// Identifies the Tile associated with the batch, chiefly for debugging
    /// purposes.（TD-37 取证接线：RenderCommands addBatch 的 [CULL] 探针/
    /// Debug Info 消费；ImdlTileTree 以瓦 contentId 填充。）
    /// Ported from: BatchOptions.ts:25-27 tileId?: string
    std::string tileId;

    /// If true, features within the batch will not be flashed on mouseover.
    /// Ported from: BatchOptions.ts:29 noFlash?: boolean
    /// TODO: 消费面 = FeatureOverrides 的 anyFlashed 门（webgl
    /// FeatureOverrides 置 appearance 时 noFlash 短路）——随该面移植接线。
    bool noFlash = false;

    /// If true, features within the batch will not be hilited when their
    /// corresponding element is in the SelectionSet.
    /// Ported from: BatchOptions.ts:31 noHilite?: boolean
    /// TODO: 消费面 = FeatureOverrides 的 anyHilited 门——随该面移植接线。
    bool noHilite = false;

    /// If true, the contents of the batch will not be emphasized when their
    /// corresponding Feature is emphasized using FeatureOverrides.
    /// Ported from: BatchOptions.ts:33 noEmphasis?: boolean
    /// TODO: 消费面 = FeatureOverrides 的 emphasis——随该面移植接线。
    bool noEmphasis = false;

    /// If true, the contents of the batch will only be drawn by readPixels,
    /// not renderFrame — locatable but invisible.
    /// Ported from: BatchOptions.ts:35 locateOnly?: boolean
    bool locateOnly = false;
};

END_DQ_RENDER_NAMESPACE
