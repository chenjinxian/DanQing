// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderBuffer implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RenderBuffer.ts
#include "RenderBuffer.h"

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/DriverEnums.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

RenderBuffer::RenderBuffer(RenderBufferType type, RenderBufferUsage usage)
    : m_type(type), m_usage(usage) {}

RenderBuffer::~RenderBuffer() = default;

// ---------------------------------------------------------------------------
// Data upload
// ---------------------------------------------------------------------------

void RenderBuffer::setData(rhi::Driver& driver, void const* data, uint32_t size)
{
    // Release any existing buffer before reallocating.
    if (m_handle) {
        driver.destroyBufferObject(m_handle);
        m_handle = {};
    }

    if (0 == size || nullptr == data) {
        m_size = 0;
        return;
    }

    // Map RenderBufferType → RHI BufferObjectBinding.
    rhi::BufferObjectBinding binding = rhi::BufferObjectBinding::VERTEX;
    if (m_type == RenderBufferType::Index) {
        binding = rhi::BufferObjectBinding::VERTEX;  // Index buffers also use VERTEX binding in Filament
    } else if (m_type == RenderBufferType::Uniform) {
        binding = rhi::BufferObjectBinding::UNIFORM;
    }

    // Map RenderBufferUsage → RHI BufferUsage.
    rhi::BufferUsage usage = rhi::BufferUsage::STATIC;
    if (m_usage == RenderBufferUsage::Dynamic) {
        usage = rhi::BufferUsage::DYNAMIC;
    }
    // RenderBufferUsage::Stream maps to STATIC (no STREAM in RHI enums).

    m_handle = driver.createBufferObject(size, binding, usage);
    if (m_handle) {
        m_size = size;
        driver.updateBufferObject(m_handle, rhi::BufferDescriptor(data, size), 0);
    } else {
        m_size = 0;
    }
}

void RenderBuffer::updateData(rhi::Driver& driver, void const* data,
                              uint32_t size, uint32_t offset)
{
    if (!m_handle || nullptr == data || 0 == size) {
        return;
    }

    // Bounds check — do not write beyond allocated region.
    if (offset + size > m_size) {
        return;
    }

    driver.updateBufferObject(m_handle, rhi::BufferDescriptor(data, size), offset);
}

// ---------------------------------------------------------------------------
// Release
// ---------------------------------------------------------------------------

void RenderBuffer::release(rhi::Driver& driver)
{
    if (m_handle) {
        driver.destroyBufferObject(m_handle);
        m_handle = {};
        m_size = 0;
    }
}

END_DQ_RENDER_NAMESPACE
