// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Shader uniform binding registrations (out-of-line)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/
//               Vertex.ts / Lighting.ts / Viewport.ts / Monochrome.ts
//
// Each function replaces an addUniform(..., nullptr) with a real ProgramUniform
// binding that reads from the target's uniform state at use() time.
// Defined here (not in the header) to avoid pulling ShaderProgramImpl.h /
// TargetImpl.h into lightweight *Shaders.h headers.
#include "ShaderBindings.h"
#include "DrawParams.h"         // DrawParams (for GraphicUniform bindings)
#include "ShaderProgramImpl.h"  // ShaderProgram, addProgramUniform, addGraphicUniform, ShaderProgramParams
#include "TargetImpl.h"         // getUniforms()/getClipStack()
#include "ClipStack.h"          // ClipStack（addClipping）
#include "shader/ClippingShaders.h"  // kClippingHelpers/kApplyClippingBody/kClipVolumeTextureUnit

BEGIN_DQ_RENDER_NAMESPACE

// u_proj — projection matrix (instanced vertex path).
// Ported from: itwinjs-core Vertex.ts addProjectionMatrix() line 8-14
// Reference: params.bindProjectionMatrix(uniform) -> target.uniforms.bindProjectionMatrix
// DanQing: use the frustum projection (instanced path is always 3D, not view-coords).
void wireProjectionMatrix(ShaderBuilder& vert)
{
    vert.addUniform("u_proj", VariableType::Mat4, [](ShaderProgram& prog) {
        prog.addProgramUniform("u_proj", [](UniformHandle& u, ShaderProgramParams const& p) {
            if (auto* t = p.getTarget())
                t->getUniforms().frustum.bindProjectionMatrix(u);
        });
    });
}

// u_sunDir — world-space sun direction (view-transformed in the binding).
// Ported from: itwinjs-core Lighting.ts addLighting() line 120-123
// Reference: params.target.uniforms.bindSunDirection(uniform)
void wireSunDirection(ShaderBuilder& frag)
{
    frag.addUniform("u_sunDir", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addProgramUniform("u_sunDir", [](UniformHandle& u, ShaderProgramParams const& p) {
            if (auto* t = p.getTarget())
                t->getUniforms().bindSunDirection(u);
        });
    }, VariablePrecision::High);
}

// u_lightSettings[16] — packed LightSettings array.
// Ported from: itwinjs-core Lighting.ts addLighting() line 126-129
// Reference: params.target.uniforms.lights.bind(uniform)
void wireLightSettings(ShaderBuilder& frag)
{
    frag.addUniform("u_lightSettings[16]", VariableType::Float, [](ShaderProgram& prog) {
        prog.addProgramUniform("u_lightSettings[0]", [](UniformHandle& u, ShaderProgramParams const& p) {
            if (auto* t = p.getTarget())
                t->getUniforms().lights.bind(u);
        });
    });
}

// u_upVector — view up vector for hemisphere lighting.
// Ported from: itwinjs-core Lighting.ts addLighting() line 132-135
// Reference: params.target.uniforms.frustum.bindUpVector(uniform)
void wireUpVector(ShaderBuilder& frag)
{
    frag.addUniform("u_upVector", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addProgramUniform("u_upVector", [](UniformHandle& u, ShaderProgramParams const& p) {
            if (auto* t = p.getTarget())
                t->getUniforms().frustum.bindUpVector(u);
        });
    });
}

// u_viewport — viewport width and height.
// Ported from: itwinjs-core Viewport.ts addViewport() line 6-10
// Reference: params.target.uniforms.viewRect.bindDimensions(uniform)
void wireViewport(ShaderBuilder& shader)
{
    shader.addUniform("u_viewport", VariableType::Vec2, [](ShaderProgram& prog) {
        prog.addProgramUniform("u_viewport", [](UniformHandle& u, ShaderProgramParams const& p) {
            if (auto* t = p.getTarget())
                t->getUniforms().viewRect.bindDimensions(u);
        });
    });
}

// u_viewportTransformation — viewport transform matrix.
// Ported from: itwinjs-core Viewport.ts addViewportTransformation() line 15-19
// Reference: params.target.uniforms.viewRect.bindViewportMatrix(uniform)
void wireViewportTransformation(ShaderBuilder& shader)
{
    shader.addUniform("u_viewportTransformation", VariableType::Mat4, [](ShaderProgram& prog) {
        prog.addProgramUniform("u_viewportTransformation", [](UniformHandle& u, ShaderProgramParams const& p) {
            if (auto* t = p.getTarget())
                t->getUniforms().viewRect.bindViewportMatrix(u);
        });
    });
}

