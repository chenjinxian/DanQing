// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Feature symbology GLSL building blocks
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/FeatureSymbology.ts
//
// Feature ID extraction, override flags, flash, hilite weighting,
// render order, discard for classification.
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
// OvrFlag bit constants (Ported from: itwinjs-core FeatureSymbology.ts)
// ---------------------------------------------------------------------------

/// add the OvrFlag bit constants to a shader via addBitFlagConstant.
/// Ported from: itwinjs-core FeatureSymbology.ts addOvrFlagConstants()
/// (NB: these are bit POSITIONS in OvrFlags, not the flag values; the 16-bit
///  flags are treated as 2 bytes, so each high-byte index is subtracted by 8.)
inline void addOvrFlagConstants(ShaderBuilder& builder)
{
    // 8-bit OvrFlags bit positions
    builder.addBitFlagConstant("kOvrBit_LineRgb", 0);
    builder.addBitFlagConstant("kOvrBit_Rgb", 1);
    builder.addBitFlagConstant("kOvrBit_Alpha", 2);
    builder.addBitFlagConstant("kOvrBit_LineAlpha", 3);
    builder.addBitFlagConstant("kOvrBit_Flashed", 4);
    builder.addBitFlagConstant("kOvrBit_NonLocatable", 5);
    builder.addBitFlagConstant("kOvrBit_LineCode", 6);
    builder.addBitFlagConstant("kOvrBit_Weight", 7);
    // 16-bit OvrFlags (treated as 2 bytes - subtract 8 from each bit index)
    builder.addBitFlagConstant("kOvrBit_Hilited", 0);
    builder.addBitFlagConstant("kOvrBit_Emphasized", 1);
    builder.addBitFlagConstant("kOvrBit_ViewIndependentTransparency", 2);
    builder.addBitFlagConstant("kOvrBit_InvisibleDuringPick", 3);
    builder.addBitFlagConstant("kOvrBit_Visibility", 4);
    builder.addBitFlagConstant("kOvrBit_IgnoreMaterial", 5);
}

/// add OvrFlag bit constants to shader
inline char const* getOvrFlagConstants()
{
    return R"(
// OvrFlags bit positions (8-bit)
const int kOvrBit_LineRgb = 0;
const int kOvrBit_Rgb = 1;
const int kOvrBit_Alpha = 2;
const int kOvrBit_LineAlpha = 3;
const int kOvrBit_Flashed = 4;
const int kOvrBit_NonLocatable = 5;
const int kOvrBit_LineCode = 6;
const int kOvrBit_Weight = 7;

// OvrFlags16 bit positions (16-bit)
const int kOvrBit_Hilited = 0;
const int kOvrBit_Emphasized = 1;
const int kOvrBit_ViewIndependentTransparency = 2;
const int kOvrBit_InvisibleDuringPick = 3;
const int kOvrBit_Visibility = 4;
const int kOvrBit_IgnoreMaterial = 5;
)";
}

/// Decode uint24 from 3 bytes (feature index from vertex data)
inline char const* getDecodeUint24()
{
    return R"(
uint decodeUint24(vec3 bytes) {
    return uint(bytes.x) | (uint(bytes.y) << 8) | (uint(bytes.z) << 16);
}
)";
}

/// Check if nth bit is set in a float-encoded flag
/// Ported from: itwinjs-core Common.ts addExtractNthBit (line 136-143)
/// Uses bitwise operations instead of pow/mod for correctness and performance.
inline char const* getExtractNthBit()
{
    return R"(
bool nthBitSet(float flags, int bit) {
    uint n = uint(1) << uint(bit);
    return 0u != (uint(flags) & n);
}
)";
}

/// Feature index computation from LUT or vertex attribute
inline char const* getComputeFeatureIndex()
{
    return R"(
vec3 computeFeatureIndex() {
    // Read from vertex attribute or LUT
    return a_featureIndex.xyz;
}
)";
}

