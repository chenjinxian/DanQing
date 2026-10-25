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
#include "AuxChannelLut.h"       // AuxChannelLUT（W-a5 动画通道绑定读面）
#include "AuxChannelTable.h"     // AuxChannel/AuxDisplacementChannel/AuxParamChannel
#include "DrawParams.h"         // DrawParams (for GraphicUniform bindings)
#include "ShaderProgramImpl.h"  // ShaderProgram, addProgramUniform, addGraphicUniform, ShaderProgramParams
#include "SurfaceGeometry.h"    // SurfaceGeometry::auxChannels（asSurface 读面）
#include "TargetImpl.h"         // getUniforms()/getClipStack()/getAnalysisFraction/getAnalysisStyle
#include "ClipStack.h"          // ClipStack（addClipping）
#include "shader/ClippingShaders.h"  // kClippingHelpers/kApplyClippingBody/kClipVolumeTextureUnit

#include <dqCommon/ThematicDisplay.h>  // ThematicGradientSettings::margin/contentRange
#include <dqCommon/AnalysisStyle.h>    // AnalysisStyle（displacement/scalar/normalChannelName）

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

// u_numColors — material-atlas color-table base row (vertex LUT offset).
// Ported from: itwinjs-core Surface.ts addMaterial u_numColors
//   (uniform.setUniform1f(numColors) — info.vertexTableOffset, :538-542).
// DanQing: the atlas base row is stored on DrawParams by the live draw loop
//        (0 for non-atlas geometry — readMaterialAtlas produces no override).
void wireNumColors(ShaderBuilder& vert)
{
    vert.addUniform("u_numColors", VariableType::Float, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_numColors", [](UniformHandle& u, DrawParams const& dp) {
            u.setUniform1f(dp.getNumColors());
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

// W-a5：绑定语义归位——参考 lambda 内读 params.geometry 的 auxChannels + target 的
// analysisStyle/analysisFraction 就地计算（Animation.ts:194-303）；DanQing 桩期的
// DrawParams 预置指针面（getAnimLutParams 等——从未有喂数方，ported-but-uncalled）
// 全部退役为就地计算。

namespace detail {

// 几何的 auxChannels 读取缝（参考 params.geometry.asLUT?.lut.auxChannels /
// asMesh?.lut.auxChannels——Animation.ts:150/:159/:167；asMesh/asLUT 的参考不一致
// 原样注记：两者对网格几何同指 SurfaceGeometry.auxChannels）。
static AuxChannelLUT const* geometryAuxChannels(DrawParams const& dp)
{
    auto* geom = dp.getGeometry();
    auto* surface = geom ? geom->asSurface() : nullptr;
    return surface ? surface->auxChannels() : nullptr;
}

static float targetAnalysisFraction(DrawParams const& dp)
{
    auto* target = dp.getTarget();
    return target ? target->getAnalysisFraction() : 0.0f;
}

// Ported from: Animation.ts:171-184 computeAnimParams。
void computeAnimParams(float* params, AuxChannel const& channel, float fraction)
{
    auto const& inputs = channel.inputs();
    auto const& indices = channel.indices();
    if (inputs.empty() || indices.empty())
        return;
    float const inputValue = fraction * inputs[inputs.size() - 1];
    for (size_t i = 0; i + 1 < inputs.size(); i++) {
        if (inputValue >= inputs[i] && inputValue < inputs[i + 1]) {
            params[0] = static_cast<float>(indices[i]);
            params[1] = static_cast<float>(indices[i + 1]);
            // **运算符优先级怪癖原样**（:178）：除法先算——
            // inputValue - (inputs[i] / (inputs[i+1] - inputs[i]))。
            params[2] = inputValue - inputs[i] / (inputs[i + 1] - inputs[i]);
            return;
        }
    }
    params[0] = params[1] = static_cast<float>(indices[inputs.size() - 1]);
    params[2] = 0.0f;
}

// Ported from: Animation.ts:145-152 getDisplacementChannel。
struct DisplacementFound {
    AuxDisplacementChannel const* channel;
    dqCommon::AnalysisStyleDisplacement const* displacement;
};
static std::optional<DisplacementFound> getDisplacementChannel(DrawParams const& dp)
{
    auto* target = dp.getTarget();
    auto const* style = target ? target->getAnalysisStyle() : nullptr;
    if (!style || !style->displacement.has_value())
        return std::nullopt;
    auto const* aux = geometryAuxChannels(dp);
    if (!aux)
        return std::nullopt;
    auto it = aux->displacements.find(style->displacement->channelName);
    if (it == aux->displacements.end())
        return std::nullopt;
    return DisplacementFound{&it->second, &*style->displacement};
}

// Ported from: Animation.ts:154-160 getNormalChannel。
static AuxChannel const* getNormalChannel(DrawParams const& dp)
{
    auto* target = dp.getTarget();
    auto const* style = target ? target->getAnalysisStyle() : nullptr;
    if (!style || !style->normalChannelName.has_value())
        return nullptr;
    auto const* aux = geometryAuxChannels(dp);
    if (!aux)
        return nullptr;
    auto it = aux->normals.find(*style->normalChannelName);
    return it != aux->normals.end() ? &it->second : nullptr;
}

// Ported from: Animation.ts:162-169 getScalarChannel。
struct ScalarFound {
    AuxParamChannel const* channel;
    dqCommon::AnalysisStyleThematic const* scalar;
};
static std::optional<ScalarFound> getScalarChannel(DrawParams const& dp)
{
    auto* target = dp.getTarget();
    auto const* style = target ? target->getAnalysisStyle() : nullptr;
    if (!style || !style->scalar.has_value())
        return std::nullopt;
    auto const* aux = geometryAuxChannels(dp);
    if (!aux)
        return std::nullopt;
    auto it = aux->params.find(style->scalar->channelName);
    if (it == aux->params.end())
        return std::nullopt;
    return ScalarFound{&it->second, &*style->scalar};
}

}  // namespace detail

// u_animLUT — animation displacement LUT texture.
// Ported from: itwinjs-core Animation.ts addAnimation() (line 194-199)
// EQUIVALENCE：参考经 channels.texture.bindSampler(uniform, TextureUnit.AuxChannelLUT)
// 在绑定内绑纹理；DanQing 的 UniformHandle 面无 driver——纹理由 dispatch 侧在
// TextureUnit::AuxChannelLUT 单元绑定（与 u_vertLUT 的 DanQing 处理同族），本绑定
// 仅置单元号。发散=绑定点拆分；验证法=W-a6 E2E 位移像素锁。
// 单元换算（M-W 回修——E2E 零内容根因之一）：参考 bindSampler 置位
// `unit - TextureUnit.Zero`（Texture.ts:462——sampler 持单元**下标**）；
// GL::TextureUnit 枚举持 GL 枚举值（Zero=0x84C0）——本绑定曾直传枚举值
// （33990）→ shader 采样无效单元。同式换算。
void wireAnimLUT(ShaderBuilder& vert)
{
    vert.addUniform("u_animLUT", VariableType::Sampler2D, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_animLUT", [](UniformHandle& u, DrawParams const& dp) {
            (void)dp;
            u.setUniform1i(static_cast<int>(GL::TextureUnit::AuxChannelLUT)
                           - static_cast<int>(GL::TextureUnit::Zero));
        });
    });
}

