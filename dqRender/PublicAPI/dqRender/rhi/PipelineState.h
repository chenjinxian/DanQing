// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Pipeline state
// Ported from: filament backend/include/backend/PipelineState.h
//
// Bundles all state needed for a draw call: program, vertex layout, raster
// state, stencil state, polygon offset, and primitive type.
#pragma once

#include "DriverEnums.h"
#include "Handle.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// PipelineLayout — descriptor set layout references
// ---------------------------------------------------------------------------
struct PipelineLayout {
    using SetLayout = DescriptorSetLayoutHandle[MAX_DESCRIPTOR_SET_COUNT];
    SetLayout setLayout;
};

// ---------------------------------------------------------------------------
// AttachmentBlendState — per-attachment blend configuration
// Used for OIT (Order-Independent Transparency) with MRT
// ---------------------------------------------------------------------------
struct AttachmentBlendState {
    BlendFunction srcRGB = BlendFunction::ONE;
    BlendFunction dstRGB = BlendFunction::ZERO;
    BlendFunction srcAlpha = BlendFunction::ONE;
    BlendFunction dstAlpha = BlendFunction::ZERO;
    BlendEquation equationRGB = BlendEquation::ADD;
    BlendEquation equationAlpha = BlendEquation::ADD;
};

// ---------------------------------------------------------------------------
// PipelineState — complete draw-call state
// ---------------------------------------------------------------------------
struct PipelineState {
    ProgramHandle program;
    VertexBufferInfoHandle vertexBufferInfo;
    PipelineLayout pipelineLayout;
    RasterState rasterState;
    StencilState stencilState;
    PolygonOffset polygonOffset;
    PrimitiveType primitiveType = PrimitiveType::TRIANGLES;
    uint8_t numClipPlanes = 0;      // 0-6, enables GL_CLIP_DISTANCE0..N
    bool perAttachmentBlend = false; // Enable per-attachment blend (for OIT)
    uint8_t perAttachmentBlendCount = 0;  // Number of attachments with custom blend
    std::array<AttachmentBlendState, MAX_COLOR_ATTACHMENT_COUNT> attachmentBlends;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