// u_monoRgb — the monochrome color (program uniform).
// Ported from: itwinjs-core Monochrome.ts addMonoRgb() (:39-44 —
// `params.target.uniforms.style.bindMonochromeRgb(uniform)`). M-O(1)：此前
// addMonoRgb 以 nullptr 注册且无人喂数（位置起后 monoColor 也恒 0）——归位
// 为参考绑定（StyleUniforms.m_monoColor 随 TargetUniforms.updateRenderPlan
// 从 plan.monochromeColor 更新）。
void wireMonoRgb(ShaderBuilder& frag)
{
    frag.addUniform("u_monoRgb", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addProgramUniform("u_monoRgb", [](UniformHandle& u, ShaderProgramParams const& p) {
            if (auto* t = p.getTarget())
                t->getUniforms().style.bindMonochromeRgb(u);
        });
    });
}

// u_mixMonoColor — monochrome mix factor (1.0 = desaturate, 0.0 = passthrough).
// Ported from: itwinjs-core Monochrome.ts addSurfaceMonochrome (:46-50 — a
// GRAPHIC uniform: `Scaled === target.plan.monochromeMode && geometry.
// wantMixMonochromeColor(target) ? 1.0 : 0.0`). M-O(1)：自造常量 0.0 上传
// 归位为参考逐 draw 语义——值由 surface 分派点经 params.setFloat 上传
// （u_shaderFlags 同款 params 通道），此处只注册 uniform。
void wireMonochromeMix(ShaderBuilder& frag)
{
    frag.addUniform("u_mixMonoColor", VariableType::Float, nullptr);
}

// ==========================================================================
// GraphicUniforms (bound per draw-call, read from DrawParams)
// ==========================================================================

// u_mv — model-view matrix (non-instanced path).
// Ported from: itwinjs-core Vertex.ts addModelViewMatrix() line 38-41
// Reference: drawParams.uniforms.branch.bindModelViewMatrix(uniform)
// DanQing: mv is stored on DrawParams by the live draw loop.
// Mat4 dispatch: glUniform4fv on a mat4 location is INVALID_OPERATION
// (silently no-op'd — TD-15 transitional double-write hid it); setMatrix4
// is the typed path.
void wireModelViewMatrix(ShaderBuilder& vert)
{
    vert.addUniform("u_mv", VariableType::Mat4, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_mv", [](UniformHandle& u, DrawParams const& dp) {
            if (float const* mv = dp.getModelViewMatrix())
                u.setMatrix4(mv);
        });
    });
}

// u_materialColor — surface material RGBA.
// Ported from: itwinjs-core SurfaceMaterial.ts addMaterialColor()
// Reference: drawParams.uniforms.batch.materialColor → vec4
// DanQing: material color is stored on DrawParams by the live draw loop.
void wireMaterialColor(ShaderBuilder& vert)
{
    vert.addUniform("u_materialColor", VariableType::Vec4, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_materialColor", [](UniformHandle& u, DrawParams const& dp) {
            if (float const* rgba = dp.getMaterialColor())
                u.setUniform4fv(rgba, 1);
        });
    });
}

// u_surfaceFlags[12] — per-surface boolean flag array (SurfaceBitIndex).
// Ported from: itwinjs-core Surface.ts addSurfaceFlags() (line 519-527)
// Reference: params.geometry.asSurface.computeSurfaceFlags(...) → uniform.setUniform1iv(arr)
// DanQing: surface flags are stored on DrawParams by the live draw loop
//        (read from SurfaceGeometry::computeSurfaceFlags).
void wireSurfaceFlags(ShaderBuilder& vert)
{
    vert.addUniformArray("u_surfaceFlags", VariableType::Boolean,
        static_cast<int>(GL::SurfaceBitIndex::Count),
        [](ShaderProgram& prog) {
            prog.addGraphicUniform("u_surfaceFlags",
                [](UniformHandle& u, DrawParams const& dp) {
                    if (int const* flags = dp.getSurfaceFlags())
                        u.setUniform1iv(flags, static_cast<size_t>(GL::SurfaceBitIndex::Count));
                });
        });
}

