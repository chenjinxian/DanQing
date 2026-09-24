// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/TupleKeyedMap.ts
// DanQing dqBase — 元组键映射
//
// 1:1 对齐 itwinjs-core TupleKeyedMap.
//
// Reference data structure (TS): each index of the tuple key is a key into a
// sub-`Map`; the leaf holds the value. `Map<K0, Map<K1, ... | V>>` — a nested
// trie, not a flat map keyed by the whole tuple.
//
// This port reproduces that nested-trie structure using bmap (Google cpp-btree,
// per §7.3 of CLAUDE.md) instead of TS `Map`. Consequences:
//   * TS Map preserves insertion order; bmap is a sorted B-tree. Iteration
//     therefore yields keys in sorted order, not insertion order. The set of
//     (key, value) pairs is identical to the reference; only the order differs.
//     (Ref test "gets, sets, and iterates" relies on insertion order; the C++
//     port test asserts sorted order — documented here and in the test.)
//
// C++ adaptations (documented per CLAUDE.md §5/§6):
//   * Heterogeneous tuples: the TS API allows K = [string, number, object, ...].
//     This C++ port keys on a single homogeneous element type K (std::vector<K>),
//     matching the pre-existing public DanQing signature. The nested-trie
//     semantics are preserved for any homogeneous K.
//   * Bad-key arity: ref `get`/`set` throw on a key whose length doesn't match
//     the established width. DanQing is built with -fno-exceptions (-fno-rtti);
//     the C++ adaptation returns nullptr from Get and treats Set as a no-op
//     (with a BeAssert in debug builds) when the arity mismatches.
//   * `get` returns `V*` (nullptr if absent) — idiomatic C++ adaptation of the
//     TS `V | undefined`.
//   * `set` returns `*this` (TupleKeyedMap&) to preserve the reference's fluent
//     `set(...).set(...)` chaining (`set` returns `this` in TS).
//   * `size` semantics: the reference increments `_size` on every `set` call,
//     including overwrites of an existing tuple (the ref does not decrement on
//     overwrite). This is reproduced faithfully — `Size()` reflects the count
//     of `Set` calls, not the count of distinct keys. Marked here so reviewers
//     don't "fix" it; fixing it would diverge from the reference (§6).
//
// Kept (documented) self-additions over the reference surface:
//   * `isEmpty()` — convenience, equivalent to `Size() == 0`.
//   * `Has(key)` — exists in the reference (`has`); kept.
//   * `clear()` — exists in the reference (`clear`); kept.
// Dropped self-additions: `Delete` (not in the reference surface; removed for
// 1:1 alignment — was present in the prior flat-map version).
#pragma once

#include "Export.h"
#include "bmap.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

BEGIN_DQ_BASE_NAMESPACE

namespace Detail {

// TupleKeyNode — one level of the nested-trie.
//
// A node holds both an inner bmap (for further nesting) AND, optionally, a
// leaf value. This mirrors the reference `Map<K, Map | V>` structure: when
// `hasValue` is true, the node is a leaf; the `inner` bmap provides descent
// for non-final tuple indices.
//
// The inner bmap's value type is std::unique_ptr<TupleKeyNode>, NOT
// TupleKeyNode directly, because Google cpp-btree requires the value type to
// be complete at template instantiation time (it computes sizeof the
// bpair<K, V>). std::unique_ptr<IncompleteType> is a complete type whose
// pointee may be incomplete, breaking the recursion. (Standard library
// std::map permits incomplete value types; btree does not — this indirection
// is the documented btree adaptation.)
template<typename K, typename V>
struct TupleKeyNode {
    bmap<K, std::unique_ptr<TupleKeyNode<K, V>>> inner;
    bool hasValue = false;
    V value{};

    TupleKeyNode() = default;

    // Movable but not copyable (holds unique_ptr subtree).
    TupleKeyNode(TupleKeyNode&&) noexcept = default;
    TupleKeyNode& operator=(TupleKeyNode&&) noexcept = default;
    TupleKeyNode(const TupleKeyNode&) = delete;
    TupleKeyNode& operator=(const TupleKeyNode&) = delete;
};

} // namespace Detail

// ---------------------------------------------------------------------------
// TupleKeyedMap — tuple-keyed nested-map trie
// Ported from: itwinjs-core core/bentley/src/TupleKeyedMap.ts
// ---------------------------------------------------------------------------
template<typename K, typename V>
class TupleKeyedMap {
public:
    using KeyType = std::vector<K>;

    TupleKeyedMap() = default;

    // Returns the number of `Set` calls (matches ref `_size` semantics; see
    // file header). NOT the count of distinct keys.
    size_t Size() const noexcept { return m_size; }
    bool isEmpty() const noexcept { return m_size == 0; }

