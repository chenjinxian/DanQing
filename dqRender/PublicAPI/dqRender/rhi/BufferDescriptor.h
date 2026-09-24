// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — CPU-to-GPU buffer transfer descriptor
// Ported from: filament backend/include/backend/BufferDescriptor.h
//
// Move-only RAII wrapper for CPU memory buffers.  The optional callback fires
// on destruction to release the underlying allocation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// BufferDescriptor — CPU memory buffer reference with ownership-release callback
// ---------------------------------------------------------------------------
class BufferDescriptor {
public:
    using Callback = void (*)(void* buffer, size_t size, void* user);

    BufferDescriptor() noexcept
        : m_buffer(nullptr), m_size(0), m_callback(nullptr), m_user(nullptr) {}

    BufferDescriptor(void const* buffer, size_t size,
                     Callback callback = nullptr, void* user = nullptr) noexcept
        : m_buffer(const_cast<void*>(buffer))
        , m_size(size)
        , m_callback(callback)
        , m_user(user) {}

    ~BufferDescriptor() { callCallback(); }

    // Move-only
    BufferDescriptor(BufferDescriptor const&) = delete;
    BufferDescriptor& operator=(BufferDescriptor const&) = delete;

    BufferDescriptor(BufferDescriptor&& rhs) noexcept
        : m_buffer(rhs.m_buffer)
        , m_size(rhs.m_size)
        , m_callback(rhs.m_callback)
        , m_user(rhs.m_user)
    {
        rhs.m_buffer = nullptr;
        rhs.m_size = 0;
        rhs.m_callback = nullptr;
        rhs.m_user = nullptr;
    }

    BufferDescriptor& operator=(BufferDescriptor&& rhs) noexcept
    {
        if (this != &rhs) {
            callCallback();
            m_buffer = rhs.m_buffer;
            m_size = rhs.m_size;
            m_callback = rhs.m_callback;
            m_user = rhs.m_user;
            rhs.m_buffer = nullptr;
            rhs.m_size = 0;
            rhs.m_callback = nullptr;
            rhs.m_user = nullptr;
        }
        return *this;
    }

    void const* buffer() const noexcept { return m_buffer; }
    size_t size() const noexcept { return m_size; }

    void setCallback(Callback callback, void* user = nullptr) noexcept
    {
        m_callback = callback;
        m_user = user;
    }

    /// Convenience: call the callback (if set) and reset.
    void release() noexcept
    {
        callCallback();
        m_buffer = nullptr;
        m_size = 0;
        m_callback = nullptr;
        m_user = nullptr;
    }

private:
    void callCallback() noexcept
    {
        if (m_callback) {
            m_callback(m_buffer, m_size, m_user);
            m_callback = nullptr;
        }
    }

    void* m_buffer;
    size_t m_size;
    Callback m_callback;
    void* m_user;
};

// ---------------------------------------------------------------------------
// PixelBufferDescriptor — extends BufferDescriptor with pixel metadata
// ---------------------------------------------------------------------------
class PixelBufferDescriptor : public BufferDescriptor {
public:
    PixelBufferDescriptor() noexcept
        : BufferDescriptor()
        , m_left(0), m_top(0), m_width(0), m_height(0), m_depth(0)
        , m_format(0), m_type(0), m_stride(0), m_alignment(1) {}

    PixelBufferDescriptor(void const* buffer, size_t size,
                          uint32_t format, uint32_t type,
                          uint32_t stride = 0, uint32_t alignment = 1,
                          uint32_t left = 0, uint32_t top = 0,
                          uint32_t width = 0, uint32_t height = 0,
                          uint32_t depth = 0,
                          Callback callback = nullptr, void* user = nullptr) noexcept
        : BufferDescriptor(buffer, size, callback, user)
        , m_left(left), m_top(top), m_width(width), m_height(height), m_depth(depth)
        , m_format(format), m_type(type), m_stride(stride), m_alignment(alignment) {}

    PixelBufferDescriptor(PixelBufferDescriptor&& rhs) noexcept = default;
    PixelBufferDescriptor& operator=(PixelBufferDescriptor&& rhs) noexcept = default;

    uint32_t left() const noexcept { return m_left; }
    uint32_t top() const noexcept { return m_top; }
    uint32_t width() const noexcept { return m_width; }
    uint32_t height() const noexcept { return m_height; }
    uint32_t depth() const noexcept { return m_depth; }
    uint32_t format() const noexcept { return m_format; }
    uint32_t type() const noexcept { return m_type; }
    uint32_t stride() const noexcept { return m_stride; }
    uint32_t alignment() const noexcept { return m_alignment; }

private:
    uint32_t m_left, m_top, m_width, m_height, m_depth;
    uint32_t m_format, m_type;
    uint32_t m_stride, m_alignment;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
