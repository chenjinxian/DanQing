// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GL buffer object wrapper
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RenderBuffer.ts
//
// Wraps an RHI BufferObjectHandle to manage vertex, index, or uniform buffer
// lifecycle.  In itwinjs-core this class directly called gl.bindBuffer() and
// gl.bufferData(); here the GL calls are routed through the RHI Driver.
#pragma once

#include "dqRender/rhi/Handle.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Buffer type — determines how the buffer is consumed by the pipeline.
// (Ported from: itwinjs-core webgl/RenderBuffer.ts BufferType)
enum class RenderBufferType : uint8_t {
    Vertex = 0,
    Index = 1,
    Uniform = 2,
};

// Buffer usage hint.
// (Ported from: itwinjs-core webgl/RenderBuffer.ts BufferUsage)
enum class RenderBufferUsage : uint8_t {
    Static = 0,
    Dynamic = 1,
    Stream = 2,
};

// Forward declaration
namespace rhi {
class Driver;
}

// Wraps an RHI buffer object for vertex/index/uniform data.
// (Ported from: itwinjs-core webgl/RenderBuffer.ts)
class RenderBuffer {
public:
    RenderBuffer(RenderBufferType type,
                 RenderBufferUsage usage = RenderBufferUsage::Static);
    ~RenderBuffer();

    RenderBuffer(RenderBuffer const&) = delete;
    RenderBuffer& operator=(RenderBuffer const&) = delete;

    // Get the RHI buffer handle.
    rhi::BufferObjectHandle getHandle() const noexcept { return m_handle; }

    // Get buffer type.
    RenderBufferType getType() const noexcept { return m_type; }

    // Get buffer usage.
    RenderBufferUsage getUsage() const noexcept { return m_usage; }

    // Get buffer size in bytes.
    uint32_t getSize() const noexcept { return m_size; }

    // Check if buffer is valid (has a non-null RHI handle).
    bool isValid() const noexcept { return m_handle != rhi::BufferObjectHandle(); }

    // Upload data to the buffer.  Allocates (or reallocates) the GPU buffer.
    void setData(rhi::Driver& driver, void const* data, uint32_t size);

    // Update a region of the buffer.  Buffer must already be allocated.
    void updateData(rhi::Driver& driver, void const* data, uint32_t size,
                    uint32_t offset);

    // Release the GPU buffer.
    void release(rhi::Driver& driver);

private:
    rhi::BufferObjectHandle m_handle;
    RenderBufferType m_type;
    RenderBufferUsage m_usage;
    uint32_t m_size = 0;
};

END_DQ_RENDER_NAMESPACE
