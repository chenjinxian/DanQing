// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Edge shader builder implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Edge.ts
//
// Creates ProgramBuilder instances for edge rendering with three type variants:
// SegmentEdge (visible), Silhouette, IndexedEdge (LUT-based).
#include "EdgeShaderBuilder.h"

#include "render/AnimationShaders.h"    // addAnimation
#include "render/CommonShaders.h"       // addFrustum, addShaderFlags
#include "render/EdgeShaderHelpers.h"   // addAdjustWidth, addLineCode
#include "render/ShaderBindings.h"      // wireNormalMatrix
#include "render/ViewportShaders.h"     // addModelToWindowCoordinates, addViewport
#include "render/VertexShaderModules.h" // addLineWeight, addSamplePosition, octDecodeNormal
#include "shader/DecodeShaders.h"       // kDecodeUint24

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GLSL constants — exact ports from Edge.ts
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Edge.ts decodeEndPointAndQuadIndices (line 32-36)
static constexpr char const* kDecodeEndPointAndQuadIndices = R"(
  g_otherIndex = decodeUInt24(a_endPointAndQuadIndices.xyz);
  g_otherPos = samplePosition(g_otherIndex);
  g_quadIndex = a_endPointAndQuadIndices.w;
)";

// Ported from: itwinjs-core Edge.ts computePosition (line 147-181)
static constexpr char const* kEdgeComputePosition = R"(
  v_lnInfo = vec4(0.0, 0.0, 0.0, 0.0);
  vec4  other = g_otherPos;
  float weight = computeLineWeight();

  vec4 pos;
  g_windowPos = modelToWindowCoordinates(rawPos, other, pos, v_eyeSpace);
  if (g_windowPos.w == 0.0)
    return g_windowPos;

  vec4 otherPos;
  vec3 otherMvPos;
  vec4 projOther = modelToWindowCoordinates(other, rawPos, otherPos, otherMvPos);

  g_windowDir = projOther.xy - g_windowPos.xy;

  adjustWidth(weight, g_windowDir, g_windowPos.xy);
  g_windowDir = normalize(g_windowDir);

  vec2  perp = vec2(-g_windowDir.y, g_windowDir.x);
  float perpDist = weight / 2.0;
  float alongDist = 0.0;

  perpDist *= sign(0.5 - float(g_quadIndex == 0.0 || g_quadIndex == 3.0));
  alongDist += distance(rawPos, other) * float(g_quadIndex >= 2.0);

  pos.x += perp.x * perpDist * 2.0 * pos.w / u_viewport.x;
  pos.y += perp.y * perpDist * 2.0 * pos.w / u_viewport.y;

  lineCodeEyePos = .5 * (rawPos + other);
  lineCodeDist = alongDist;

  return pos;
)";

// Ported from: itwinjs-core Edge.ts adjustContrast (line 184-199)
static constexpr char const* kEdgeAdjustContrast = R"(
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

// Ported from: itwinjs-core Edge.ts checkForSilhouetteDiscard (line 107-130)
static constexpr char const* kCheckForSilhouetteDiscard = R"(
  if (kFrustumType_Perspective != u_frustum.z) {
    float perpTol = 4.75e-6;
    return (n0.z * n1.z > perpTol);
  } else {
    float perpTol = 2.5e-4;
    vec4  viewPos = u_mv * rawPos;
    vec3  toEye = normalize(viewPos.xyz);
    float dot0 = dot(n0, toEye);
    float dot1 = dot(n1, toEye);

    if (dot0 * dot1 > perpTol)
      return true;

    vec4 otherPosition = g_otherPos;
    viewPos = u_mv * otherPosition;
    toEye = normalize(viewPos.xyz);
    dot0 = dot(n0, toEye);
    dot1 = dot(n1, toEye);

    return dot0 * dot1 > perpTol;
  }
)";

// Ported from: itwinjs-core Edge.ts checkForSilhouetteDiscardNonIndexed (line 132-136)
static std::string getCheckForSilhouetteDiscardNonIndexed()
{
    return R"(
  vec3 n0 = u_normalMatrix * octDecodeNormal(a_normals.xy);
  vec3 n1 = u_normalMatrix * octDecodeNormal(a_normals.zw);
)" + std::string(kCheckForSilhouetteDiscard);
}

