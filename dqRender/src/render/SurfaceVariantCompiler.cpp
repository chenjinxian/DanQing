// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface/Edge variant shader compiler implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//              core/frontend/src/internal/render/webgl/glsl/Edge.ts
//
// Refactored to use modular addXxx helpers (createCommon, addColor,
// addSurfaceFlags, addNormal, addTexture, addMaterial, addLighting, addFragData)
// instead of inline GLSL generation.  Each helper is a self-contained composition
// unit that adds its uniforms/varyings/functions/slots to a ProgramBuilder.
#include "SurfaceVariantCompiler.h"
#include "AnimationShaders.h"  // addAnimation
#include "AtmosphereShaderHelpers.h"  // addAtmosphericScatteringEffect
#include "CommonShaders.h"     // addFrustum, addShaderFlags
#include "FeatureEffectShaderBuilders.h"
#include "FeatureSymbologyShaders.h"
#include "LightingShaders.h"   // addLighting
#include "MonochromeShaders.h" // addSurfaceMonochrome
#include "ShaderBuilder.h"
#include "SolarShadowShaders.h"  // addSolarShadowMap
#include "SurfaceCommon.h"     // createCommon, addColor, addFragData
#include "SurfaceFlags.h"      // addSurfaceFlags
#include "SurfaceMaterial.h"   // addMaterial
#include "SurfaceNormal.h"     // addNormal
#include "SurfaceTexture.h"    // addTexture
#include "ThematicDisplayShaders.h"  // addThematicDisplay
#include "shader/EdgeShaderBuilder.h"      // createEdgeProgramBuilder

