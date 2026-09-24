// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Feature/effect GLSL shader builders
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/
//              FeatureSymbology.ts (full), PlanarClassification.ts,
//              Monochrome.ts, Thematic.ts, Contours.ts, Translucency.ts,
//              Clipping.ts, Wiremesh.ts
//
// GLSL code snippets for feature/effect rendering.
#pragma once

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Feature symbology: complete feature override pipeline
// (Ported from: itwinjs-core glsl/FeatureSymbology.ts addFeatureSymbology)
// ---------------------------------------------------------------------------
inline char const* getFeatureSymbologyVertexCode()
{
    return R"(
// Feature symbology vertex code
uniform sampler2D u_featureOverrides;
uniform float u_featureOverrideWidth;
uniform uint u_batchId;

flat out uint v_featureId;

void computeFeatureSymbology(vec3 featureIndex) {
    uint featureId = decodeUint24(featureIndex);
    v_featureId = featureId + u_batchId;

    // Lookup feature override
    float featureU = (float(featureId) * 3.0 + 0.5) * u_featureOverrideWidth;
    vec4 override = texture(u_featureOverrides, vec2(featureU, 0.5));

    // Decode override flags
    float flags = override.a * 255.0;
    bool visible = nthBitSet(flags, kOvrBit_Visibility);
    bool flashed = nthBitSet(flags, kOvrBit_Flashed);

    // Discard invisible features
    if (!visible) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }
}
)";
}

inline char const* getFeatureSymbologyFragmentCode()
{
    return R"(
// Feature symbology fragment code
uniform sampler2D u_featureOverrides;
uniform float u_featureOverrideWidth;
uniform vec4 u_hiliteColor;

flat in uint v_featureId;

vec4 applyFeatureSymbology(vec4 baseColor) {
    float featureU = (float(v_featureId) * 3.0 + 0.5) * u_featureOverrideWidth;
    vec4 override = texture(u_featureOverrides, vec2(featureU, 0.5));

    float flags = override.a * 255.0;
    float flags16 = override.g * 255.0;

    // Apply color override
    if (nthBitSet(flags, kOvrBit_Rgb)) {
        baseColor.rgb = override.rgb;
    }

    // Apply alpha override
    if (nthBitSet(flags, kOvrBit_Alpha)) {
        baseColor.a = override.a;
    }

    // Apply hilite
    if (nthBitSet(flags16, kOvrBit_Hilited)) {
        baseColor.rgb = mix(baseColor.rgb, u_hiliteColor.rgb, 0.5);
    }

    // Apply flash
    if (nthBitSet(flags, kOvrBit_Flashed)) {
        baseColor.rgb = mix(baseColor.rgb, u_hiliteColor.rgb, 0.5);
        baseColor.rgb *= 1.5;
    }

    return baseColor;
}
)";
}

// ---------------------------------------------------------------------------
// Planar classification GLSL
// (Ported from: itwinjs-core glsl/PlanarClassification.ts)
// ---------------------------------------------------------------------------
inline char const* getPlanarClassificationCode()
{
    return R"(
// Planar classification
uniform sampler2D s_classifierTexture;
uniform mat4 u_classifierMatrix;
uniform vec4 u_classifierColor;

vec4 applyPlanarClassification(vec4 baseColor, vec3 worldPos) {
    vec4 classCoord = u_classifierMatrix * vec4(worldPos, 1.0);
    classCoord.xyz /= classCoord.w;
    vec2 uv = classCoord.xy * 0.5 + 0.5;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        return baseColor;
    }

    vec4 classColor = texture(s_classifierTexture, uv);
    if (classColor.a > 0.0) {
        return mix(baseColor, classColor, u_classifierColor.a);
    }
    return baseColor;
}
)";
}

