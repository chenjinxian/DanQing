// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeThread.h
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeSharedMutex.h
// DanQing dqBase — 线程工具 / 条件变量 / 互斥锁 / 读写锁
//
// 1:1 对齐 imodel-native BeThread.h / BeSharedMutex.h:
//   - BeMutex (ref:45-81): opaque void*[] backed by std::recursive_mutex, Enter/Leave + lock/unlock
//   - BeMutexHolder (ref:88-123): std::unique_lock<recursive_mutex>-backed RAII wrapper
//   - BeConditionVariable (ref:129-203): IConditionVariablePredicate* surface + ConditionVariablePredicate<T>
//   - T_ThreadStart / THREAD_MAIN_* (ref:206-213)
//   - BeThreadUtilities (ref:219-252): StartNewThread, BeSleep(BeDuration/uint32_t),
//     GetCurrentThreadId→intptr_t, GetHardwareConcurrency→uint32_t, GetDefaultStackSize
//   - BeSystemMutexHolder (ref:259-269): process-global mutex holder
// BeSharedMutex 使用手写的读写锁实现（对齐参考），而非 std::shared_mutex。
#pragma once

#include "Export.h"
#include "BeAssert.h"
#include "NonCopyable.h"
#include "DqStatus.h"
#include "DqSync.h"
#include "DqTime.h"

#include <atomic>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

// ---------------------------------------------------------------------------
// Opaque-storage size constants — match ref:39-40 (sane-platform branch).
// We expose the std types directly via these arrays so the public ABI matches
// imodel-native (which static_asserts BeMutex == std::recursive_mutex).
// ---------------------------------------------------------------------------
#define BEMUTEX_DATA_ARRAY_LENGTH              (sizeof(std::recursive_mutex) / sizeof(void*))
#define BECONDITIONVARIABLE_DATA_ARRAY_LENGTH   (sizeof(std::condition_variable_any) / sizeof(void*))

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeRecursiveMutex — 递归互斥锁（对齐 ref BeSharedMutex.h:17）
// ---------------------------------------------------------------------------
using BeRecursiveMutex = std::recursive_mutex;

// ---------------------------------------------------------------------------
// BeSharedMutex — 手写读写锁（对齐 imodel-native BeSharedMutex.h:26-177）
//
// 基于 Howard Hinnant 的参考实现。使用 writeEnteredFlag + readersMask
// 在单个 unsigned state 中同时跟踪写入状态和读者计数。
// ---------------------------------------------------------------------------
struct BeSharedMutex
{
    typedef std::condition_variable cond_t;
    typedef unsigned                count_t;

    std::mutex m_gateMutex;
    cond_t     m_gate1;
    cond_t     m_gate2;
    count_t    m_state;

    static const count_t writeEnteredFlag = 1U << (sizeof(count_t) * CHAR_BIT - 1);
    static const count_t readersMask = ~writeEnteredFlag;

public:
    BeSharedMutex() : m_state(0) {}
    ~BeSharedMutex() { std::lock_guard<std::mutex> _(m_gateMutex); }

    // --- Exclusive ownership ---

    void lock()
        {
        std::unique_lock<std::mutex> lk(m_gateMutex);
        while (m_state & writeEnteredFlag)
            m_gate1.wait(lk);
        m_state |= writeEnteredFlag;
        while (m_state & readersMask)
            m_gate2.wait(lk);
        }

    bool try_lock()
        {
        std::unique_lock<std::mutex> lk(m_gateMutex);
        if (m_state == 0)
            {
            m_state = writeEnteredFlag;
            return true;
            }
        return false;
        }

    void unlock()
        {
        std::lock_guard<std::mutex> _(m_gateMutex);
        m_state = 0;
        m_gate1.notify_all();
        }

    // --- Shared ownership ---

    void lock_shared()
        {
        std::unique_lock<std::mutex> lk(m_gateMutex);
        while ((m_state & writeEnteredFlag) || (m_state & readersMask) == readersMask)
            m_gate1.wait(lk);
        count_t num_readers = (m_state & readersMask) + 1;
        m_state &= ~readersMask;
        m_state |= num_readers;
        }

    bool try_lock_shared()
        {
        std::unique_lock<std::mutex> lk(m_gateMutex);
        count_t num_readers = m_state & readersMask;
        if (!(m_state & writeEnteredFlag) && num_readers != readersMask)
            {
            ++num_readers;
            m_state &= ~readersMask;
            m_state |= num_readers;
            return true;
            }
        return false;
        }

