// SPDX-License-Identifier: Apache-2.0
// Ported from: filament libs/utils/include/utils/Mutex.h
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeThread.h
// DanQing dqBase — 同步原语 thin wrapper
//
// 对齐 imodel-native BeMutex/BeMutexHolder + filament utils::Mutex 模式。
// 直接用 C++ 标准库，消除 Qt QMutex/QMutexLocker 依赖。
#pragma once

#include "Export.h"

#include <atomic>
#include <mutex>

BEGIN_DQ_BASE_NAMESPACE

// --- 互斥锁（对齐 imodel-native BeMutex） ---
// BeThread.h:52 明确 BeMutex "offers exclusive, recursive ownership semantics"
// 且 "@see std::recursive_mutex"。为保持移植代码的同线程重入行为不死锁，
// DqMutex 必须是 std::recursive_mutex（与 DqRecursiveMutex 同型）。
using DqMutex          = std::recursive_mutex;
using DqRecursiveMutex = std::recursive_mutex;

// --- RAII 锁守卫（对齐 imodel-native BeMutexHolder / filament LockGuard） ---
template<typename M>
using DqLockGuard = std::lock_guard<M>;

// --- 原子整型（对齐 imodel-native BeAtomic<int>） ---
using DqAtomicInt = std::atomic<int>;

END_DQ_BASE_NAMESPACE