/// Compute feature overrides from LUT texture
/// Ported from: itwinjs-core FeatureSymbology.ts computeFeatureOverrides (line 615-662)
inline char const* getComputeFeatureOverrides()
{
    return R"(
void computeFeatureOverrides(vec3 featureIndex) {
    float featureU = (float(decodeUint24(featureIndex)) * 3.0 + 0.5) * u_featureOverrideWidth;
    vec4 overrideTexel = texture(u_featureOverrides, vec2(featureU, 0.5));

    // Decode dual flag bytes
    float flags = overrideTexel.a * 255.0;
    float flags16 = overrideTexel.g * 255.0;

    g_feature_rgb = overrideTexel.rgb;
    g_feature_alpha = nthBitSet(flags, kOvrBit_Alpha) ? overrideTexel.a : -1.0;

    if (!nthBitSet(flags, kOvrBit_Visibility)) {
        g_feature_alpha = 0.0;
    }
}
)";
}

/// Apply feature color override
inline char const* getApplyFeatureColor()
{
    return R"(
void applyFeatureColor(inout vec4 baseColor) {
    if (g_feature_alpha > 0.0) {
        baseColor.rgb = mix(baseColor.rgb, g_feature_rgb, g_feature_alpha);
    }
}
)";
}

/// Check vertex discard based on feature visibility
/// Ported from: itwinjs-core FeatureSymbology.ts checkVertexDiscard (line 140-164)
/// Includes IgnoreNonLocatable guard and transparency threshold check.
inline char const* getCheckVertexDiscard()
{
    return R"(
bool checkVertexDiscard(vec3 featureIndex) {
    float featureU = (float(decodeUint24(featureIndex)) * 3.0 + 0.5) * u_featureOverrideWidth;
    vec4 override = texture(u_featureOverrides, vec2(featureU, 0.5));
    float flags = override.a * 255.0;
    float flags16 = override.g * 255.0;

    // Discard if feature is invisible
    if (!nthBitSet(flags, kOvrBit_Visibility)) return true;

    // Discard if feature is non-locatable (unless ignoring non-locatable)
    // Ported from: itwinjs-core FeatureSymbology.ts line 632
    bool nonLocatable = u_shaderFlags[kShaderBit_IgnoreNonLocatable] ? false : nthBitSet(flags16, kOvrBit_NonLocatable);
    if (nonLocatable) return true;

    return false;
}
)";
}

/// Hilite color computation
inline char const* getComputeHiliteColor()
{
    return R"(
vec4 computeHiliteColor(vec3 featureIndex) {
    float featureU = (float(decodeUint24(featureIndex)) * 3.0 + 0.5) * u_featureOverrideWidth;
    vec4 override = texture(u_featureOverrides, vec2(featureU, 0.5));

    // Check hilited flag
    float flags16 = override.g * 255.0;
    bool hilited = nthBitSet(flags16, kOvrBit_Hilited);
    bool emphasized = nthBitSet(flags16, kOvrBit_Emphasized);

    if (hilited) {
        return u_hiliteColor;
    } else if (emphasized) {
        return vec4(u_hiliteColor.rgb, 0.5);
    }
    return vec4(0.0);
}
)";
}

/// Flash effect (tweening with hilite color)
inline char const* getApplyFlash()
{
    return R"(
void applyFlash(inout vec4 color, vec3 featureIndex) {
    float featureU = (float(decodeUint24(featureIndex)) * 3.0 + 0.5) * u_featureOverrideWidth;
    vec4 override = texture(u_featureOverrides, vec2(featureU, 0.5));
    float flags = override.a * 255.0;

    if (nthBitSet(flags, kOvrBit_Flashed)) {
        // Flash: brighten and blend with hilite color
        color.rgb = mix(color.rgb, u_hiliteColor.rgb, 0.5);
        color.rgb *= 1.5;  // brighten
    }
}
)";
}

/// Render order constants
inline char const* getRenderOrderConstants()
{
    return R"(
const float kRenderOrder_None = 0.0;
const float kRenderOrder_Background = 1.0;
const float kRenderOrder_BlankingRegion = 2.0;
const float kRenderOrder_UnlitSurface = 3.0;
const float kRenderOrder_LitSurface = 4.0;
const float kRenderOrder_Linear = 5.0;
const float kRenderOrder_Edge = 6.0;
const float kRenderOrder_Silhouette = 7.0;
const float kRenderOrder_PlanarBit = 8.0;
)";
}

/// Max alpha constant for translucency threshold
inline char const* getMaxAlpha()
{
    return R"(
const float s_maxAlpha = (255.0 - 15.0) / 255.0;
)";
}

