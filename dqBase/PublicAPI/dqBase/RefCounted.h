// SPDX-License-Identifier: Apache-2.0
// Authored: intrusive reference counting; no equivalent in itwinjs-core or imodel-native
// DanQing dqBase — 侵入式引用计数（跨 DLL 安全，自研）
//
// 设计依据：CLAUDE.md §9 所有权规则（RefCounted<T> CRTP + RefPtr<T>，禁 shared_ptr）
// 计数器用 std::atomic<int>（替代 Qt QAtomicInt），内存序同原设计。
#pragma once

#include "DqBase.h"
#include "DqSync.h"

#include <atomic>
#include <cstdint>
#include <utility>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// 纯虚接口（跨 DLL 契约）
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT IRefCounted {
    virtual ~IRefCounted() = default;
    virtual void AddRef() const noexcept = 0;
    virtual void Release() const noexcept = 0;
};

// ---------------------------------------------------------------------------
// CRTP 基类：注入原子引用计数
// ---------------------------------------------------------------------------
template<typename Derived>
class RefCounted : public IRefCounted {
public:
    RefCounted() noexcept = default;
    ~RefCounted() override = default;

    RefCounted(const RefCounted&) = delete; // 引用计数对象不可拷贝
    RefCounted& operator=(const RefCounted&) = delete;

    void AddRef() const noexcept override {
        // relaxed：仅本对象引用计数的局部递增，无需建立跨线程同步
        m_refCount.fetch_add(1, std::memory_order_relaxed);
    }
    void Release() const noexcept override {
        // acq_rel：归零时确保此前所有写操作对释放/析构可见
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }

    // 仅供测试与诊断观察当前引用计数
    uint32_t RefCount() const noexcept {
        return static_cast<uint32_t>(m_refCount.load(std::memory_order_relaxed));
    }

private:
    mutable DqAtomicInt m_refCount{ 0 };
};

// 便捷非 CRTP 基类
using RefCountedBase = RefCounted<IRefCounted>;

// ---------------------------------------------------------------------------
// 非侵入式智能指针（调用 AddRef/Release）
// ---------------------------------------------------------------------------
template<typename T>
class RefPtr {
public:
    RefPtr() noexcept = default;
    RefPtr(std::nullptr_t) noexcept {} // NOLINT(google-explicit-constructor)
    explicit RefPtr(T* p) noexcept
            : m_ptr(p) {
        if (m_ptr)
            m_ptr->AddRef();
    }

    // 跨类型转换构造（继承层次，从 U* 可隐式到 T*）
    template<typename U, typename = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    RefPtr(const RefPtr<U>& o) noexcept
            : m_ptr(o.Get()) {
        if (m_ptr)
            m_ptr->AddRef();
    }

    RefPtr(const RefPtr& o) noexcept
            : m_ptr(o.m_ptr) {
        if (m_ptr)
            m_ptr->AddRef();
    }
    RefPtr(RefPtr&& o) noexcept
            : m_ptr(o.m_ptr) {
        o.m_ptr = nullptr;
    }

    // copy-and-swap 赋值（自赋值安全）
    RefPtr& operator=(RefPtr o) noexcept {
        swap(o);
        return *this;
    }

    ~RefPtr() {
        if (m_ptr)
            m_ptr->Release();
    }

    T& operator*() const noexcept {
        return *m_ptr;
    }
    T* operator->() const noexcept {
        return m_ptr;
    }
    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

    T* Get() const noexcept {
        return m_ptr;
    }
    bool IsValid() const noexcept {
        return m_ptr != nullptr;
    }
    bool IsNull() const noexcept {
        return m_ptr == nullptr;
    }

    void Reset(T* p = nullptr) noexcept {
        if (p)
            p->AddRef();
        if (m_ptr)
            m_ptr->Release();
        m_ptr = p;
    }
    void swap(RefPtr& o) noexcept {
        std::swap(m_ptr, o.m_ptr);
    }

    // 静态转换（继承层次下转/侧转）
    template<typename U>
    RefPtr<U> StaticCast() const noexcept {
        return RefPtr<U>(static_cast<U*>(m_ptr));
    }

private:
    template<typename U>
    friend class RefPtr;
    T* m_ptr = nullptr;
};

// 自由函数形式的静态转换
template<typename Dst, typename Src>
RefPtr<Dst> StaticCast(const RefPtr<Src>& p) noexcept {
    return p.template StaticCast<Dst>();
}

END_DQ_BASE_NAMESPACE