#include <string>
#include <utility>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// SurfaceVariantCompiler::buildProgram
// Ported from: itwinjs-core Surface.ts createSurfaceBuilder() (line 725-850)
//
// Composes the Surface shader from modular helpers, matching the itwinjs-core
// composition order:
//   createCommon → addColor → addSurfaceFlags → addNormal → addTexture →
//   addMaterial → addLighting → addFragData → (feature/classify/thematic)
//
// GLSL generation is delegated to the helpers; each adds its own uniforms,
// varyings, functions, and component slots to the ProgramBuilder.
// ---------------------------------------------------------------------------
void SurfaceVariantCompiler::buildProgram(ShaderProgram& prog, TechniqueFlags const& flags)
{
    bool quantized = flags.usesQuantizedPositions();
    bool translucent = flags.isTranslucent;
    FeatureMode featureMode = flags.featureMode;
    bool instanced = flags.isInstanced;
    bool shadowable = flags.isShadowable;
    bool classified = flags.isClassified;
    bool thematic = flags.isThematic;
    bool animated = flags.isAnimated;

    ProgramBuilder builder;
    builder.getVertexBuilder().setVersion("410 core");
    builder.getFragmentBuilder().setVersion("410 core");

    // Use function-call vertex main convention (itwinjs VertexShaderBuilder.
    // buildSource) — the modular addXxx vertex slots are function bodies that
    // return values, matching the itwinjs composition pattern.
    builder.enableFunctionCallVertexMain();

    // Use function-call fragment-main convention (itwinjs FragmentShaderBuilder.
    // buildSource) — the modular addXxx fragment slots are function bodies that
    // return / assign, matching the itwinjs composition pattern.
    builder.enableFunctionCallFragmentMain();

    // --- Foundation: position pipeline ---
    // Ported from: itwinjs-core Surface.ts createCommon() (line 265-298)
    createCommon(builder, instanced, quantized);

    // --- Animation displacement ---
    // Ported from: itwinjs-core Surface.ts addAnimation() (line 760-762)
    if (animated) {
        addAnimation(builder.getVertexBuilder());
    }

    // --- Shader flags (u_shaderFlags[5] + kShaderBit_* constants) ---
    // Ported from: itwinjs-core Common.ts addShaderFlags() (line 126-131)
    addShaderFlags(builder);

    // --- Surface flags ---
    // Ported from: itwinjs-core Surface.ts addSurfaceFlags() (line 507-517)
    bool withFeatureOverrides = (featureMode == FeatureMode::Overrides);
    addSurfaceFlags(builder, withFeatureOverrides, /*withFeatureColor*/true);

    // --- Surface normals ---
    // Ported from: itwinjs-core Surface.ts addNormal() (line 529-566)
    addNormal(builder);

    // --- Surface texture (a_texCoord) ---
    // Ported from: itwinjs-core Surface.ts addTexture() (line 571-687)
    // addTexture owns the full texture pipeline including the constant-LOD path
    // (v_uvCustom, u_constantLod*, constantLodTextureLookup, sampleSurfaceTexture,
    // computeBaseColor) — matching itwinjs's composition. Previously SVC added the
    // constantLod pieces separately, duplicating addTexture's basic versions (no
    // dedup) and breaking compilation. addFunction dedups exact strings now, so
    // any re-add would be a no-op regardless.
    addTexture(builder);

    // NOTE: finalizeNormal's full body (prelude + normalMap TBN + postlude) and
    // getSurfaceColor are now owned by addNormal / addTexture respectively (their
    // slots hold the body; buildFragmentMain wraps them as functions). Previously
    // they were re-added here as standalone functions, duplicating the slot
    // definitions (no dedup) and breaking compilation.

    // --- Vertex color (a_color) ---
    // Ported from: itwinjs-core Color.ts addColor() (line 51-70).
    // Attribute locations are bound explicitly (setAttributeMap below +
    // glBindAttribLocation-before-link in OpenGLProgram::compile), so declaration
    // order here is free — matches itwinjs's composition (addColor after addTexture).
    addColor(builder);

    // --- Lighting ---
    // Ported from: itwinjs-core Lighting.ts addLighting()
    addLighting(builder);

    // --- White-on-white reversal ---
    // Ported from: itwinjs-core Fragment.ts reverseWhiteOnWhite (ShaderBuilder.ts:1093).
    // Function body in the slot; buildFragmentMain wraps + calls it.
    {
        auto& frag = builder.getFragmentBuilder();
        frag.addUniform("u_reverseWhiteOnWhite", VariableType::Boolean, nullptr);
        frag.setFragmentComponent(FragmentShaderComponent::ReverseWhiteOnWhite,
            "    const vec3 white = vec3(1.0);\n"
            "    const vec3 epsilon = vec3(0.0001);\n"
            "    vec3 color = baseColor.rgb;\n"
            "    vec3 delta = (color + epsilon) - white;\n"
            "    vec4 wowColor = vec4(baseColor.rgb * vec3(float(delta.x <= 0.0 || delta.y <= 0.0 || delta.z <= 0.0)), baseColor.a);\n"
            "    return u_reverseWhiteOnWhite ? wowColor : baseColor;\n");
    }

    // --- Monochrome display ---
    // Ported from: itwinjs-core Monochrome.ts addSurfaceMonochrome()
    addSurfaceMonochrome(builder);

    // --- Material system ---
    // Ported from: itwinjs-core Surface.ts addMaterial() (line 183-238)
    addMaterial(builder, instanced, quantized);

    // --- Translucency or opaque output ---
    // Ported from: itwinjs-core Surface.ts lines 780-795
    if (translucent) {
        // Simplified translucent path: alpha cutoff + discard.
        // Full OIT (addTranslucency) deferred to Batch 3.
        auto& frag = builder.getFragmentBuilder();
        frag.addUniform("u_alphaCutoff", VariableType::Float, nullptr);
        // CheckForDiscard slot = bool function body (buildFragmentMain emits
        // `if (checkForDiscard(baseColor)) { discard; return; }`).
        frag.setFragmentComponent(FragmentShaderComponent::CheckForDiscard,
            "    return baseColor.a < u_alphaCutoff;\n");
    }

    // --- Fragment output ---
    // Ported from: itwinjs-core Common.ts addFragColorWithPreMultipliedAlpha()
    //               + Fragment.ts addPickBufferOutputs（pick pass 整体换输出）
    addFragData(builder, /*pickOutput=*/featureMode == FeatureMode::Pick);

    // --- Feature symbology (pick / overrides) ---
    // Ported from: itwinjs-core FeatureSymbology.ts addFeatureSymbology()
    if (featureMode != FeatureMode::None) {
        auto& vert = builder.getVertexBuilder();
        auto& frag = builder.getFragmentBuilder();

        // Feature ID attribute + varying
        // The attribute stays Uint (PolyfaceGraphic packs a_featureId as UINT), but
        // the VARYING must be float — GLSL requires flat interpolation for integer
        // varyings (the reference declares v_feature_id as Vec4; a bare `out uint`
        // without `flat` fails to compile on desktop GL).
        vert.addVariable({"a_featureId", VariableType::Uint, VariableScope::Attribute, 0});
        builder.addVarying("v_featureId", VariableType::Float);

        // Vertex: the feature id carries DIFFERENT values per mode, matching the
        // reference's two consumers (glsl/FeatureSymbology.ts):
        //   - Pick output: GLOBAL id = batch-local feature index + batch id
        //     (:514 v_feature_id = addUInt32s(u_batch_id, featureIndex)) — the
        //     pick buffer stores it; readPixels translates back via BatchState.
        //   - Overrides LUT lookup: LOCAL feature index ONLY (computeLUTCoords(
        //     getFeatureIndex()) — LookupTable.ts:24-28; the LUT is per-batch,
        //     rows are batch-local). Adding u_batchId here sampled beyond the
        //     single-feature LUT (garbage flags → no hilite / spurious discard).
        vert.addVariable({"u_batchId", VariableType::Float, VariableScope::Uniform, 0});
        if (featureMode == FeatureMode::Pick) {
            vert.setVertexComponent(VertexShaderComponent::ComputeFeatureOverrides,
                                    "    v_featureId = float(a_featureId) + u_batchId;\n");
        } else {
            // Overrides: the reference's computeFeatureOverrides PROLOGUE —
            // feature_rgb/feature_alpha start at "not overridden" sentinels
            // (FeatureSymbology.ts:615-616). GLSL globals default to (0,0,0);
            // without the -1 init, computeSurfaceFlags' `feature_rgb.r >= 0.0`
            // fires kSurfaceMask_OverrideRgb for EVERY fragment → texture color
            // replaced by the vertex color (blue-dot/chroma regressions).
            // The LUT-driven overrides are applied in the fragment
            // (OverrideFeatureId slot); DanQing's split of the reference body.
            vert.setVertexComponent(VertexShaderComponent::ComputeFeatureOverrides,
                                    "    v_featureId = float(a_featureId);\n"
                                    "    feature_rgb = vec3(-1.0);\n"
                                    "    feature_alpha = -1.0;\n");
        }

        if (featureMode == FeatureMode::Pick) {
            // Pick mode: the fragment output (declared by addFragData's
            // pickOutput path as `out uint fragColor`) carries the global
            // feature id; readPixels translates it to an element id.
            // 附件 1（fragDepthOrder）= 参考 addPickBufferOutputs 的 output2：
            // renderOrder*0.0625 + encodeDepthRgb(linearDepth)。u_renderOrder 在
            // DanQing 管线恒未上传（默认 0——与现网一致）；深度回读只消费 .yzw。
            frag.addFunction(kComputeLinearDepth.data());
            frag.addFunction(kEncodeDepthRgb.data());
            frag.setFragmentComponent(FragmentShaderComponent::AssignFragData,
                                      "    fragColor = uint(v_featureId + 0.5);\n"
                                      "    float linearDepthPick = computeLinearDepth(v_eyeSpace.z);\n"
                                      "    fragDepthOrder = vec4(0.0, encodeDepthRgb(linearDepthPick));\n");
        }

        if (featureMode == FeatureMode::Overrides) {
            // Feature override uniforms
            frag.addVariable({"u_featureOverrides", VariableType::Sampler2D, VariableScope::Uniform, 0});
            frag.addVariable({"u_featureOverrideWidth", VariableType::Float, VariableScope::Uniform, 0});
            frag.addVariable({"u_hiliteColor", VariableType::Vec4, VariableScope::Uniform, 0});
            // Flash uniforms（doApplyFlash，glsl/FeatureSymbology.ts:698-705：
            // u_flash_intensity 为 ProgramUniform 读 target.flashIntensity；
            // u_flash_mode 默认 Brighten=1（FlashSettings.ts:15-20 + :70）。）
            frag.addVariable({"u_flashIntensity", VariableType::Float, VariableScope::Uniform, 0});
            frag.addVariable({"u_flashMode", VariableType::Float, VariableScope::Uniform, 0});

            // Globals consumed by the override components (the reference declares them
            // in addFeatureSymbology, FeatureSymbology.ts:750-759 — the Surface-Overrides
            // variant compiled WITHOUT these, so the vertex stage referenced undeclared
            // identifiers and failed to link: 'feature_ignore_material'/'feature_rgb'
            // undeclared → the hilite-pass draw produced a broken program → 0 fragments).
            // Ported from: itwinjs-core FeatureSymbology.ts addFeatureSymbology().
            builder.addGlobal("feature_rgb", VariableType::Vec3);
            builder.addGlobal("feature_alpha", VariableType::Float);
            builder.addVarying("v_feature_emphasis", VariableType::Float);
            vert.addGlobal("feature_invisible", VariableType::Boolean, "false");
            vert.addGlobal("feature_viewIndependentTransparency", VariableType::Boolean, "false");
            vert.addGlobal("feature_ignore_material", VariableType::Boolean, "false");
            vert.addGlobal("use_material", VariableType::Boolean, "true");

            // OvrFlag bit constants and helper functions
            // Ported from: itwinjs-core FeatureSymbology.ts + FeatureEffectShaderBuilders.h
            frag.addFunction(getOvrFlagConstants());
            frag.addFunction(getExtractNthBit());

            // Feature override component
            // Ported from: itwinjs-core FeatureSymbology.ts applyFeatureSymbology()
            //               + doApplyFlash (:685-707 — hiliteRatio = visibleRatio,
            //                 默认 0.25，Hilite.ts:53)
            // LUT texel0 layout (FeatureOverrideLUT.writeFeatureTexels): R=OvrFlags,
            // G=OvrFlags16, B=lineCode, A=lineWeight. Read flags from .r and flags16
            // from .g — the prior .a/.g read texel0.A (lineWeight, always 0) as the
            // flags, so the Hilited mix never fired (and the Visibility read from
            // flags16.A=0 would have discarded the whole feature had it run).
            frag.setFragmentComponent(FragmentShaderComponent::OverrideFeatureId,
                // LUT 坐标：参考 addLookupTable(vert, "feature", "3.0")（FeatureSymbology.ts:255）
                // ——每特征 3 texel（FeatureOverrides._initialize 的 computeDimensions(n,3)），
                // texel0 = flags/flags16/lineCode/lineWeight。此前漏乘 3 → featureIndex>0
                // 全部采到别的特征的 texel（单特征 batch 恒 0 才没暴露——Deco 示例
                // 20 特征 batch 的悬停 flash 因此全灭）。
                "    float featureU = (float(v_featureId) * 3.0 + 0.5) * u_featureOverrideWidth;\n"
                "    vec4 overrideTexel = texture(u_featureOverrides, vec2(featureU, 0.5));\n"
                "    float ovrFlags = overrideTexel.r * 255.0;\n"
                "    float ovrFlags16 = overrideTexel.g * 255.0;\n"
                "    if (!nthBitSet(ovrFlags16, kOvrBit_Visibility)) discard;\n"
                "    if (nthBitSet(ovrFlags, kOvrBit_Rgb)) baseColor.rgb = overrideTexel.rgb;\n"
                "    if (nthBitSet(ovrFlags, kOvrBit_Alpha)) baseColor.a = overrideTexel.a;\n"
                "    if (nthBitSet(ovrFlags16, kOvrBit_Hilited)) baseColor.rgb = mix(baseColor.rgb, u_hiliteColor.rgb, 0.25);\n"
                // doApplyFlash（glsl/FeatureSymbology.ts:685-707）：Brighten（默认
                // 模式，u_flash_mode=1）取 brightRgb = baseColor + intensity×0.2；
                // Hilite 模式（=0）取 tweenRgb（向 hilite 色 0.75×intensity 过渡）。
                // intensity 是逐帧 uniform（processFlash 爬升，默认 0.25s 满强）。
                "    if (nthBitSet(ovrFlags, kOvrBit_Flashed)) {\n"
                "        float brighten = u_flashIntensity * 0.2;\n"
                "        vec3 brightRgb = baseColor.rgb + brighten;\n"
                "        float hiliteFraction = u_flashIntensity * 0.75;\n"
                "        vec3 tweenRgb = baseColor.rgb * (1.0 - hiliteFraction);\n"
                "        tweenRgb += u_hiliteColor.rgb * hiliteFraction;\n"
                "        baseColor.rgb = mix(tweenRgb, brightRgb, u_flashMode);\n"
                "    }\n"
                "    return baseColor;\n");
        }
    }

    // --- Planar classification ---
    // Ported from: itwinjs-core PlanarClassification.ts
    // TODO: ApplyPlanarClassifier slot body must become a vec4-returning function
    // body for `vec4 applyPlanarClassifications(vec4, float)` when the classified
    // path is wired. The glTF Surface-Opaque variant does not set `classified`, so
    // this slot is not emitted by the function-call path today.
    if (classified) {
        auto& frag = builder.getFragmentBuilder();
        frag.addFunction(getPlanarClassificationCode());
        frag.setFragmentComponent(FragmentShaderComponent::ApplyPlanarClassifier,
            "    fragColor = applyPlanarClassification(fragColor, v_eyeSpace);\n");
    }

    // --- Solar shadow mapping ---
    // Ported from: itwinjs-core SolarShadowMapping.ts addSolarShadowMapping() (line 93-140)
    if (shadowable) {
        addSolarShadowMap(builder);
    }

    // --- Thematic display ---
    // Ported from: itwinjs-core Thematic.ts addThematicDisplay() (line 214-336)
    if (thematic) {
        addThematicDisplay(builder);
    }

    // --- Clip planes ---
    // Ported from: itwinjs-core Surface.ts (clip distance output)
    {
        auto& vert = builder.getVertexBuilder();
        vert.addVariable({"u_numClipPlanes", VariableType::Int, VariableScope::Uniform, 0});
        vert.addVariable({"u_clipPlanes", VariableType::Vec4, VariableScope::Uniform, 6});
        vert.addCode("out float gl_ClipDistance[6];");
        vert.setVertexComponent(VertexShaderComponent::FinalizePosition,
            "    for (int i = 0; i < u_numClipPlanes; i++)\n"
            "        gl_ClipDistance[i] = dot(gl_Position.xyz, u_clipPlanes[i].xyz) + u_clipPlanes[i].w;\n"
            "    for (int i = u_numClipPlanes; i < 6; i++)\n"
            "        gl_ClipDistance[i] = 1.0;\n"
            "    return gl_Position;\n");
    }

    // --- Generate GLSL source ---
    std::string vert = builder.getVertexBuilder().buildSourceWithComponents();
    std::string frag = builder.getFragmentBuilder().buildSourceWithComponents();

    std::string description = std::string("Surface-") + flags.buildDescription();
    prog.setSource(std::move(vert), std::move(frag), std::move(description));

    // Explicit attribute locations — bound via glBindAttribLocation BEFORE link
    // (Program::attributeLocation → ShaderProgram::compile → OpenGLProgram::compile).
    // Matches PolyfaceGraphic's VAO (0=pos,1=normal,2=color,3=texCoord,4=featureId).
    // Without this, GL auto-assigns locations unpredictably and the cube's a_color
    // reads the wrong VBO slot (invisible cube). Ported from: itwinjs-core
    // AttributeMap (explicit per-technique attribute name→location mapping).
    prog.setAttributeMap({
        {"a_position", 0},
        {"a_normal", 1},
        {"a_color", 2},
        {"a_texCoord", 3},
        {"a_featureId", 4},
    });

    // --- Attach uniform bindings ---
    // The modular helpers (addFrustum, wireModelViewMatrix, etc.) registered
    // their bindings on the builder's ShaderBuilders. Transfer them to prog.
    builder.getVertexBuilder().addBindings(prog);
    builder.getFragmentBuilder().addBindings(prog);
}

// ---------------------------------------------------------------------------
// EdgeVariantCompiler::buildProgram
// Ported from: itwinjs-core Edge.ts createEdgeBuilder() (line 306-313).
// Uses modular EdgeShaderBuilder to compose the full edge shader with
// modelToWindowCoordinates, perpendicular offset, line code, and edge contrast.
// ---------------------------------------------------------------------------
void EdgeVariantCompiler::buildProgram(ShaderProgram& prog, TechniqueFlags const& flags)
{
    FeatureMode featureMode = flags.featureMode;
    PositionType posType = flags.positionType;

    auto builder = createEdgeProgramBuilder(EdgeBuilderType::SegmentEdge, featureMode, posType);

    std::string vert = builder.getVertexBuilder().buildSourceWithComponents();
    std::string frag = builder.getFragmentBuilder().buildSourceWithComponents();

    std::string description = std::string("Edge-") + flags.buildDescription();
    prog.setSource(std::move(vert), std::move(frag), std::move(description));

    // Transfer uniform bindings
    builder.getVertexBuilder().addBindings(prog);
    builder.getFragmentBuilder().addBindings(prog);
}

END_DQ_RENDER_NAMESPACE
