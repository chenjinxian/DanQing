// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/GlobalHandleContainer.h
// DanQing dqBase — 全局句柄容器实现
#include "dqBase/GlobalHandleContainer.h"

#include <mutex>
#include <unordered_map>
#include <atomic>

BEGIN_DQ_BASE_NAMESPACE

namespace {
std::mutex g_mutex;
std::unordered_map<uint32_t, void*> g_handleToPtr;
std::unordered_map<void*, uint32_t> g_ptrToHandle;
std::atomic<uint32_t> g_nextHandle{1};
} // namespace

uint32_t GlobalHandleContainer::AllocateHandle(void* ptr) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_ptrToHandle.find(ptr);
    if (it != g_ptrToHandle.end())
        return it->second;
    uint32_t handle = g_nextHandle++;
    g_handleToPtr[handle] = ptr;
    g_ptrToHandle[ptr] = handle;
    return handle;
}

void GlobalHandleContainer::ReleaseHandle(uint32_t handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_handleToPtr.find(handle);
    if (it != g_handleToPtr.end()) {
        g_ptrToHandle.erase(it->second);
        g_handleToPtr.erase(it);
    }
}

void* GlobalHandleContainer::GetPointer(uint32_t handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_handleToPtr.find(handle);
    return it != g_handleToPtr.end() ? it->second : nullptr;
}

void GlobalHandleContainer::Destroy() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_handleToPtr.clear();
    g_ptrToHandle.clear();
}

END_DQ_BASE_NAMESPACE