// u_normalMatrix — surface normal matrix (transpose(inverse(mat3(mv)))).
// Ported from: itwinjs-core Vertex.ts addNormalMatrix()
// DanQing: normal matrix is computed from mv by the live draw loop
//        (computeNormalMatrixFromMv) and stored on DrawParams.
void wireNormalMatrix(ShaderBuilder& vert)
{
    vert.addUniform("u_normalMatrix", VariableType::Mat3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_normalMatrix", [](UniformHandle& u, DrawParams const& dp) {
            if (float const* nm = dp.getNormalMatrix())
                u.setMatrix3(nm);
        });
    });
}

// u_materialParams — surface material params (diffuse/specular weights + specular RGB/exponent).
// Ported from: itwinjs-core Surface.ts addMaterial() (line 214-220)
// DanQing: material params stored on DrawParams. RenderMaterialInternal has no
// fragUniforms field yet, so the draw loop uploads itwinjs defaults — TODO wire
// materialInfo.fragUniforms when RenderMaterialInternal is extended.
void wireMaterialParams(ShaderBuilder& vert)
{
    vert.addUniform("u_materialParams", VariableType::Vec4, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_materialParams", [](UniformHandle& u, DrawParams const& dp) {
            if (float const* mp = dp.getMaterialParams())
                u.setUniform4fv(mp, 1);
        });
    });
}

// ==========================================================================
// Animation displacement
// ==========================================================================

// u_animLUT — animation displacement LUT texture.
// Ported from: itwinjs-core Animation.ts addAnimation() (line 194-199)
void wireAnimLUT(ShaderBuilder& vert)
{
    vert.addUniform("u_animLUT", VariableType::Sampler2D, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_animLUT", [](UniformHandle& u, DrawParams const& dp) {
            // Texture binding is handled separately by the compositor
            // (driver.bindTexture at the appropriate texture unit).
            // The uniform just needs the texture unit index.
            (void)dp;
            u.setUniform1i(static_cast<int>(GL::TextureUnit::AuxChannelLUT));
        });
    });
}

// u_animLUTParams — animation LUT texture dimensions (width, height, numRgbaPerVert).
// Ported from: itwinjs-core Animation.ts addAnimation() (line 202-213)
void wireAnimLUTParams(ShaderBuilder& vert)
{
    vert.addUniform("u_animLUTParams", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_animLUTParams", [](UniformHandle& u, DrawParams const& dp) {
            if (float const* p = dp.getAnimLutParams())
                u.setUniform3fv(p);
        });
    });
}

// u_animDispParams — animation frame indices + interpolation fraction.
// Ported from: itwinjs-core Animation.ts addAnimation() (line 223-232)
void wireAnimDispParams(ShaderBuilder& vert)
{
    vert.addUniform("u_animDispParams", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_animDispParams", [](UniformHandle& u, DrawParams const& dp) {
            if (float const* p = dp.getAnimDispParams())
                u.setUniform3fv(p);
        });
    });
}

// u_qAnimDispScale — animation displacement quantization scale.
// Ported from: itwinjs-core Animation.ts addAnimation() (line 233-243)
void wireAnimDispScale(ShaderBuilder& vert)
{
    vert.addUniform("u_qAnimDispScale", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_qAnimDispScale", [](UniformHandle& u, DrawParams const& dp) {
            if (float const* p = dp.getQAnimDispScale())
                u.setUniform3fv(p);
        });
    });
}

// u_qAnimDispOrigin — animation displacement quantization origin.
// Ported from: itwinjs-core Animation.ts addAnimation() (line 244-254)
void wireAnimDispOrigin(ShaderBuilder& vert)
{
    vert.addUniform("u_qAnimDispOrigin", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_qAnimDispOrigin", [](UniformHandle& u, DrawParams const& dp) {
            if (float const* p = dp.getQAnimDispOrigin())
                u.setUniform3fv(p);
        });
    });
}


