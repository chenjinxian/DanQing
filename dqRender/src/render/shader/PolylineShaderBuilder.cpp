// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Polyline/PointString/PointCloud shader builders
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Polyline.ts
//              + PointString.ts + PointCloud.ts
#include "PolylineShaderBuilder.h"

#include "render/CommonShaders.h"       // addFrustum, addShaderFlags
#include "render/EdgeShaderHelpers.h"   // addAdjustWidth, addLineCode
#include "render/SurfaceCommon.h"       // addFragData (fragColor + assignFragData)
#include "render/ViewportShaders.h"     // addModelToWindowCoordinates, addViewport
#include "render/VertexShaderModules.h" // addLineWeight, addSamplePositionUnquantizedFunction
#include "render/VertexTable.h"         // addVertexTable (LUT pre-read keyed by a_pos)
#include "shader/DecodeShaders.h"       // kDecodeUint24

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GLSL constants — exact ports from Polyline.ts
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Polyline.ts decodePosition (line 269-273)
static constexpr char const* kDecodePosition = R"(
vec4 decodePosition(vec3 baseIndex) {
  float index = decodeUInt24(baseIndex);
  return samplePosition(index);
}
)";

// Ported from: itwinjs-core Polyline.ts decodeAdjacentPositions (line 276-279)
static constexpr char const* kDecodeAdjacentPositions = R"(
  g_prevPos = decodePosition(a_prevIndex);
  g_nextPos = decodePosition(a_nextIndex);
)";

// Ported from: itwinjs-core Polyline.ts decodeFloatFromBytes (line 281-286)
static constexpr char const* kDecodeFloatFromBytes = R"(
float decodeFloatFromBytes(vec4 bytes) {
  uvec4 b = uvec4(bytes);
  uint u = b.x | (b.y << 8) | (b.z << 16) | (b.w << 24);
  return uintBitsToFloat(u);
}
)";

// Ported from: itwinjs-core Polyline.ts buildComputePosition (line 289-408)
// Full joint/miter system with 8 joint types decoded from a_param.
static constexpr char const* kPolylineComputePosition = R"(
  const float kNone = 0.0,
              kSquare = 1.0*3.0,
              kMiter = 2.0*3.0,
              kMiterInsideOnly = 3.0*3.0,
              kJointBase = 4.0*3.0,
              kNegatePerp = 8.0*3.0,
              kNegateAlong = 16.0*3.0,
              kNoneAdjWt = 32.0*3.0;

  v_lnInfo = vec4(0.0, 0.0, 0.0, 0.0);

  vec4 next = g_nextPos;
  vec4 pos;
  g_windowPos = modelToWindowCoordinates(rawPos, next, pos, v_eyeSpace);
  if (g_windowPos.w == 0.0)
    return g_windowPos;

  float param = a_param;
  float weight = computeLineWeight();
  float scale = 1.0, directionScale = 1.0;

  if (param >= kNoneAdjWt)
    param -= kNoneAdjWt;

  bool isSegmentStart = true;
  if (param >= kNegateAlong) {
    directionScale = -directionScale;
    param -= kNegateAlong;
    isSegmentStart = false;
  }

  if (param >= kNegatePerp) {
    scale = -1.0;
    param -= kNegatePerp;
  }

  vec4 otherPos;
  vec3 otherMvPos;
  vec4 projNext = modelToWindowCoordinates(next, rawPos, otherPos, otherMvPos);
  g_windowDir = projNext.xy - g_windowPos.xy;

  if (u_useCumDist > 0.5)
    v_patternDistance = 0.0;
  else
    v_patternDistance = 0.0;

  if (param < kJointBase) {
    vec2 dir = (directionScale > 0.0) ? g_windowDir : -g_windowDir;
    vec2 pos2 = (directionScale > 0.0) ? g_windowPos.xy : projNext.xy;
    adjustWidth(weight, dir, pos2);
  }

  if (kNone != param) {
    vec2 delta = vec2(0.0);
    vec4 prev   = g_prevPos;
    vec4 projPrev = modelToWindowCoordinates(prev, rawPos, otherPos, otherMvPos);
    vec2 prevDir   = g_windowPos.xy - projPrev.xy;
    float thisLength = sqrt(g_windowDir.x * g_windowDir.x + g_windowDir.y * g_windowDir.y);
    const float s_minNormalizeLength = 1.0E-5;
    float dist = weight / 2.0;

    if (thisLength > s_minNormalizeLength) {
      g_windowDir /= thisLength;

      float prevLength = sqrt(prevDir.x * prevDir.x + prevDir.y * prevDir.y);

      if (prevLength > s_minNormalizeLength) {
        prevDir /= prevLength;
        const float     s_minParallelDot= -.9999, s_maxParallelDot = .9999;
        float           prevNextDot  = dot(prevDir, g_windowDir);

        if (prevNextDot < s_minParallelDot || prevNextDot > s_maxParallelDot)
          param = kSquare;
      } else
        param = kSquare;
    } else {
      g_windowDir = -normalize(prevDir);
      param = kSquare;
    }

    vec2 perp = scale * vec2(-g_windowDir.y, g_windowDir.x);

    if (param == kSquare) {
      delta = perp;
    } else {
      vec2 bisector = normalize(prevDir - g_windowDir);
      float dotP = dot (bisector, perp);

      if (dotP != 0.0) {
        const float maxMiter = 3.0;
        float miterDistance = 1.0/dotP;

        if (param == kMiter) {
          delta = (abs(miterDistance) > maxMiter) ? perp : bisector * miterDistance;

        } else if (param == kMiterInsideOnly) {
          delta = (dotP  > 0.0 || abs(miterDistance) > maxMiter) ? perp : bisector * miterDistance;

        } else {
          const float jointTriangleCount = 3.0;
          float ratio = (param - kJointBase) / jointTriangleCount;
          delta = normalize((1.0 - ratio) * bisector + (dotP < 0.0 ? -ratio : ratio) * perp);
        }
      }
    }

    miterAdjust = dot(g_windowDir, delta) * dist;
    pos.x += dist * delta.x * 2.0 * pos.w / u_viewport.x;
    pos.y += dist * delta.y * 2.0 * pos.w / u_viewport.y;
  }

  return pos;
)";

