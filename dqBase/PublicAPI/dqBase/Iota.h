// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Iota.h
// DanQing dqBase — 迭代器范围生成器
//
// 1:1 对齐 imodel-native Iota。
// 生成 [0, N) 的整数范围，用于测试和 parallel_for。
#pragma once

#include "Export.h"

#include <cstddef>
#include <iterator>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// Iota — 整数范围生成器
// Ported from: imodel-native Iota.h
// ---------------------------------------------------------------------------
struct Iota {
    struct const_iterator {
        using iterator_category = std::random_access_iterator_tag;
        using value_type = size_t;
        using difference_type = ptrdiff_t;
        using pointer = const size_t*;
        using reference = const size_t&;

        size_t value;

        const_iterator() : value(0) {}
        explicit const_iterator(size_t v) : value(v) {}

        size_t operator*() const { return value; }
        const_iterator& operator++() { ++value; return *this; }
        const_iterator operator++(int) { auto tmp = *this; ++value; return tmp; }
        const_iterator& operator--() { --value; return *this; }
        const_iterator operator--(int) { auto tmp = *this; --value; return tmp; }

        const_iterator& operator+=(ptrdiff_t n) { value += n; return *this; }
        const_iterator& operator-=(ptrdiff_t n) { value -= n; return *this; }
        const_iterator operator+(ptrdiff_t n) const { return const_iterator(value + n); }
        const_iterator operator-(ptrdiff_t n) const { return const_iterator(value - n); }
        ptrdiff_t operator-(const const_iterator& rhs) const { return ptrdiff_t(value) - ptrdiff_t(rhs.value); }

        size_t operator[](ptrdiff_t n) const { return value + n; }

        bool operator==(const const_iterator& rhs) const { return value == rhs.value; }
        bool operator!=(const const_iterator& rhs) const { return value != rhs.value; }
        bool operator<(const const_iterator& rhs) const { return value < rhs.value; }
        bool operator<=(const const_iterator& rhs) const { return value <= rhs.value; }
        bool operator>(const const_iterator& rhs) const { return value > rhs.value; }
        bool operator>=(const const_iterator& rhs) const { return value >= rhs.value; }
    };

    explicit Iota(size_t n) : m_count(n) {}

    const_iterator begin() const { return const_iterator(0); }
    const_iterator end() const { return const_iterator(m_count); }
    size_t size() const { return m_count; }

private:
    size_t m_count;
};

END_DQ_BASE_NAMESPACE
