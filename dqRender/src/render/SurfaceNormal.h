// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface normal builder helper
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//              addNormal() (line 529-566)
//
// Provides the addNormal() function that wires the surface normal system
// into a ProgramBuilder: u_normalMatrix + MAT_NORM macro, v_n computed varying,
// g_normal global, and the FinalizeNormal fragment component.
//
// §3.4 deviation (non-quantized only): itwinjs reads oct-encoded normals from
// the VertexLUT (g_vertLutData + octDecodeNormal); DanQing's non-quantized
// geometry path uses a pre-decoded a_normal attribute. The quantized path
// matches the reference LUT reads verbatim.
// Normal-map TBN (finalizeNormalNormalMap) omitted — TODO Step 3.
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
//   builder   — the ProgramBuilder to configure. MUST have surface flags already
//               added via addSurfaceFlags(), since computeSurfaceNormal reads
//               u_surfaceFlags[kSurfaceBitIndex_HasNormals].
//   quantized — true for LUT geometry: the oct-encoded normal is read from
//               g_vertLutData3.xy / g_vertLutData1.zw (Surface.ts
//               getComputeNormal(true) :396-406) and NO a_normal attribute is
//               declared. false keeps the a_normal attribute path (§3.4
//               deviation from the itwinjs unquantized LUT reads).
//   instanced — true for instanced geometry: the normal matrix is computed
//               IN-SHADER from g_mv (= u_instanced_modelView *
//               g_modelMatrixRTC — includes the per-instance rotation),
//               Ported from Vertex.ts:162-166 (`g_nmx = transpose(inverse(
//               mat3(MAT_MV)))` with MAT_MV = g_mv for instanced). A CPU
//               branch-only u_normalMatrix cannot see the per-instance matrix
//               (it exists only as instance attributes on the GPU) — leaving
//               rotated instances' normals wrong (M-M(1) instance-spheres-dark
//               saga: instances60 tile rotates local +Z → world -Y).
//
// Adds:
//   Vertex:  u_normalMatrix uniform + MAT_NORM macro (non-instanced) /
//            in-shader transpose(inverse(mat3(g_mv))) (instanced) +
//            (non-quantized: a_normal attribute) + octDecodeNormal function +
//            computeSurfaceNormal function + v_n computed varying
//   Fragment: g_normal global + FinalizeNormal component (flip + return)
// ---------------------------------------------------------------------------
inline void addNormal(ProgramBuilder& builder, bool quantized = false, bool instanced = false)
{
    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    // Normal matrix source. Non-instanced: u_normalMatrix uniform +
    // GraphicUniform binding (transpose(inverse(mat3(mv))) computed by the live
    // draw loop, stored on DrawParams) — wireNormalMatrix declares the uniform
    // AND registers the binding (previously a nullptr registration fed by the
    // legacy name-value upload — TD-15). §3.4 deviation from itwinjs which
    // computes g_nmx in-shader from u_frustumScale (Vertex.ts:169-179).
    // Instanced: in-shader from g_mv (the reference's MAT_MV for instanced
    // geometry). The frustumScale diagonal adjustment (Vertex.ts:164-165) is
    // omitted — (1,1) for 3D world branches (EQUIVALENCE with the CPU path,
    // which likewise omits it).
    std::string normalXform;
    if (instanced) {
        normalXform =
            "  mat3 nmx = transpose(inverse(mat3(g_mv)));\n"
            "  return normalize(nmx * octDecodeNormal(normal));\n";
    } else {
        wireNormalMatrix(vert);
        vert.addMacro("MAT_NORM", "u_normalMatrix");
        normalXform =
            "  return normalize(MAT_NORM * octDecodeNormal(normal));\n";
    }

    // octDecodeNormal function — octahedral normal decoding from 2 bytes.
    // Added in BOTH paths (matching the reference addNormal :533, which adds
    // it unconditionally).
    // Ported from: itwinjs-core Surface.ts octDecodeNormal (line 383-394)
    vert.addFunction(std::string(kOctDecodeNormal));

    if (quantized) {
        // Quantized LUT path — Ported from: itwinjs-core Surface.ts
        // getComputeNormal(true) (line 396-406): the oct normal u16 lives in
        // texel3.xy when the vertex has color+normal (4 texels/vertex), else
        // texel1.zw. g_vertLutData* are pre-read in [0..255] byte range
        // (kPreReadVertexDataQuantized = floor(TEXTURE*255+0.5), Vertex.ts
        // :200-204), so octDecodeNormal's /255 normalization applies directly
        // — the reference passes the vec2 through WITHOUT decodeUInt16.
        // No a_normal attribute is declared (Task 4's VAO binds only
        // a_qPosition at location 0).
        vert.addFunction(std::string("vec3 computeSurfaceNormal() {") +
                         std::string(kComputeSurfaceNormalQuantizedPrelude) +
                         normalXform + "}\n");
    } else {
        // a_normal attribute (dedup — caller may have declared it).
        vert.addVariable({"a_normal", VariableType::Vec3, VariableScope::Attribute, 0});

        // computeSurfaceNormal — attribute path (§3.4 deviation from itwinjs LUT:
        // NO LUT globals exist on this path — the pre-decoded vec3 arrives via
        // the a_normal attribute; the LUT-read prelude does not apply).
        // Ported from: itwinjs-core Surface.ts getComputeNormal() (line 396-406)
        // itwinjs reads oct-encoded normal from g_vertLutData + octDecodeNormal;
        // DanQing reads pre-decoded vec3 from a_normal directly. TODO Step 3 VertexLUT.
        vert.addFunction(std::string("vec3 computeSurfaceNormal() {\n") +
                         "  if (!u_surfaceFlags[kSurfaceBitIndex_HasNormals])\n"
                         "    return vec3(0.0);\n" +
                         (instanced ? std::string(
                                           "  mat3 nmx = transpose(inverse(mat3(g_mv)));\n"
                                           "  return normalize(nmx * a_normal);\n")
                                    : std::string(
                                           "  return normalize(MAT_NORM * a_normal);\n")) +
                         "}\n");
    }

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
        finalizeNormalPrelude() +
        std::string(kFinalizeNormalMap) +
        std::string(kFinalizeNormalPostlude));
}

END_DQ_RENDER_NAMESPACE