// u_animLUTParams — animation LUT texture dimensions (width, height, numBytesPerVert/2).
// Ported from: itwinjs-core Animation.ts addAnimation() (line 202-213)
void wireAnimLUTParams(ShaderBuilder& vert)
{
    vert.addUniform("u_animLUTParams", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_animLUTParams", [](UniformHandle& u, DrawParams const& dp) {
            float array[3] = {0.0f, 0.0f, 0.0f};
            if (auto const* aux = detail::geometryAuxChannels(dp)) {
                array[0] = static_cast<float>(aux->width());
                array[1] = static_cast<float>(aux->height());
                array[2] = static_cast<float>(aux->numBytesPerVertex()) / 2.0f;
            }
            u.setUniform3fv(array);
        });
    });
}

// u_animDispParams — animation frame indices + interpolation fraction.
// Ported from: itwinjs-core Animation.ts addAnimation() (line 223-232)
void wireAnimDispParams(ShaderBuilder& vert)
{
    vert.addUniform("u_animDispParams", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_animDispParams", [](UniformHandle& u, DrawParams const& dp) {
            float animParams[3] = {0.0f, 0.0f, 0.0f};
            if (auto const disp = detail::getDisplacementChannel(dp))
                detail::computeAnimParams(animParams, *disp->channel,
                                          detail::targetAnalysisFraction(dp));
            u.setUniform3fv(animParams);
        });
    });
}

