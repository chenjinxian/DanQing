// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderState
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RenderState.ts
//
// Encapsulates the entire GL state machine (blend, depth, stencil, cull).
// Each sub-class mirrors the itwinjs-core structure exactly.
// apply() diffs against the previous state and only issues GL calls for
// things that changed, matching itwinjs-core's System.instance.applyRenderState().
#pragma once

// GL namespace enums (BlendFactor, DepthFunc, etc.) + raw GL functions.
// gl/GL.h includes rhi/opengl/Gl.h which provides <OpenGL/gl3.h>.
#include "gl/GL.h"

#include <array>
#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// RenderStateFlags
// Ported from: itwinjs-core RenderState.ts RenderStateFlags (line 13-79)
// ---------------------------------------------------------------------------
class RenderStateFlags {
public:
    bool cull = false;
    bool depthTest = false;
    bool blend = false;
    bool stencilTest = false;
    bool depthMask = true;
    bool colorWrite = true;

    RenderStateFlags() noexcept = default;

    void copyFrom(RenderStateFlags const& src) noexcept {
        cull = src.cull;
        depthTest = src.depthTest;
        blend = src.blend;
        stencilTest = src.stencilTest;
        depthMask = src.depthMask;
        colorWrite = src.colorWrite;
    }

    RenderStateFlags clone() const noexcept { return RenderStateFlags(*this); }

    bool equals(RenderStateFlags const& rhs) const noexcept {
        return cull == rhs.cull
            && depthTest == rhs.depthTest
            && blend == rhs.blend
            && stencilTest == rhs.stencilTest
            && depthMask == rhs.depthMask
            && colorWrite == rhs.colorWrite;
    }

    void apply(RenderStateFlags const& previousFlags) const noexcept {
        enableOrDisable(cull, GL::Capability::CullFace, previousFlags.cull);
        enableOrDisable(depthTest, GL::Capability::DepthTest, previousFlags.depthTest);
        enableOrDisable(blend, GL::Capability::Blend, previousFlags.blend);
        enableOrDisable(stencilTest, GL::Capability::StencilTest, previousFlags.stencilTest);

        if (previousFlags.depthMask != depthMask) {
            glDepthMask(depthMask ? GL_TRUE : GL_FALSE);
        }

        if (previousFlags.colorWrite != colorWrite) {
            GLboolean v = colorWrite ? GL_TRUE : GL_FALSE;
            glColorMask(v, v, v, v);
        }
    }

    static void enableOrDisable(bool currentFlag, GL::Capability value, bool previousFlag) noexcept {
        if (currentFlag != previousFlag) {
            if (currentFlag)
                glEnable(static_cast<GLenum>(value));
            else
                glDisable(static_cast<GLenum>(value));
        }
    }
};

// ---------------------------------------------------------------------------
// RenderStateBlend
// Ported from: itwinjs-core RenderState.ts RenderStateBlend (line 82-165)
// ---------------------------------------------------------------------------
class RenderStateBlend {
public:
    std::array<float, 4> color = {0.0f, 0.0f, 0.0f, 0.0f};
    GL::BlendEquation equationRgb = GL::BlendEquation::Default;
    GL::BlendEquation equationAlpha = GL::BlendEquation::Default;
    GL::BlendFactor functionSourceRgb = GL::BlendFactor::DefaultSrc;
    GL::BlendFactor functionSourceAlpha = GL::BlendFactor::DefaultSrc;
    GL::BlendFactor functionDestRgb = GL::BlendFactor::DefaultDst;
    GL::BlendFactor functionDestAlpha = GL::BlendFactor::DefaultDst;

    RenderStateBlend() noexcept = default;

    void apply(RenderStateBlend const* previousBlend) const noexcept {
        if (previousBlend == nullptr || !equalColors(*previousBlend)) {
            glBlendColor(color[0], color[1], color[2], color[3]);
        }
        if (previousBlend == nullptr
            || previousBlend->equationRgb != equationRgb
            || previousBlend->equationAlpha != equationAlpha) {
            glBlendEquationSeparate(
                static_cast<GLenum>(equationRgb),
                static_cast<GLenum>(equationAlpha));
        }
        if (previousBlend == nullptr
            || previousBlend->functionSourceRgb != functionSourceRgb
            || previousBlend->functionSourceAlpha != functionSourceAlpha
            || previousBlend->functionDestRgb != functionDestRgb
            || previousBlend->functionDestAlpha != functionDestAlpha) {
            glBlendFuncSeparate(
                static_cast<GLenum>(functionSourceRgb),
                static_cast<GLenum>(functionDestRgb),
                static_cast<GLenum>(functionSourceAlpha),
                static_cast<GLenum>(functionDestAlpha));
        }
    }