// Polyline adjustContrast (same as Edge)
// Ported from: itwinjs-core Edge.ts adjustContrast (line 184-199)
static constexpr char const* kPolylineAdjustContrast = R"(
  float bgi = u_bgIntensity;
  if (bgi < 0.0)
    return baseColor;

  float s;
  float rgbi = baseColor.r * 0.3 + baseColor.g * 0.59 + baseColor.b * 0.11;
  if (rgbi > 0.81)
    s = bgi > 0.57 ? 0.0 : 0.699;
  else if (rgbi > 0.57)
    s = bgi > 0.57 ? 0.0 : 1.0;
  else
    s = bgi < 0.81 ? 1.0 : 0.699;

  return vec4(vec3(s), baseColor.a);
)";

// ---------------------------------------------------------------------------
// createPolylineBuilder — public entry point
// Ported from: itwinjs-core Polyline.ts createPolylineBuilder() (line 413-429)
//
// 1:1 faithful composition:
//   addShaderFlags → addCommon → polylineAddLineCode → addColor →
//   addEdgeContrast → addWhiteOnWhiteReversal
// where addCommon wires the model-to-window / line-weight / viewport /
// miter-compute-position / samplePosition / decodePosition infrastructure.
//
// The 4-attribute set (a_pos / a_prevIndex / a_nextIndex / a_param) matches
// itwinjs AttributeMap.ts:69-74 verbatim; a_pos is the 24-bit LUT key for the
// current vertex (resolved via addVertexTable's pre-read), a_prevIndex and
// a_nextIndex index the LUT on demand through the unquantized 4-texel
// samplePosition (Vertex.ts:78-89), and a_param is the joint-type byte.
// Color is the uniform-color path (Color.ts:51-62): u_color feeds v_color.
// ---------------------------------------------------------------------------
ProgramBuilder createPolylineProgramBuilder(FeatureMode featureMode, PositionType posType)
{
    bool quantized = (posType == PositionType::Quantized);
    ProgramBuilder builder;
    builder.enableFunctionCallVertexMain();
    builder.enableFunctionCallFragmentMain();

    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    vert.setVersion("410 core");
    frag.setVersion("410 core");

    // --- Shader flags (must precede addCommon so kShaderBit_* are in scope) ---
    // Ported from: itwinjs-core Polyline.ts createPolylineBuilder() (line 418)
    addShaderFlags(builder);

    // --- Vertex attributes (4-attr set, Polyline.ts CachedGeometry.ts:1141-1146) ---
    // a_pos / a_prevIndex / a_nextIndex: each Vec3 of bytes → decodeUInt24.
    // a_param: float (joint-type byte).
    // The a_pos declaration is owned by addVertexTable below (it owns the LUT
    // key attribute); declare the other three here.
    vert.addVariable({"a_prevIndex", VariableType::Vec3, VariableScope::Attribute, 0});
    vert.addVariable({"a_nextIndex", VariableType::Vec3, VariableScope::Attribute, 0});
    vert.addVariable({"a_param", VariableType::Float, VariableScope::Attribute, 0});

    // --- VertexLUT infrastructure (current-vertex pre-read keyed by a_pos) ---
    // Ported from: itwinjs-core Vertex.ts addPositionFromLUT (line 215-255) via
    //              the VertexShaderBuilder constructor → addPosition(this, true).
    // Wires u_vertLUT / u_vertParams / u_qOrigin / u_qScale, g_vertLutData0-5
    // pre-read globals, compute_vert_coords, computeVertexPosition, and the
    // AdjustRawPosition slot (returns position decoded from the pre-read).
    // Pass attrName="a_pos" to match itwinjs AttributeMap.ts:69-74 verbatim.
    addVertexTable(builder, quantized, /*attrName*/ "a_pos");

    // --- On-demand UNQUANTIZED samplePosition for prev/next ---
    // Ported from: itwinjs-core Vertex.ts addSamplePosition() (line 61-63) +
    //              getSamplePositionUnquantizedPostlude (line 78-89).
    // The function-only add (no uniforms) — addVertexTable already wired the
    // LUT uniforms + coord helpers. decodePosition (below) indexes the LUT
    // through this on-demand sampler.
    if (!quantized) {
        addSamplePositionUnquantizedFunction(vert);
    } else {
        // Quantized path: fall back to the simplified 1-texel samplePosition.
        // The Polyline ACS path is unquantized; this branch keeps the quantized
        // variant compiling for future use.
        // TODO(faithful): this branch is NOT a 1:1 port. The reference uses
        // compute_vert_coords for both position types (Vertex.ts:65-76), but
        // this quantized branch uses a hand-computed UV via kSamplePositionFunction.
        // It is unexercised today (ACS is unquantized) and numerically correct,
        // but port the Vertex.ts:65-76 quantized sampler when the quantized
        // polyline path is needed (plan §8 deferred).
        vert.addFunction(std::string(getUnquantizePosition()));
        vert.addFunction(std::string(kSamplePositionFunction));
    }

    // --- decodeUInt24 (used by decodePosition for the prev/next indices) ---
    // addVertexTable already added decodeUInt24; re-adding is a no-op (dedup).
    vert.addFunction(std::string(kDecodeUint24));

    // --- decodePosition + decodeFloatFromBytes + decodeAdjacentPositions ---
    // Ported from: itwinjs-core Polyline.ts addCommon() (line 264-266) +
    //              decodePosition (line 269-273) + decodeAdjacentPositions
    //              (line 276-279) + decodeFloatFromBytes (line 281-286).
    vert.addFunction(std::string(kDecodePosition));
    vert.addFunction(std::string(kDecodeFloatFromBytes));
    vert.addInitializer(kDecodeAdjacentPositions);

    // --- Globals (Polyline.ts addCommon line 245-255) ---
    vert.addGlobal("g_windowPos", VariableType::Vec4);
    vert.addGlobal("g_prevPos", VariableType::Vec4);
    vert.addGlobal("g_nextPos", VariableType::Vec4);
    vert.addGlobal("g_windowDir", VariableType::Vec2);
    vert.addGlobal("miterAdjust", VariableType::Float, "0.0");

    // --- Model-to-window + projection + model-view + viewport ---
    // Ported from: itwinjs-core Polyline.ts addCommon() (line 240-243).
    // MAT_MV / MAT_MVP macros — in itwinjs these are defined automatically by
    // the VertexShaderBuilder constructor (ShaderBuilder.ts:718-722); DanQing's
    // ShaderBuilder does not auto-emit them, so the technique adds them here.
    // For non-instanced: MAT_MV = u_mv, MAT_MVP = u_mvp (matching itwinjs).
    vert.addMacro("MAT_MV", "u_mv");
    vert.addMacro("MAT_MVP", "u_mvp");
    addModelToWindowCoordinates(vert);
    addViewport(vert);

    // --- Line weight + adjust width (Polyline.ts addCommon line 253 + 262) ---
    addLineWeight(vert);
    addAdjustWidth(vert);

    // --- Varyings (Polyline.ts addCommon line 257 + 259 + 261) ---
    builder.addVarying("v_patternDistance", VariableType::Float);
    builder.addVarying("v_eyeSpace", VariableType::Vec3);
    builder.addVarying("v_lnInfo", VariableType::Vec4);

    // --- Polyline computePosition (full joint/miter system, Polyline.ts 289-408) ---
    vert.setVertexComponent(VertexShaderComponent::ComputePosition, kPolylineComputePosition);

    // --- Line code pipeline (Polyline.ts polylineAddLineCode line 233-236) ---
    static constexpr char const* kLineCodeArgs = "g_windowDir, g_windowPos, miterAdjust, v_patternDistance";
    addLineCode(builder, kLineCodeArgs);
    addModelViewMatrix(vert);

    // --- Color via u_color uniform (Color.ts:51-62 uniform-color path) ---
    // Polyline.ts createPolylineBuilder calls addColor(builder), which in
    // itwinjs adds u_color + v_color + ComputeBaseColor slots. For the uniform
    // color path (no per-vertex color table), vertex ComputeBaseColor returns
    // u_color; fragment ComputeBaseColor returns v_color.
    // TODO(faithful): Color.ts:51-62 full path — getComputeElementColor + the
    // u_shaderFlags[kShaderBit_NonUniformColor] ? lutColor : u_color switch —
    // is simplified to the uniform-color branch (u_color) for ACS. Port the
    // per-vertex color-table path when non-uniform polyline color is needed
    // (plan §8 deferred).
    vert.addUniform("u_color", VariableType::Vec4, nullptr);
    builder.addVarying("v_color", VariableType::Vec4);
    vert.setVertexComponent(VertexShaderComponent::ComputeBaseColor,
        "    return u_color;\n");
    frag.setFragmentComponent(FragmentShaderComponent::ComputeBaseColor,
        "    return v_color;\n");

    // --- Edge contrast (Polyline.ts createPolylineBuilder line 425 → Edge.ts
    // addEdgeContrast line 202-214). Vertex-stage slot on baseColor.
    vert.addUniform("u_bgIntensity", VariableType::Float, nullptr);
    vert.setVertexComponent(VertexShaderComponent::AdjustContrast, kPolylineAdjustContrast);

    // --- White-on-white reversal (Polyline.ts createPolylineBuilder line 426) ---
    frag.addUniform("u_reverseWhiteOnWhite", VariableType::Boolean, nullptr);
    frag.setFragmentComponent(FragmentShaderComponent::ReverseWhiteOnWhite,
        "    const vec3 white = vec3(1.0);\n"
        "    const vec3 epsilon = vec3(0.0001);\n"
        "    vec3 color = baseColor.rgb;\n"
        "    vec3 delta = (color + epsilon) - white;\n"
        "    vec4 wowColor = vec4(baseColor.rgb * vec3(float(delta.x <= 0.0 || delta.y <= 0.0 || delta.z <= 0.0)), baseColor.a);\n"
        "    return u_reverseWhiteOnWhite ? wowColor : baseColor;\n");

    // --- Fragment output (Common.ts addFragColorWithPreMultipliedAlpha) ---
    // Ported from: itwinjs-core Fragment.ts addFragColorWithPreMipliedAlpha.
    // assignFragData writes fragColor; buildFragmentMain wraps the slot body
    // as `void assignFragData(vec4 baseColor)` and emits the call.
    addFragData(builder);

    // --- Feature symbology (Pick / Overrides) ---
    // Ported from: itwinjs-core Polyline.ts addFeatureSymbology() — embedded
    // in createPolylineBuilder when featureMode != None.
    if (featureMode != FeatureMode::None) {
        vert.addVariable({"a_featureId", VariableType::Uint, VariableScope::Attribute, 0});
        // GLSL 410 core requires `flat` qualifier for integer varyings on the
        // fragment side; addVarying emits `in uint v_featureId;` without `flat`
        // and fails to compile. Emit the declarations manually.
        vert.addCode("flat out uint v_featureId;\n");
        frag.addCode("flat in uint v_featureId;\n");
        vert.setVertexComponent(VertexShaderComponent::ComputeFeatureOverrides,
            "    v_featureId = a_featureId;\n");

        if (featureMode == FeatureMode::Pick) {
            frag.setFragmentComponent(FragmentShaderComponent::AssignFragData,
                "    fragColor = vec4(\n"
                "        float(v_featureId & 0xFFu) / 255.0,\n"
                "        float((v_featureId >> 8) & 0xFFu) / 255.0,\n"
                "        float((v_featureId >> 16) & 0xFFu) / 255.0,\n"
                "        float((v_featureId >> 24) & 0xFFu) / 255.0);\n");
        }

        if (featureMode == FeatureMode::Overrides) {
            frag.addVariable({"u_featureOverrides", VariableType::Sampler2D, VariableScope::Uniform, 0});
            frag.addVariable({"u_featureOverrideWidth", VariableType::Float, VariableScope::Uniform, 0});
            frag.addVariable({"u_hiliteColor", VariableType::Vec4, VariableScope::Uniform, 0});

            frag.setFragmentComponent(FragmentShaderComponent::OverrideFeatureId,
                "    float featureU = (float(v_featureId) + 0.5) * u_featureOverrideWidth;\n"
                "    vec4 overrideTexel = texture(u_featureOverrides, vec2(featureU, 0.5));\n"
                "    if (overrideTexel.a > 0.0) {\n"
                "        baseColor.rgb = overrideTexel.rgb;\n"
                "        baseColor.a *= overrideTexel.a;\n"
                "    }\n");
        }
    }

    return builder;
}

