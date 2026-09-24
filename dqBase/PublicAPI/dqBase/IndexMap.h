// SPDX-License-Identifier: Apache-2.0
// DanQing dqBase — IndexMap<T>
//
// Ported from: itwinjs-core core/bentley/src/IndexMap.ts
//
// Maintains a set of unique elements in sorted order and retains the insertion order of each.
// Each element is assigned an index equal to its insertion order.
// 替代 Qt QVector，底层用 std::vector。
#pragma once

#include "Export.h"
#include "SortedArray.h"
#include "DqTypes.h"

#include <cstddef>
#include <climits>
#include <optional>

namespace dqBase {

/// Associates a value of type T with an index representing its insertion order.
/// Ported from: itwinjs-core IndexedValue<T>
template<typename T>
struct IndexedValue {
    T value;
    int index;
};

/// Maintains a set of unique elements in sorted order, retaining insertion order.
/// Ported from: itwinjs-core IndexMap<T>
template<typename T>
class IndexMap {
public:
    IndexMap(
        const OrderedComparator<T>& compare,
        int maximumSize = INT_MAX,
        const CloneFunction<T>& clone = shallowClone<T>
    )
        : m_compareValues(compare)
        , m_clone(clone)
        , m_maximumSize(maximumSize)
    {}

    /// Number of elements in the map.
    int length() const { return static_cast<int>(m_array.size()); }

    /// True if maximum number of elements have been inserted.
    bool isFull() const { return static_cast<int>(m_array.size()) >= m_maximumSize; }

    /// True if map contains no elements.
    bool isEmpty() const { return m_array.empty(); }

    /// Remove all elements.
    void clear() { m_array.clear(); }

    /// Insert a new value. If equivalent element exists, returns its index.
    /// If map is full, returns -1.
    /// Otherwise assigns next-available index and returns it.
    int insert(const T& value)
    {
        auto bound = lowerBound(value);
        if (bound.equal)
            return m_array[static_cast<size_t>(bound.index)].index;

        if (isFull())
            return -1;

        int newIndex = static_cast<int>(m_array.size());
        m_array.insert(m_array.begin() + bound.index, {m_clone(value), newIndex});
        return newIndex;
    }

    /// Find index of element equivalent to value. Returns -1 if not found.
    int indexOf(const T& value) const
    {
        auto bound = lowerBound(value);
        return bound.equal ? m_array[static_cast<size_t>(bound.index)].index : -1;
    }

    /// Return array where array index corresponds to insertion order.
    DqVector<T> toArray() const
    {
        DqVector<T> result;
        if (m_array.empty())
            return result;

        // Find max index
        int maxIndex = 0;
        for (const auto& entry : m_array)
            if (entry.index > maxIndex)
                maxIndex = entry.index;

        result.resize(static_cast<size_t>(maxIndex + 1));
        for (const auto& entry : m_array)
            result[static_cast<size_t>(entry.index)] = entry.value;

        return result;
    }

private:
    DqVector<IndexedValue<T>> m_array;
    OrderedComparator<T> m_compareValues;
    CloneFunction<T> m_clone;
    int m_maximumSize;

    struct BoundResult { int index; bool equal; };

    BoundResult lowerBound(const T& value) const
    {
        int low = 0;
        int high = static_cast<int>(m_array.size());
        while (low < high) {
            int mid = (low + high) / 2;
            int comp = m_compareValues(value, m_array[static_cast<size_t>(mid)].value);
            if (comp == 0)
                return {mid, true};
            else if (comp < 0)
                high = mid;
            else
                low = mid + 1;
        }
        return {low, false};
    }
};

} // namespace dqBase
