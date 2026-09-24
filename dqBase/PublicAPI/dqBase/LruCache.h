// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/LRUMap.ts
// DanQing dqBase — LRU 缓存（Tile/几何缓存用）
// 替代 Qt QHash，用 std::unordered_map。
// 对应 itwinjs core-bentley LRUMap。
#pragma once

#include "DqBase.h"
#include "DqTypes.h"

#include <cstddef>
#include <list>
#include <optional>
#include <utility>

BEGIN_DQ_BASE_NAMESPACE

template<typename Key, typename Value>
class LruCache {
public:
    explicit LruCache(size_t capacity)
            : m_capacity(capacity) {}

    // 命中返回指向缓存值的指针（并提升到队首）；未命中返回 nullptr
    Value* Find(const Key& key) {
        auto it = m_index.find(key);
        if (it == m_index.end())
            return nullptr;
        // 提升到队首（splice，O(1)）
        m_order.splice(m_order.begin(), m_order, it->second);
        return &(it->second->second);
    }

    void Put(const Key& key, Value value) {
        auto it = m_index.find(key);
        if (it != m_index.end()) {
            it->second->second = std::move(value);
            m_order.splice(m_order.begin(), m_order, it->second);
            return;
        }
        if (m_capacity == 0)
            return;
        if (m_index.size() >= m_capacity) {
            EvictOne();
        }
        m_order.emplace_front(key, std::move(value));
        m_index[key] = m_order.begin();
    }

    bool Remove(const Key& key) {
        auto it = m_index.find(key);
        if (it == m_index.end())
            return false;
        m_order.erase(it->second);
        m_index.erase(it);
        return true;
    }

    void clear() {
        m_order.clear();
        m_index.clear();
    }

    // 移除并返回最久未用项（队尾）；空则返回 nullopt。对应 itwinjs LRUMap::shift()
    std::optional<std::pair<Key, Value>> Shift() {
        if (m_order.empty())
            return std::nullopt;
        auto oldest = m_order.back(); // 拷贝（pair<Key,Value>）
        m_index.erase(oldest.first);
        m_order.pop_back();
        return oldest;
    }

    size_t Size() const noexcept {
        return m_index.size();
    }
    void SetCapacity(size_t c) {
        m_capacity = c;
        while (m_index.size() > m_capacity)
            EvictOne();
    }

private:
    using ListIt = typename std::list<std::pair<Key, Value>>::iterator;

    void EvictOne() {
        if (m_order.empty())
            return;
        const Key& k = m_order.back().first;
        m_index.erase(k);
        m_order.pop_back();
    }

    size_t m_capacity;
    std::list<std::pair<Key, Value>> m_order; // 头部 = 最近用
    DqHashMap<Key, ListIt> m_index;
};

END_DQ_BASE_NAMESPACE
