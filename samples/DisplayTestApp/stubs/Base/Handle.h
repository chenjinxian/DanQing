// Stub: Base::Reference<T> and Base::Handled for FreeCAD UI shell
// Original: /Users/xunzhang/Documents/GitHub/FreeCAD/src/Base/Handle.h
#pragma once

#include <FCGlobal.h>
#include <atomic>

namespace Base {

class BaseExport Handled
{
public:
    Handled() = default;
    Handled(const Handled&) = delete;
    Handled& operator=(const Handled&) = delete;
    Handled(Handled&&) = delete;
    Handled& operator=(Handled&&) = delete;
    virtual ~Handled() = default;

    void ref() const { ++m_refCount; }
    void unref() const { if (--m_refCount == 0) delete this; }

private:
    mutable std::atomic<int> m_refCount{0};
};

template<typename T>
class Reference
{
public:
    Reference() = default;
    Reference(T* ptr) : m_ptr(ptr) { if (m_ptr) m_ptr->ref(); }
    Reference(const Reference& other) : m_ptr(other.m_ptr) { if (m_ptr) m_ptr->ref(); }
    Reference(Reference&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
    ~Reference() { if (m_ptr) m_ptr->unref(); }

    Reference& operator=(const Reference& other) {
        if (m_ptr) m_ptr->unref();
        m_ptr = other.m_ptr;
        if (m_ptr) m_ptr->ref();
        return *this;
    }
    Reference& operator=(Reference&& other) noexcept {
        if (m_ptr) m_ptr->unref();
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
        return *this;
    }

    T* operator->() const { return m_ptr; }
    T& operator*() const { return *m_ptr; }
    bool isValid() const { return m_ptr != nullptr; }

private:
    T* m_ptr = nullptr;
};

} // namespace Base
