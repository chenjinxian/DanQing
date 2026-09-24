// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Nullable.h
// DanQing dqBase — Nullable<T> 可空值类型
//
// 1:1 对齐 imodel-native Nullable<T>。
// 比 std::optional 更符合 BIM 语义的可空值包装。
#pragma once

#include "Export.h"
#include "BeAssert.h"

#include <cstddef>
#include <utility>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// Nullable<T> — 可空值类型包装
// Ported from: imodel-native Nullable.h
//
// 与参考实现的差异（最小化改动，记录如下）：
//  - 成员命名沿用 DanQing 的 m_hasValue（true=valid），与参考实现的 m_isNull（true=invalid）
//    极性相反。IsNull()/IsValid() 语义与参考完全一致。不切换极性是因为所有方法
//    均引用此标志，切换属侵入式重写且无语义收益。
//  - 成员顺序对齐参考实现：先 T m_value; 后 bool m_hasValue;。
//  - 额外保留 ValueOr()（参考实现无；audit §6 容忍）。
// 其余 API（get() nullptr-when-null、DqAssert(IsValid()) on operator->/Value()、
// struct final、所有构造/赋值/比较）与参考实现 1:1 对齐。
// ---------------------------------------------------------------------------
template<typename T>
struct Nullable final {
public:
    constexpr Nullable() noexcept : m_value{}, m_hasValue(false) {}
    constexpr Nullable(std::nullptr_t) noexcept : m_value{}, m_hasValue(false) {} // NOLINT
    constexpr Nullable(const T& value) : m_value(value), m_hasValue(true) {}      // NOLINT
    constexpr Nullable(T&& value) noexcept : m_value(std::move(value)), m_hasValue(true) {} // NOLINT

    constexpr Nullable(const Nullable&) = default;
    constexpr Nullable(Nullable&&) noexcept = default;
    Nullable& operator=(const Nullable&) = default;
    Nullable& operator=(Nullable&&) noexcept = default;

    constexpr bool IsNull() const noexcept { return !m_hasValue; }
    constexpr bool IsValid() const noexcept { return m_hasValue; }
    constexpr explicit operator bool() const noexcept { return m_hasValue; }

    /// 指针访问；null 时返回 nullptr（1:1 对齐参考 get()）
    constexpr const T* get() const noexcept { return m_hasValue ? &m_value : nullptr; }
    constexpr T* get() noexcept { return m_hasValue ? &m_value : nullptr; }

    /// 获取值（BeAssert(IsValid()) —— 参考实现）
    const T& Value() const { DqAssert(IsValid()); return m_value; }
    T& ValueR() { DqAssert(IsValid()); return m_value; }

    /// 获取值或默认值（DanQing 扩展；audit §6 容忍）
    constexpr T ValueOr(T fallback) const {
        return m_hasValue ? m_value : std::move(fallback);
    }

    /// 解引用（参考实现的 operator* 转发到 Value()/ValueR()，带 BeAssert）
    const T& operator*() const { return Value(); }
    T& operator*() { return ValueR(); }
    const T* operator->() const { DqAssert(IsValid()); return &m_value; }
    T* operator->() { DqAssert(IsValid()); return &m_value; }

    /// 赋值
    Nullable& operator=(std::nullptr_t) noexcept {
        m_hasValue = false;
        return *this;
    }
    Nullable& operator=(const T& value) {
        m_hasValue = true;
        m_value = value;
        return *this;
    }
    Nullable& operator=(T&& value) noexcept {
        m_hasValue = true;
        m_value = std::move(value);
        return *this;
    }

    /// 比较
    constexpr bool operator==(const Nullable& rhs) const noexcept {
        if (m_hasValue != rhs.m_hasValue) return false;
        if (!m_hasValue) return true;
        return m_value == rhs.m_value;
    }
    constexpr bool operator!=(const Nullable& rhs) const noexcept { return !(*this == rhs); }
    constexpr bool operator==(std::nullptr_t) const noexcept { return !m_hasValue; }
    constexpr bool operator!=(std::nullptr_t) const noexcept { return m_hasValue; }

private:
    T m_value;
    bool m_hasValue;
};

END_DQ_BASE_NAMESPACE
