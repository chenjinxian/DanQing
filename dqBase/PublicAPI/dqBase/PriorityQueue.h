// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/PriorityQueue.ts
// DanQing dqBase — 优先级队列（Tile 请求调度用）
//
// Min-heap: with the default Compare = std::less<T>, the value that "compares
// less" lives at the front, so Top()/Pop() return the SMALLEST element. This
// matches the reference:
//   itwinjs PriorityQueue.ts:17 — "the value in the queue that compares less
//   than all other values is always located at the front of the queue"
//   imodel-native DPoint3dOps.h:2234 MinimumValuePriorityQueue —
//   "m_heap.front() is the minimum value entry" (same min-heap, different API).
//
// Note on the heap algorithm: std::push_heap/std::pop_heap with std::less<T>
// produce a max-heap (the STL priority_queue adapter inverts). To preserve the
// reference's "less at front" semantics with the natural default, we invert the
// comparator when invoking the STL algorithms, so std::greater-like ordering is
// used internally and the smallest element ends up at m_data.front().
#pragma once

#include "DqBase.h"

#include <algorithm>
#include <functional>
#include <vector>

BEGIN_DQ_BASE_NAMESPACE

template<typename T, typename Compare = std::less<T>>
class PriorityQueue {
public:
    PriorityQueue() = default;
    explicit PriorityQueue(Compare cmp) : m_cmp(cmp), m_inverted{cmp} {}

    void Push(T item) {
        m_data.push_back(std::move(item));
        std::push_heap(m_data.begin(), m_data.end(), m_inverted);
    }

    // 移除并返回队首（最小元素，参考: PriorityQueue.ts pop / DPoint3dOps.h RemoveMin）
    T Pop() {
        std::pop_heap(m_data.begin(), m_data.end(), m_inverted);
        T v = std::move(m_data.back());
        m_data.pop_back();
        return v;
    }

    // 队首（最小元素）。参考: PriorityQueue.ts front / DPoint3dOps.h MinData
    const T& Top() const {
        return m_data.front();
    }

    bool isEmpty() const noexcept {
        return m_data.empty();
    }
    size_t Size() const noexcept {
        return m_data.size();
    }

    void clear() {
        m_data.clear();
    }

private:
    std::vector<T> m_data;
    Compare m_cmp;

    // Invert so that std::push_heap/pop_heap place the Compare-minimum at front.
    // For Compare = std::less<T> this yields std::greater<T> ordering internally,
    // i.e. a min-heap matching the reference's "compares less at front".
    // Compare is held BY VALUE (not by reference) so that PriorityQueue itself
    // remains copy-assignable and move-assignable for SDK consumers.
    struct Inverted {
        Compare cmp;
        bool operator()(const T& a, const T& b) const noexcept {
            return cmp(b, a);
        }
    };
    Inverted m_inverted{};
};

END_DQ_BASE_NAMESPACE
