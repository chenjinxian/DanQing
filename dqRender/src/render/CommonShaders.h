// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Common shader module
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Common.ts
//
// Shared GLSL helper functions, the u_frustum uniform + frustum-type constants,
// and the shader-flag bit constants. These are referenced by most other glsl
// modules (Lighting, Instancing, FeatureSymbology, etc.).
#pragma once

#include "ShaderBuilder.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Reference GLSL fragments (exact ports from Common.ts)
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Common.ts chooseVec2With2BitFlags
inline constexpr char const* kChooseVec2With2BitFlags = R"(
vec2 chooseVec2With2BitFlags(vec2 v1, vec2 v2, uint flags, uint n1, uint n2) { return 0u != (flags & (n1 | n2)) ? v2 : v1; }
)";

// Ported from: itwinjs-core Common.ts chooseVec3WithBitFlag
inline constexpr char const* kChooseVec3WithBitFlag = R"(
vec3 chooseVec3WithBitFlag(vec3 v1, vec3 v2, uint flags, uint n) { return 0u != (flags & n) ? v2 : v1; }
)";

// Ported from: itwinjs-core Common.ts addUInt32s
inline constexpr char const* kAddUInt32s = R"(
vec4 addUInt32s(vec4 a, vec4 b) {
  vec4 c = a + b;
  if (c.x > 255.0) { c.x -= 256.0; c.y += 1.0; }
  if (c.y > 255.0) { c.y -= 256.0; c.z += 1.0; }
  if (c.z > 255.0) { c.z -= 256.0; c.w += 1.0; }
  return c;
}
)";

// Ported from: itwinjs-core Common.ts nthBitSet
inline constexpr char const* kNthBitSet = R"(
bool nthBitSet(float flags, uint n) { return 0u != (uint(flags) & n); }
)";

// Ported from: itwinjs-core Common.ts extractNthBit
inline constexpr char const* kExtractNthBit = R"(
float extractNthBit(float flags, uint n) { return 0u != (uint(flags) & n) ? 1.0 : 0.0; }
)";

// ---------------------------------------------------------------------------
// Wiring functions (exact ports of Common.ts addXxx)
//
// Uniform VALUE bindings (u_frustum) are registered with binding=nullptr for
// now, matching the addMaterial()/Monochrome convention — they wire up when
// the TargetUniforms -> ShaderProgram binding system is connected.
// ---------------------------------------------------------------------------

/// add the nthBitSet + extractNthBit functions.
/// Ported from: itwinjs-core Common.ts addExtractNthBit()
inline void addExtractNthBit(ShaderBuilder& shader)
{
    shader.addFunction(std::string(kNthBitSet));
    shader.addFunction(std::string(kExtractNthBit));
}

/// add chooseVec2With2BitFlags (and its extractNthBit dependency).
/// Ported from: itwinjs-core Common.ts addChooseVec2WithBitFlagsFunction()
inline void addChooseVec2WithBitFlagsFunction(ShaderBuilder& shader)
{
    shader.addFunction(std::string(kExtractNthBit));
    shader.addFunction(std::string(kChooseVec2With2BitFlags));
}

/// add chooseVec3WithBitFlag (and its extractNthBit dependency).
/// Ported from: itwinjs-core Common.ts addChooseVec3WithBitFlagFunction()
inline void addChooseVec3WithBitFlagFunction(ShaderBuilder& shader)
{
    shader.addFunction(std::string(kExtractNthBit));
    shader.addFunction(std::string(kChooseVec3WithBitFlag));
}

/// add the kShaderBit_* constants (indices into u_shaderFlags).
/// Ported from: itwinjs-core Common.ts addShaderFlagsConstants()
inline void addShaderFlagsConstants(ShaderBuilder& shader)
{
    shader.addConstant("kShaderBit_Monochrome", VariableType::Int, "0");
    shader.addConstant("kShaderBit_NonUniformColor", VariableType::Int, "1");
    shader.addConstant("kShaderBit_OITFlatAlphaWeight", VariableType::Int, "2");
    shader.addConstant("kShaderBit_OITScaleOutput", VariableType::Int, "3");
    shader.addConstant("kShaderBit_IgnoreNonLocatable", VariableType::Int, "4");
}

/// add shader flags: u_shaderFlags[5] bool uniform array + kShaderBit_* constants
/// to both vertex and fragment builders.
/// Ported from: itwinjs-core Common.ts addShaderFlags() (line 126-131)
inline void addShaderFlags(ProgramBuilder& builder)
{
    addShaderFlagsConstants(builder.getVertexBuilder());
    addShaderFlagsConstants(builder.getFragmentBuilder());
    builder.addUniformArray("u_shaderFlags", VariableType::Boolean, 5, nullptr);
}

/// add the u_frustum uniform + frustum-type constants. The u_frustum binding
/// reads target.frustum {near, far, type} per-use (a ProgramUniform).
/// Defined out-of-line in CommonShaders.cpp (the binding needs ShaderProgram /
/// TargetImpl, kept out of this lightweight header).
/// Ported from: itwinjs-core Common.ts addFrustum() (line 115-120)
void addFrustum(ProgramBuilder& builder);

// TODO (deferred from Common.ts):
//   - addShaderFlags(builder): adds the u_shaderFlags bool[5] uniform array +
//     the kShaderBit_* constants to both stages. Blocked on ProgramBuilder
//     gaining addUniformArray (and ShaderBuilder::addUniformArray) — the
//     reference ProgramBuilder.addUniformArray forwards to vert+frag.
//   - addEyeSpace(builder): adds v_eyeSpace inline-computed varying; depends on
//     addModelViewMatrix from glsl/Vertex.ts (not yet ported).
// Both port once their prerequisites land.

END_DQ_RENDER_NAMESPACE