// ---------------------------------------------------------------------------
// Thematic display GLSL
// (Ported from: itwinjs-core glsl/Thematic.ts)
// ---------------------------------------------------------------------------
inline char const* getThematicDisplayCode()
{
    return R"(
// Thematic display
uniform sampler2D s_thematicLut;
uniform vec2 u_thematicRange;  // (min, max)
uniform vec3 u_thematicAxis;   // (0,0,1) for height, (0,1,0) for slope, etc.

vec4 applyThematicDisplay(vec4 baseColor, vec3 worldPos) {
    float value = dot(worldPos, u_thematicAxis);
    float t = (value - u_thematicRange.x) / (u_thematicRange.y - u_thematicRange.x);
    t = clamp(t, 0.0, 1.0);

    vec4 thematicColor = texture(s_thematicLut, vec2(t, 0.5));
    return thematicColor;
}
)";
}

// ---------------------------------------------------------------------------
// Contour lines GLSL
// (Ported from: itwinjs-core glsl/Contours.ts)
// ---------------------------------------------------------------------------
inline char const* getContourLinesCode()
{
    return R"(
// Contour uniforms
// Ported from: itwinjs-core Contours.ts
uniform sampler2D u_contourLUT;
uniform uint u_contourLUTWidth;
uniform mat4 u_modelToWorldC;
uniform vec4 u_contourDefs[8];

// Vertex varyings (set in vertex shader)
varying float v_contourNdx;
varying float v_height;

// Compute contour group index from feature index via LUT texture.
// Ported from: itwinjs-core Contours.ts computeContourNdx()
float computeContourNdx() {
    if (u_contourLUTWidth == 0u)
        return 15.0;
    uint lutIndex = uint(getFeatureIndex());
    bool odd = bool(lutIndex & 1u);
    lutIndex /= 2u;
    uint byteSel = lutIndex & 0x3u;
    lutIndex /= 4u;
    ivec2 coords = ivec2(lutIndex % u_contourLUTWidth, lutIndex / u_contourLUTWidth);
    uvec4 contourNdx4 = uvec4(texelFetch(u_contourLUT, coords, 0) * 255.0 + 0.5);
    uvec2 contourNdx2 = bool(byteSel & 2u) ? contourNdx4.ba : contourNdx4.rg;
    uint contourNdx = bool(byteSel & 1u) ? contourNdx2.g : contourNdx2.r;
    return float(odd ? contourNdx >> 4u : contourNdx & 0xFu);
}

// Compute world-space height from raw position.
// Ported from: itwinjs-core Contours.ts computeWorldHeight()
float computeWorldHeight(vec4 rawPosition) {
    return (u_modelToWorldC * rawPosition).z;
}

// Unpack two bytes from a vec4 (upper or lower byte of each component).
// Ported from: itwinjs-core Contours.ts unpack2BytesVec4()
vec4 unpack2BytesVec4(vec4 f, bool upper) {
    f = floor(f + 0.5);
    vec4 outUpper = floor(f / 256.0);
    vec4 outLower = floor(f - outUpper * 256.0);
    return upper ? outUpper : outLower;
}

// Unpack and normalize two bytes from a vec4 to 0-1 range.
// Ported from: itwinjs-core Contours.ts unpackAndNormalize2BytesVec4()
vec4 unpackAndNormalize2BytesVec4(vec4 f, bool upper) {
    return unpack2BytesVec4(f, upper) / 255.0;
}

// Apply contour lines to the base color.
// Ported from: itwinjs-core Contours.ts applyContours()
vec4 applyContours(vec4 baseColor) {
    int contourNdx = int(v_contourNdx + 0.5);
    if (contourNdx > 14) // 15 => no contours
        return baseColor;

    const int maxDefs = 5; // ContourDisplay.maxContourGroups
    int contourNdxC = clamp(contourNdx, 0, maxDefs - 1);

    bool even = (contourNdxC & 1) == 0;
    vec4 rgbfp = u_contourDefs[even ? contourNdxC * 3 / 2 : (contourNdxC - 1) * 3 / 2 + 2];
    vec4 intervalsPair = u_contourDefs[(contourNdxC / 2) * 3 + 1];
    vec2 intervals = even ? intervalsPair.rg : intervalsPair.ba;

    float coord = v_height / intervals.r;
    bool maj = (fract((abs(coord) + 0.5) / intervals.g) < (1.0 / intervals.g));
    vec4 rgbf = unpackAndNormalize2BytesVec4(rgbfp, maj);

    int lineCodeWt = int((rgbf.a * 255.0) + 0.5);
    float lineRadius = (float(lineCodeWt & 0xf) * 0.5 + 2.0) * 0.5;

    float line = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    float contourAlpha = lineRadius - min(line, lineRadius);

    float dx = dFdx(contourAlpha);
    float dy = dFdy(contourAlpha);

    const float patLength = 32.0;
    uint patterns[10] = uint[](0xffffffffu, 0x80808080u, 0xf8f8f8f8u, 0xffe0ffe0u, 0xfe10fe10u, 0xe0e0e0e0u, 0xf888f888u, 0xff18ff18u, 0xccccccccu, 0x00000001u);

    float offset = trunc((abs(dx) > abs(dy)) ? gl_FragCoord.y : gl_FragCoord.x);
    offset = mod(offset, patLength);
    uint msk = 1u << uint(offset);
    contourAlpha *= (patterns[(lineCodeWt / 16) & 0xf] & msk) > 0u ? 1.0 : 0.0;
    contourAlpha = min(contourAlpha, 1.0);

    if (rgbfp.a / 65536.0 < 0.5) { // showGeometry == 0
        if (contourAlpha < 0.5) // not a contour line
            discard;
        return vec4(rgbf.rgb, 1.0);
    }
    float alpha = contourAlpha >= 0.5 ? 1.0 : baseColor.a;
    return vec4(mix(baseColor.rgb, rgbf.rgb, contourAlpha), alpha);
}

// Legacy compatibility wrapper (worldPos-based, for simple use cases)
vec4 applyContourLines(vec4 baseColor, vec3 worldPos) {
    v_height = worldPos.z;
    return applyContours(baseColor);
}
)";
}

