// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Uniform Buffer Object (UBO) management
// Ported from: itwinjs-core + filament UBO patterns
//
// Manages a GPU uniform buffer with CPU-side staging and dirty tracking.
// Supports sub-allocation via dynamic offsets for efficient per-draw uniform updates.
#pragma once

#include "dqRender/Export.h"
#include "dqRender/rhi/DriverEnums.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>
#include <cstring>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

namespace rhi {

class Driver;
struct HwBufferObject;
using BufferObjectHandle = Handle<HwBufferObject>;

// ---------------------------------------------------------------------------
// UniformBuffer — CPU-managed uniform buffer with GPU upload
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT UniformBuffer {
public:
    UniformBuffer() = default;
    ~UniformBuffer();

    UniformBuffer(UniformBuffer const&) = delete;
    UniformBuffer& operator=(UniformBuffer const&) = delete;

    /// Allocate a UBO of the given size.
    /// @param byteCount Size of the buffer in bytes
    /// @param driver RHI driver for GPU resource creation
    void allocate(uint32_t byteCount, Driver& driver);

    /// Release GPU resources.
    void destroy(Driver& driver);

    /// Upload dirty data to the GPU.
    /// Ported from: filament UniformBuffer::commit()
    void commit(Driver& driver);

    /// Set data at the given offset.
    /// Marks the affected range as dirty.
    void set(void const* data, uint32_t size, uint32_t offset = 0);

    /// Set a typed value at the given offset.
    template<typename T>
    void set(uint32_t offset, T const& value)
    {
        set(&value, sizeof(T), offset);
    }

    /// Get the CPU-side data pointer.
    uint8_t* getData() noexcept { return m_data.data(); }
    uint8_t const* getData() const noexcept { return m_data.data(); }

    /// Get the buffer size in bytes.
    uint32_t getSize() const noexcept { return m_size; }

    /// Get the GPU buffer handle.
    BufferObjectHandle getHandle() const noexcept { return m_handle; }

    /// Check if the buffer has been allocated.
    bool isAllocated() const noexcept { return m_size > 0; }

private:
    std::vector<uint8_t> m_data;
    BufferObjectHandle m_handle;
    uint32_t m_size = 0;
    bool m_dirty = false;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
