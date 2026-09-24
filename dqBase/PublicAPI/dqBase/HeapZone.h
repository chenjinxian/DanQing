// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/HeapZone.h
// DanQing dqBase — 内存池（使用 Boost.Pool bpool 实现）
//
// 1:1 对齐 imodel-native HeapZone。
// 使用 bpool.h (Boost.Pool 移植) 实现固定大小块的高效分配。
#pragma once

#include "Export.h"
#include "DqAllocator.h"
#include "bpool.h"
#include "BeAssert.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// Heapzone_allocator — 使用 DqAllocator 的池分配器
// Ported from: imodel-native HeapZone.h
// ---------------------------------------------------------------------------
struct Heapzone_allocator {
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    static char* malloc(const size_type bytes) { return reinterpret_cast<char*>(DqAllocator_Malloc(bytes)); }
    static void free(char* const block) { DqAllocator_Free(block, 0); }
};

// ---------------------------------------------------------------------------
// DgnMemoryPool — 固定类型内存池模板
// Ported from: imodel-native HeapZone.h
// ---------------------------------------------------------------------------
template<class STYPE, int MAXBLOCKSIZE>
class DgnMemoryPool : dqBase::bpool<Heapzone_allocator> {
public:
    DgnMemoryPool() : dqBase::bpool<Heapzone_allocator>(sizeof(STYPE)) {}

    STYPE* AllocateNode() {
        if (next_size > MAXBLOCKSIZE) next_size = MAXBLOCKSIZE;
        return static_cast<STYPE*>(malloc());
    }

    void FreeNode(STYPE* node) { free(node); }
    void Clear() { purge_memory(); }

    void SetEntrySize(int n, int firstSize) {
        *reinterpret_cast<int*>(const_cast<size_type*>(&requested_size)) = n;
        next_size = firstSize;
    }

    size_t GetMemoryAllocated() const {
        size_type total = 0;
        for (dqBase::details::PODptr<size_type> ptr = list; ptr.valid(); ptr = ptr.next())
            total += ptr.total_size();
        return total;
    }
};

// ---------------------------------------------------------------------------
// T_BoostPoolWithMemutilAllocator — 类型别名
// Ported from: imodel-native HeapZone.h
// ---------------------------------------------------------------------------
typedef dqBase::bpool<Heapzone_allocator> T_BoostPoolWithMemutilAllocator;

// ---------------------------------------------------------------------------
// FixedSizePool1 — 固定大小块池
// Ported from: imodel-native HeapZone.h
// ---------------------------------------------------------------------------
struct FixedSizePool1 : private T_BoostPoolWithMemutilAllocator {
    // DEFINE_T_SUPER is verbatim reference code (Bentley.h:241) ending with `public:`;
    // its trailing-`;` call-site form (ref HeapZone.h:59) trips -Wextra-semi under
    // DanQing's stricter -Werror flags. Suppress locally — same pattern as btree/.
#if defined(__GNUC__) // GCC-only pragma（MSVC 未知 pragma 触发 C4068）
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra-semi"
#endif
    DEFINE_T_SUPER(T_BoostPoolWithMemutilAllocator);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    size_type m_originalNextSize;

    FixedSizePool1(uint32_t entrySize, const char* /*dbgnm*/ = "")
        : T_BoostPoolWithMemutilAllocator(entrySize)
        , m_originalNextSize(next_size)
    {}

    FixedSizePool1(const char* /*dbgnm*/ = "")
        : T_BoostPoolWithMemutilAllocator(8)
        , m_originalNextSize(next_size)
    {}

    ~FixedSizePool1() {}

    void SetName(const char* /*nm*/) {}

    void SetSize(int entrySize, int chunkSize = 32) {
        *const_cast<int*>(reinterpret_cast<int const*>(&requested_size)) = entrySize;
        m_originalNextSize = next_size = chunkSize;
    }

    size_t GetSize() const { return requested_size; }

    size_t GetFreeBytes() const {
        size_t nFree = 0;
        for (void* freeBlock = first; freeBlock != nullptr; freeBlock = nextof(freeBlock))
            ++nFree;
        return nFree * GetSize();
    }

    size_t GetTotalBytes() const {
        size_type total = 0;
        for (dqBase::details::PODptr<size_type> ptr = list; ptr.valid(); ptr = ptr.next())
            total += ptr.total_size();
        return total;
    }

