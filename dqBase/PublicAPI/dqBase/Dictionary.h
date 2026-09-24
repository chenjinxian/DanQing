// SPDX-License-Identifier: Apache-2.0
// DanQing dqBase — Dictionary<K, V>
//
// Ported from: itwinjs-core core/bentley/src/Dictionary.ts
//
// Maintains a mapping of keys to values in sorted order.
// Supports custom comparison logic for keys of any type.
// 替代 Qt QVector，底层用 std::vector。
#pragma once

#include "Export.h"
#include "SortedArray.h"
#include "DqTypes.h"

#include <cstddef>
#include <utility>

namespace dqBase {

/// Represents an entry in a Dictionary.
/// Ported from: itwinjs-core DictionaryEntry<K, V>
template<typename K, typename V>
struct DictionaryEntry {
    K key;
    V value;
};

/// Maintains a mapping of keys to values in sorted order.
/// Ported from: itwinjs-core Dictionary<K, V>
template<typename K, typename V>
class Dictionary {
public:
    Dictionary(
        const OrderedComparator<K>& compareKeys,
        const CloneFunction<K>& cloneKey = shallowClone<K>,
        const CloneFunction<V>& cloneValue = shallowClone<V>
    )
        : m_compareKeys(compareKeys)
        , m_cloneKey(cloneKey)
        , m_cloneValue(cloneValue)
    {}

    /// Number of entries in the dictionary.
    int size() const { return static_cast<int>(m_keys.size()); }

    /// True if dictionary contains no entries.
    bool isEmpty() const { return m_keys.empty(); }

    /// Look up value by key. Returns nullptr if not found.
    V* get(const K& key)
    {
        auto bound = lowerBound(key);
        return bound.equal ? &m_values[static_cast<size_t>(bound.index)] : nullptr;
    }

    const V* get(const K& key) const
    {
        auto bound = lowerBound(key);
        return bound.equal ? &m_values[static_cast<size_t>(bound.index)] : nullptr;
    }

    /// True if entry exists for specified key.
    bool has(const K& key) const { return lowerBound(key).equal; }

    /// Delete entry by key. Returns true if found and deleted.
    bool erase(const K& key)
    {
        auto bound = lowerBound(key);
        if (bound.equal) {
            m_keys.erase(m_keys.begin() + bound.index);
            m_values.erase(m_values.begin() + bound.index);
            return true;
        }
        return false;
    }

    /// Insert a new entry. If key already exists, dictionary is unmodified.
    /// Returns true if new entry was inserted.
    bool insert(const K& key, const V& value)
    {
        auto result = findOrInsert(key, value);
        return result.second;
    }

    /// Get value for key, or insert if not present.
    /// Returns pair of (value reference, inserted flag).
    std::pair<V&, bool> findOrInsert(const K& key, const V& value)
    {
        auto bound = lowerBound(key);
        if (bound.equal)
            return {m_values[static_cast<size_t>(bound.index)], false};

        m_keys.insert(m_keys.begin() + bound.index, m_cloneKey(key));
        m_values.insert(m_values.begin() + bound.index, m_cloneValue(value));
        return {m_values[static_cast<size_t>(bound.index)], true};
    }

    /// Set value for key. Inserts or replaces.
    void set(const K& key, const V& value)
    {
        auto bound = lowerBound(key);
        if (bound.equal) {
            m_values[static_cast<size_t>(bound.index)] = m_cloneValue(value);
        } else {
            m_keys.insert(m_keys.begin() + bound.index, m_cloneKey(key));
            m_values.insert(m_values.begin() + bound.index, m_cloneValue(value));
        }
    }

    /// Clear all entries.
    void clear()
    {
        m_keys.clear();
        m_values.clear();
    }

    /// Extract entries as vector of pairs and empty dictionary.
    DqVector<DictionaryEntry<K, V>> extractPairs()
    {
        DqVector<DictionaryEntry<K, V>> pairs;
        pairs.reserve(m_keys.size());
        for (size_t i = 0; i < m_keys.size(); ++i)
            pairs.push_back({m_keys[i], m_values[i]});
        clear();
        return pairs;
    }

    /// Extract as separate key/value arrays and empty dictionary.
    std::pair<DqVector<K>, DqVector<V>> extractArrays()
    {
        auto keys = std::move(m_keys);
        auto values = std::move(m_values);
        m_keys.clear();
        m_values.clear();
        return {keys, values};
    }

    /// Apply function to each (key, value) pair in sorted order.
    void forEach(const std::function<void(const K&, const V&)>& func) const
    {
        for (size_t i = 0; i < m_keys.size(); ++i)
            func(m_keys[i], m_values[i]);
    }

    /// Access keys (read-only).
    const DqVector<K>& keys() const { return m_keys; }

    /// Access values (read-only).
    const DqVector<V>& values() const { return m_values; }

private:
    DqVector<K> m_keys;
    DqVector<V> m_values;
    OrderedComparator<K> m_compareKeys;
    CloneFunction<K> m_cloneKey;
    CloneFunction<V> m_cloneValue;

    struct BoundResult { int index; bool equal; };

    BoundResult lowerBound(const K& key) const
    {
        int low = 0;
        int high = static_cast<int>(m_keys.size());
        while (low < high) {
            int mid = (low + high) / 2;
            int comp = m_compareKeys(key, m_keys[static_cast<size_t>(mid)]);
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
