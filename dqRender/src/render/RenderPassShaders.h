// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderPass shader module
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/RenderPass.ts
//
// Adds the u_renderPass uniform and the kRenderPass_* constants used by shader
// code to branch on the current render pass.
#pragma once

#include "ShaderBuilder.h"
#include "gl/RenderFlags.h"  // GL::RenderPass

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// addRenderPass — u_renderPass uniform + kRenderPass_* globals
// Ported from: itwinjs-core RenderPass.ts addRenderPass()
//
// The uniform VALUE binding maps some passes for the shader's POV
// (HiddenEdge -> OpaqueGeneral; OverlayLayers/TranslucentLayers -> OpaqueLayers).
// Registered with binding=nullptr per convention; the mapping lands when the
// TargetUniforms -> ShaderProgram binding system is connected.
// ---------------------------------------------------------------------------
inline void addRenderPass(ShaderBuilder& builder)
{
    struct PassConst { RenderPass pass; char const* name; };
    // The render passes actually used in shader code (exact ports from
    // RenderPass.ts renderPasses[]). OpaqueLayers is named "Layers" because
    // shaders treat all layer passes the same.
    static constexpr PassConst kPasses[] = {
        { RenderPass::Background,           "Background" },
        { RenderPass::OpaqueLayers,         "Layers" },
        { RenderPass::OpaqueLinear,         "OpaqueLinear" },
        { RenderPass::OpaquePlanar,         "OpaquePlanar" },
        { RenderPass::OpaqueGeneral,        "OpaqueGeneral" },
        { RenderPass::Classification,       "Classification" },
        { RenderPass::Translucent,          "Translucent" },
        { RenderPass::HiddenEdge,           "HiddenEdge" },
        { RenderPass::Hilite,               "Hilite" },
        { RenderPass::WorldOverlay,         "WorldOverlay" },
        { RenderPass::ViewOverlay,          "ViewOverlay" },
        { RenderPass::PlanarClassification, "PlanarClassification" },
    };

    builder.addUniform("u_renderPass", VariableType::Float, nullptr);

    for (auto const& p : kPasses) {
        std::string name = "kRenderPass_";
        name += p.name;
        builder.addGlobal(name, VariableType::Float,
                          std::to_string(static_cast<int>(p.pass)) + ".0", true);
    }
}

END_DQ_RENDER_NAMESPACE