    void ReleaseFreeMemory() { release_memory(); }

    void* malloc() { return T_Super::malloc(); }
    void* ordered_malloc(size_t nChunks) { return T_Super::ordered_malloc(nChunks); }
    void free(void* p) { T_Super::free(p); }
    void ordered_free(void* p, size_t nChunks) { T_Super::ordered_free(p, nChunks); }

    void purge_memory_and_reinitialize() {
        T_Super::purge_memory();
        set_next_size(m_originalNextSize);
    }
};

// ---------------------------------------------------------------------------
// HeapZone — 多级内存池
// Ported from: imodel-native HeapZone.h
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT HeapZone {
private:
    enum {
        POOL_ENTRY_SIZE = 8,
        NUM_FIXED_POOLS = 2 * sizeof(void*),  // 16 on 64-bit, 8 on 32-bit
        LARGE_SIZE_CHUNK = 32,
    };

    FixedSizePool1 m_fixed[NUM_FIXED_POOLS];
    FixedSizePool1 m_ordered;

    static size_t NumChunks(size_t size) { return (size + POOL_ENTRY_SIZE - 1) / POOL_ENTRY_SIZE; }
    static size_t LargeChunks(size_t size) { return (size + LARGE_SIZE_CHUNK - 1) / LARGE_SIZE_CHUNK; }

public:
    HeapZone(bool useMallocForLarge = true, const char* /*dbgnm*/ = "") {
        for (int i = 0; i < NUM_FIXED_POOLS; ++i)
            m_fixed[i].SetSize(static_cast<int>((i + 1) * POOL_ENTRY_SIZE));
        m_ordered.SetSize(useMallocForLarge ? 0 : LARGE_SIZE_CHUNK);
    }

    ~HeapZone() {}

    void ReleaseFreeMemory() {
        for (int i = 0; i < NUM_FIXED_POOLS; ++i)
            m_fixed[i].ReleaseFreeMemory();
        m_ordered.ReleaseFreeMemory();
    }

    void* Alloc(size_t size) {
        if (size == 0) return nullptr;
        size_t numChunks = NumChunks(size);
        if (numChunks <= NUM_FIXED_POOLS)
            return m_fixed[numChunks - 1].malloc();
        return (0 == m_ordered.GetSize())
            ? DqAllocator_Malloc(size)
            : m_ordered.ordered_malloc(LargeChunks(size));
    }

    void* Realloc(void* ptr, size_t newBytes, size_t oldBytes) {
        size_t oldNumChunks = NumChunks(oldBytes);
        size_t newNumChunks = NumChunks(newBytes);

        if (oldNumChunks > NUM_FIXED_POOLS && newNumChunks > NUM_FIXED_POOLS) {
            if (0 == m_ordered.GetSize())
                return DqAllocator_Realloc(ptr, newBytes);
            if (LargeChunks(newBytes) == LargeChunks(oldBytes))
                return ptr;
        } else {
            if (oldNumChunks <= NUM_FIXED_POOLS && newNumChunks <= NUM_FIXED_POOLS) {
                if (oldNumChunks == newNumChunks)
                    return ptr;
            }
        }

        void* newPtr = Alloc(newBytes);
        if (!newPtr) return nullptr;
        if (ptr) {
            std::memcpy(newPtr, ptr, std::min(newBytes, oldBytes));
            Free(ptr, oldBytes);
        }
        return newPtr;
    }

    void Free(void* ptr, size_t size) {
        DqAssert(ptr != this);
        size_t numChunks = NumChunks(size);
        if (numChunks <= NUM_FIXED_POOLS)
            m_fixed[numChunks - 1].free(ptr);
        else {
            if (0 == m_ordered.GetSize())
                DqAllocator_Free(ptr, size);
            else
                m_ordered.ordered_free(ptr, LargeChunks(size));
        }
    }

    void EmptyAll() {
        for (int i = 0; i < NUM_FIXED_POOLS; ++i)
            m_fixed[i].purge_memory_and_reinitialize();
        if (0 != m_ordered.GetSize())
            m_ordered.purge_memory_and_reinitialize();
    }
};

END_DQ_BASE_NAMESPACE