// Ported from: itwinjs-core Edge.ts checkForSilhouetteDiscardIndexed (line 138-145)
static std::string getCheckForSilhouetteDiscardIndexed()
{
    return R"(
  if (!g_isSilhouette)
    return false;

  vec3 n0 = u_normalMatrix * octDecodeNormal(g_normals.xy);
  vec3 n1 = u_normalMatrix * octDecodeNormal(g_normals.zw);
)" + std::string(kCheckForSilhouetteDiscard);
}

// ---------------------------------------------------------------------------
// Edge LUT coordinate computation
// Ported from: itwinjs-core LookupTable.ts computeLUTCoords + edge-specific init
// ---------------------------------------------------------------------------
static constexpr char const* kEdgeLutFunctions = R"(
vec2 compute_edge_coords(float index) {
  float epsilon = 0.5 / u_edgeParams.x;
  float yId = floor(index / u_edgeParams.x + epsilon);
  float xId = index - u_edgeParams.x * yId;
  return g_edge_center + vec2(xId / u_edgeParams.x, yId / u_edgeParams.y);
}
)";

// Ported from: itwinjs-core Edge.ts computeIndexedQuantizedPosition (line 43-92)
static constexpr char const* kComputeIndexedQuantizedPosition = R"(
  g_vertexId = gl_VertexID % 6;
  if (g_vertexId == 0)
    g_quadIndex = 0.0;
  else if (g_vertexId == 2 || g_vertexId == 3)
    g_quadIndex = 1.0;
  else if (g_vertexId == 1 || g_vertexId == 4)
    g_quadIndex = 2.0;
  else
    g_quadIndex = 3.0;

  float fEdgeIndex = decodeUInt24(a_position);
  g_isSilhouette = fEdgeIndex >= u_edgeParams.z;
  int edgeIndex = int(fEdgeIndex);
  bool isEven = 0 == (edgeIndex & 1);
  float edgeBaseIndex;
  if (!g_isSilhouette) {
    edgeBaseIndex = float(edgeIndex + (edgeIndex / 2));
  } else {
    int shift = isEven ? 0 : 1;
    int pad = int(u_edgeParams.w);
    if (0 != (pad % 4)) {
      isEven = !isEven;
      shift = shift + 1;
    }
    edgeBaseIndex = float(edgeIndex + edgeIndex + edgeIndex / 2 + pad / 4 - int(u_edgeParams.z) + shift / 2);
  }

  vec2 tc = compute_edge_coords(floor(edgeBaseIndex));
  vec4 s0 = floor(TEXTURE(u_edgeLUT, tc) * 255.0 + 0.5);
  tc.x += g_edge_stepX;
  vec4 s1 = floor(TEXTURE(u_edgeLUT, tc) * 255.0 + 0.5);
  tc.x += g_edge_stepX;
  vec4 s2 = floor(TEXTURE(u_edgeLUT, tc) * 255.0 + 0.5);

  vec3 i0 = isEven ? s0.xyz : vec3(s0.zw, s1.x);
  vec3 i1 = isEven ? vec3(s0.w, s1.xy) : s1.yzw;
  g_otherIndexIndex = g_quadIndex < 2.0 ? i1 : i0;

  g_normals = isEven ? vec4(s1.zw, s2.xy) : s2;

  return vec4(g_quadIndex < 2.0 ? i0 : i1, 1.0);
)";

// Ported from: itwinjs-core Edge.ts initializeIndexed (line 94-97)
static constexpr char const* kInitializeIndexed = R"(
  g_otherIndex = decodeUInt24(g_otherIndexIndex);
  g_otherPos = samplePosition(g_otherIndex);
)";

