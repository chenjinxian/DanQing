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

END_DQ_RENDER_NAMESPACE
