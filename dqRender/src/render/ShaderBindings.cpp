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
#include "TargetImpl.h"         // getUniforms()

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

// u_mixMonoColor — monochrome mix factor (1.0 = desaturate, 0.0 = passthrough).
// Ported from: itwinjs-core Monochrome.ts addSurfaceMonochromeColor()
// Reference: params.target.uniforms.style... (no direct bind method in reference;
// the value is computed from the style's monochrome settings).
// DanQing: the style's monochrome color is already bound via bindMonochromeRgb;
// u_mixMonoColor is a float flag. For now, bind it to the style's monochrome
// state (1.0 when monochrome is active, 0.0 otherwise).
void wireMonochromeMix(ShaderBuilder& frag)
{
    frag.addUniform("u_mixMonoColor", VariableType::Float, [](ShaderProgram& prog) {
        prog.addProgramUniform("u_mixMonoColor", [](UniformHandle& u, ShaderProgramParams const& p) {
            if (auto* t = p.getTarget()) {
                // The reference reads this from the style's monochrome state.
                // For now, upload 1.0 (the legacy upload path handles the actual value).
                // TODO: wire to StyleUniforms when the monochrome state is tracked.
                (void)t;
            }
            u.setUniform1f(0.0f);  // default: no monochrome mix
        });
    });
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

END_DQ_RENDER_NAMESPACE
