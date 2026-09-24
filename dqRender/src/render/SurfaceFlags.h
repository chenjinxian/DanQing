// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface flags builder helper
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//              addSurfaceFlags() (line 507-517)
//
// Provides the addSurfaceFlags() function that wires the surface flags system
// into a ProgramBuilder: adds constants, uniforms, varyings, and initializers.
#pragma once

#include "ShaderBuilder.h"
#include "ShaderBindings.h"  // wireSurfaceFlags
#include "shader/SurfaceFlagsShaders.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// addSurfaceFlagsLookup — add surface flag constants to a shader builder
// Ported from: itwinjs-core Surface.ts addSurfaceFlagsLookup() (line 319-351)
//
// Adds: index constants, bit/mask constants, helper functions, global variable
// ---------------------------------------------------------------------------
inline void addSurfaceFlagsLookup(ShaderBuilder& builder)
{
    builder.addGlobal("surfaceFlags", VariableType::Uint);
    builder.addConstant("kSurfaceBitIndex_HasTexture", VariableType::Int, "0");
    builder.addConstant("kSurfaceBitIndex_ApplyLighting", VariableType::Int, "1");
    builder.addConstant("kSurfaceBitIndex_HasNormals", VariableType::Int, "2");
    builder.addConstant("kSurfaceBitIndex_IgnoreMaterial", VariableType::Int, "3");
    builder.addConstant("kSurfaceBitIndex_TransparencyThreshold", VariableType::Int, "4");
    builder.addConstant("kSurfaceBitIndex_BackgroundFill", VariableType::Int, "5");
    builder.addConstant("kSurfaceBitIndex_HasColorAndNormal", VariableType::Int, "6");
    builder.addConstant("kSurfaceBitIndex_OverrideRgb", VariableType::Int, "7");
    builder.addConstant("kSurfaceBitIndex_HasNormalMap", VariableType::Int, "8");
    builder.addConstant("kSurfaceBitIndex_HasMaterialAtlas", VariableType::Int, "9");
    builder.addConstant("kSurfaceBitIndex_UseConstantLodTextureMapping", VariableType::Int, "10");
    builder.addConstant("kSurfaceBitIndex_UseConstantLodNormalMapMapping", VariableType::Int, "11");

    builder.addBitFlagConstant("kSurfaceBit_HasTexture", 0);
    builder.addBitFlagConstant("kSurfaceBit_IgnoreMaterial", 3);
    builder.addBitFlagConstant("kSurfaceBit_OverrideRgb", 7);
    builder.addBitFlagConstant("kSurfaceBit_HasNormalMap", 8);

    builder.addBitFlagConstant("kSurfaceMask_HasTexture", 0);
    builder.addBitFlagConstant("kSurfaceMask_IgnoreMaterial", 3);
    builder.addBitFlagConstant("kSurfaceMask_OverrideRgb", 7);
    builder.addBitFlagConstant("kSurfaceMask_HasNormalMap", 8);

    builder.addFunction(std::string(kSurfaceFlagsHelperFunctions));
}

// ---------------------------------------------------------------------------
// addSurfaceFlags — wire the surface flags system into a ProgramBuilder
// Ported from: itwinjs-core Surface.ts addSurfaceFlags() (line 507-517)
//
// Parameters:
//   builder — the ProgramBuilder to configure
//   withFeatureOverrides — true if feature symbology overrides are active
//   withFeatureColor — true if feature color overrides are active
//
// Adds:
//   - Surface flag constants to both vert and frag builders
//   - u_surfaceFlags uniform array (boolean[12])
//   - v_surfaceFlags computed varying (float)
//   - Fragment initializer to unpack v_surfaceFlags
// ---------------------------------------------------------------------------
inline void addSurfaceFlags(ProgramBuilder& builder,
                             bool withFeatureOverrides,
                             bool withFeatureColor)
{
    addSurfaceFlagsLookup(builder.getVertexBuilder());
    addSurfaceFlagsLookup(builder.getFragmentBuilder());

    // Build the computeSurfaceFlags function body
    std::string compute;
    compute += kInitSurfaceFlags;
    if (withFeatureOverrides) {
        compute += kComputeBaseSurfaceFlags;
        if (withFeatureColor)
            compute += kComputeColorSurfaceFlags;
    }
    compute += kReturnSurfaceFlags;

    // add as a function-computed varying
    builder.addFunctionComputedVarying("v_surfaceFlags", VariableType::Float,
                                        "computeSurfaceFlags", compute);

    // Fragment initializer: unpack v_surfaceFlags back to uint
    builder.getFragmentBuilder().addInitializer(
        "surfaceFlags = uint(floor(v_surfaceFlags + 0.5));");

    // add u_surfaceFlags uniform array to BOTH stages — the vertex reads it
    // (computeBaseSurfaceFlags) AND the fragment reads it (kApplyLighting checks
    // kSurfaceBitIndex_ApplyLighting). GLSL links same-named uniforms across
    // stages to one location; both stages must declare it.
    // The VERTEX registration carries the per-draw GraphicUniform binding
    // (setUniform1iv from DrawParams.m_surfaceFlags) — matching the reference,
    // where addSurfaceFlags itself registers the upload (Surface.ts:519-526).
    // NOTE: GraphicUniform bindings do not yet dispatch to GL (UniformHandle is
    // cache-only); the live per-draw upload rides the legacy params map at the
    // compositor draw site (SceneCompositorImpl — same transitional state as
    // u_mv/u_renderPass). Without that upload the array stays at the GL default
    // (all false): HasTexture=0 made sampleSurfaceTexture() return white and
    // ApplyLighting=0 skipped lighting — textured surfaces rendered as flat
    // vertex color (glTF texture-on-screen regression, GltfTexturePixelTest).
    wireSurfaceFlags(builder.getVertexBuilder());
    builder.getFragmentBuilder().addUniformArray(
        "u_surfaceFlags", VariableType::Boolean,
        static_cast<int>(SurfaceBitIndex::Count), nullptr);
}

END_DQ_RENDER_NAMESPACE