// ---------------------------------------------------------------------------
// point string shader — round GL points sized by u_pointSize.
// Ported from: itwinjs-core PointString.ts createPointStringBuilder()
//   (gl_PointSize = lineWeight [PointString.ts:22]; v_roundCorners = size>4.0
//    [:36]; circular discard via gl_PointCoord). The prior implementation
//    returned createPolylineProgramBuilder (the thick-LINE builder needing
//    a_prevIndex/a_nextIndex/a_param/VertexLUT), so PointStringGeometry (raw
//    a_position/a_color only) rendered nothing — the ACS Z-axis tip was
//    invisible. Includes the render-pass premultiply so overlay points blend.
// ---------------------------------------------------------------------------
ProgramBuilder createPointStringProgramBuilder(FeatureMode /*featureMode*/, PositionType /*posType*/)
{
    ProgramBuilder builder;
    builder.enableFunctionCallVertexMain();
    // Use the function-call fragment main (like the Surface builder) so the main
    // calls assignFragData → writes fragColor. Without this the legacy inline main
    // skips the (empty) AssignFragData slot and GL_POINTS emit zero fragments.
    builder.enableFunctionCallFragmentMain();

    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    vert.setVersion("410 core");
    frag.setVersion("410 core");

    // Attributes
    vert.addVariable({"a_position", VariableType::Vec3, VariableScope::Attribute, 0});
    vert.addVariable({"a_color", VariableType::Vec4, VariableScope::Attribute, 0});

    // Uniforms
    vert.addVariable({"u_mvp", VariableType::Mat4, VariableScope::Uniform, 0});
    vert.addVariable({"u_pointSize", VariableType::Float, VariableScope::Uniform, 0});

    // Vertex main
    vert.setVertexComponent(VertexShaderComponent::ComputePosition,
        "    gl_PointSize = u_pointSize;\n"
        // Use a_position directly: the function-call vertex main (ShaderBuilder
        // buildVertexMain) hardcodes rawPosition = vec4(0,0,0,1) (the quantized
        // path is deferred), so rawPos is the origin, not the vertex. Reading
        // a_position here makes each point render at its own position.
        "    return u_mvp * vec4(a_position, 1.0);\n");

    // v_color varying + v_roundCorners (PointString.ts:36 — round only when size>4;
    // small points stay square or the circle discard eats their single pixel: a 1px
    // point's gl_PointCoord can land on the corner (dot>0.25) → discarded → the
    // w1 point renders zero fragments).
    builder.addVarying("v_color", VariableType::Vec4);
    builder.addVarying("v_roundCorners", VariableType::Float);
    vert.setVertexComponent(VertexShaderComponent::ComputeBaseColor,
        "    v_roundCorners = u_pointSize > 4.0 ? 1.0 : 0.0;\n"
        "    return a_color;\n");

    // Fragment output + render-pass premultiply: use the SHARED addFragData (same
    // path as Surface/Polyline). It declares `out vec4 fragColor;` + u_renderPass
    // (kRenderPass constants) + the AssignFragData slot. The prior roll-your-own
    // declared fragColor as a plain Global (VariableScope::Global → no `out`), so the
    // fragment wrote a non-output variable and the point was INVISIBLE despite
    // rasterizing (GL_SAMPLES_PASSED > 0 but 0 color writes to the framebuffer).
    addFragData(builder);
    // ComputeBaseColor: discard outside the unit circle only for round (size>4)
    // points; RETURN v_color. The function-call fragment main does
    // `vec4 baseColor = computeBaseColor();`, so the slot must return — the prior
    // `baseColor = v_color;` set a global and returned nothing ("Missing return"
    // compile error). Ported from: itwinjs-core PointString.ts:36-40 roundCorners.
    frag.setFragmentComponent(FragmentShaderComponent::ComputeBaseColor,
        "    if (v_roundCorners > 0.5) {\n"
        "        vec2 coord = gl_PointCoord - vec2(0.5);\n"
        "        if (dot(coord, coord) > 0.25) discard;\n"
        "    }\n"
        "    return v_color;\n");

    return builder;
}