    void unlock_shared()
        {
        std::lock_guard<std::mutex> _(m_gateMutex);
        count_t num_readers = (m_state & readersMask) - 1;
        m_state &= ~readersMask;
        m_state |= num_readers;
        if (m_state & writeEnteredFlag)
            {
            if (num_readers == 0)
                m_gate2.notify_one();
            }
        else
            {
            if (num_readers == readersMask - 1)
                m_gate1.notify_one();
            }
        }

    void unlock_and_lock_shared()
        {
        std::lock_guard<std::mutex> _(m_gateMutex);
        m_state &= ~writeEnteredFlag;
        m_state |= 1;  // one reader
        m_gate1.notify_all();
        }
};

// ---------------------------------------------------------------------------
// BeSharedMutexLock — 读写锁的共享锁守卫（对齐 BeSharedMutexLock<Mutex>）
// Ported from: imodel-native BeSharedMutex.h:182-292
//
// -fno-exceptions 适配: lock()/try_lock()/unlock() 中的异常路径改为 assert+abort。
// ---------------------------------------------------------------------------
template <class Mutex>
struct BeSharedMutexLock
{
public:
    typedef Mutex mutex_type;

private:
    mutex_type* m_mutex;
    bool        m_owns;

public:
    BeSharedMutexLock() : m_mutex(nullptr), m_owns(false) {}
    explicit BeSharedMutexLock(mutex_type& m) : m_mutex(&m), m_owns(true) { m_mutex->lock_shared(); }
    BeSharedMutexLock(mutex_type& m, std::defer_lock_t) : m_mutex(&m), m_owns(false) {}
    BeSharedMutexLock(mutex_type& m, std::try_to_lock_t) : m_mutex(&m), m_owns(m.try_lock_shared()) {}
    BeSharedMutexLock(mutex_type& m, std::adopt_lock_t) : m_mutex(&m), m_owns(true) {}

    ~BeSharedMutexLock()
        {
        if (m_owns)
            m_mutex->unlock_shared();
        }

    BeSharedMutexLock(BeSharedMutexLock&& sl) : m_mutex(sl.m_mutex), m_owns(sl.m_owns) { sl.m_mutex = nullptr; sl.m_owns = false; }

    BeSharedMutexLock& operator=(BeSharedMutexLock&& sl)
        {
        if (m_owns)
            m_mutex->unlock_shared();
        m_mutex = sl.m_mutex;
        m_owns = sl.m_owns;
        sl.m_mutex = nullptr;
        sl.m_owns = false;
        return *this;
        }

    void lock()
        {
        if (m_owns)
            {
            DqAssert(false && "BeSharedMutexLock::lock: already locked");
            return;
            }
        m_mutex->lock_shared();
        m_owns = true;
        }

    bool try_lock()
        {
        if (m_owns)
            {
            DqAssert(false && "BeSharedMutexLock::try_lock: already locked");
            return false;
            }
        m_owns = m_mutex->try_lock_shared();
        return m_owns;
        }

    void unlock()
        {
        if (!m_owns)
            {
            DqAssert(false && "BeSharedMutexLock::unlock: not locked");
            return;
            }
        m_mutex->unlock_shared();
        m_owns = false;
        }

    bool owns_lock() const { return m_owns; }
    mutex_type* mutex() const { return m_mutex; }

    mutex_type* release()
        {
        mutex_type* r = m_mutex;
        m_mutex = nullptr;
        m_owns = false;
        return r;
        }

    void swap(BeSharedMutexLock& u)
        {
        std::swap(m_mutex, u.m_mutex);
        std::swap(m_owns, u.m_owns);
        }
};

typedef BeSharedMutexLock<BeSharedMutex> BeSharedMutexHolder;

// ---------------------------------------------------------------------------
// BeMutex — 递归独占互斥锁（对齐 ref BeThread.h:56-81）
// Ported from: imodel-native BeThread.h BeMutex
//
// ref 的实现：`void* m_osMutex[]` 由 std::recursive_mutex 支撑，构造/析构/lock/unlock
// 都是 BENTLEYDLL_EXPORT 导出的（在 .cpp 中 placement-new std::recursive_mutex）。
// 我们保持同样的不透明数组布局以对齐 ABI（ref 在 .cpp 中有 static_assert 大小相等）。
// 提供 ref:76,80 的 Enter()/Leave() 别名。
// ---------------------------------------------------------------------------
struct BeMutex : DqNonCopyable
{
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif

private:
    void* m_osMutex[BEMUTEX_DATA_ARRAY_LENGTH];

#ifdef __clang__
#pragma clang diagnostic pop
#endif

public:
    DQ_BASE_EXPORT BeMutex();
    DQ_BASE_EXPORT ~BeMutex();

