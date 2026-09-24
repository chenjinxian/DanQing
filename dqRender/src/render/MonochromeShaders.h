// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Monochrome shader module
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Monochrome.ts
//
// Provides the ApplyMonochrome fragment component for unlit and surface shaders,
// plus the u_monoRgb / u_mixMonoColor uniform wiring. Replaces the invented
// getMonochromeFunctions() stub in RemainingShaderModules.h.
#pragma once

#include "ShaderBindings.h"  // wireMonochromeMix
#include "ShaderBuilder.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Reference GLSL fragments (exact ports from Monochrome.ts)
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Monochrome.ts applyUnlitMonochromeColor
inline constexpr char const* kApplyUnlitMonochromeColor = R"(
  vec4 monoColor = vec4(u_monoRgb, baseColor.a);
  return u_shaderFlags[kShaderBit_Monochrome] ? monoColor : baseColor;
)";

// Ported from: itwinjs-core Monochrome.ts applySurfaceMonochromeColor
// The luminance weights (.222, .707, .071) match the reference exactly.
// Function body; under the function-call fragment convention buildFragmentMain
// wraps it as `vec4 applyMonochrome(vec4 baseColor)` and emits the call.
inline constexpr char const* kApplySurfaceMonochromeColor = R"(
  vec4 monoColor = vec4(u_monoRgb, baseColor.a);
  if (1.0 == u_mixMonoColor) {
    vec3 rgb = baseColor.rgb;
    rgb = vec3(dot(rgb, vec3(.222, .707, .071)));
    rgb *= u_monoRgb;
    monoColor.rgb = rgb;
  }

  return u_shaderFlags[kShaderBit_Monochrome] ? monoColor : baseColor;
)";

// ---------------------------------------------------------------------------
// Wiring functions (exact ports of Monochrome.ts addXxx)
//
// NOTE: uniform VALUE bindings (style.bindMonochromeRgb; the u_mixMonoColor
// graphic binding) are registered with binding=nullptr for now, matching the
// established addMaterial() convention — they wire up when the
// TargetUniforms -> ShaderProgram binding system is connected.
// ---------------------------------------------------------------------------

/// add the u_monoRgb uniform. Ported from: itwinjs-core Monochrome.ts addMonoRgb()
inline void addMonoRgb(ProgramBuilder& builder)
{
    builder.getFragmentBuilder().addUniform("u_monoRgb", VariableType::Vec3, nullptr);
}

/// Wire unlit monochrome (ApplyMonochrome component).
/// Ported from: itwinjs-core Monochrome.ts addUnlitMonochrome()
inline void addUnlitMonochrome(ProgramBuilder& builder)
{
    addMonoRgb(builder);
    builder.getFragmentBuilder().setFragmentComponent(
        FragmentShaderComponent::ApplyMonochrome, std::string(kApplyUnlitMonochromeColor));
}

/// Wire surface monochrome (ApplyMonochrome component + u_mixMonoColor).
/// Ported from: itwinjs-core Monochrome.ts addSurfaceMonochrome()
inline void addSurfaceMonochrome(ProgramBuilder& builder)
{
    addMonoRgb(builder);
    auto& frag = builder.getFragmentBuilder();
    wireMonochromeMix(frag);
    frag.setFragmentComponent(
        FragmentShaderComponent::ApplyMonochrome, std::string(kApplySurfaceMonochromeColor));
}

END_DQ_RENDER_NAMESPACE
