// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Vertex shader building blocks
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Vertex.ts
//
// Core vertex shader GLSL functions: MVP transforms, position decoding,
// line weight/code, alpha, instanced RTC matrix.
//
// These are string constants injected into ShaderBuilder instances.
#pragma once

#include "ShaderBuilder.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GLSL code strings (Ported from: itwinjs-core glsl/Vertex.ts)
// ---------------------------------------------------------------------------

/// unquantize a quantized position: origin + scale * pos
inline char const* getUnquantizePosition()
{
    return R"(
vec4 unquantizePosition(vec3 pos, vec3 origin, vec3 scale) {
    return vec4(origin + scale * pos, 1.0);
}
)";
}

/// Early vertex discard (moves vertex off-screen)
inline char const* getEarlyVertexDiscard()
{
    return R"(
if (checkForEarlyDiscard(rawPosition)) {
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    return;
}
)";
}

/// Standard vertex discard
inline char const* getVertexDiscard()
{
    return R"(
if (checkForDiscard(rawPosition)) {
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    return;
}
)";
}

/// Late vertex discard
inline char const* getLateVertexDiscard()
{
    return R"(
if (checkForLateDiscard(rawPosition)) {
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    return;
}
)";
}

/// Sample position from LUT texture (quantized)
inline char const* getSamplePositionFromLUT()
{
    return R"(
vec4 samplePosition(float index) {
    float texWidth = u_vertParams.x;
    float numRgbaPerVert = u_vertParams.z;
    float vertIndex = index * numRgbaPerVert;
    float row = floor(vertIndex / texWidth);
    float col = vertIndex - row * texWidth;
    vec2 uv = (vec2(col, row) + 0.5) / vec2(texWidth, u_vertParams.y);
    vec4 raw = texture(u_vertLUT, uv);
    return unquantizePosition(raw.xyz, u_qOrigin, u_qScale);
}
)";
}

/// normal matrix from model-view matrix
inline char const* getNormalMatrixComputation()
{
    return R"(
mat3 computeNormalMatrix(mat4 mv) {
    return mat3(transpose(inverse(mv)));
}
)";
}

/// Octahedral normal decoding (from 2-component encoding)
inline char const* getOctDecodeNormal()
{
    return R"(
vec3 octDecodeNormal(vec2 e) {
    e = e / 255.0 * 2.0 - 1.0;
    vec3 n = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    float t = max(-n.z, 0.0);
    n.xy += vec2(n.x >= 0.0 ? -t : t, n.y >= 0.0 ? -t : t);
    return normalize(n);
}
)";
}

/// Lambert lighting computation
inline char const* getLambertLighting()
{
    return R"(
vec3 computeLambertLighting(vec3 normal, vec3 sunDir, float sunIntensity, vec3 ambientColor) {
    float nDotL = max(dot(normal, sunDir), 0.0);
    return ambientColor + sunIntensity * nDotL;
}
)";
}

/// Model-to-window coordinate transform (for edge rendering)
inline char const* getModelToWindowCoordinates()
{
    return R"(
vec4 modelToWindowCoordinates(vec4 position) {
    vec4 mvPos = u_mv * position;
    vec4 mvpPos = u_proj * mvPos;
    // NDC to window coordinates
    vec3 ndc = mvpPos.xyz / mvpPos.w;
    vec2 windowPos = (ndc.xy * 0.5 + 0.5) * u_viewport;
    return vec4(windowPos, ndc.z, 1.0);
}
)";
}

// ---------------------------------------------------------------------------
// Edge/Polyline Vertex Helpers
// Ported from: itwinjs-core glsl/Vertex.ts addLineWeight / addLineCode / addSamplePosition
// ---------------------------------------------------------------------------

/// GLSL: computeLineWeight() — returns g_lineWeight
/// Ported from: itwinjs-core Vertex.ts line 58
inline constexpr char const* kComputeLineWeight = "\nfloat computeLineWeight() { return g_lineWeight; }\n";

/// GLSL: computeLineCode() — returns g_lineCode
/// Ported from: itwinjs-core Vertex.ts line 59
inline constexpr char const* kComputeLineCode = "\nfloat computeLineCode() { return g_lineCode; }\n";

/// add line weight uniform + g_lineWeight global + computeLineWeight function.
/// Ported from: itwinjs-core Vertex.ts addLineWeight() (line 292-308)
inline void addLineWeight(ShaderBuilder& vert)
{
    vert.addUniform("u_lineWeight", VariableType::Float, nullptr);
    vert.addGlobal("g_lineWeight", VariableType::Float);
    vert.addInitializer("g_lineWeight = u_lineWeight;");
    vert.addFunction(std::string(kComputeLineWeight));
}

/// add line code uniform + g_lineCode global + computeLineCode function.
/// Ported from: itwinjs-core Vertex.ts addLineCode() (line 316-332)
inline void addLineCodeUniform(ShaderBuilder& vert)
{
    vert.addUniform("u_lineCode", VariableType::Float, nullptr);
    vert.addGlobal("g_lineCode", VariableType::Float);
    vert.addInitializer("g_lineCode = u_lineCode;");
    vert.addFunction(std::string(kComputeLineCode));
}

