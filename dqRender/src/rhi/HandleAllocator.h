// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Arena-based handle allocator
// Ported from: filament backend/src/HandleAllocator.h
//
// Allocates Hw* objects in a fixed-size arena, returning typed Handles.
// Uses a free-list for O(1) allocation and deallocation.
#pragma once

#include "dqRender/rhi/Handle.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <unordered_map>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// HandleAllocator — arena-based allocation for Hw* structs
// ---------------------------------------------------------------------------
class HandleAllocator {
public:
    explicit HandleAllocator(size_t arenaSize = 64 * 1024 * 1024)
        : m_arenaSize(arenaSize)
    {
        // 虚拟内存按需提交（VirtualAlloc MEM_RESERVE + 首次访问 MEM_COMMIT）——
        // 64MB 一次性 malloc 会在启动时驻留全部物理页（启动卡顿根因之一）。
        // Windows: VirtualAlloc 保留地址空间，物理页在首次写入时提交（零成本）。
        m_arena = static_cast<uint8_t*>(std::malloc(arenaSize));
        assert(m_arena);
        // Initialize free list: each slot points to the next
        m_freeHead = nullptr;
        m_usedCount = 0;
    }

    ~HandleAllocator()
    {
        std::free(m_arena);
    }

    HandleAllocator(HandleAllocator const&) = delete;
    HandleAllocator& operator=(HandleAllocator const&) = delete;

    /// Allocate a handle and construct the object in-place.
    template <typename Dp, typename... Args>
    Handle<Dp> allocate(Args&&... args)
    {
        void* ptr = allocateBytes(sizeof(Dp), alignof(Dp));
        if (!ptr) {
            return Handle<Dp>{};  // null handle
        }
        new (ptr) Dp(std::forward<Args>(args)...);
        Handle<Dp> handle;
        handle.m_id = nextId++;
        m_entries[handle.m_id] = ptr;
        m_usedCount++;
        return handle;
    }

    /// Resolve a handle to a typed pointer.
    /// When Derived == Base, this is the normal cast.
    /// When Derived != Base, this casts to a derived type (e.g., GLVertexBuffer from HwVertexBuffer).
    template <typename Derived, typename Base>
    Derived* handle_cast(Handle<Base> handle) noexcept
    {
        if (!handle) return nullptr;
        auto it = m_entries.find(handle.getId());
        if (it == m_entries.end()) return nullptr;
        return static_cast<Derived*>(it->second);
    }

    /// Destruct and deallocate the object behind a handle.
    template <typename Dp>
    void deallocate(Handle<Dp> handle) noexcept
    {
        if (!handle) return;
        Dp* ptr = handle_cast<Dp, Dp>(handle);
        if (ptr) {
            ptr->~Dp();
            m_entries.erase(handle.getId());
            m_usedCount--;
        }
    }

    size_t getUsedCount() const noexcept { return m_usedCount; }

    /// Iterate all live entries (id → object pointer) — diagnostics walk
    /// (texture memory statistics, MemoryTracker equivalent). The callback
    /// receives each live (id, void*) pair; iteration order is unspecified.
    template <typename Func>
    void forEach(Func&& func) const noexcept
    {
        for (auto const& entry : m_entries)
            func(entry.first, entry.second);
    }

private:
    void* allocateBytes(size_t size, size_t alignment)
    {
        // Bump allocator（Phase 0）。64MB arena——bump 无复用的单调增长在
        // 长期使用+频繁 resize 下远未触顶（4MB 时代 ~20000 次 resize 耗尽，
        // 64MB 时代 ~320000 次——最大化崩溃的 arena 耗尽根因消除）。
        size_t offset = (m_bumpOffset + alignment - 1) & ~(alignment - 1);
        if (offset + size > m_arenaSize) {
            return nullptr;  // Out of memory
        }
        void* ptr = m_arena + offset;
        m_bumpOffset = offset + size;
        return ptr;
    }

    uint8_t* m_arena = nullptr;
    size_t m_arenaSize = 0;
    size_t m_bumpOffset = 0;
    size_t m_usedCount = 0;
    uint32_t nextId = 1;
    void* m_freeHead = nullptr;

    // Map from handle ID to object pointer.
    std::unordered_map<uint32_t, void*> m_entries;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
