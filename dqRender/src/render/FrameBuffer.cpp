// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — FrameBuffer implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/FrameBuffer.ts
#include "FrameBuffer.h"

BEGIN_DQ_RENDER_NAMESPACE

FrameBuffer FrameBuffer::create(rhi::RenderTargetHandle renderTarget)
{
    FrameBuffer fb;
    fb.m_renderTarget = renderTarget;
    return fb;
}

void FrameBufferStack::push(rhi::Driver& driver, FrameBuffer const& fb,
                            rhi::RenderPassParams const& params)
{
    m_stack.push_back(fb);
    driver.beginRenderPass(fb.getRenderTarget(), params);
}

void FrameBufferStack::pop(rhi::Driver& driver)
{
    if (!m_stack.empty()) {
        driver.endRenderPass();
        m_stack.pop_back();
    }
}

FrameBuffer const* FrameBufferStack::getCurrent() const
{
    return m_stack.empty() ? nullptr : &m_stack.back();
}

END_DQ_RENDER_NAMESPACE