// ---------------------------------------------------------------------------
// createBase — core edge ProgramBuilder
// Ported from: itwinjs-core Edge.ts createBase() (line 218-303)
// ---------------------------------------------------------------------------
static ProgramBuilder createBase(EdgeBuilderType type, FeatureMode /*featureMode*/,
                                 PositionType /*posType*/)
{
    bool isSilhouette = (type == EdgeBuilderType::Silhouette);
    bool isIndexed = (type == EdgeBuilderType::IndexedEdge);

    ProgramBuilder builder;
    builder.enableFunctionCallVertexMain();

    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    vert.setVersion("410 core");
    frag.setVersion("410 core");

    // --- Vertex attributes ---
    vert.addVariable({"a_position", VariableType::Vec3, VariableScope::Attribute, 0});
    vert.addVariable({"a_color", VariableType::Vec4, VariableScope::Attribute, 0});

    if (!isIndexed) {
        // Non-indexed: decode other endpoint from a_endPointAndQuadIndices
        vert.addVariable({"a_endPointAndQuadIndices", VariableType::Vec4, VariableScope::Attribute, 0});
    }

    // --- Globals ---
    // Ported from: itwinjs-core Edge.ts createBase() (line 228-232)
    vert.addGlobal("g_otherPos", VariableType::Vec4);
    vert.addGlobal("g_quadIndex", VariableType::Float);
    vert.addGlobal("g_windowPos", VariableType::Vec4);
    vert.addGlobal("g_windowDir", VariableType::Vec2);
    vert.addGlobal("g_otherIndex", VariableType::Float);

    // --- Decode functions ---
    // Ported from: itwinjs-core Decode.ts
    vert.addFunction(std::string(kDecodeUint24));

    // --- Sample position from LUT ---
    // addSamplePosition adds samplePosition() function + u_vertLUT/u_vertParams/u_qOrigin/u_qScale
    addSamplePosition(vert);

    if (isIndexed) {
        // Indexed edge globals
        vert.addGlobal("g_vertexId", VariableType::Int);
        vert.addGlobal("g_otherIndexIndex", VariableType::Vec3);
        vert.addGlobal("g_isSilhouette", VariableType::Boolean, "false");
        vert.addGlobal("g_normals", VariableType::Vec4);
        vert.addGlobal("g_edge_stepX", VariableType::Float);
        vert.addGlobal("g_edge_center", VariableType::Vec2);

        // Edge LUT uniforms
        vert.addUniform("u_edgeLUT", VariableType::Sampler2D, nullptr);
        vert.addUniform("u_edgeParams", VariableType::Vec4, nullptr);

        // Edge LUT coordinate computation
        vert.addFunction(std::string(kEdgeLutFunctions));

        // LUT initialization (step size + center)
        vert.addInitializer(
            "g_edge_stepX = 1.0 / u_edgeParams.x;\n"
            "g_edge_center = vec2(0.5 * g_edge_stepX, 0.5 / u_edgeParams.y);");

        // Full indexed quantized position reading from edge LUT
        vert.setVertexComponent(VertexShaderComponent::ComputeQuantizedPosition,
            kComputeIndexedQuantizedPosition);
        vert.addInitializer(kInitializeIndexed);

        // Render order constants
        // Ported from: itwinjs-core FeatureSymbology.ts addRenderOrderConstants()
        vert.addConstant("kRenderOrder_Edge", VariableType::Float, "6.0");
        vert.addConstant("kRenderOrder_Silhouette", VariableType::Float, "7.0");
        vert.addConstant("kRenderOrder_PlanarBit", VariableType::Float, "8.0");

        // v_renderOrder varying for indexed edges
        // Ported from: itwinjs-core Edge.ts computeIndexedRenderOrder (line 100-105)
        builder.addVarying("v_renderOrder", VariableType::Float);
        vert.addUniform("u_renderOrder", VariableType::Float, nullptr);
        vert.addInitializer(
            "if (g_isSilhouette)\n"
            "    v_renderOrder = kRenderOrder_Edge == u_renderOrder ? kRenderOrder_Silhouette : kRenderOrder_Silhouette + kRenderOrder_PlanarBit;\n"
            "else\n"
            "    v_renderOrder = u_renderOrder;\n");

        // Fragment: override render order
        frag.setFragmentComponent(FragmentShaderComponent::OverrideRenderOrder,
            "    return v_renderOrder;\n");

        // Animation displacement support
        // Ported from: itwinjs-core Edge.ts createBase() (line 274-277)
        addAnimation(vert);
        vert.addInitializer(
            "g_otherPos.xyz += computeAnimationDisplacement(g_otherIndex, u_animDispParams.x, u_animDispParams.y, u_animDispParams.z, u_qAnimDispOrigin, u_qAnimDispScale);");
    } else {
        // Non-indexed: decode other endpoint from attribute
        vert.addInitializer(kDecodeEndPointAndQuadIndices);
    }

    // --- Line code globals ---
    vert.addGlobal("lineCodeEyePos", VariableType::Vec4);
    vert.addGlobal("lineCodeDist", VariableType::Float, "0.0");

    // --- Model-to-window coordinates ---
    // Ported from: itwinjs-core Viewport.ts addModelToWindowCoordinates()
    // This adds: u_mv, u_proj, u_viewportTransformation, u_renderPass, modelToWindowCoordinates()
    addModelToWindowCoordinates(vert);

    // --- Line code pipeline ---
    // Ported from: itwinjs-core Polyline.ts addLineCode() (line 212-231)
    static constexpr char const* kLineCodeArgs = "g_windowDir, g_windowPos, 0.0, 0.0";
    addLineCode(builder, kLineCodeArgs);

    // --- Varyings ---
    builder.addVarying("v_eyeSpace", VariableType::Vec3);
    builder.addVarying("v_lnInfo", VariableType::Vec4);

    // --- Edge computePosition ---
    // Ported from: itwinjs-core Edge.ts computePosition (line 147-181)
    vert.setVertexComponent(VertexShaderComponent::ComputePosition, kEdgeComputePosition);

    // --- Adjust width ---
    // Ported from: itwinjs-core Polyline.ts addAdjustWidth() (line 189-197)
    addAdjustWidth(vert);

    // --- Viewport + ModelView ---
    addViewport(vert);
    addModelViewMatrix(vert);

    // MAT_MV / MAT_MVP macros — Ported from: itwinjs-core ShaderBuilder.ts line 718-722
    // In itwinjs, these are defined by the ShaderBuilder constructor based on instancing.
    // For non-instanced: MAT_MV = u_mv, MAT_MVP = u_mvp.
    vert.addMacro("MAT_MV", "u_mv");
    vert.addMacro("MAT_MVP", "u_mvp");

    // --- Line weight ---
    addLineWeight(vert);

    // --- Silhouette-specific: normals attribute + normal matrix + frustum + octDecodeNormal + discard ---
    if (isSilhouette || isIndexed) {
        // Normals attribute (4 components: 2 for each endpoint's oct-encoded normal)
        vert.addVariable({"a_normals", VariableType::Vec4, VariableScope::Attribute, 0});
        wireNormalMatrix(vert);
        addFrustum(builder);
        vert.addFunction(std::string(getOctDecodeNormal()));

        if (isSilhouette) {
            vert.setVertexComponent(VertexShaderComponent::CheckForEarlyDiscard,
                getCheckForSilhouetteDiscardNonIndexed());
        } else {
            vert.setVertexComponent(VertexShaderComponent::CheckForEarlyDiscard,
                getCheckForSilhouetteDiscardIndexed());
        }
    }

    return builder;
}

