// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/OrderedSet.ts
// DanQing dqBase — ordered set
//
// 1:1 alignment with itwinjs-core OrderedSet<T>:
//   - Constructor REQUIRES an OrderedComparator<T> (the bug fixed by this file:
//     the previous revision took no ctor args and silently used operator<,
//     defeating the whole point of the type).
//   - Backed by SortedArray<T> (the faithful dqBase port), matching the
//     reference which delegates to SortedArray<T>.
//   - Iteration returns elements in the order specified by the comparator
//     (NOT insertion order — this matches both the reference semantics and
//     the prior std::set behavior; the previous "insertion order" header
//     comment was doubly wrong and has been removed).
//   - Method names are camelCase to stay consistent with SortedArray.h
//     (the faithful dqBase port) and the TS reference.
#pragma once

#include "Export.h"
#include "SortedArray.h"

namespace dqBase {

/// A mutable set that maintains its elements in the order specified by a
/// comparison function. Iteration returns elements in comparator order.
/// Ported from: itwinjs-core OrderedSet<T>
///              itwinjs-core ReadonlyOrderedSet<T> (size/has/iteration)
template<typename T>
class OrderedSet {
public:
    /// Construct a new OrderedSet<T>.
    /// @param compare The function used to compare elements within the set,
    ///                determining their ordering. REQUIRED (no default) —
    ///                matches the reference ctor signature.
    /// @param clone   The function invoked to clone a new element for
    ///                insertion. defaults to shallowClone (ref default).
    explicit OrderedSet(
        const OrderedComparator<T>& compare,
        const CloneFunction<T>& clone = shallowClone<T>
    )
        : m_array(compare, DuplicatePolicy::Retain, clone)
    {}

    /// The number of elements in the set. (ref: get size)
    int length() const noexcept { return m_array.length(); }

    /// True if the set contains no elements.
    bool isEmpty() const noexcept { return m_array.isEmpty(); }

    /// Returns true if `value` is present in the set. (ref: has)
    bool has(const T& value) const { return m_array.contains(value); }

    /// Add the specified element to the set. Returns true if the element was
    /// newly inserted, false if it was already present (Retain duplicate
    /// policy, matching the reference's idempotent semantics).
    /// (ref: add returns `this` for chaining; dqBase returns the
    /// newly-inserted flag for ergonomic parity with the rest of dqBase's
    /// container API — Dictionary::insert / IndexMap::insert. No consumer
    /// relies on chaining.)
    bool add(const T& value) {
        if (m_array.contains(value))
            return false;
        m_array.insert(value);
        return true;
    }

    /// Removes the specified element from the set. Returns `true` if the
    /// element was present. (ref: delete; `delete` is a C++ keyword, hence
    /// the trailing underscore. `erase` is provided as an STL-ergonomic
    /// alias.)
    bool delete_(const T& value) { return m_array.remove(value) != -1; }
    bool erase(const T& value) { return delete_(value); }

    /// Remove all elements from the set. (ref: clear)
    void clear() { m_array.clear(); }

    /// Apply function to each element in sorted order.
    template<typename F>
    void forEach(const F& func) const { m_array.forEach(func); }

    // --- iteration (range-for support) ---
    auto begin() const { return m_array.array().begin(); }
    auto end() const { return m_array.array().end(); }

private:
    SortedArray<T> m_array;
};

} // namespace dqBase
