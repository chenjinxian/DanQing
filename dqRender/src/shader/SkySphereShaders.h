// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — SkySphere shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/SkySphere.ts
//
// GLSL functions for sky sphere rendering. Two modes:
//   - Gradient: procedural 2-color or 4-color gradient with configurable
//     zenith/sky/ground/nadir colors and exponents.
//   - Texture: equirectangular texture mapping with rotation and z-offset.
// Also includes the atmospheric scattering variant (delegates to Atmosphere shaders).
// These functions are compositional: they are added to viewport-quad
// shaders via ShaderBuilder, not standalone programs.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// SkySphere — computeGradientValue (vertex)
// ---------------------------------------------------------------------------
// Computes the gradient interpolation parameter for gradient sky.
// For 2-color gradient: simple linear mapping from elevation angle.
// For 4-color gradient: computes above-horizon, below-horizon, and
// horizon-band parameters.
// Ported from: itwinjs-core SkySphere.ts computeGradientValue
static char const* kSkySphereComputeGradientValue = R"glsl(
  // For the gradient sky it's good enough to calculate these in the vertex shader.
  vec3 eyeToVert = a_worldPos - u_worldEye;
  float radius = sqrt(eyeToVert.x * eyeToVert.x + eyeToVert.y * eyeToVert.y);
  float zValue = eyeToVert.z - radius * u_zOffset;
  float d = atan(zValue, radius);
  if (u_skyParams.x < 0.0) { // 2-color gradient
    d = 0.5 - d / 3.14159265359;
    return vec4(d, 0.0, 0.0, 0.0);
  }
  d = d / 1.570796326795;
  return vec4(d, 1.0 - (d - horizonSize) / (1.0 - horizonSize), 1.0 - (-d - horizonSize) / (1.0 - horizonSize), (d + horizonSize) / (horizonSize * 2.0));
)glsl";

// ---------------------------------------------------------------------------
// SkySphere — computeSkySphereColorGradient (fragment)
// ---------------------------------------------------------------------------
// Computes sky color from the gradient value. For 2-color: linear mix
// between zenith and nadir. For 4-color: three-band gradient with
// configurable exponents for sky and ground falloff.
// Ported from: itwinjs-core SkySphere.ts computeSkySphereColorGradient
static char const* kSkySphereComputeColorGradient = R"glsl(
  if (u_skyParams.x < 0.0) // 2-color
    return vec4(mix(u_zenithColor, u_nadirColor, v_gradientValue.x), 1.0);

  if (v_gradientValue.x > horizonSize) // above horizon
    return vec4(mix(u_zenithColor, u_skyColor, pow(v_gradientValue.y, u_skyParams.y)), 1.0);
  else if (v_gradientValue.x < -horizonSize) // below horizon
    return vec4(mix(u_nadirColor, u_groundColor, pow(v_gradientValue.z, u_skyParams.z)), 1.0);

  return vec4(mix(u_groundColor, u_skyColor, v_gradientValue.w), 1.0);
)glsl";

// ---------------------------------------------------------------------------
// SkySphere — computeSkySphereColorAtmosphere (fragment)
// ---------------------------------------------------------------------------
// Placeholder for atmospheric scattering mode. Returns transparent black;
// actual scattering is computed by the Atmosphere effect.
// Ported from: itwinjs-core SkySphere.ts computeSkySphereColorAtmosphere
static char const* kSkySphereComputeColorAtmosphere = R"glsl(
  return vec4(0.0, 0.0, 0.0, 1.0);
)glsl";

// ---------------------------------------------------------------------------
// SkySphere — computeEyeToVert (vertex)
// ---------------------------------------------------------------------------
// Computes the eye-to-vertex vector for texture-mode sky sphere.
// Ported from: itwinjs-core SkySphere.ts computeEyeToVert
static char const* kSkySphereComputeEyeToVert = R"glsl(
v_eyeToVert = a_worldPos - u_worldEye;
)glsl";

// ---------------------------------------------------------------------------
// SkySphere — computeSkySphereColorTexture (fragment)
// ---------------------------------------------------------------------------
// Computes sky color by sampling an equirectangular texture. Converts
// the eye-to-vertex direction to spherical coordinates, applies z-offset
// and rotation, then samples the texture.
// Ported from: itwinjs-core SkySphere.ts computeSkySphereColorTexture
static char const* kSkySphereComputeColorTexture = R"glsl(
  // For the texture we must calculate these per pixel.  Alternatively we could use a finer mesh.
  float radius = sqrt(v_eyeToVert.x * v_eyeToVert.x + v_eyeToVert.y * v_eyeToVert.y);
  float zValue = v_eyeToVert.z - radius * u_zOffset;
  float u = 0.25 - (atan(v_eyeToVert.y, v_eyeToVert.x) + u_rotation) / 6.28318530718;
  float v = 0.5 - atan(zValue, radius) / 3.14159265359;
  if (u < 0.0)
    u += 1.0;
  if (v < 0.0)
    v += 1.0;
  return TEXTURE(s_skyTxtr, vec2(u, v));
)glsl";

// ---------------------------------------------------------------------------
// SkySphere — computeEyeSpace (vertex)
// ---------------------------------------------------------------------------
// Eye Space for the SkySphere is unique because the ViewportQuad is
// already aligned with the view. The modelView matrix is not useful;
// instead, eye-space is computed from the frustum values directly.
// Ported from: itwinjs-core SkySphere.ts computeEyeSpace
static char const* kSkySphereComputeEyeSpace = R"glsl(
vec3 computeEyeSpace(vec4 rawPos) {
  vec3 pos01 = rawPos.xyz * 0.5 + 0.5;

  float top = u_frustumPlanes.x;
  float bottom = u_frustumPlanes.y;
  float left = u_frustumPlanes.z;
  float right = u_frustumPlanes.w;

  return vec3(
    mix(left, right, pos01.x),
    mix(bottom, top, pos01.y),
    -u_frustum.x
  );
}
)glsl";

END_DQ_RENDER_NAMESPACE
