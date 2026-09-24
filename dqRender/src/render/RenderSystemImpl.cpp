// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderSystem implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/System.ts
#include "RenderSystemImpl.h"
#include "TargetImpl.h"

BEGIN_DQ_RENDER_NAMESPACE

RenderSystemImpl::RenderSystemImpl(std::unique_ptr<rhi::Driver> driver)
    : m_driver(std::move(driver))
{
    // Create default render target
    m_defaultRenderTarget = m_driver->createDefaultRenderTarget();
}

RenderSystemImpl::~RenderSystemImpl()
{
    if (m_defaultRenderTarget) {
        m_driver->destroyRenderTarget(m_defaultRenderTarget);
    }
}

TargetImpl* RenderSystemImpl::createTarget(ViewRect const& rect, Techniques& techniques)
{
    return new TargetImpl(*this, techniques, rect);
}

void RenderSystemImpl::beginFrame(int64_t monotonicClockNs, int64_t refreshIntervalNs,
                                  uint32_t frameId)
{
    m_driver->beginFrame(monotonicClockNs, refreshIntervalNs, frameId);
}

void RenderSystemImpl::endFrame(uint32_t frameId)
{
    m_driver->endFrame(frameId);
}

void RenderSystemImpl::terminate()
{
    m_driver->terminate();
}

// ---------------------------------------------------------------------------
// Texture binding cache
// Ported from: itwinjs-core System.ts bindTexture/activateTexture/disposeTexture
// ---------------------------------------------------------------------------

void RenderSystemImpl::bindTexture2d(uint32_t unit, rhi::TextureHandle texture)
{
    // unit is a GL::TextureUnit value (e.g. GL_TEXTURE0 = 0x84C0).
    // Cache uses 0-based index: unit - GL::TextureUnit::Zero.
    // Ported from: itwinjs-core System.ts bindTexture() (line 837)
    uint32_t const idx = unit - static_cast<uint32_t>(GL::TextureUnit::Zero);
    if (idx >= kMaxTextureUnits) return;
    if (m_textureBindings[idx] == texture) return;  // already bound

    m_textureBindings[idx] = texture;
    m_driver->bindTexture(unit, texture);
}

void RenderSystemImpl::activateTexture2d(uint32_t unit, rhi::TextureHandle texture)
{
    uint32_t const idx = unit - static_cast<uint32_t>(GL::TextureUnit::Zero);
    if (idx >= kMaxTextureUnits) return;
    // Always rebind when activating (even if same texture)
    m_textureBindings[idx] = texture;
    m_driver->bindTexture(unit, texture);
}

void RenderSystemImpl::bindTextureCubeMap(uint32_t unit, rhi::TextureHandle texture)
{
    // Cube maps use the same binding path as 2D textures in the RHI.
    bindTexture2d(unit, texture);
}

void RenderSystemImpl::disposeTexture(rhi::TextureHandle texture)
{
    for (uint32_t i = 0; i < kMaxTextureUnits; ++i) {
        if (m_textureBindings[i] == texture) {
            m_textureBindings[i] = rhi::TextureHandle{};
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Vertex attribute state management
// Ported from: itwinjs-core System.ts enableVertexAttribArray/updateVertexAttribArrays
// ---------------------------------------------------------------------------

void RenderSystemImpl::enableVertexAttribArray(uint32_t id, bool instanced)
{
    if (id >= kMaxVertexAttribs) return;
    m_nextVertexAttribStates[id] = instanced ? (kVAEnabled | kVAInstanced) : kVAEnabled;
}

void RenderSystemImpl::updateVertexAttribArrays()
{
    for (uint32_t i = 0; i < kMaxVertexAttribs; ++i) {
        uint8_t oldState = m_curVertexAttribStates[i];
        uint8_t newState = m_nextVertexAttribStates[i];

        if (oldState != newState) {
            bool wasEnabled = (oldState & kVAEnabled) != 0;
            bool nowEnabled = (newState & kVAEnabled) != 0;

            if (wasEnabled != nowEnabled) {
                if (nowEnabled)
                    glEnableVertexAttribArray(i);
                else
                    glDisableVertexAttribArray(i);
            }

            if (nowEnabled) {
                bool wasInstanced = (oldState & kVAInstanced) != 0;
                bool nowInstanced = (newState & kVAInstanced) != 0;
                if (wasInstanced != nowInstanced) {
                    glVertexAttribDivisor(i, nowInstanced ? 1 : 0);
                }
            }

            m_curVertexAttribStates[i] = newState;
        }

        // Reset next state (clear enabled bit, preserve divisor)
        m_nextVertexAttribStates[i] &= ~kVAEnabled;
    }
}

void RenderSystemImpl::vertexAttribDivisor(uint32_t index, uint32_t divisor)
{
    glVertexAttribDivisor(index, divisor);
}

END_DQ_RENDER_NAMESPACE
