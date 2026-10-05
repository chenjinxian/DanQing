// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Shader uniform binding registrations
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/
//               Vertex.ts / Lighting.ts / Viewport.ts / Monochrome.ts
//
// These functions wire glsl-module addUniform calls to real ProgramUniform
// bindings that read from the target's uniform state at use() time.
// Defined out-of-line in ShaderBindings.cpp (needs ShaderProgramImpl.h +
// TargetImpl.h); declared here so lightweight *Shaders.h headers can call
// them without pulling in heavy internal includes.
#pragma once

#include "ShaderBuilder.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// --- ProgramUniforms (bound once per use(), read from target.uniforms) ---

/// Wire u_proj (projection matrix, instanced path).
/// Ported from: itwinjs-core Vertex.ts addProjectionMatrix() line 8-14
void wireProjectionMatrix(ShaderBuilder& vert);

/// Wire u_sunDir (world-space sun direction, view-transformed).
/// Ported from: itwinjs-core Lighting.ts addLighting() line 120-123
void wireSunDirection(ShaderBuilder& frag);

/// Wire u_lightSettings[16] (packed LightSettings array).
/// Ported from: itwinjs-core Lighting.ts addLighting() line 126-129
void wireLightSettings(ShaderBuilder& frag);

/// Wire u_upVector (view up vector for hemisphere lighting).
/// Ported from: itwinjs-core Lighting.ts addLighting() line 132-135
void wireUpVector(ShaderBuilder& frag);

/// Wire u_viewport (viewport width, height).
/// Ported from: itwinjs-core Viewport.ts addViewport() line 6-10
void wireViewport(ShaderBuilder& shader);

/// Wire u_viewportTransformation (viewport transform matrix).
/// Ported from: itwinjs-core Viewport.ts addViewportTransformation() line 15-19
void wireViewportTransformation(ShaderBuilder& shader);

/// Wire u_monoRgb (monochrome color — program uniform fed by StyleUniforms).
/// Ported from: itwinjs-core Monochrome.ts addMonoRgb() (:39-44)
void wireMonoRgb(ShaderBuilder& frag);

/// Wire u_mixMonoColor (monochrome mix factor).
/// Ported from: itwinjs-core Monochrome.ts addSurfaceMonochromeColor()
void wireMonochromeMix(ShaderBuilder& frag);

// --- GraphicUniforms (bound per draw-call, read from DrawParams) ---

/// Wire u_mv (model-view matrix, non-instanced path).
/// Ported from: itwinjs-core Vertex.ts addModelViewMatrix() line 38-41
void wireModelViewMatrix(ShaderBuilder& vert);

/// Wire u_materialColor (surface material RGBA).
/// Ported from: itwinjs-core SurfaceMaterial.ts addMaterialColor()
void wireMaterialColor(ShaderBuilder& vert);

/// Wire u_surfaceFlags[12] (per-surface boolean flag array, SurfaceBitIndex).
/// Ported from: itwinjs-core Surface.ts addSurfaceFlags() (line 519-527)
void wireSurfaceFlags(ShaderBuilder& vert);

/// Wire u_normalMatrix (surface normal matrix, mat3).
/// Ported from: itwinjs-core Vertex.ts addNormalMatrix()
void wireNormalMatrix(ShaderBuilder& vert);

/// Wire u_materialParams (surface material params, vec4).
/// Ported from: itwinjs-core Surface.ts addMaterial() (line 214-220)
void wireMaterialParams(ShaderBuilder& vert);

// --- Animation displacement ---

/// Wire u_animLUT (animation displacement LUT texture).
/// Ported from: itwinjs-core Animation.ts addAnimation() (line 194-199)
void wireAnimLUT(ShaderBuilder& vert);

/// Wire u_animLUTParams (animation LUT texture dimensions).
/// Ported from: itwinjs-core Animation.ts addAnimation() (line 202-213)
void wireAnimLUTParams(ShaderBuilder& vert);

/// Wire u_animDispParams (animation frame indices + fraction).
/// Ported from: itwinjs-core Animation.ts addAnimation() (line 223-232)
void wireAnimDispParams(ShaderBuilder& vert);

/// Wire u_qAnimDispScale (animation displacement quantization scale).
/// Ported from: itwinjs-core Animation.ts addAnimation() (line 233-243)
void wireAnimDispScale(ShaderBuilder& vert);

/// Wire u_qAnimDispOrigin (animation displacement quantization origin).
/// Ported from: itwinjs-core Animation.ts addAnimation() (line 244-254)
void wireAnimDispOrigin(ShaderBuilder& vert);

/// Wire the full view-clip fragment path（M-P P-D）。
/// Ported from: itwinjs-core glsl/Clipping.ts addClipping (:136-208)：
/// u_outsideRgba/u_insideRgba（graphic ← clipStack 颜色）+ u_clipParams[3]
///（graphic ← startIndex/endIndex/textureHeight）+ s_clipSampler（单元号；
/// 纹理由 dispatch 绑定）+ u_colorizeIntersection（program uniform）+
/// u_clipIntersection（graphic ← intersectionStyle）+ u_pixelWidthFactor
///（graphic ← PixelWidthFactor 移植计算）+ g_clipColor/g_hasClipColor 全局 +
/// ApplyClipping 槽（kClippingFunctions）。
/// 前置（参考 addEyeSpace/addFrustum/addModelViewMatrix）：宿主 surface builder
/// 经 createCommon 已含 v_eyeSpace/u_frustum/u_mv——不重复添加。
/// §3.4/EQUIVALENCE：translucent 的 AssignFragData clip 变体（参考 :205-207，
/// FragColor1=(1,1,0,1) revealage 标记）随 DanQing 单输出 OIT 结构登记 TODO
///（复合层协作面）；u_pixelWidthFactor 的 PixelWidthFactor 类（TargetUniforms.ts
/// :28-68）TODO：计算需 FrustumUniforms 的 planes 面（top/bottom/left/right，
/// 未移植）——恒绑 0.0（colorizeIntersection 默认关，交线着色距离阈值 0 =
/// 不着色；随 planes 面落地补全计算）。
void addClipping(ShaderBuilder& frag);



END_DQ_RENDER_NAMESPACE