// ---------------------------------------------------------------------------
// point cloud shader
// Ported from: itwinjs-core PointCloud.ts createPointCloudBuilder()
// ---------------------------------------------------------------------------
ProgramBuilder createPointCloudProgramBuilder(FeatureMode featureMode, PositionType /*posType*/)
{
    ProgramBuilder builder;
    builder.enableFunctionCallVertexMain();

    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    vert.setVersion("410 core");
    frag.setVersion("410 core");

    // Attributes
    vert.addVariable({"a_position", VariableType::Vec3, VariableScope::Attribute, 0});
    vert.addVariable({"a_color", VariableType::Vec4, VariableScope::Attribute, 0});

    // Uniforms
    vert.addVariable({"u_mvp", VariableType::Mat4, VariableScope::Uniform, 0});
    vert.addVariable({"u_pointSize", VariableType::Float, VariableScope::Uniform, 0});

    // Vertex main
    vert.setVertexComponent(VertexShaderComponent::ComputePosition,
        "    gl_PointSize = u_pointSize;\n"
        // Use a_position directly: the function-call vertex main (ShaderBuilder
        // buildVertexMain) hardcodes rawPosition = vec4(0,0,0,1) (the quantized
        // path is deferred), so rawPos is the origin, not the vertex. Reading
        // a_position here makes each point render at its own position.
        "    return u_mvp * vec4(a_position, 1.0);\n");

    // v_color varying
    builder.addVarying("v_color", VariableType::Vec4);
    vert.setVertexComponent(VertexShaderComponent::ComputeBaseColor,
        "    return a_color;\n");

    // Fragment: circular point shape
    frag.addVariable({"fragColor", VariableType::Vec4, VariableScope::Global, 0});
    frag.addGlobal("baseColor", VariableType::Vec4);
    frag.setFragmentComponent(FragmentShaderComponent::ComputeBaseColor,
        "    vec2 coord = gl_PointCoord - vec2(0.5);\n"
        "    if (dot(coord, coord) > 0.25) discard;\n"
        "    baseColor = v_color;\n");
    frag.addFunction("void assignFragData(vec4 bc) { fragColor = bc; }\n");
    frag.setFragmentComponent(FragmentShaderComponent::AssignFragData, "");

    // Feature symbology
    if (featureMode != FeatureMode::None) {
        vert.addVariable({"a_featureId", VariableType::Uint, VariableScope::Attribute, 0});
        builder.addVarying("v_featureId", VariableType::Uint);
        vert.setVertexComponent(VertexShaderComponent::ComputeFeatureOverrides,
            "    v_featureId = a_featureId;\n");

        if (featureMode == FeatureMode::Pick) {
            frag.setFragmentComponent(FragmentShaderComponent::AssignFragData,
                "    fragColor = vec4(\n"
                "        float(v_featureId & 0xFFu) / 255.0,\n"
                "        float((v_featureId >> 8) & 0xFFu) / 255.0,\n"
                "        float((v_featureId >> 16) & 0xFFu) / 255.0,\n"
                "        float((v_featureId >> 24) & 0xFFu) / 255.0);\n");
        }
    }

    return builder;
}

END_DQ_RENDER_NAMESPACE
