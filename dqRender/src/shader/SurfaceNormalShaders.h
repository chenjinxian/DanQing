// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface normal pipeline shader constants
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//              addNormal() (line 529-566) + normal GLSL functions
#pragma once

#include <string_view>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GLSL — octDecodeNormal: octahedral normal decoding from 2 bytes
// Ported from: itwinjs-core Surface.ts line 383-394
// ---------------------------------------------------------------------------
inline constexpr std::string_view kOctDecodeNormal = R"(
vec3 octDecodeNormal(vec2 e) {
  e = e / 255.0 * 2.0 - 1.0;
  vec3 n = vec3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
  if (n.z < 0.0) {
    vec2 signNotZero = vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
    n.xy = (1.0 - abs(n.yx)) * signNotZero;
  }
  return normalize(n);
}
)";

// ---------------------------------------------------------------------------
// GLSL — computeSurfaceNormal (quantized variant)
// Ported from: itwinjs-core Surface.ts getComputeNormal(true) (line 396-406)
// Reads normal from LUT data g_vertLutData3.xy or g_vertLutData1.zw
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeSurfaceNormalQuantized = R"(
  if (!u_surfaceFlags[kSurfaceBitIndex_HasNormals])
    return vec3(0.0);
  vec2 normal = (u_surfaceFlags[kSurfaceBitIndex_HasColorAndNormal]) ? g_vertLutData3.xy : g_vertLutData1.zw;
  return normalize(MAT_NORM * octDecodeNormal(normal));
)";

// ---------------------------------------------------------------------------
// GLSL — computeSurfaceNormal (non-quantized variant)
// Ported from: itwinjs-core Surface.ts getComputeNormal(false) (line 396-406)
// Reads normal from LUT data g_vertLutData4.zw or g_vertLutData5.xy
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeSurfaceNormalNonQuantized = R"(
  if (!u_surfaceFlags[kSurfaceBitIndex_HasNormals])
    return vec3(0.0);
  vec2 normal = (u_surfaceFlags[kSurfaceBitIndex_HasColorAndNormal]) ? g_vertLutData4.zw : g_vertLutData5.xy;
  return normalize(MAT_NORM * octDecodeNormal(normal));
)";

// ---------------------------------------------------------------------------
// GLSL — finalizeNormal prelude: flip normal based on gl_FrontFacing
// Ported from: itwinjs-core Surface.ts finalizeNormalPrelude (line 408-410)
// ---------------------------------------------------------------------------
inline constexpr std::string_view kFinalizeNormalPrelude = R"(
  vec3 normal = normalize(v_n) * (2.0 * float(gl_FrontFacing) - 1.0);
)";

// ---------------------------------------------------------------------------
// GLSL — finalizeNormal normal map: TBN basis + normal map sampling
// Ported from: itwinjs-core Surface.ts finalizeNormalNormalMap (line 412-442)
// ---------------------------------------------------------------------------
inline constexpr std::string_view kFinalizeNormalMap = R"(
  if (isSurfaceBitSet(kSurfaceBit_HasNormalMap)) {
    vec3 dp1 = dFdx(v_eyeSpace);
    vec3 dp2 = dFdy(v_eyeSpace);
    vec2 duv1 = dFdx(v_texCoord);
    vec2 duv2 = dFdy(v_texCoord);
    vec3 tangent = normalize(duv2.y * dp1 - duv1.y * dp2);
    tangent = normalize(tangent - normal * dot(normal, tangent));
    bool flip = (duv1.x * duv2.y - duv2.x * duv1.y) < 0.0;
    if (flip) tangent = -tangent;
    vec3 biTangent = cross(normal, tangent);
    if (flip) biTangent = -biTangent;
    vec3 normM;
    if (u_surfaceFlags[kSurfaceBitIndex_UseConstantLodNormalMapMapping])
      normM = constantLodTextureLookup(s_normalMap).xyz;
    else
      normM = TEXTURE(s_normalMap, v_texCoord).xyz;
    if (length(normM) > 0.0001) {
      normM = (normM - 0.5) * 2.0;
      normM = normalize(normM);
      normM.x *= abs(u_normalMapScale);
      normM.y *= u_normalMapScale;
      normM = normalize(normM);
      normal = normalize(normM.x * tangent + normM.y * biTangent + normM.z * normal);
    }
  }
)";

// ---------------------------------------------------------------------------
// GLSL — finalizeNormal postlude: return final normal
// Ported from: itwinjs-core Surface.ts finalizeNormalPostlude (line 444-446)
// ---------------------------------------------------------------------------
inline constexpr std::string_view kFinalizeNormalPostlude = R"(
  return normal;
)";

// ---------------------------------------------------------------------------
// Complete finalizeNormal function body (prelude + normalMap + postlude)
// NOTE: std::string_view variables cannot be concatenated at compile time
// (the previous `kA kB kC` adjacency was ill-formed). Assemble at the call
// site via std::string addition, e.g.
//   std::string(kFinalizeNormalPrelude) + std::string(kFinalizeNormalMap) +
//   std::string(kFinalizeNormalPostlude)
// ---------------------------------------------------------------------------

END_DQ_RENDER_NAMESPACE
