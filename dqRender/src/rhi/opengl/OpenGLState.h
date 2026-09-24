// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGL state tracking/cache
// Ported from: filament backend/src/opengl/OpenGLState.h
//
// Wraps every glEnable/glBind*/glBlend* etc. call with a comparison against
// cached state, avoiding redundant GL calls.
#pragma once

#include "Gl.h"
#include "dqRender/rhi/DriverEnums.h"

#include <array>
#include <bitset>
#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// OpenGLState — GL state cache
// ---------------------------------------------------------------------------
class OpenGLState {
public:
    OpenGLState() noexcept = default;
    void initialize();

    // --- State cache ---
    struct State {
        GLuint drawFbo = 0;
        GLuint readFbo = 0;

        struct {
            GLuint use = 0;
        } program;

        struct {
            GLenum frontFace = GL_CCW;
            GLenum cullFace = GL_BACK;
            GLenum blendEquationRGB = GL_FUNC_ADD;
            GLenum blendEquationA = GL_FUNC_ADD;
            GLenum blendFunctionSrcRGB = GL_ONE;
            GLenum blendFunctionSrcA = GL_ONE;
            GLenum blendFunctionDstRGB = GL_ZERO;
            GLenum blendFunctionDstA = GL_ZERO;
            GLboolean colorMask = GL_TRUE;
            GLboolean depthMask = GL_TRUE;
            GLenum depthFunc = GL_LEQUAL;
        } raster;

        struct {
            struct {
                GLenum func = GL_ALWAYS;
                GLenum stencilFail = GL_KEEP;
                GLenum depthFail = GL_KEEP;
                GLenum stencilDepthPass = GL_KEEP;
                GLuint mask = 0xff;
            } front, back;
            GLuint readMask = 0xff;
            GLuint ref = 0;
        } stencil;

        struct {
            GLfloat factor = 0.0f;
            GLfloat units = 0.0f;
        } polygonOffset;

        struct {
            std::bitset<12> caps;  // indexed by getIndexForCap()
        } enables;

        struct {
            GLint viewport[4] = {0, 0, 0, 0};
            GLint scissor[4] = {0, 0, 0, 0};
            GLfloat depthRange[2] = {0.0f, 1.0f};
        } window;

        struct {
            GLuint active = 0;
            struct {
                GLuint id = 0;
                GLenum target = GL_TEXTURE_2D;
            } units[32];  // MAX_TEXTURE_UNIT_COUNT
        } textures;
    } state;

    // --- State change methods ---
    void reset() noexcept;
    void useProgram(GLuint program) noexcept;
    void bindFramebuffer(GLenum target, GLuint fbo) noexcept;
    void enable(GLenum cap) noexcept;
    void disable(GLenum cap) noexcept;
    void depthFunc(GLenum func) noexcept;
    void depthMask(GLboolean mask) noexcept;
    void colorMask(GLboolean mask) noexcept;
    void blendEquation(GLenum rgb, GLenum alpha) noexcept;
    void blendFunc(GLenum srcRGB, GLenum dstRGB, GLenum srcA, GLenum dstA) noexcept;
    void cullFace(GLenum mode) noexcept;
    void frontFace(GLenum mode) noexcept;
    void polygonOffset(GLfloat factor, GLfloat units) noexcept;
    void activeTexture(GLuint unit) noexcept;
    void bindTexture(GLuint unit, GLenum target, GLuint texture) noexcept;
    // Invalidate cached bindings pointing at `texture` — the name may be recycled
    // by a subsequent glGenTextures after glDeleteTextures, so the cache (which
    // keys on the GL name) must not short-circuit the next bindTexture. Without
    // this, a per-frame-recreated texture (e.g. the Polyline LUT) reuses a freed
    // name, bindTexture is a cache hit, glBindTexture is skipped, and the new
    // texture's storage/upload goes to the deleted binding → the shader samples
    // an empty texture → 0 fragments.
    void invalidateTexture(GLuint texture) noexcept;
    void viewport(GLint x, GLint y, GLsizei w, GLsizei h) noexcept;
    void scissor(GLint x, GLint y, GLsizei w, GLsizei h) noexcept;
    void stencilFunc(GLenum func, GLuint ref, GLuint mask, bool front) noexcept;
    void stencilOp(GLenum sfail, GLenum dpfail, GLenum dppass, bool front) noexcept;
    void stencilMask(GLuint mask, bool front) noexcept;

private:
    static int getIndexForCap(GLenum cap) noexcept;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
