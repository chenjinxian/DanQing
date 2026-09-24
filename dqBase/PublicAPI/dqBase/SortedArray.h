// SPDX-License-Identifier: Apache-2.0
// DanQing dqBase — SortedArray<T>
//
// Ported from: itwinjs-core core/bentley/src/SortedArray.ts
//   ReadonlySortedArray<T>, SortedArray<T>, DuplicatePolicy, lowerBound
//
// Maintains an array of type T in sorted order using a user-supplied comparator.
// Binary search for efficient lookup. Supports duplicate policies: Allow, Retain, Replace.
// 替代 Qt QVector，底层用 std::vector。
#pragma once

#include "Export.h"
#include "DqTypes.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

namespace dqBase {

/// How duplicate values are handled when inserting into a SortedArray.
/// Ported from: itwinjs-core DuplicatePolicy
enum class DuplicatePolicy : uint8_t {
    Allow,   ///< Duplicates allowed (adjacent, order unspecified)
    Retain,  ///< Duplicates forbidden — existing value retained
    Replace, ///< Duplicates forbidden — existing value replaced
};

/// A function that returns a copy of a value T.
/// Ported from: itwinjs-core CloneFunction<T>
template<typename T>
using CloneFunction = std::function<T(const T&)>;

/// Default clone: return input as-is (shallow copy).
/// Ported from: itwinjs-core shallowClone
template<typename T>
T shallowClone(const T& value) { return value; }

/// Comparator function: returns negative if lhs < rhs, 0 if equal, positive if lhs > rhs.
/// Ported from: itwinjs-core OrderedComparator<T, U>
template<typename T, typename U = T>
using OrderedComparator = std::function<int(const T&, const U&)>;

/// Given a sorted array, compute the insertion position for value to maintain sorted order.
/// Ported from: itwinjs-core lowerBound
template<typename T, typename U>
struct LowerBoundResult {
    int index;
    bool equal;
};

template<typename T, typename U>
LowerBoundResult<T, U> lowerBound(
    const T& value,
    const DqVector<U>& list,
    const OrderedComparator<T, U>& compare
)
{
    int low = 0;
    int high = static_cast<int>(list.size());
    while (low < high) {
        int mid = (low + high) / 2;
        int comp = compare(value, list[mid]);
        if (comp == 0)
            return {mid, true};
        else if (comp < 0)
            high = mid;
        else
            low = mid + 1;
    }
    return {low, false};
}

/// Read-only view of a sorted array.
/// Ported from: itwinjs-core ReadonlySortedArray<T>
template<typename T>
class ReadonlySortedArray {
public:
    ReadonlySortedArray(
        const OrderedComparator<T>& compare,
        DuplicatePolicy policy = DuplicatePolicy::Retain,
        const CloneFunction<T>& clone = shallowClone<T>
    )
        : m_compare(compare)
        , m_clone(clone)
        , m_policy(policy)
    {}

    virtual ~ReadonlySortedArray() = default;

    /// Number of elements in the array.
    int length() const { return static_cast<int>(m_array.size()); }

    /// True if array contains no elements.
    bool isEmpty() const { return m_array.empty(); }

    /// Look up index of an element equal to value using binary search.
    /// Returns -1 if not found.
    int indexOf(const T& value) const
    {
        auto bound = lowerBoundOf(value);
        return bound.equal ? bound.index : -1;
    }

    /// True if array contains at least one value equal to the specified value.
    bool contains(const T& value) const { return indexOf(value) != -1; }

    /// Look up an element equal to value. Returns nullptr if not found.
    const T* findEqual(const T& value) const
    {
        int idx = indexOf(value);
        return idx >= 0 ? &m_array[static_cast<size_t>(idx)] : nullptr;
    }

    /// Look up an element by index. Returns nullptr if out of range.
    const T* get(int index) const
    {
        return (index >= 0 && static_cast<size_t>(index) < m_array.size()) ? &m_array[static_cast<size_t>(index)] : nullptr;
    }

    /// Apply function to each element in sorted order.
    void forEach(const std::function<void(const T&)>& func) const
    {
        for (const auto& item : m_array)
            func(item);
    }

    /// Access underlying array (read-only).
    const DqVector<T>& array() const { return m_array; }

protected:
    DqVector<T> m_array;
    OrderedComparator<T> m_compare;
    CloneFunction<T> m_clone;
    DuplicatePolicy m_policy;

    struct BoundResult { int index; bool equal; };

    BoundResult lowerBoundOf(const T& value) const
    {
        int low = 0;
        int high = static_cast<int>(m_array.size());
        while (low < high) {
            int mid = (low + high) / 2;
            int comp = m_compare(value, m_array[static_cast<size_t>(mid)]);
            if (comp == 0)
                return {mid, true};
            else if (comp < 0)
                high = mid;
            else
                low = mid + 1;
        }
        return {low, false};
    }

    /// Insert value at sorted position. Returns index of inserted/existing element.
    int insertImpl(const T& value)
    {
        auto bound = lowerBoundOf(value);

        if (bound.equal) {
            switch (m_policy) {
            case DuplicatePolicy::Retain:
                return bound.index;
            case DuplicatePolicy::Replace:
                m_array[static_cast<size_t>(bound.index)] = m_clone(value);
                return bound.index;
            case DuplicatePolicy::Allow:
                break;
            }
        }

        m_array.insert(m_array.begin() + bound.index, m_clone(value));
        return bound.index;
    }

    /// Remove first occurrence of value. Returns index of removed element, or -1.
    int removeImpl(const T& value)
    {
        auto bound = lowerBoundOf(value);
        if (bound.equal) {
            m_array.erase(m_array.begin() + bound.index);
            return bound.index;
        }
        return -1;
    }

    void clearImpl() { m_array.clear(); }

    DqVector<T> extractArrayImpl()
    {
        DqVector<T> result;
        result.swap(m_array);
        return result;
    }
};

/// Maintains an array of type T in sorted order.
/// Ported from: itwinjs-core SortedArray<T>
template<typename T>
class SortedArray : public ReadonlySortedArray<T> {
public:
    SortedArray(
        const OrderedComparator<T>& compare,
        DuplicatePolicy policy = DuplicatePolicy::Retain,
        const CloneFunction<T>& clone = shallowClone<T>
    )
        : ReadonlySortedArray<T>(compare, policy, clone)
    {}

    /// Clear contents.
    void clear() { this->clearImpl(); }

    /// Extract sorted array and empty contents.
    DqVector<T> extractArray() { return this->extractArrayImpl(); }

    /// Insert value at sorted position. Returns index of inserted/existing element.
    int insert(const T& value) { return this->insertImpl(value); }

    /// Remove first occurrence of value. Returns index of removed element, or -1.
    int remove(const T& value) { return this->removeImpl(value); }
};

} // namespace dqBase