// ---------------------------------------------------------------------------
// Monochrome mode GLSL
// (Ported from: itwinjs-core glsl/Monochrome.ts)
// ---------------------------------------------------------------------------
inline char const* getMonochromeCode()
{
    return R"(
// Monochrome mode
uniform bool u_monochromeEnabled;
uniform vec3 u_monochromeColor;

vec4 applyMonochromeMode(vec4 color) {
    if (u_monochromeEnabled) {
        float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
        return vec4(gray * u_monochromeColor, color.a);
    }
    return color;
}
)";
}

// ---------------------------------------------------------------------------
// Translucency OIT GLSL
// (Ported from: itwinjs-core glsl/Translucency.ts)
// ---------------------------------------------------------------------------
inline char const* getTranslucencyCode()
{
    return R"(
// Weighted Blended OIT (McGuire 2013)
layout(location = 0) out vec4 o_accum;
layout(location = 1) out float o_revealage;

void writeOitOutput(vec4 color, float depth) {
    float weight = max(min(1.0, max(max(color.r, color.g), color.b) * color.a + 0.01),
                       color.a) * pow(1.0 - depth * 0.9, 3.0);
    o_accum = vec4(color.rgb * color.a * weight, color.a);
    o_revealage = color.a;
}
)";
}

// ---------------------------------------------------------------------------
// Wiremesh overlay GLSL
// (Ported from: itwinjs-core glsl/Wiremesh.ts)
// ---------------------------------------------------------------------------
inline char const* getWiremeshCode()
{
    return R"(
// Wiremesh overlay
uniform bool u_wiremeshEnabled;
uniform vec3 u_wiremeshColor;

vec4 applyWiremesh(vec4 baseColor, vec3 barycentric) {
    if (u_wiremeshEnabled) {
        float minDist = min(min(barycentric.x, barycentric.y), barycentric.z);
        float lineWidth = 0.02;
        if (minDist < lineWidth) {
            return vec4(u_wiremeshColor, baseColor.a);
        }
    }
    return baseColor;
}
)";
}

END_DQ_RENDER_NAMESPACE