    void copyFrom(RenderStateBlend const& src) noexcept {
        setColor(src.color);
        equationRgb = src.equationRgb;
        equationAlpha = src.equationAlpha;
        functionSourceRgb = src.functionSourceRgb;
        functionSourceAlpha = src.functionSourceAlpha;
        functionDestRgb = src.functionDestRgb;
        functionDestAlpha = src.functionDestAlpha;
    }

    RenderStateBlend clone() const noexcept { return RenderStateBlend(*this); }

    bool equals(RenderStateBlend const& rhs) const noexcept {
        return equalColors(rhs)
            && equationRgb == rhs.equationRgb
            && equationAlpha == rhs.equationAlpha
            && functionSourceRgb == rhs.functionSourceRgb
            && functionSourceAlpha == rhs.functionSourceAlpha
            && functionDestRgb == rhs.functionDestRgb
            && functionDestAlpha == rhs.functionDestAlpha;
    }

    bool equalColors(RenderStateBlend const& rhs) const noexcept {
        return color[0] == rhs.color[0]
            && color[1] == rhs.color[1]
            && color[2] == rhs.color[2]
            && color[3] == rhs.color[3];
    }

    void setColor(std::array<float, 4> const& c) noexcept {
        color = c;
    }

    void setBlendFunc(GL::BlendFactor src, GL::BlendFactor dst) noexcept {
        setBlendFuncSeparate(src, src, dst, dst);
    }

    void setBlendFuncSeparate(
        GL::BlendFactor srcRgb, GL::BlendFactor srcAlpha,
        GL::BlendFactor dstRgb, GL::BlendFactor dstAlpha) noexcept
    {
        functionSourceRgb = srcRgb;
        functionSourceAlpha = srcAlpha;
        functionDestRgb = dstRgb;
        functionDestAlpha = dstAlpha;
    }
};

// ---------------------------------------------------------------------------
// RenderStateStencilOperation
// Ported from: itwinjs-core RenderState.ts RenderStateStencilOperation (line 168-199)
// ---------------------------------------------------------------------------
class RenderStateStencilOperation {
public:
    GL::StencilOperation fail = GL::StencilOperation::Default;
    GL::StencilOperation zFail = GL::StencilOperation::Default;
    GL::StencilOperation zPass = GL::StencilOperation::Default;

    RenderStateStencilOperation() noexcept = default;

    void copyFrom(RenderStateStencilOperation const& src) noexcept {
        fail = src.fail;
        zFail = src.zFail;
        zPass = src.zPass;
    }

    RenderStateStencilOperation clone() const noexcept { return RenderStateStencilOperation(*this); }

    bool equals(RenderStateStencilOperation const& rhs) const noexcept {
        return fail == rhs.fail
            && zFail == rhs.zFail
            && zPass == rhs.zPass;
    }
};

// ---------------------------------------------------------------------------
// RenderStateStencilFunction
// Ported from: itwinjs-core RenderState.ts RenderStateStencilFunction (line 202-233)
// ---------------------------------------------------------------------------
class RenderStateStencilFunction {
public:
    GL::StencilFunction function = GL::StencilFunction::Default;
    GLint ref = 0;
    GLuint mask = 0xFFFFFFFF;

    RenderStateStencilFunction() noexcept = default;

    void copyFrom(RenderStateStencilFunction const& src) noexcept {
        function = src.function;
        ref = src.ref;
        mask = src.mask;
    }

    RenderStateStencilFunction clone() const noexcept { return RenderStateStencilFunction(*this); }

    bool equals(RenderStateStencilFunction const& rhs) const noexcept {
        return function == rhs.function
            && ref == rhs.ref
            && mask == rhs.mask;
    }
};

// ---------------------------------------------------------------------------
// RenderStateStencil
// Ported from: itwinjs-core RenderState.ts RenderStateStencil (line 236-286)
// ---------------------------------------------------------------------------
class RenderStateStencil {
public:
    RenderStateStencilFunction frontFunction;
    RenderStateStencilFunction backFunction;
    RenderStateStencilOperation frontOperation;
    RenderStateStencilOperation backOperation;

    RenderStateStencil() noexcept = default;