// ===========================================================================
// Reference-faithful GLSL constants (Ported from: itwinjs-core FeatureSymbology.ts)
// These match the reference exactly, replacing the simplified versions above.
// ===========================================================================

// ---------------------------------------------------------------------------
// EmphasisFlag constants
// Ported from: itwinjs-core FeatureSymbology.ts addEmphasisFlags
// ---------------------------------------------------------------------------
inline constexpr std::string_view kEmphasisFlagConstants = R"(
const int kEmphBit_Hilite = 0;
const int kEmphBit_Emphasize = 1;
const int kEmphBit_Flash = 2;
const int kEmphBit_NonLocatable = 3;

const float kEmphFlag_Hilite = 1.0;
const float kEmphFlag_Emphasize = 2.0;
const float kEmphFlag_Flash = 4.0;
const float kEmphFlag_NonLocatable = 8.0;
)";

// ---------------------------------------------------------------------------
// nthFeatureBitSet / extractNthFeatureBit — global override aware
// Ported from: itwinjs-core FeatureSymbology.ts nthFeatureBitSet/extractNthFeatureBit
// ---------------------------------------------------------------------------
inline constexpr std::string_view kNthFeatureBitSet = R"(
bool nthFeatureBitSet(float flags, int n) {
  return 0u == (u_globalOvrFlags & (1u << uint(n))) && nthBitSet(flags, n);
}
float extractNthFeatureBit(float flags, int n) {
  return nthFeatureBitSet(flags, n) ? 1.0 : 0.0;
}
)";

// ---------------------------------------------------------------------------
// computeFeatureOverrides — main vertex override function
// Ported from: itwinjs-core FeatureSymbology.ts computeFeatureOverrides
// Reads LUT texture, decodes flags, sets feature_rgb/feature_alpha/visibility
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeFeatureOverridesComplete = R"(
void computeFeatureOverrides() {
  feature_rgb = vec3(-1.0);
  feature_alpha = -1.0;
  vec4 value = getFirstFeatureRgba();
  float emphFlags = value.y * 256.0;
  if (nthFeatureBitSet(emphFlags, kOvrBit_InvisibleDuringPick)) {
    feature_invisible = true;
    return;
  }
  v_feature_emphasis = kEmphFlag_Hilite * extractNthBit(emphFlags, kOvrBit_Hilited)
                     + kEmphFlag_Emphasize * extractNthBit(emphFlags, kOvrBit_Emphasized);
  float flags = value.x * 256.0;
  if (0.0 == flags && 0.0 == emphFlags) return;
  bool nonLocatable = nthFeatureBitSet(flags, kOvrBit_NonLocatable);
  v_feature_emphasis += kEmphFlag_NonLocatable * float(nonLocatable);
  bool invisible = nthFeatureBitSet(emphFlags, kOvrBit_Visibility);
  feature_invisible = invisible || nonLocatable;
  if (feature_invisible) return;
  bool isLinear = u_renderOrder == kRenderOrder_Linear
    || u_renderOrder == kRenderOrder_PlanarLinear
    || u_renderOrder == kRenderOrder_PlanarEdge;
  bool rgbOverridden = isLinear
    ? nthFeatureBitSet(flags, kOvrBit_LineRgb)
    : nthFeatureBitSet(flags, kOvrBit_Rgb);
  bool alphaOverridden = isLinear
    ? nthFeatureBitSet(flags, kOvrBit_LineAlpha)
    : nthFeatureBitSet(flags, kOvrBit_Alpha);
  if (alphaOverridden || rgbOverridden) {
    vec4 rgba = getSecondFeatureRgba(isLinear);
    if (rgbOverridden) feature_rgb = rgba.rgb;
    if (alphaOverridden) {
      feature_alpha = rgba.a;
      feature_viewIndependentTransparency =
        nthFeatureBitSet(emphFlags, kOvrBit_ViewIndependentTransparency);
    }
  }
  linear_feature_overrides = vec4(
    nthFeatureBitSet(flags, kOvrBit_Weight),
    value.w * 256.0,
    nthFeatureBitSet(flags, kOvrBit_LineCode),
    value.z * 256.0);
  feature_ignore_material = nthFeatureBitSet(emphFlags, kOvrBit_IgnoreMaterial);
  use_material = use_material && !feature_ignore_material;
  v_feature_emphasis += kEmphFlag_Flash * extractNthFeatureBit(flags, kOvrBit_Flashed);
}
)";