// ---------------------------------------------------------------------------
// addClipping — full view-clip fragment path（M-P P-D）
// Ported from: itwinjs-core glsl/Clipping.ts addClipping (:136-208)
// ---------------------------------------------------------------------------
void addClipping(ShaderBuilder& frag)
{
    // g_clipColor（prelude 全局——ShaderBuilder.ts:1017 prelude.addline）+
    // g_hasClipColor（Clipping.ts:163 addGlobal）。
    frag.addGlobal("g_clipColor", VariableType::Vec3);
    frag.addGlobal("g_hasClipColor", VariableType::BVec2);

    // u_outsideRgba — clip-outside color (alpha>0 → colorize, else discard)。
    // Ported from: Clipping.ts:142-152（outsideColor.bind）
    frag.addUniform("u_outsideRgba", VariableType::Vec4, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_outsideRgba", [](UniformHandle& u, DrawParams const& p) {
            if (auto* t = p.getTarget()) {
                auto const& c = t->getClipStack().outsideColor();
                float rgba[4] = {c.r, c.g, c.b, c.a};
                u.setUniform4fv(rgba, 1);
            }
        });
    });

    // u_insideRgba — clip-inside color。
    // Ported from: Clipping.ts:142-152（insideColor.bind）
    frag.addUniform("u_insideRgba", VariableType::Vec4, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_insideRgba", [](UniformHandle& u, DrawParams const& p) {
            if (auto* t = p.getTarget()) {
                auto const& c = t->getClipStack().insideColor();
                float rgba[4] = {c.r, c.g, c.b, c.a};
                u.setUniform4fv(rgba, 1);
            }
        });
    });

    // u_clipParams[3] — [0]=first plane, [1]=one past last, [2]=texture height。
    // Ported from: Clipping.ts:165-177（doClipping 恒 true）
    // NOTE：绑定名带 "[0]" 后缀——GLSL 数组 uniform 的 location 解析需要元素名
    //（u_lightSettings[0] 先例，ShaderBindings.cpp:49；裸数组名在多数驱动解析
    // -1 → TD-15 的 null-location 静默跳过 → M-P P-D 剖切无效果 saga 的根因）。
    frag.addUniformArray("u_clipParams", VariableType::Int, 3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_clipParams[0]", [](UniformHandle& u, DrawParams const& p) {
            if (auto* t = p.getTarget()) {
                auto const& stack = t->getClipStack();
                int clipParams[3] = {static_cast<int>(stack.startIndex()),
                                     static_cast<int>(stack.endIndex()),
                                     static_cast<int>(stack.textureHeight())};
                u.setUniform1iv(clipParams, 3);
            }
        });
    });

    // u_colorizeIntersection — program uniform（clipStack.colorizeIntersection）。
    // Ported from: Clipping.ts:179-183
    frag.addUniform("u_colorizeIntersection", VariableType::Boolean, [](ShaderProgram& prog) {
        prog.addProgramUniform("u_colorizeIntersection", [](UniformHandle& u, ShaderProgramParams const& p) {
            if (auto* t = p.getTarget())
                u.setUniform1i(t->getClipStack().colorizeIntersection() ? 1 : 0);
        });
    });

    // u_clipIntersection — intersection style（alpha 段=线宽，参考怪癖）。
    // Ported from: Clipping.ts:185-189（intersectionStyle.bind）
    frag.addUniform("u_clipIntersection", VariableType::Vec4, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_clipIntersection", [](UniformHandle& u, DrawParams const& p) {
            if (auto* t = p.getTarget()) {
                auto const& s = t->getClipStack().intersectionStyle();
                float rgba[4] = {s.r, s.g, s.b, s.a};
                u.setUniform4fv(rgba, 1);
            }
        });
    });

    // u_pixelWidthFactor — TODO（见 ShaderBindings.h 登记）：planes 面未移植，
    // 恒 0.0（colorizeIntersection 默认关 = 无行为面）。
    // Ported from: Clipping.ts:154-155 + FeatureSymbology.ts:498-504
    frag.addUniform("u_pixelWidthFactor", VariableType::Float, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_pixelWidthFactor", [](UniformHandle& u, DrawParams const&) {
            u.setUniform1f(0.0f);
        });
    });

    // s_clipSampler — clip 平面纹理（单元号经 uniform；纹理由 dispatch 绑定
    // 到 kClipVolumeTextureUnit——见头注 EQUIVALENCE：unit 0 被 s_texture 占）。
    // Ported from: Clipping.ts:194-201（TextureUnit.ClipVolume）
    frag.addUniform("s_clipSampler", VariableType::Sampler2D, [](ShaderProgram& prog) {
        prog.addGraphicUniform("s_clipSampler", [](UniformHandle& u, DrawParams const&) {
            u.setUniform1i(kClipVolumeTextureUnit);
        });
    }, VariablePrecision::High);

    // 辅助函数（getClipPlane/calcClipPlaneDist——Clipping.ts:191-193
    // frag.addFunction）+ ApplyClipping 槽体（Clipping.ts:203 frag.set）。
    frag.addFunction(kClippingHelpers);
    frag.setFragmentComponent(FragmentShaderComponent::ApplyClipping, kApplyClippingBody);
}

END_DQ_RENDER_NAMESPACE
