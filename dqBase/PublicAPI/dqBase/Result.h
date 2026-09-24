// SPDX-License-Identifier: Apache-2.0
// Authored: no reference type exists for this.
//   itwinjs-core uses exceptions (core/bentley/src/BentleyError.ts) — unavailable
//   under -fno-exceptions (CLAUDE.md §9). imodel-native uses BentleyStatus/DbResult
//   return codes (core/bentley/src/BeSQLite.ts) which carry no value payload.
//   Result<T,E> is the minimal C++20 no-exceptions adaptation. Documented deviation
//   per CLAUDE.md §6 — NOT a port. Do not cite a reference source for this type.
// DanQing dqBase — Result<T,E>（错误处理，优于状态码与异常）
#pragma once

#include "DqBase.h"

#include <utility>
#include <variant>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// Result<T, E>：成功值 T 或错误 E
// ---------------------------------------------------------------------------
template<typename T, typename E>
class Result {
public:
    // 静态工厂：避免 T/E 类型构造歧义
    static Result Ok(T value) noexcept {
        return Result(std::in_place_index<0>, std::move(value));
    }
    static Result Err(E error) noexcept {
        return Result(std::in_place_index<1>, std::move(error));
    }

    bool IsOk() const noexcept {
        return m_data.index() == 0;
    }
    bool IsErr() const noexcept {
        return m_data.index() == 1;
    }

    const T& Value() const {
        return std::get<0>(m_data);
    }
    const E& Error() const {
        return std::get<1>(m_data);
    }

    T ValueOr(T fallback) const noexcept {
        return IsOk() ? std::get<0>(m_data) : std::move(fallback);
    }

private:
    template<size_t I, typename Arg>
    Result(std::in_place_index_t<I> tag, Arg&& arg)
            : m_data(tag, std::forward<Arg>(arg)) {}

    std::variant<T, E> m_data;
};

// ---------------------------------------------------------------------------
// Result<void, E>：无成功值，仅有错误可能
// ---------------------------------------------------------------------------
template<typename E>
class Result<void, E> {
public:
    static Result Ok() noexcept {
        return Result(/*hasError=*/false);
    }
    static Result Err(E error) noexcept {
        return Result(std::move(error));
    }

    Result() noexcept
            : m_hasError(false) {} // 默认即成功
    explicit Result(E error) noexcept
            : m_hasError(true),
              m_error(std::move(error)) {}

    bool IsOk() const noexcept {
        return !m_hasError;
    }
    bool IsErr() const noexcept {
        return m_hasError;
    }
    const E& Error() const {
        return m_error;
    }

private:
    bool m_hasError;
    E m_error;
};

END_DQ_BASE_NAMESPACE
