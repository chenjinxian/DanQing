// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface flags shader constants and functions
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//              addSurfaceFlagsLookup() (line 319-351) + Surface flags GLSL
//
// The surface flags system uses a dual-layer approach:
// - Vertex shader: reads from u_surfaceFlags[] boolean array, packs into uint bitmask
// - Fragment shader: unpacks v_surfaceFlags varying into surfaceFlags uint global
#pragma once

#include <string_view>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// SurfaceBitIndex — indices into the u_surfaceFlags boolean array
// Ported from: itwinjs-core RenderFlags.ts SurfaceBitIndex enum (line 251)
// ---------------------------------------------------------------------------
enum class SurfaceBitIndex : uint8_t {
    hasTexture = 0,
    ApplyLighting = 1,
    HasNormals = 2,
    IgnoreMaterial = 3,
    TransparencyThreshold = 4,
    BackgroundFill = 5,
    HasColorAndNormal = 6,
    OverrideRgb = 7,
    HasNormalMap = 8,
    HasMaterialAtlas = 9,
    UseConstantLodTextureMapping = 10,
    UseConstantLodNormalMapMapping = 11,
    Count = 12
};

// ---------------------------------------------------------------------------
// SurfaceFlags — bit flags for the surfaceFlags uint bitmask
// Ported from: itwinjs-core RenderFlags.ts SurfaceFlags enum (line 270)
// ---------------------------------------------------------------------------
enum class SurfaceFlags : uint32_t {
    None = 0,
    hasTexture = 1u << 0,           // 1
    ApplyLighting = 1u << 1,        // 2
    HasNormals = 1u << 2,           // 4
    IgnoreMaterial = 1u << 3,       // 8
    TransparencyThreshold = 1u << 4, // 16
    BackgroundFill = 1u << 5,       // 32
    HasColorAndNormal = 1u << 6,    // 64
    OverrideRgb = 1u << 7,          // 128
    HasNormalMap = 1u << 8,         // 256
    HasMaterialAtlas = 1u << 9,     // 512
};

// ---------------------------------------------------------------------------
// GLSL constants — index constants for u_surfaceFlags[] lookups
// Ported from: itwinjs-core Surface.ts addSurfaceFlagsLookup() (line 319-351)
// ---------------------------------------------------------------------------
inline constexpr std::string_view kSurfaceFlagsIndexConstants = R"(
const int kSurfaceBitIndex_HasTexture = 0;
const int kSurfaceBitIndex_ApplyLighting = 1;
const int kSurfaceBitIndex_HasNormals = 2;
const int kSurfaceBitIndex_IgnoreMaterial = 3;
const int kSurfaceBitIndex_TransparencyThreshold = 4;
const int kSurfaceBitIndex_BackgroundFill = 5;
const int kSurfaceBitIndex_HasColorAndNormal = 6;
const int kSurfaceBitIndex_OverrideRgb = 7;
const int kSurfaceBitIndex_HasNormalMap = 8;
const int kSurfaceBitIndex_HasMaterialAtlas = 9;
const int kSurfaceBitIndex_UseConstantLodTextureMapping = 10;
const int kSurfaceBitIndex_UseConstantLodNormalMapMapping = 11;
)";

// ---------------------------------------------------------------------------
// GLSL constants — bit flag and mask constants for surfaceFlags uint bitmask
// ---------------------------------------------------------------------------
inline constexpr std::string_view kSurfaceFlagsBitConstants = R"(
const uint kSurfaceBit_HasTexture = 1u;
const uint kSurfaceBit_IgnoreMaterial = 8u;
const uint kSurfaceBit_OverrideRgb = 128u;
const uint kSurfaceBit_HasNormalMap = 256u;

const uint kSurfaceMask_HasTexture = 1u;
const uint kSurfaceMask_IgnoreMaterial = 8u;
const uint kSurfaceMask_OverrideRgb = 128u;
const uint kSurfaceMask_HasNormalMap = 256u;
)";

// ---------------------------------------------------------------------------
// GLSL helper — isSurfaceBitSet
// Ported from: itwinjs-core Surface.ts addSurfaceFlagsLookup()
// ---------------------------------------------------------------------------
inline constexpr std::string_view kSurfaceFlagsHelperFunctions = R"(
bool isSurfaceBitSet(uint flag) { return 0u != (surfaceFlags & flag); }
)";

// ---------------------------------------------------------------------------
// GLSL — surfaceFlags global declaration
// ---------------------------------------------------------------------------
inline constexpr std::string_view kSurfaceFlagsGlobal = "uint surfaceFlags;\n";

// ---------------------------------------------------------------------------
// GLSL — initSurfaceFlags (vertex shader)
// Ported from: itwinjs-core Surface.ts initSurfaceFlags (line 353-358)
// Packs selected u_surfaceFlags[] booleans into the surfaceFlags uint bitmask.
// ---------------------------------------------------------------------------
inline constexpr std::string_view kInitSurfaceFlags = R"(
  surfaceFlags = u_surfaceFlags[kSurfaceBitIndex_HasTexture] ? kSurfaceMask_HasTexture : 0u;
  surfaceFlags += u_surfaceFlags[kSurfaceBitIndex_IgnoreMaterial] ? kSurfaceMask_IgnoreMaterial : 0u;
  surfaceFlags += u_surfaceFlags[kSurfaceBitIndex_OverrideRgb] ? kSurfaceMask_OverrideRgb : 0u;
  surfaceFlags += u_surfaceFlags[kSurfaceBitIndex_HasNormalMap] ? kSurfaceMask_HasNormalMap : 0u;
)";

// ---------------------------------------------------------------------------
// GLSL — computeBaseSurfaceFlags (vertex shader, feature override path)
// Ported from: itwinjs-core Surface.ts computeBaseSurfaceFlags (line 362-372)
// Modifies surfaceFlags based on feature symbology overrides.
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeBaseSurfaceFlags = R"(
  if (feature_ignore_material) {
    if (u_surfaceFlags[kSurfaceBitIndex_HasTexture])
      surfaceFlags -= kSurfaceMask_HasTexture;
    if (u_surfaceFlags[kSurfaceBitIndex_HasNormalMap])
      surfaceFlags -= kSurfaceMask_HasNormalMap;

    surfaceFlags += kSurfaceMask_IgnoreMaterial;
  }
)";

// ---------------------------------------------------------------------------
// GLSL — computeColorSurfaceFlags (vertex shader, feature color path)
// Ported from: itwinjs-core Surface.ts computeColorSurfaceFlags (line 374-376)
// Adds OverrideRgb flag when feature provides an RGB override.
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeColorSurfaceFlags = R"(
  if (feature_rgb.r >= 0.0)
    surfaceFlags += kSurfaceMask_OverrideRgb;
)";

// ---------------------------------------------------------------------------
// GLSL — returnSurfaceFlags (vertex shader)
// Ported from: itwinjs-core Surface.ts returnSurfaceFlags (line 377)
// ---------------------------------------------------------------------------
inline constexpr std::string_view kReturnSurfaceFlags = R"(
  return float(surfaceFlags);
)";

// ---------------------------------------------------------------------------
// GLSL — fragment initializer to unpack v_surfaceFlags
// Ported from: itwinjs-core Surface.ts addSurfaceFlags() (line 514)
// ---------------------------------------------------------------------------
inline constexpr std::string_view kUnpackSurfaceFlags = R"(
  surfaceFlags = uint(floor(v_surfaceFlags + 0.5));
)";

END_DQ_RENDER_NAMESPACE