    void apply(RenderStateStencil const* previousStencil) const noexcept {
        if (previousStencil == nullptr || !previousStencil->frontFunction.equals(frontFunction)) {
            glStencilFuncSeparate(
                static_cast<GLenum>(GL::CullFace::Front),
                static_cast<GLenum>(frontFunction.function),
                frontFunction.ref,
                frontFunction.mask);
        }
        if (previousStencil == nullptr || !previousStencil->backFunction.equals(backFunction)) {
            glStencilFuncSeparate(
                static_cast<GLenum>(GL::CullFace::Back),
                static_cast<GLenum>(backFunction.function),
                backFunction.ref,
                backFunction.mask);
        }
        if (previousStencil == nullptr || !previousStencil->frontOperation.equals(frontOperation)) {
            glStencilOpSeparate(
                static_cast<GLenum>(GL::CullFace::Front),
                static_cast<GLenum>(frontOperation.fail),
                static_cast<GLenum>(frontOperation.zFail),
                static_cast<GLenum>(frontOperation.zPass));
        }
        if (previousStencil == nullptr || !previousStencil->backOperation.equals(backOperation)) {
            glStencilOpSeparate(
                static_cast<GLenum>(GL::CullFace::Back),
                static_cast<GLenum>(backOperation.fail),
                static_cast<GLenum>(backOperation.zFail),
                static_cast<GLenum>(backOperation.zPass));
        }
    }

    void copyFrom(RenderStateStencil const& src) noexcept {
        frontFunction.copyFrom(src.frontFunction);
        backFunction.copyFrom(src.backFunction);
        frontOperation.copyFrom(src.frontOperation);
        backOperation.copyFrom(src.backOperation);
    }

    RenderStateStencil clone() const noexcept { return RenderStateStencil(*this); }

    bool equals(RenderStateStencil const& rhs) const noexcept {
        return frontFunction.equals(rhs.frontFunction)
            && backFunction.equals(rhs.backFunction)
            && frontOperation.equals(rhs.frontOperation)
            && backOperation.equals(rhs.backOperation);
    }
};

// ---------------------------------------------------------------------------
// RenderState
// Ported from: itwinjs-core RenderState.ts RenderState (line 297-386)
//
// Encapsulates the state of an OpenGL context. To modify the context for a
// rendering operation, do NOT directly call glDepthMask(), glBlendFunc(), etc.
// Instead, set up a RenderState and invoke System.instance.applyRenderState().
// The context tracks the most-recently applied RenderState, minimizing GL calls.
// ---------------------------------------------------------------------------
class RenderState {
public:
    RenderStateFlags flags;
    RenderStateBlend blend;
    RenderStateStencil stencil;
    GL::FrontFace frontFace = GL::FrontFace::Default;
    GL::CullFace cullFace = GL::CullFace::Default;
    GL::DepthFunc depthFunc = GL::DepthFunc::Default;
    GLuint stencilMask = 0xFFFFFFFF;

    RenderState() noexcept = default;

    // Frozen default instance (matches itwinjs-core RenderState.defaults).
    static RenderState const& defaults() noexcept {
        static RenderState sDefaults;
        return sDefaults;
    }

    void copyFrom(RenderState const& src) noexcept {
        flags.copyFrom(src.flags);
        blend.copyFrom(src.blend);
        stencil.copyFrom(src.stencil);
        frontFace = src.frontFace;
        cullFace = src.cullFace;
        depthFunc = src.depthFunc;
        stencilMask = src.stencilMask;
    }

    RenderState clone() const noexcept { return RenderState(*this); }

    void setClockwiseFrontFace(bool clockwise) noexcept {
        frontFace = clockwise ? GL::FrontFace::Clockwise : GL::FrontFace::CounterClockwise;
    }

    bool equals(RenderState const& rhs) const noexcept {
        return flags.equals(rhs.flags)
            && blend.equals(rhs.blend)
            && stencil.equals(rhs.stencil)
            && frontFace == rhs.frontFace
            && cullFace == rhs.cullFace
            && depthFunc == rhs.depthFunc
            && stencilMask == rhs.stencilMask;
    }

    // Diff against prevState and issue only the GL calls that changed.
    // Ported from: itwinjs-core RenderState.apply() (line 347-383)
    void apply(RenderState const& prevState) const noexcept {
        flags.apply(prevState.flags);

        if (flags.blend) {
            if (prevState.flags.blend)
                blend.apply(&prevState.blend);
            else
                blend.apply(nullptr);
        }

        if (flags.cull) {
            if (!prevState.flags.cull || prevState.cullFace != cullFace) {
                glCullFace(static_cast<GLenum>(cullFace));
            }
        }

        if (flags.depthTest) {
            if (!prevState.flags.depthTest || prevState.depthFunc != depthFunc) {
                glDepthFunc(static_cast<GLenum>(depthFunc));
            }
        }

        if (flags.stencilTest) {
            if (prevState.flags.stencilTest)
                stencil.apply(&prevState.stencil);
            else
                stencil.apply(nullptr);
        }

        if (frontFace != prevState.frontFace) {
            glFrontFace(static_cast<GLenum>(frontFace));
        }

        if (stencilMask != prevState.stencilMask) {
            glStencilMask(stencilMask);
        }
    }
};

END_DQ_RENDER_NAMESPACE