// ---------------------------------------------------------------------------
// createEdgeProgramBuilder — public entry point
// Ported from: itwinjs-core Edge.ts createEdgeBuilder() (line 306-313)
// ---------------------------------------------------------------------------
ProgramBuilder createEdgeProgramBuilder(EdgeBuilderType type, FeatureMode featureMode,
                                        PositionType posType)
{
    auto builder = createBase(type, featureMode, posType);

    // Shader flags (u_shaderFlags[5] + kShaderBit_* constants)
    addShaderFlags(builder);

    // Vertex color
    // Ported from: itwinjs-core Color.ts addColor()
    // Add v_color varying to pass vertex color to fragment shader.
    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();
    builder.addVarying("v_color", VariableType::Vec4);
    vert.setVertexComponent(VertexShaderComponent::ComputeBaseColor,
        "    return a_color;\n");
    // Fragment: declare baseColor as a global so slot functions can access it.
    // buildFragmentMain() injects slot bodies as raw statements into void main(),
    // and slot functions (finalizeBaseColor, overrideFeatureId) reference baseColor.
    frag.addGlobal("baseColor", VariableType::Vec4);
    frag.setFragmentComponent(FragmentShaderComponent::ComputeBaseColor,
        "    baseColor = v_color;\n");

    // Edge contrast
    // Ported from: itwinjs-core Edge.ts addEdgeContrast() (line 202-214)
    vert.addUniform("u_bgIntensity", VariableType::Float, nullptr);
    vert.setVertexComponent(VertexShaderComponent::AdjustContrast, kEdgeAdjustContrast);

    // White-on-white reversal
    // Ported from: itwinjs-core Fragment.ts addWhiteOnWhiteReversal()
    frag.addUniform("u_reverseWhiteOnWhite", VariableType::Boolean, nullptr);
    frag.addFunction(R"(
vec4 reverseWhiteOnWhite(vec4 baseColor) {
    const vec3 white = vec3(1.0);
    const vec3 epsilon = vec3(0.0001);
    vec3 color = baseColor.rgb;
    vec3 delta = (color + epsilon) - white;
    vec4 wowColor = vec4(baseColor.rgb * vec3(float(delta.x <= 0.0 || delta.y <= 0.0 || delta.z <= 0.0)), baseColor.a);
    return u_reverseWhiteOnWhite ? wowColor : baseColor;
}
)");
    frag.setFragmentComponent(FragmentShaderComponent::ReverseWhiteOnWhite,
        "    baseColor = reverseWhiteOnWhite(baseColor);\n");

    // Fragment output — define assignFragData function outside main().
    // buildFragmentMain() appends: assignFragData(baseColor);
    frag.addVariable({"fragColor", VariableType::Vec4, VariableScope::Global, 0});
    frag.addFunction("void assignFragData(vec4 bc) { fragColor = bc; }\n");
    frag.setFragmentComponent(FragmentShaderComponent::AssignFragData, "");

    // Feature symbology (pick / overrides)
    if (featureMode != FeatureMode::None) {
        vert.addVariable({"a_featureId", VariableType::Uint, VariableScope::Attribute, 0});
        // GLSL requires `flat` qualifier for integer varyings.
        vert.addCode("flat out uint v_featureId;\n");
        frag.addCode("flat in uint v_featureId;\n");

        vert.setVertexComponent(VertexShaderComponent::ComputeFeatureOverrides,
            "    v_featureId = a_featureId;\n");

        if (featureMode == FeatureMode::Pick) {
            // Define overrideFeatureId as a function outside main.
            frag.addFunction(R"(
void overrideFeatureId() {
    fragColor = vec4(
        float(v_featureId & 0xFFu) / 255.0,
        float((v_featureId >> 8) & 0xFFu) / 255.0,
        float((v_featureId >> 16) & 0xFFu) / 255.0,
        float((v_featureId >> 24) & 0xFFu) / 255.0);
}
)");
            frag.setFragmentComponent(FragmentShaderComponent::AssignFragData,
                "    fragColor = vec4(0.0);\n");
            frag.setFragmentComponent(FragmentShaderComponent::OverrideFeatureId, "");
        }

        if (featureMode == FeatureMode::Overrides) {
            frag.addVariable({"u_featureOverrides", VariableType::Sampler2D, VariableScope::Uniform, 0});
            frag.addVariable({"u_featureOverrideWidth", VariableType::Float, VariableScope::Uniform, 0});
            frag.addVariable({"u_hiliteColor", VariableType::Vec4, VariableScope::Uniform, 0});

            // Define overrideFeatureId as a function outside main.
            frag.addFunction(R"(
void overrideFeatureId() {
    float featureU = (float(v_featureId) + 0.5) * u_featureOverrideWidth;
    vec4 overrideTexel = texture(u_featureOverrides, vec2(featureU, 0.5));
    if (overrideTexel.a > 0.0) {
        baseColor.rgb = overrideTexel.rgb;
        baseColor.a *= overrideTexel.a;
    }
}
)");
            frag.setFragmentComponent(FragmentShaderComponent::OverrideFeatureId, "");
        }
    }

    return builder;
}

END_DQ_RENDER_NAMESPACE