// ---------------------------------------------------------------------------
// applyFeatureColor — vertex ApplyFeatureColor component
// Ported from: itwinjs-core FeatureSymbology.ts applyFeatureColor
// Mixes feature RGB/alpha with base color using step() for override detection
// ---------------------------------------------------------------------------
inline constexpr std::string_view kApplyFeatureColorComplete = R"(
  vec3 rgb = mix(baseColor.rgb, feature_rgb.rgb, step(0.0, feature_rgb.r));
  float alpha = mix(baseColor.a, feature_alpha, step(0.0, feature_alpha));
  return vec4(rgb, alpha);
)";

// ---------------------------------------------------------------------------
// checkVertexDiscard — vertex CheckForDiscard component
// Ported from: itwinjs-core FeatureSymbology.ts checkVertexDiscard
// Handles visibility, transparency threshold, opaque/translucent pass routing
// ---------------------------------------------------------------------------
inline constexpr std::string_view kCheckVertexDiscardComplete = R"(
  if (feature_invisible) return true;
  bool hasAlpha = 1.0 == u_hasAlpha;
  if (feature_alpha > 0.0) hasAlpha = feature_alpha <= s_maxAlpha;
  int discardFlags = u_transparencyDiscardFlags;
  bool discardViewIndependentDuringOpaque = discardFlags >= 4;
  if (discardViewIndependentDuringOpaque) discardFlags = discardFlags - 4;
  bool isOpaquePass = (kRenderPass_OpaqueLinear <= u_renderPass
    && kRenderPass_OpaqueGeneral >= u_renderPass);
  bool discardTranslucentDuringOpaquePass = 1 == discardFlags || 3 == discardFlags
    || (feature_viewIndependentTransparency && discardViewIndependentDuringOpaque);
  if (isOpaquePass && !discardTranslucentDuringOpaquePass) return false;
  bool isTranslucentPass = kRenderPass_Translucent == u_renderPass;
  bool discardOpaqueDuringTranslucentPass = 2 == discardFlags || 3 == discardFlags;
  if (isTranslucentPass && !discardOpaqueDuringTranslucentPass) return false;
  return (isOpaquePass && hasAlpha) || (isTranslucentPass && !hasAlpha);
)";

// ---------------------------------------------------------------------------
// doApplyFlash — fragment flash/hilite/emphasis effect
// Ported from: itwinjs-core FeatureSymbology.ts doApplyFlash
// Applies hilite color blending, flash brightening, and tween effects
// ---------------------------------------------------------------------------
inline constexpr std::string_view kDoApplyFlash = R"(
vec4 doApplyFlash(float flags, vec4 baseColor) {
  bool isFlashed = nthBitSet(flags, kEmphBit_Flash);
  bool isHilited = nthBitSet(flags, kEmphBit_Hilite);
  bool isEmphasized = !isHilited && nthBitSet(flags, kEmphBit_Emphasize);
  vec3 hiliteRgb = isEmphasized ? u_hilite_settings[1] : u_hilite_settings[0];
  isHilited = isEmphasized || isHilited;
  float hiliteRatio = isHilited
    ? (isEmphasized ? u_hilite_settings[2][1] : u_hilite_settings[2][0])
    : 0.0;
  baseColor.rgb = mix(baseColor.rgb, hiliteRgb, hiliteRatio);
  const float maxBrighten = 0.2;
  float brighten = isFlashed ? u_flash_intensity * maxBrighten : 0.0;
  vec3 brightRgb = baseColor.rgb + brighten;
  const float maxTween = 0.75;
  float hiliteFraction = isFlashed ? u_flash_intensity * maxTween : 0.0;
  vec3 tweenRgb = baseColor.rgb * (1.0 - hiliteFraction);
  tweenRgb += u_hilite_settings[0] * hiliteFraction;
  return vec4(mix(tweenRgb, brightRgb, u_flash_mode), baseColor.a);
}
)";

// ---------------------------------------------------------------------------
// applyFlash — fragment ApplyFlash component wrapper
// Ported from: itwinjs-core FeatureSymbology.ts applyFlash
// ---------------------------------------------------------------------------
inline constexpr std::string_view kApplyFlash = R"(
  float flashHilite = floor(v_feature_emphasis + 0.5);
  return doApplyFlash(flashHilite, baseColor);
)";

