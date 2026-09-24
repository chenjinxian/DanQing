// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BentleyAllocator.h
// DanQing dqBase — 跨 DLL 安全内存分配器实现
#include "dqBase/DqAllocator.h"

BEGIN_DQ_BASE_NAMESPACE

void* DqAllocator_New(size_t size) {
    return ::operator new(size);
}

void DqAllocator_Delete(void* ptr, size_t /*size*/) noexcept {
    ::operator delete(ptr);
}

void* DqAllocator_Malloc(size_t size) {
    return std::malloc(size);
}

void* DqAllocator_Calloc(size_t count, size_t size) {
    return std::calloc(count, size);
}

void* DqAllocator_Realloc(void* ptr, size_t newSize) {
    return std::realloc(ptr, newSize);
}

void DqAllocator_Free(void* ptr, size_t /*size*/) noexcept {
    std::free(ptr);
}

void* DqAllocator_GetNullRefBuffer() {
    static char nullBuf[1] = {0};
    return nullBuf;
}

void DqAllocator_EnableLowFragmentationCRTHeap() {
#if defined(_WIN32)
    // Windows 低碎片堆：HeapSetInformation
    // 简化实现：不操作
#endif
}

void* DqAllocator_AllocateRefCounted(size_t size) {
    return DqAllocator_New(size);
}

void DqAllocator_DeleteRefCounted(void* ptr, size_t size) noexcept {
    DqAllocator_Delete(ptr, size);
}

void* DqAllocator_AllocateArrayRefCounted(size_t size) {
    return DqAllocator_New(size);
}

void DqAllocator_DeleteArrayRefCounted(void* ptr, size_t size) noexcept {
    DqAllocator_Delete(ptr, size);
}

void* DqAllocator_AllocateIRefCounted(size_t size) {
    return DqAllocator_New(size);
}

void DqAllocator_DeleteIRefCounted(void* ptr, size_t size) noexcept {
    DqAllocator_Delete(ptr, size);
}

void* DqAllocator_AllocateArrayIRefCounted(size_t size) {
    return DqAllocator_New(size);
}

void DqAllocator_DeleteArrayIRefCounted(void* ptr, size_t size) noexcept {
    DqAllocator_Delete(ptr, size);
}

END_DQ_BASE_NAMESPACE
