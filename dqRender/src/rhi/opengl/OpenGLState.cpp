// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGL state tracking implementation
// Ported from: filament backend/src/opengl/OpenGLState.cpp
//
// DEVIATION（2026-09-12，Grid/ACS 真窗口黑屏根因链）：filament 的去重缓存
// （state 命中即跳过 GL 调用）在 DanQing 不成立——本进程存在不经本 tracker 的
// 裸 GL 写入方：(a) itwinjs 移植的 RenderState::apply 通道（裸 glEnable/
// glDepthMask/glBlendFuncSeparate 等，参考机制本身如此）；(b) RHI 资源创建与
// blit 路径的裸 glBindFramebuffer/glBindTexture。裸写入使 tracker 的缓存与
// 真实 GL 状态分叉，随后"缓存命中跳过"把本应发出的状态设置吞掉——实例：
//   - RenderState 通道 glDepthMask(GL_FALSE)（translucent pass）后，下一帧
//     OIT 清屏的 m_state.depthMask(GL_TRUE) 被去重跳过 → glClear(DEPTH) 失效
//     → OIT 深度恒 0 → translucent pass 片元全灭（真窗口网格不可见根因）；
//   - RenderState 通道 glEnable(GL_BLEND)+加法因子后，composite 的 RHI 侧关
//     混合被跳过 → 合成 quad 加法叠加 → 天空饱和爆白。
// 因此本文件的所有 mutator 一律直发 GL（幂等，last-writer-wins），state 仅作
// 记录保留（reset()/自省用途）。filament 原版无裸写入方，去重才安全——此处
// 是与 filament 的有意偏差（正确性优先于省几个冗余 GL 调用）。
#include "OpenGLState.h"

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

void OpenGLState::initialize()
{
    // Reset all cached state to force re-application
    state = State{};
}

void OpenGLState::reset() noexcept
{
    // invalidate all cached state so the next GL call will actually happen.
    // Use impossible values that won't match any real GL state.
    state.drawFbo = ~0u;
    state.readFbo = ~0u;
    state.program.use = ~0u;
    state.raster.frontFace = 0;
    state.raster.cullFace = 0;
    state.raster.blendEquationRGB = 0;
    state.raster.blendEquationA = 0;
    state.raster.blendFunctionSrcRGB = 0;
    state.raster.blendFunctionSrcA = 0;
    state.raster.blendFunctionDstRGB = 0;
    state.raster.blendFunctionDstA = 0;
    state.raster.colorMask = 0;
    state.raster.depthMask = 0;
    state.raster.depthFunc = 0;
    state.enables.caps.reset();
    state.window.viewport[0] = state.window.viewport[1] = -1;
    state.window.viewport[2] = state.window.viewport[3] = -1;
}

void OpenGLState::useProgram(GLuint program) noexcept
{
    glUseProgram(program);
    state.program.use = program;
}

void OpenGLState::bindFramebuffer(GLenum target, GLuint fbo) noexcept
{
    // GL_FRAMEBUFFER binds BOTH the read and draw framebuffer targets.
    // Matches filament: GL_READ_FRAMEBUFFER→readFbo,
    // GL_DRAW_FRAMEBUFFER→drawFbo, GL_FRAMEBUFFER→both.
    if (target == GL_READ_FRAMEBUFFER) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        state.readFbo = fbo;
    } else if (target == GL_DRAW_FRAMEBUFFER) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        state.drawFbo = fbo;
    } else {  // GL_FRAMEBUFFER — binds both read and draw
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        state.drawFbo = fbo;
        state.readFbo = fbo;
    }
}

void OpenGLState::enable(GLenum cap) noexcept
{
    int idx = getIndexForCap(cap);
    glEnable(cap);
    if (idx >= 0)
        state.enables.caps.set(static_cast<size_t>(idx));
}

void OpenGLState::disable(GLenum cap) noexcept
{
    int idx = getIndexForCap(cap);
    glDisable(cap);
    if (idx >= 0)
        state.enables.caps.reset(static_cast<size_t>(idx));
}

void OpenGLState::depthFunc(GLenum func) noexcept
{
    glDepthFunc(func);
    state.raster.depthFunc = func;
}

void OpenGLState::depthMask(GLboolean mask) noexcept
{
    glDepthMask(mask);
    state.raster.depthMask = mask;
}

void OpenGLState::colorMask(GLboolean mask) noexcept
{
    GLboolean m = mask;
    glColorMask(m, m, m, m);
    state.raster.colorMask = mask;
}

