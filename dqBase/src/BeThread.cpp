// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/Bentley/nonport/BeThread.cpp
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeThread.h
// DanQing dqBase — BeMutex/BeMutexHolder/BeConditionVariable/BeThreadUtilities/BeSystemMutexHolder 实现
//
// 对齐 ref BeThread.cpp 的实现策略：
//   - BeMutex/BeMutexHolder/BeConditionVariable 的 void*[] 用 placement-new 支撑对应的
//     std 类型（recursive_mutex / unique_lock<recursive_mutex> / condition_variable_any），
//     并通过 reinterpret_cast 在方法内转发（ref:152-176 static_assert 大小一致后同样手法）。
//   - StartNewThread 用 pthread_create + detach（ref:338-371）；-fno-exceptions 下
//     失败仅返回 ERROR，不抛异常（CLAUDE.md §9）。
//   - GetCurrentThreadId 返回 intptr_t（ref:211-232）：POSIX 用 (intptr_t)pthread_self()。
#include "dqBase/BeThread.h"

#include <chrono>
#include <cstring>
#include <new>

#if defined(__APPLE__)
    #include <pthread.h>
    #include <unistd.h>
#elif defined(__linux__)
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/types.h>
#elif defined(_WIN32)
    #include <windows.h>
    #include <process.h>
#endif

BEGIN_DQ_BASE_NAMESPACE

//=======================================================================================
// BeMutex / BeMutexHolder / BeConditionVariable — backed by std types via placement-new
// (对齐 ref BeThread.cpp:142-176。ref static_asserts 大小一致后用 reinterpret_cast 转发。)
//=======================================================================================
static_assert(sizeof(BeMutex) == sizeof(std::recursive_mutex),
              "BeMutex must match std::recursive_mutex layout");
static_assert(sizeof(BeMutexHolder) == sizeof(std::unique_lock<std::recursive_mutex>),
              "BeMutexHolder must match std::unique_lock<std::recursive_mutex> layout");
static_assert(sizeof(BeConditionVariable) >= sizeof(std::condition_variable_any),
              "BeConditionVariable must hold std::condition_variable_any");

static std::recursive_mutex& to_mutex(BeMutex* bmutex) noexcept {
    return *reinterpret_cast<std::recursive_mutex*>(bmutex);
}

static std::unique_lock<std::recursive_mutex>& to_uniquelock(BeMutexHolder* bholder) noexcept {
    return *reinterpret_cast<std::unique_lock<std::recursive_mutex>*>(bholder);
}

static std::condition_variable_any& to_cv(BeConditionVariable* bcv) noexcept {
    return *reinterpret_cast<std::condition_variable_any*>(bcv);
}

// --- BeMutex ---
BeMutex::BeMutex() {
    new (m_osMutex) std::recursive_mutex();
}
BeMutex::~BeMutex() {
    // ref 注释：May assert if mutex is still locked.
    to_mutex(this).~recursive_mutex();
}
void BeMutex::lock() { to_mutex(this).lock(); }
void BeMutex::unlock() { to_mutex(this).unlock(); }

// --- BeMutexHolder ---
BeMutexHolder::BeMutexHolder(BeMutex& mutex, Lock lock) {
    if (lock == Lock::Yes)
        new (m_osHolder) std::unique_lock<std::recursive_mutex>(to_mutex(&mutex));
    else
        new (m_osHolder) std::unique_lock<std::recursive_mutex>(to_mutex(&mutex), std::defer_lock);
}
BeMutexHolder::~BeMutexHolder() {
    to_uniquelock(this).~unique_lock();
}
BeMutex* BeMutexHolder::GetMutex() {
    return reinterpret_cast<BeMutex*>(to_uniquelock(this).mutex());
}
void BeMutexHolder::unlock() { to_uniquelock(this).unlock(); }
void BeMutexHolder::lock() { to_uniquelock(this).lock(); }
bool BeMutexHolder::owns_lock() { return to_uniquelock(this).owns_lock(); }

// --- BeConditionVariable ---
BeConditionVariable::BeConditionVariable() {
    new (m_osCV) std::condition_variable_any();
}
BeConditionVariable::~BeConditionVariable() {
    to_cv(this).~condition_variable_any();
}
void BeConditionVariable::InfiniteWait(BeMutexHolder& holder) {
    to_cv(this).wait(to_uniquelock(&holder));
}
bool BeConditionVariable::RelativeWait(BeMutexHolder& holder, uint32_t timeoutMillis) {
    return std::cv_status::timeout == to_cv(this).wait_for(to_uniquelock(&holder), std::chrono::milliseconds(timeoutMillis));
}
void BeConditionVariable::notify_all() { to_cv(this).notify_all(); }
void BeConditionVariable::notify_one() { to_cv(this).notify_one(); }

