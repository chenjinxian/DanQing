// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface normal builder helper
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//              addNormal() (line 529-566)
//
// Provides the addNormal() function that wires the surface normal system
// into a ProgramBuilder: u_normalMatrix + MAT_NORM macro, v_n computed varying,
// g_normal global, and the FinalizeNormal fragment component.
//
// §3.4 deviation: itwinjs uses VertexLUT (g_vertLutData + octDecodeNormal);
// DanQing uses a_normal attribute (vec3, pre-decoded). TODO Step 3 VertexLUT.
#pragma once

#include "ShaderBuilder.h"
#include "ShaderBindings.h"  // wireNormalMatrix
#include "shader/SurfaceNormalShaders.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// addNormal — wire the surface normal system into a ProgramBuilder
// Ported from: itwinjs-core Surface.ts addNormal() (line 529-566)
//
// Parameters:
//   builder — the ProgramBuilder to configure. MUST have surface flags already
//             added via addSurfaceFlags(), since computeSurfaceNormal reads
//             u_surfaceFlags[kSurfaceBitIndex_HasNormals].
//
// Adds:
//   Vertex:  u_normalMatrix uniform + MAT_NORM macro + a_normal attribute +
//            computeSurfaceNormal function + v_n computed varying
//   Fragment: g_normal global + FinalizeNormal component (flip + return)
//
// §3.4 deviation: computeSurfaceNormal uses a_normal attribute (vec3) instead
// of itwinjs's VertexLUT path (g_vertLutData + octDecodeNormal). TODO Step 3.
// Normal-map TBN (finalizeNormalNormalMap) omitted — TODO Step 3.
// ---------------------------------------------------------------------------
inline void addNormal(ProgramBuilder& builder)
{
    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    // u_normalMatrix uniform + GraphicUniform binding (transpose(inverse(mat3(mv)))
    // computed by the live draw loop, stored on DrawParams).  wireNormalMatrix
    // declares the uniform AND registers the binding — previously declared with
    // a nullptr registration and fed by the legacy name-value upload (TD-15).
    // Ported from: itwinjs-core Vertex.ts addNormalMatrix() (line 169-179) —
    // the reference computes g_nmx in-shader from u_frustumScale; DanQing's
    // §3.4 deviation uploads the CPU value via the GraphicUniform binding.
    wireNormalMatrix(vert);
    vert.addMacro("MAT_NORM", "u_normalMatrix");

    // a_normal attribute (dedup — caller may have declared it).
    vert.addVariable({"a_normal", VariableType::Vec3, VariableScope::Attribute, 0});

    // octDecodeNormal function — octahedral normal decoding from 2 bytes.
    // Ported from: itwinjs-core Surface.ts octDecodeNormal (line 383-394)
    vert.addFunction(std::string(kOctDecodeNormal));

    // computeSurfaceNormal — attribute path (§3.4 deviation from itwinjs LUT).
    // Ported from: itwinjs-core Surface.ts getComputeNormal() (line 396-406)
    // itwinjs reads oct-encoded normal from g_vertLutData + octDecodeNormal;
    // DanQing reads pre-decoded vec3 from a_normal directly. TODO Step 3 VertexLUT.
    vert.addFunction(R"(
vec3 computeSurfaceNormal() {
  if (!u_surfaceFlags[kSurfaceBitIndex_HasNormals])
    return vec3(0.0);
  return normalize(MAT_NORM * a_normal);
}
)");

    // v_n computed varying — eye-space normal passed to fragment.
    // Ported from: itwinjs-core Surface.ts addNormal() (line 535)
    builder.addFunctionComputedVarying("v_n", VariableType::Vec3,
                                        "computeLightingNormal",
                                        "  return computeSurfaceNormal();");

    // Fragment g_normal global + FinalizeNormal (prelude + normalMap + postlude).
    // Ported from: itwinjs-core Surface.ts addNormal() (line 537-557)
    //
    // buildFragmentMain emits `g_normal = finalizeNormal();` when the
    // FinalizeNormal slot is non-empty, so we define the function via
    // addFunction and use setFragmentComponent only to trigger that call.
    frag.addGlobal("g_normal", VariableType::Vec3);

    // s_normalMap sampler for normal map textures.
    // Ported from: itwinjs-core Surface.ts addNormal() (line 541-554)
    frag.addUniform("s_normalMap", VariableType::Sampler2D, nullptr);

    // u_normalMapScale uniform.
    // Ported from: itwinjs-core Surface.ts addNormal() (line 541-554)
    frag.addUniform("u_normalMapScale", VariableType::Float, nullptr);

    // finalizeNormal — full body (prelude + normalMap TBN + postlude) in the
    // FinalizeNormal slot. buildFragmentMain wraps it as `vec3 finalizeNormal()`
    // and emits `g_normal = finalizeNormal();`. constantLodTextureLookup (used by
    // the normalMap branch) is added by addTexture into m_functions, which
    // buildSourceWithComponents emits before this slot def — so the call resolves.
    // Ported from: itwinjs-core Surface.ts finalizeNormal* (line 408-446).
    frag.setFragmentComponent(FragmentShaderComponent::FinalizeNormal,
        std::string(kFinalizeNormalPrelude) +
        std::string(kFinalizeNormalMap) +
        std::string(kFinalizeNormalPostlude));
}

END_DQ_RENDER_NAMESPACE