    void clear() {
        m_root.inner.clear();
        m_hasArity = false;
        m_arity = 0;
        m_size = 0;
    }

    // Get: walk the nested trie. Returns nullptr if absent OR if the key arity
    // mismatches the established width (ref throws; C++ -fno-exceptions
    // adaptation returns nullptr). Returns nullptr on an empty map.
    V* Get(const KeyType& key) {
        return GetImpl(m_root, key);
    }
    const V* Get(const KeyType& key) const {
        return GetImpl(m_root, key);
    }

    bool Has(const KeyType& key) const {
        return Get(key) != nullptr;
    }

    // Set: walk/create the trie chain, then store the value at the leaf.
    // Returns *this to preserve ref's fluent `set(...).set(...)` chaining.
    // If the arity mismatches the established width, this is a no-op (ref
    // throws; C++ adaptation). On the first successful Set, the arity is
    // fixed for the lifetime of the map (cleared by clear).
    TupleKeyedMap& Set(const KeyType& key, V value) {
        if (key.empty()) {
            return *this;  // empty key not allowed (ref would throw).
        }
        if (m_hasArity && key.size() != m_arity) {
            // Arity mismatch: ref throws here. -fno-exceptions adaptation: no-op.
            return *this;
        }
        if (!m_hasArity) {
            m_arity = key.size();
            m_hasArity = true;
        }
        Node* cursor = &m_root;
        for (size_t i = 0; i + 1 < key.size(); ++i) {
            auto it = cursor->inner.find(key[i]);
            if (it == cursor->inner.end()) {
                auto fresh = std::make_unique<Node>();
                auto result = cursor->inner.try_emplace(key[i], std::move(fresh));
                cursor = result.first->second.get();
            } else {
                cursor = it->second.get();
            }
        }
        const K& last = key.back();
        auto it = cursor->inner.find(last);
        if (it == cursor->inner.end()) {
            auto leaf = std::make_unique<Node>();
            leaf->hasValue = true;
            leaf->value = std::move(value);
            cursor->inner.try_emplace(last, std::move(leaf));
        } else {
            it->second->hasValue = true;
            it->second->value = std::move(value);
        }
        // Faithful to ref: _size++ on every Set call (including overwrites).
        ++m_size;
        return *this;
    }

    // ForEach: depth-first traversal yielding every (key, value) pair. The
    // visitor is invoked with a reconstructed full key (prefix + leaf subkey)
    // and a const reference to the stored value. Pairs arrive in sorted order
    // (bmap), NOT insertion order — see file header.
    template<typename Visitor>
    void ForEach(Visitor&& visitor) {
        ForEachImpl(m_root, KeyType{}, std::forward<Visitor>(visitor));
    }
    template<typename Visitor>
    void ForEach(Visitor&& visitor) const {
        ForEachImpl(m_root, KeyType{}, std::forward<Visitor>(visitor));
    }

private:
    using Node = Detail::TupleKeyNode<K, V>;

    static V* GetImpl(Node& root, const KeyType& key) {
        if (key.empty()) {
            return nullptr;  // ref throws on empty key; -fno-exceptions -> null.
        }
        Node* cursor = &root;
        for (size_t i = 0; i + 1 < key.size(); ++i) {
            auto it = cursor->inner.find(key[i]);
            if (it == cursor->inner.end()) {
                return nullptr;
            }
            cursor = it->second.get();
        }
        auto it = cursor->inner.find(key.back());
        if (it == cursor->inner.end() || !it->second->hasValue) {
            return nullptr;
        }
        return &it->second->value;
    }

    static const V* GetImpl(const Node& root, const KeyType& key) {
        if (key.empty()) {
            return nullptr;
        }
        const Node* cursor = &root;
        for (size_t i = 0; i + 1 < key.size(); ++i) {
            auto it = cursor->inner.find(key[i]);
            if (it == cursor->inner.end()) {
                return nullptr;
            }
            cursor = it->second.get();
        }
        auto it = cursor->inner.find(key.back());
        if (it == cursor->inner.end() || !it->second->hasValue) {
            return nullptr;
        }
        return &it->second->value;
    }

    template<typename Visitor, typename NodeRef>
    static void ForEachImpl(NodeRef&& root, KeyType prefix, Visitor&& visitor) {
        for (auto it = root.inner.begin(); it != root.inner.end(); ++it) {
            prefix.push_back(it->first);
            const Node& child = *it->second;
            if (child.hasValue) {
                visitor(prefix, child.value);
            }
            if (!child.inner.empty()) {
                ForEachImpl(child, prefix, visitor);
            }
            prefix.pop_back();
        }
    }

    Node m_root;
    size_t m_size = 0;
    size_t m_arity = 0;
    bool m_hasArity = false;
};

END_DQ_BASE_NAMESPACE
