// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — UniformBuffer implementation
// Ported from: filament UniformBuffer
#include "dqRender/rhi/UniformBuffer.h"
#include "dqRender/rhi/BufferDescriptor.h"
#include "dqRender/rhi/Driver.h"

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

UniformBuffer::~UniformBuffer()
{
    // Note: destroy() must be called explicitly before destruction
    // to release GPU resources with the driver.
}

void UniformBuffer::allocate(uint32_t byteCount, Driver& driver)
{
    if (m_size == byteCount && m_handle)
        return;  // Already allocated with the correct size.

    destroy(driver);

    m_size = byteCount;
    m_data.resize(byteCount, 0);
    m_handle = driver.createBufferObject(
        byteCount,
        BufferObjectBinding::UNIFORM,
        BufferUsage::STATIC);
    if (!m_handle) {
        // Driver failed to create UBO — fallback to CPU-only path
        m_size = 0;
        m_data.clear();
        return;
    }
    m_dirty = true;
}

void UniformBuffer::destroy(Driver& driver)
{
    if (m_handle) {
        driver.destroyBufferObject(m_handle);
        m_handle = BufferObjectHandle{};
    }
    m_size = 0;
    m_data.clear();
    m_dirty = false;
}

void UniformBuffer::commit(Driver& driver)
{
    if (!m_dirty || !m_handle || m_size == 0)
        return;

    BufferDescriptor bd(m_data.data(), m_data.size());
    driver.updateBufferObject(m_handle, std::move(bd), 0);
    m_dirty = false;
}

void UniformBuffer::set(void const* data, uint32_t size, uint32_t offset)
{
    if (offset + size > m_size)
        return;

    std::memcpy(m_data.data() + offset, data, size);
    m_dirty = true;
}

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