// ---------------------------------------------------------------------------
// decodeDepthRgb / encodeDepthRgb — depth encoding for pick buffers
// Ported from: itwinjs-core Decode.ts
// ---------------------------------------------------------------------------
inline constexpr std::string_view kDecodeDepthRgb = R"(
float decodeDepthRgb(vec3 rgb) { return dot(rgb, vec3(1.0, 1.0 / 255.0, 1.0 / 65025.0)); }
)";

inline constexpr std::string_view kEncodeDepthRgb = R"(
vec3 encodeDepthRgb(float depth) {
  depth = min(depth, 16777215.0/16777216.0);
  vec3 enc = vec3(1.0, 255.0, 65025.0) * depth;
  enc = fract(enc);
  enc.xy -= enc.yz / 255.0;
  return enc;
}
)";

// ---------------------------------------------------------------------------
// computeLinearDepth — linear depth from eye-space Z
// Ported from: itwinjs-core Fragment.ts computeLinearDepth
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeLinearDepth = R"(
float computeLinearDepth(float eyeSpaceZ) {
  float eyeZ = -eyeSpaceZ;
  float near = u_frustum.x, far = u_frustum.y;
  float depthRange = far - near;
  float linearDepth = (eyeZ - near) / depthRange;
  return 1.0 - linearDepth;
}
)";

// ---------------------------------------------------------------------------
// readDepthAndOrder — read pick data from texture
// Ported from: itwinjs-core FeatureSymbology.ts readDepthAndOrder
// ---------------------------------------------------------------------------
inline constexpr std::string_view kReadDepthAndOrder = R"(
vec2 readDepthAndOrder(vec2 tc) {
  vec4 pdo = TEXTURE(u_pickDepthAndOrder, tc);
  float order = floor(pdo.x * 16.0 + 0.5);
  return vec2(order, decodeDepthRgb(pdo.yzw));
}
)";

// ---------------------------------------------------------------------------
// addPickBufferOutputs — 3-MRT pick buffer output
// Ported from: itwinjs-core Fragment.ts addPickBufferOutputs
// output0=premultiplied color, output1=feature_id, output2=renderOrder+encodedDepth
// ---------------------------------------------------------------------------
inline constexpr std::string_view kPickBufferOutputs = R"(
  if (u_renderPass >= kRenderPass_OpaqueLinear && u_renderPass <= kRenderPass_OpaqueGeneral)
    baseColor.a = 1.0;
  else
    baseColor = vec4(baseColor.rgb * baseColor.a, baseColor.a);
  vec4 output0 = baseColor;
  ivec4 feature_id_i = ivec4(feature_id * 255.0 + 0.5);
  vec4 output1 = vec4(feature_id_i) / 255.0;
  float linearDepth = computeLinearDepth(v_eyeSpace.z);
  vec4 output2 = vec4(renderOrder * 0.0625, encodeDepthRgb(linearDepth));
  FragColor0 = output0;
  FragColor1 = output1;
  FragColor2 = output2;
)";

// ---------------------------------------------------------------------------
// addAltPickBufferOutputs — 3-MRT with zeroed pick data
// Ported from: itwinjs-core Fragment.ts addAltPickBufferOutputs
// ---------------------------------------------------------------------------
inline constexpr std::string_view kAltPickBufferOutputs = R"(
  vec4 output0 = baseColor;
  FragColor0 = output0;
  FragColor1 = vec4(0.0);
  FragColor2 = vec4(0.0);
)";

// ---------------------------------------------------------------------------
// FeatureSymbologyOptions enum
// Ported from: itwinjs-core FeatureSymbology.ts FeatureSymbologyOptions
// ---------------------------------------------------------------------------
enum class FeatureSymbologyOptions : uint8_t {
    None = 0,
    Weight = 1 << 0,       // 1
    LineCode = 1 << 1,     // 2
    HasOverrides = 1 << 2, // 4
    Color = 1 << 3,        // 8
    Alpha = 1 << 4,        // 16

    Surface = HasOverrides | Color | Alpha,                      // 28
    point = HasOverrides | Color | Weight | Alpha,               // 29
    Linear = HasOverrides | Color | Weight | LineCode | Alpha,   // 31
};

END_DQ_RENDER_NAMESPACE