// u_qAnimDispScale — animation displacement quantization scale.
// Ported from: itwinjs-core Animation.ts addAnimation() (line 233-243)
void wireAnimDispScale(ShaderBuilder& vert)
{
    vert.addUniform("u_qAnimDispScale", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_qAnimDispScale", [](UniformHandle& u, DrawParams const& dp) {
            float animParams[3] = {0.0f, 0.0f, 0.0f};
            if (auto const disp = detail::getDisplacementChannel(dp))
                for (int i = 0; i < 3; i++)
                    animParams[i] = disp->channel->qScale()[i]
                        * static_cast<float>(disp->displacement->scale);
            u.setUniform3fv(animParams);
        });
    });
}

// u_qAnimDispOrigin — animation displacement quantization origin.
// Ported from: itwinjs-core Animation.ts addAnimation() (line 244-254)
void wireAnimDispOrigin(ShaderBuilder& vert)
{
    vert.addUniform("u_qAnimDispOrigin", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_qAnimDispOrigin", [](UniformHandle& u, DrawParams const& dp) {
            float animParams[3] = {0.0f, 0.0f, 0.0f};
            if (auto const disp = detail::getDisplacementChannel(dp))
                for (int i = 0; i < 3; i++)
                    animParams[i] = disp->channel->qOrigin()[i]
                        * static_cast<float>(disp->displacement->scale);
            u.setUniform3fv(animParams);
        });
    });
}

// u_animNormalParams — normal channel frame indices + fraction（isSurface 半）。
// Ported from: itwinjs-core Animation.ts addAnimation() (line 265-274)
void wireAnimNormalParams(ShaderBuilder& vert)
{
    vert.addUniform("u_animNormalParams", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_animNormalParams", [](UniformHandle& u, DrawParams const& dp) {
            float animParams[3] = {-1.0f, -1.0f, -1.0f};
            if (auto const* channel = detail::getNormalChannel(dp))
                detail::computeAnimParams(animParams, *channel,
                                          detail::targetAnalysisFraction(dp));
            u.setUniform3fv(animParams);
        });
    });
}

// u_animScalarParams — scalar channel frame indices + fraction（isSurface 半）。
// Ported from: itwinjs-core Animation.ts addAnimation() (line 276-285)
void wireAnimScalarParams(ShaderBuilder& vert)
{
    vert.addUniform("u_animScalarParams", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_animScalarParams", [](UniformHandle& u, DrawParams const& dp) {
            float animParams[3] = {-1.0f, -1.0f, -1.0f};
            if (auto const scalars = detail::getScalarChannel(dp))
                detail::computeAnimParams(animParams, *scalars->channel,
                                          detail::targetAnalysisFraction(dp));
            u.setUniform3fv(animParams);
        });
    });
}

// u_animScalarQParams — scalar 量化参数（margin+contentRange 归一映射——
// ThematicGradientSettings.margin/contentRange，M-S 渐变机制复用）。
// Ported from: itwinjs-core Animation.ts addAnimation() (line 287-303)
void wireAnimScalarQParams(ShaderBuilder& vert)
{
    vert.addUniform("u_animScalarQParams", VariableType::Vec2, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_animScalarQParams", [](UniformHandle& u, DrawParams const& dp) {
            float animParams[2] = {1.0f, 1.0f};
            if (auto const scalars = detail::getScalarChannel(dp)) {
                auto const& range = scalars->scalar->range;
                double rangeScale = range.high - range.low;
                if (rangeScale == 0.0)
                    rangeScale = 1.0;

                animParams[0] = static_cast<float>(
                    dqCommon::ThematicGradientSettings::margin()
                    + (scalars->channel->qOrigin() - range.low) / rangeScale);
                animParams[1] = static_cast<float>(
                    dqCommon::ThematicGradientSettings::contentRange()
                    * scalars->channel->qScale() / rangeScale);
            }
            u.setUniform2fv(animParams);
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