    //! lock the mutex (ref:75)
    DQ_BASE_EXPORT void lock();
    //! Enter — alias for lock (ref:76)
    void Enter() { lock(); }

    //! unlock the mutex (ref:79)
    DQ_BASE_EXPORT void unlock();
    //! Leave — alias for unlock (ref:80)
    void Leave() { unlock(); }
};

// Pointer-suffix typedefs (ref uses DEFINE_POINTER_SUFFIX_TYPEDEFS; DanQing spells it out)
using BeMutexP = BeMutex*;

// ---------------------------------------------------------------------------
// BeMutexHolder — BeMutex 的 RAII 持有者（对齐 ref BeThread.h:88-123）
// Ported from: imodel-native BeThread.h BeMutexHolder
//
// ref 的实现：`std::unique_lock<std::recursive_mutex>` 支撑的不透明数组，
// 提供 Lock::Yes/No 构造、GetMutex/lock/unlock/owns_lock 查询。
// ---------------------------------------------------------------------------
struct BeMutexHolder : DqNonCopyable
{
private:
    // Opaque storage sized to hold std::unique_lock<std::recursive_mutex> (ref:151).
    void* m_osHolder[(sizeof(std::unique_lock<std::recursive_mutex>) + sizeof(void*) - 1) / sizeof(void*)];

public:
    //! Whether to acquire the lock on construction (ref:105)
    enum class Lock : bool { No = false, Yes = true };

    //! Associate this BeMutexHolder with a BeMutex, optionally locking it. (ref:110)
    DQ_BASE_EXPORT BeMutexHolder(BeMutex& mutex, Lock lock = Lock::Yes);
    //! Unlocks BeMutex if locked. (ref:113)
    DQ_BASE_EXPORT ~BeMutexHolder();

    //! Get the BeMutex associated with this BeMutexHolder (ref:116)
    DQ_BASE_EXPORT BeMutex* GetMutex();
    //! Unlock the BeMutex (ref:118)
    DQ_BASE_EXPORT void unlock();
    //! Lock the BeMutex (ref:120)
    DQ_BASE_EXPORT void lock();
    //! Determine whether the BeMutex is currently locked by this BeMutexHolder (ref:122)
    DQ_BASE_EXPORT bool owns_lock();
};

using BeMutexHolderP = BeMutexHolder*;

// ---------------------------------------------------------------------------
// IConditionVariablePredicate — 条件变量谓词接口（对齐 ref BeThread.h:129-134）
// Ported from: imodel-native BeThread.h IConditionVariablePredicate
// ---------------------------------------------------------------------------
struct BeConditionVariable;

struct IConditionVariablePredicate
{
    //! WaitOnCondition calls _TestCondition with the mutex locked. WaitOnCondition
    //! returns to its caller if _TestCondition returns true or it gets a timeout.
    virtual bool _TestCondition(BeConditionVariable& cv) = 0;
    virtual ~IConditionVariablePredicate() = default;
};

using IConditionVariablePredicateP = IConditionVariablePredicate*;

// ---------------------------------------------------------------------------
// ConditionVariablePredicate<T> — wraps a callable as IConditionVariablePredicate
// Ported from: imodel-native BeThread.h ConditionVariablePredicate (ref:140-147)
// ---------------------------------------------------------------------------
template<typename T>
struct ConditionVariablePredicate : IConditionVariablePredicate
{
    T m_predicate;

    explicit ConditionVariablePredicate(T predicate) : m_predicate(predicate) {}

    virtual bool _TestCondition(BeConditionVariable& cv) override { return m_predicate(cv); }
};

// ---------------------------------------------------------------------------
// BeConditionVariable — 条件变量（对齐 ref BeThread.h:157-203）
// Ported from: imodel-native BeThread.h BeConditionVariable
//
// ref 实现：内部 mutable BeMutex + std::condition_variable_any 支撑的 void*[]。
// 提供 InfiniteWait / RelativeWait / ProtectedWaitOnCondition / WaitOnCondition
// （谓词参数为 IConditionVariablePredicate* —— ref:193-197）。
// ---------------------------------------------------------------------------
struct BeConditionVariable : DqNonCopyable
{
private:
    void*             m_osCV[BECONDITIONVARIABLE_DATA_ARRAY_LENGTH];
    mutable BeMutex   m_mutex;

public:
    static const uint32_t Infinite = 0xffffffff;

    DQ_BASE_EXPORT BeConditionVariable();
    DQ_BASE_EXPORT ~BeConditionVariable();

