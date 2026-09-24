// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Disposable interface
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Disposable.ts
//
// Simple interface for GPU resources that can be explicitly disposed.
// In itwinjs, WebGLDisposable = { dispose(): void; get isDisposed(): boolean }.
#pragma once

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Interface for GPU resources that can be disposed.
// (Ported from: itwinjs-core Disposable.ts WebGLDisposable)
class IRenderDisposable {
public:
    virtual ~IRenderDisposable() = default;

    /// Returns true if this resource has been disposed.
    virtual bool isDisposed() const noexcept = 0;

    /// Release GPU resources.  After calling dispose(), isDisposed() must
    /// return true and further use of the resource is undefined.
    virtual void dispose() = 0;
};

END_DQ_RENDER_NAMESPACE