// ---------------------------------------------------------------------------
// addSamplePosition — lightweight LUT sampling for Edge/Polyline
// Ported from: itwinjs-core Vertex.ts addSamplePosition() (line 61-63)
//
// Adds the samplePosition() function + LUT uniforms. This is a simplified
// version that works for Edge/Polyline without the full VertexTable system.
// For the full LUT path (Surface), use addVertexTable() instead.
// ---------------------------------------------------------------------------

/// GLSL: samplePosition() — read position from vertex LUT texture
/// Ported from: itwinjs-core Vertex.ts getSamplePosition (line 65-90)
inline constexpr char const* kSamplePositionFunction = R"(
vec4 samplePosition(float index) {
  float texWidth = u_vertParams.x;
  float numRgbaPerVert = u_vertParams.z;
  float vertIndex = index * numRgbaPerVert;
  float row = floor(vertIndex / texWidth);
  float col = vertIndex - row * texWidth;
  vec2 uv = (vec2(col, row) + 0.5) / vec2(texWidth, u_vertParams.y);
  vec4 raw = TEXTURE(u_vertLUT, uv);
  return unquantizePosition(raw.xyz, u_qOrigin, u_qScale);
}
)";

// ---------------------------------------------------------------------------
// GLSL: samplePosition() — on-demand UNQUANTIZED 4-texel sampler.
// Ported from: itwinjs-core Vertex.ts getSamplePositionUnquantizedPostlude
//              (line 78-89) + getSamplePositionPrelude (line 65-67).
//
// Reads 4 RGBA texels from u_vertLUT at compute_vert_coords(index), packing
// 12 bytes into 3 IEEE floats via uintBitsToFloat. This is the on-demand
// counterpart to addVertexTable's pre-read path: prev/next vertex positions
// in Polyline/Edge are fetched on demand by indexing the LUT with the 24-bit
// a_prevIndex / a_nextIndex attributes (Polyline.ts:269-279 decodePosition).
//
// REQUIRES: compute_vert_coords, g_vert_stepX, u_vertLUT, u_vertParams already
// wired (e.g. by addVertexTable). Add ONLY the function — not the uniforms —
// to avoid duplicate declarations when composed with addVertexTable.
// ---------------------------------------------------------------------------
inline constexpr char const* kSamplePositionUnquantizedFunction = R"(
vec4 samplePosition(float index) {
  vec2 tc = compute_vert_coords(index);
  uvec3 vux = uvec3(floor(TEXTURE(u_vertLUT, tc).xyz * 255.0 + 0.5));
  tc.x += g_vert_stepX;
  uvec3 vuy = uvec3(floor(TEXTURE(u_vertLUT, tc).xyz * 255.0 + 0.5));
  tc.x += g_vert_stepX;
  uvec3 vuz = uvec3(floor(TEXTURE(u_vertLUT, tc).xyz * 255.0 + 0.5));
  tc.x += g_vert_stepX;
  uvec3 vuw = uvec3(floor(TEXTURE(u_vertLUT, tc).xyz * 255.0 + 0.5));
  uvec3 u = (vuw << 24) | (vuz << 16) | (vuy << 8) | vux;
  return vec4(uintBitsToFloat(u), 1.0);
}
)";

/// add samplePosition function + LUT uniforms for Edge/Polyline.
/// Ported from: itwinjs-core Vertex.ts addSamplePosition() (line 61-63)
inline void addSamplePosition(ShaderBuilder& vert)
{
    // LUT uniforms (nullptr bindings — uploaded by draw loop)
    vert.addUniform("u_vertLUT", VariableType::Sampler2D, nullptr);
    vert.addUniform("u_vertParams", VariableType::Vec4, nullptr);
    vert.addUniform("u_qOrigin", VariableType::Vec3, nullptr);
    vert.addUniform("u_qScale", VariableType::Vec3, nullptr);

    // unquantizePosition function
    vert.addFunction(std::string(getUnquantizePosition()));

    // samplePosition function
    vert.addFunction(std::string(kSamplePositionFunction));
}

/// Add ONLY the on-demand unquantized samplePosition() function (no uniforms).
/// Ported from: itwinjs-core Vertex.ts addSamplePosition() (line 61-63) for
/// the unquantized positionType. Use when addVertexTable has already wired
/// u_vertLUT/u_vertParams/g_vert_stepX/compute_vert_coords — this avoids the
/// duplicate uniform declarations that the full addSamplePosition would add.
/// Polyline uses this to resolve prev/next positions from a_prevIndex /
/// a_nextIndex via decodePosition (Polyline.ts:269-279).
inline void addSamplePositionUnquantizedFunction(ShaderBuilder& vert)
{
    vert.addFunction(std::string(kSamplePositionUnquantizedFunction));
}

END_DQ_RENDER_NAMESPACE