    DQ_BASE_EXPORT void InfiniteWait(BeMutexHolder& holder);
    DQ_BASE_EXPORT bool RelativeWait(BeMutexHolder& holder, uint32_t timeoutMillis);

    //! Get the internal mutex (ref:173)
    BeMutex& GetMutex() const { return m_mutex; }

    //! Like WaitOnCondition, but does not enter the mutex at the start of the method or leave it at the end. (ref:177)
    //! Use this if the caller has already locked the mutex.
    DQ_BASE_EXPORT bool ProtectedWaitOnCondition(BeMutexHolder& holder, IConditionVariablePredicate* predicate, uint32_t timeoutMillis);

    //! Enters the mutex, calls the predicate, and then waits on the condition if the predicate returns false. (ref:193)
    bool WaitOnCondition(IConditionVariablePredicate* predicate, uint32_t timeoutMillis)
        {
        BeMutexHolder holder(m_mutex);
        return ProtectedWaitOnCondition(holder, predicate, timeoutMillis);
        }

    //! Notify one thread waiting on this BeConditionVariable. (ref:200)
    DQ_BASE_EXPORT void notify_one();
    //! Notify all threads waiting on this BeConditionVariable. (ref:202)
    DQ_BASE_EXPORT void notify_all();
};

using BeConditionVariableP = BeConditionVariable*;

// ---------------------------------------------------------------------------
// T_ThreadStart / THREAD_MAIN_* — 线程入口函数指针类型（对齐 ref:205-213）
// Ported from: imodel-native BeThread.h (ref:205-213)
// ---------------------------------------------------------------------------
#if defined(__APPLE__) || defined(ANDROID) || defined(__linux__) || defined(__EMSCRIPTEN__)
    typedef void* (*T_ThreadStart)(void*);
    #define THREAD_MAIN_IMPL void*
#elif defined(_WIN32)
    typedef unsigned (__stdcall *T_ThreadStart)(void*);
    #define THREAD_MAIN_IMPL unsigned __stdcall
#endif

#define THREAD_MAIN_DECL static THREAD_MAIN_IMPL

// ---------------------------------------------------------------------------
// BeThreadUtilities — 线程工具（对齐 ref BeThread.h:219-252）
// Ported from: imodel-native BeThread.h BeThreadUtilities
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT BeThreadUtilities
{
    //! Set the name for the current thread. Useful for debugging only, not guaranteed to do anything (ref:223)
    static void SetCurrentThreadName(const char* newName);

    //! Get the default size of the stack for a thread in bytes. (ref:226)
    static int GetDefaultStackSize();

    //! Start a new thread. (ref:232)
    //! @param[in] startAddr the function to call at thread start. Thread exits when this function returns.
    //! @param[in] arg Argument to startAddr
    //! @param[in] stackSize the number of bytes for the newly created thread's stack
    static DqStatus StartNewThread(T_ThreadStart startAddr, void* arg = nullptr, int stackSize = GetDefaultStackSize());

    //! Suspend the current thread for a specified amount of time (ref:237)
    //! @note this method is deprecated. Use BeDuration::Sleep
    static void BeSleep(BeDuration sleepTime) { sleepTime.Sleep(); }

    //! Suspend the current thread for a specified number of milliseconds (ref:242)
    //! @note this method is deprecated. Use BeDuration::Sleep
    static void BeSleep(uint32_t millis) { BeDuration::FromMilliseconds(millis).Sleep(); }

    //! Get the identifier of the current thread (ref:245). Returns intptr_t.
    static intptr_t GetCurrentThreadId();

    //! see documentation for std::thread::hardware_concurrency() (ref:248). Returns uint32_t.
    static uint32_t GetHardwareConcurrency();

    //! Get the identifier of the currently running process (ref:251)
    static uint64_t GetCurrentProcessId();
};

// ---------------------------------------------------------------------------
// BeSystemMutexHolder — 进程级全局互斥锁持有者（对齐 ref BeThread.h:259-269）
// Ported from: imodel-native BeThread.h BeSystemMutexHolder
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT BeSystemMutexHolder : BeMutexHolder
{
    //! Enters the system mutex (ref:262)
    BeSystemMutexHolder() : BeMutexHolder(GetSystemMutex()) {}

    //! Initialize the system mutex (ref:264). Must be called once before GetSystemMutex.
    static void StartupInitializeSystemMutex();

    //! Get the system mutex. Can be used to bootstrap other BeMutexs. (ref:268)
    static BeMutex& GetSystemMutex();
};

END_DQ_BASE_NAMESPACE
