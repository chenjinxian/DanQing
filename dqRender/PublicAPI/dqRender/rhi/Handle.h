// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Typed GPU resource handle
// Ported from: filament backend/include/backend/Handle.h
//
// A type-safe handle wrapping a uint32_t ID.  The template parameter T is a
// forward-declared Hw* struct (e.g., HwTexture).  Handles are move-only and
// use UINT32_MAX as the null sentinel.
#pragma once

#include "dqRender/Export.h"

#include <cstdint>
#include <limits>
#include <type_traits>

// Namespace macros (matching DqRender.h, but self-contained for RHI headers)
#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// Forward declarations for hardware resource structs (defined in DriverBase.h)
struct HwBase;
struct HwVertexBufferInfo;
struct HwVertexBuffer;
struct HwBufferObject;
struct HwIndexBuffer;
struct HwRenderPrimitive;
struct HwProgram;
struct HwTexture;
struct HwRenderTarget;
struct HwFence;
struct HwSwapChain;
struct HwStream;
struct HwSync;
struct HwTimerQuery;
struct HwDescriptorSetLayout;
struct HwDescriptorSet;
struct HwMemoryMappedBuffer;

// ---------------------------------------------------------------------------
// HandleBase — non-templated base with a uint32_t ID
// ---------------------------------------------------------------------------
class HandleBase {
public:
    using HandleId = uint32_t;

    static constexpr HandleId nullid = std::numeric_limits<HandleId>::max();

    HandleBase() noexcept : m_id(nullid) {}

    explicit constexpr operator bool() const noexcept { return m_id != nullid; }

    HandleId getId() const noexcept { return m_id; }

protected:
    explicit HandleBase(HandleId id) noexcept : m_id(id) {}

    HandleId m_id;
};

// ---------------------------------------------------------------------------
// Handle<T> — typed handle
// ---------------------------------------------------------------------------
template <typename T>
class Handle : public HandleBase {
public:
    Handle() noexcept = default;

    // Allow safe upcasting: Handle<Base> from Handle<Derived>
    template <typename B, typename = std::enable_if_t<std::is_base_of_v<T, B>>>
    Handle(Handle<B> const& base) noexcept : HandleBase(base.getId()) {}

    // Allow move construction from a derived handle
    template <typename B, typename = std::enable_if_t<std::is_base_of_v<T, B>>>
    Handle(Handle<B>&& base) noexcept : HandleBase(base.getId()) { base.clear(); }

    // copy/move
    Handle(Handle const& rhs) noexcept = default;
    Handle(Handle&& rhs) noexcept : HandleBase(rhs.m_id) { rhs.m_id = nullid; }

    Handle& operator=(Handle const& rhs) noexcept = default;
    Handle& operator=(Handle&& rhs) noexcept
    {
        m_id = rhs.m_id;
        rhs.m_id = nullid;
        return *this;
    }

    void clear() noexcept { m_id = nullid; }

    bool operator==(Handle const& rhs) const noexcept { return m_id == rhs.m_id; }
    bool operator!=(Handle const& rhs) const noexcept { return m_id != rhs.m_id; }

private:
    explicit Handle(HandleId id) noexcept : HandleBase(id) {}

    template <typename B>
    friend class Handle;
    friend class HandleAllocator;
};

// ---------------------------------------------------------------------------
// Handle type aliases (matching Filament's naming)
// ---------------------------------------------------------------------------
using BufferObjectHandle        = Handle<HwBufferObject>;
using FenceHandle               = Handle<HwFence>;
using IndexBufferHandle         = Handle<HwIndexBuffer>;
using ProgramHandle             = Handle<HwProgram>;
using RenderPrimitiveHandle     = Handle<HwRenderPrimitive>;
using RenderTargetHandle        = Handle<HwRenderTarget>;
using StreamHandle              = Handle<HwStream>;
using SwapChainHandle           = Handle<HwSwapChain>;
using SyncHandle                = Handle<HwSync>;
using TextureHandle             = Handle<HwTexture>;
using TimerQueryHandle          = Handle<HwTimerQuery>;
using VertexBufferHandle        = Handle<HwVertexBuffer>;
using VertexBufferInfoHandle    = Handle<HwVertexBufferInfo>;
using DescriptorSetLayoutHandle = Handle<HwDescriptorSetLayout>;
using DescriptorSetHandle       = Handle<HwDescriptorSet>;
using MemoryMappedBufferHandle  = Handle<HwMemoryMappedBuffer>;

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
