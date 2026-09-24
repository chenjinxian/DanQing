// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BentleyAllocator.h
// DanQing dqBase — 跨 DLL 安全内存分配器
//
// 1:1 对齐 imodel-native BentleyAllocator。
// 确保分配和释放在同一 DLL 堆上，避免跨 DLL 堆损坏。
#pragma once

#include "Export.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <utility>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// 跨 DLL 分配/释放函数
// Ported from: imodel-native BentleyAllocator.h
// ---------------------------------------------------------------------------
DQ_BASE_EXPORT void* DqAllocator_New(size_t size);
DQ_BASE_EXPORT void  DqAllocator_Delete(void* ptr, size_t size) noexcept;
DQ_BASE_EXPORT void* DqAllocator_Malloc(size_t size);
DQ_BASE_EXPORT void* DqAllocator_Calloc(size_t count, size_t size);
DQ_BASE_EXPORT void* DqAllocator_Realloc(void* ptr, size_t newSize);
DQ_BASE_EXPORT void  DqAllocator_Free(void* ptr, size_t size) noexcept;
DQ_BASE_EXPORT void* DqAllocator_GetNullRefBuffer();

// ---------------------------------------------------------------------------
// 启用低碎片 CRT 堆（Windows）
// Ported from: imodel-native BentleyAllocator.h
// ---------------------------------------------------------------------------
DQ_BASE_EXPORT void DqAllocator_EnableLowFragmentationCRTHeap();

// ---------------------------------------------------------------------------
// RefCounted 专用分配/释放
// Ported from: imodel-native BentleyAllocator.h
// ---------------------------------------------------------------------------
DQ_BASE_EXPORT void* DqAllocator_AllocateRefCounted(size_t size);
DQ_BASE_EXPORT void  DqAllocator_DeleteRefCounted(void* ptr, size_t size) noexcept;
DQ_BASE_EXPORT void* DqAllocator_AllocateArrayRefCounted(size_t size);
DQ_BASE_EXPORT void  DqAllocator_DeleteArrayRefCounted(void* ptr, size_t size) noexcept;

// ---------------------------------------------------------------------------
// IRefCounted 专用分配/释放
// ---------------------------------------------------------------------------
DQ_BASE_EXPORT void* DqAllocator_AllocateIRefCounted(size_t size);
DQ_BASE_EXPORT void  DqAllocator_DeleteIRefCounted(void* ptr, size_t size) noexcept;
DQ_BASE_EXPORT void* DqAllocator_AllocateArrayIRefCounted(size_t size);
DQ_BASE_EXPORT void  DqAllocator_DeleteArrayIRefCounted(void* ptr, size_t size) noexcept;

// ---------------------------------------------------------------------------
// DqAllocator<T> — STL 兼容的跨 DLL 安全分配器
// Ported from: imodel-native BentleyAllocator.h
// ---------------------------------------------------------------------------
template<typename T>
struct DqAllocator {
    // Ported from: imodel-native BentleyAllocator.h (pre-C++11 std::allocator interop surface)
    using value_type      = T;
    using pointer         = T*;
    using reference       = T&;
    using const_pointer   = const T*;
    using const_reference = const T&;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<class U>
    struct rebind {
        using other = DqAllocator<U>;
    };

    pointer address(reference val) const noexcept {
        return (&val);
    }

    const_pointer address(const_reference val) const noexcept {
        return (&val);
    }

    DqAllocator() noexcept = default;

    DqAllocator(const DqAllocator<T>&) noexcept {}

    DqAllocator<T>& operator=(const DqAllocator<T>&) noexcept {
        return (*this);
    }

    template<typename U>
    DqAllocator(const DqAllocator<U>&) noexcept {} // NOLINT

    template<typename U>
    DqAllocator<T>& operator=(const DqAllocator<U>&) noexcept {
        return (*this);
    }

    pointer allocate(size_type count) {
        return static_cast<pointer>(DqAllocator_New(count * sizeof(T)));
    }

    // ref:99-102 — allocate with hint (hint ignored, mirrors std::allocator interop)
    pointer allocate(size_type count, void const* /*hint*/) {
        return allocate(count);
    }

    void deallocate(pointer p, size_type n) noexcept {
        DqAllocator_Delete(p, n * sizeof(T));
    }

    // ref:104-107 — copy-construct object at p
    void construct(pointer p, const T& val) {
        new (static_cast<void*>(p)) T(val);
    }

    // ref:109-112 — move-construct object at p
    void construct(pointer p, T&& val) {
        new (static_cast<void*>(p)) T(std::move(val));
    }

    // ref:115-119 — variadic construct (parameter pack)
    template<typename... Args>
    void construct(pointer p, Args&&... args) {
        new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
    }

    // ref:122-125 — destroy object at p
    void destroy(pointer p) {
        p->~T();
    }

    // ref:127-131 — maximum allocatable count
    size_type max_size() const noexcept {
        size_type count = static_cast<size_type>(-1) / sizeof(T);
        return (0 < count ? count : 1);
    }

    template<typename U>
    bool operator==(const DqAllocator<U>&) const noexcept { return true; }

    template<typename U>
    bool operator!=(const DqAllocator<U>&) const noexcept { return false; }
};

// ---------------------------------------------------------------------------
// DEFINE_UE_NEW_DELETE_OPERATORS — 用于类定义中覆盖 new/delete
// Ported from: imodel-native BentleyAllocator.h
// ---------------------------------------------------------------------------
#define DEFINE_UE_NEW_DELETE_OPERATORS \
    void* operator new(size_t size) { return ::dqBase::DqAllocator_New(size); } \
    void  operator delete(void* ptr, size_t size) noexcept { ::dqBase::DqAllocator_Delete(ptr, size); } \
    void* operator new[](size_t size) { return ::dqBase::DqAllocator_New(size); } \
    void  operator delete[](void* ptr, size_t size) noexcept { ::dqBase::DqAllocator_Delete(ptr, size); }

END_DQ_BASE_NAMESPACE