void OpenGLState::blendEquation(GLenum rgb, GLenum alpha) noexcept
{
    glBlendEquationSeparate(rgb, alpha);
    state.raster.blendEquationRGB = rgb;
    state.raster.blendEquationA = alpha;
}

void OpenGLState::blendFunc(GLenum srcRGB, GLenum dstRGB, GLenum srcA, GLenum dstA) noexcept
{
    glBlendFuncSeparate(srcRGB, dstRGB, srcA, dstA);
    state.raster.blendFunctionSrcRGB = srcRGB;
    state.raster.blendFunctionDstRGB = dstRGB;
    state.raster.blendFunctionSrcA = srcA;
    state.raster.blendFunctionDstA = dstA;
}

void OpenGLState::cullFace(GLenum mode) noexcept
{
    glCullFace(mode);
    state.raster.cullFace = mode;
}

void OpenGLState::frontFace(GLenum mode) noexcept
{
    glFrontFace(mode);
    state.raster.frontFace = mode;
}

void OpenGLState::polygonOffset(GLfloat factor, GLfloat units) noexcept
{
    glPolygonOffset(factor, units);
    state.polygonOffset.factor = factor;
    state.polygonOffset.units = units;
}

void OpenGLState::activeTexture(GLuint unit) noexcept
{
    glActiveTexture(GL_TEXTURE0 + unit);
    state.textures.active = unit;
}

void OpenGLState::bindTexture(GLuint unit, GLenum target, GLuint texture) noexcept
{
    activeTexture(unit);
    glBindTexture(target, texture);
    state.textures.units[unit].id = texture;
    state.textures.units[unit].target = target;
}

void OpenGLState::invalidateTexture(GLuint texture) noexcept
{
    // Clear any cached unit binding that points at `texture` (the name may be
    // recycled after glDeleteTextures, so the cache must not short-circuit the
    // next bindTexture).
    for (auto& u : state.textures.units) {
        if (u.id == texture) {
            u.id = 0;
            u.target = GL_TEXTURE_2D;
        }
    }
}

void OpenGLState::viewport(GLint x, GLint y, GLsizei w, GLsizei h) noexcept
{
    glViewport(x, y, w, h);
    state.window.viewport[0] = x;
    state.window.viewport[1] = y;
    state.window.viewport[2] = w;
    state.window.viewport[3] = h;
}

void OpenGLState::scissor(GLint x, GLint y, GLsizei w, GLsizei h) noexcept
{
    glScissor(x, y, w, h);
    state.window.scissor[0] = x;
    state.window.scissor[1] = y;
    state.window.scissor[2] = w;
    state.window.scissor[3] = h;
}

void OpenGLState::stencilFunc(GLenum func, GLuint ref, GLuint mask, bool front) noexcept
{
    auto& s = front ? state.stencil.front : state.stencil.back;
    GLenum face = front ? GL_FRONT : GL_BACK;
    glStencilFuncSeparate(face, func, ref, mask);
    s.func = func;
    state.stencil.ref = ref;
    state.stencil.readMask = mask;
}

void OpenGLState::stencilOp(GLenum sfail, GLenum dpfail, GLenum dppass, bool front) noexcept
{
    auto& s = front ? state.stencil.front : state.stencil.back;
    GLenum face = front ? GL_FRONT : GL_BACK;
    glStencilOpSeparate(face, sfail, dpfail, dppass);
    s.stencilFail = sfail;
    s.depthFail = dpfail;
    s.stencilDepthPass = dppass;
}

void OpenGLState::stencilMask(GLuint mask, bool front) noexcept
{
    auto& s = front ? state.stencil.front : state.stencil.back;
    glStencilMaskSeparate(front ? GL_FRONT : GL_BACK, mask);
    s.mask = mask;
}

int OpenGLState::getIndexForCap(GLenum cap) noexcept
{
    switch (cap) {
        case GL_BLEND: return 0;
        case GL_CULL_FACE: return 1;
        case GL_SCISSOR_TEST: return 2;
        case GL_DEPTH_TEST: return 3;
        case GL_STENCIL_TEST: return 4;
        case GL_DITHER: return 5;
        case GL_SAMPLE_ALPHA_TO_COVERAGE: return 6;
        case GL_SAMPLE_COVERAGE: return 7;
        case GL_POLYGON_OFFSET_FILL: return 8;
        case GL_TEXTURE_CUBE_MAP_SEAMLESS: return 9;
        case GL_PROGRAM_POINT_SIZE: return 10;
        case GL_DEPTH_CLAMP: return 11;
        default: return -1;
    }
}

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