// Ported from: imodel-native BeThread.cpp:181-206 ProtectedWaitOnCondition
bool BeConditionVariable::ProtectedWaitOnCondition(BeMutexHolder& holder, IConditionVariablePredicate* condition, uint32_t timeoutMillis) {
    bool conditionSatisfied = (nullptr == condition) ? false : condition->_TestCondition(*this);
    bool timedOut           = (timeoutMillis == 0);

    while (!conditionSatisfied && (Infinite == timeoutMillis || !timedOut)) {
        if (Infinite == timeoutMillis) {
            InfiniteWait(holder);
        } else {
            uint32_t startTicks = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();

            timedOut = RelativeWait(holder, timeoutMillis);

            uint32_t elapsedTicks = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count() - startTicks;
            timeoutMillis = elapsedTicks > timeoutMillis ? 0 : (timeoutMillis - elapsedTicks);
        }

        conditionSatisfied = (nullptr == condition) ? true : condition->_TestCondition(*this);
    }

    return conditionSatisfied;
}

//=======================================================================================
// BeSystemMutexHolder — process-global system mutex (ref:276-289)
//=======================================================================================
static BeMutex* s_systemCS = nullptr;

void BeSystemMutexHolder::StartupInitializeSystemMutex() {
    if (nullptr == s_systemCS)
        s_systemCS = new BeMutex();
}

BeMutex& BeSystemMutexHolder::GetSystemMutex() {
    StartupInitializeSystemMutex();
    return *s_systemCS;
}

//=======================================================================================
// BeThreadUtilities — thread utility implementations (ref:294-371)
//=======================================================================================

// Ported from: imodel-native BeThread.cpp:294-323
void BeThreadUtilities::SetCurrentThreadName(const char* name) {
#if defined(__APPLE__)
    pthread_setname_np(name);
#elif defined(__linux__)
    pthread_setname_np(pthread_self(), name);
#elif defined(_WIN32)
    // Windows: 用 SetThreadDescription (Win10+) 或忽略
    (void)name;
#else
    (void)name;
#endif
}

// Ported from: imodel-native BeThread.cpp:328-333
int BeThreadUtilities::GetDefaultStackSize() {
    // ref: 8MB default on linux; we use 2MB as ref's GetDefaultStackSize returns.
    return 2 * 1024 * 1024;
}

// Ported from: imodel-native BeThread.cpp:338-371
// -fno-exceptions (CLAUDE.md §9): pthread_create failures return ERROR, no throw.
DqStatus BeThreadUtilities::StartNewThread(T_ThreadStart startAddr, void* arg, int stackSize) {
#if defined(__APPLE__) || defined(__linux__) || defined(ANDROID)
    pthread_attr_t threadAttr;
    int result = pthread_attr_init(&threadAttr);
    DqAssert(0 == result);
    (void)result;

    result = pthread_attr_setstacksize(&threadAttr, stackSize);
    DqAssert(0 == result);
    (void)result;

    pthread_t threadHandle;
    int retval = pthread_create(&threadHandle, &threadAttr, startAddr, arg);
    DqAssert(0 == retval);

    if (0 == retval) {
        result = pthread_detach(threadHandle);
        DqAssert(0 == result);
    }
    result = pthread_attr_destroy(&threadAttr);
    DqAssert(0 == result);
    return (0 == retval) ? DqStatus::Success : DqStatus::Error;
#elif defined(_WIN32)
    // _beginthreadex returns 0 on failure.
    uintptr_t handle = _beginthreadex(nullptr, (unsigned)stackSize, startAddr, arg, 0, nullptr);
    if (0 == handle)
        return DqStatus::Error;
    CloseHandle((HANDLE)handle);
    return DqStatus::Success;
#else
    #error unknown platform
    return DqStatus::Error;
#endif
}

// Ported from: imodel-native BeThread.cpp:211-232
intptr_t BeThreadUtilities::GetCurrentThreadId() {
#if defined(__APPLE__) || defined(__linux__) || defined(ANDROID)
    // ref 注释：不要改成 gettid — MobileDgnRPC 依赖 pthread_create 的返回值。
    return (intptr_t)pthread_self();
#elif defined(_WIN32)
    return (intptr_t)::GetCurrentThreadId();
#else
    return (intptr_t)-1;
#endif
}

// Ported from: imodel-native BeThread.cpp:136-139
uint32_t BeThreadUtilities::GetHardwareConcurrency() {
    return std::thread::hardware_concurrency();
}

uint64_t BeThreadUtilities::GetCurrentProcessId() {
#if defined(__APPLE__) || defined(__linux__)
    return static_cast<uint64_t>(::getpid());
#elif defined(_WIN32)
    return static_cast<uint64_t>(::GetCurrentProcessId());
#else
    return 0;
#endif
}

END_DQ_BASE_NAMESPACE
