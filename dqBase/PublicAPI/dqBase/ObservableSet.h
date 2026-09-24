// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/ObservableSet.ts
// DanQing dqBase — 可观察集合
//
// 1:1 对齐 itwinjs-core ObservableSet：
//   - 5 个事件: onAdded/onDeleted/onCleared/onBatchAdded/onBatchDeleted (ref:16-24)
//   - add/Delete 触发单项事件 (ref:33-52)
//   - clear 仅在非空时触发 onCleared (ref:57-62)
//   - AddAll/DeleteAll 收集后触发 SINGLE 批量事件 + 返回增删计数 (ref:69-94)
#pragma once

#include "Export.h"
#include "DqEvent.h"

#include <cstddef>
#include <unordered_set>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// ObservableSet<T> — 可观察集合
// Ported from: itwinjs-core core/bentley/src/ObservableSet.ts
// ---------------------------------------------------------------------------
template<typename T>
class ObservableSet {
public:
    // --- 事件 ---
    // Ported from: ObservableSet.ts lines 16-24
    DqEvent<const T&> onAdded;
    DqEvent<const T&> onDeleted;
    DqEvent<> onCleared;
    DqEvent<> onBatchAdded;
    DqEvent<> onBatchDeleted;

    // --- 查询 ---
    size_t Size() const noexcept { return m_set.size(); }
    bool isEmpty() const noexcept { return m_set.empty(); }
    bool Contains(const T& value) const { return m_set.count(value) > 0; }

    // --- 修改 ---
    // Ported from: ObservableSet.ts add() (ref:33-40) — 仅在真正插入时触发 onAdded
    bool add(const T& value) {
        auto [it, inserted] = m_set.insert(value);
        if (inserted) onAdded.Raise(value);
        return inserted;
    }

    // Ported from: ObservableSet.ts delete() (ref:46-52) — 仅在真正删除时触发 onDeleted
    bool Delete(const T& value) {
        auto it = m_set.find(value);
        if (it == m_set.end()) return false;
        T copy = *it;
        m_set.erase(it);
        onDeleted.Raise(copy);
        return true;
    }

    // Ported from: ObservableSet.ts clear() (ref:57-62)
    // 仅在集合非空时才清空 + 触发 onCleared（对齐 ref 的 `if (0 !== this.size)` 守卫）。
    void clear() {
        if (!m_set.empty()) {
            m_set.clear();
            onCleared.Raise();
        }
    }

    /// 批量添加。Ported from: ObservableSet.ts addAll() (ref:69-78)
    /// 直接插入（绕过 add() 的事件路径），仅当 size 变化时触发 onBatchAdded 一次，
    /// 返回实际新增的元素数（与 ref 的 `this.size - prevSize` 一致）。
    template<typename InputIt>
    size_t AddAll(InputIt first, InputIt last) {
        const size_t prevSize = m_set.size();
        for (auto it = first; it != last; ++it)
            m_set.insert(*it);

        if (m_set.size() != prevSize)
            onBatchAdded.Raise();

        return m_set.size() - prevSize;
    }

    /// 批量删除。Ported from: ObservableSet.ts deleteAll() (ref:85-94)
    /// 直接删除（绕过 Delete() 的事件路径），仅当 size 变化时触发 onBatchDeleted 一次，
    /// 返回实际删除的元素数（与 ref 的 `prevSize - this.size` 一致）。
    template<typename InputIt>
    size_t DeleteAll(InputIt first, InputIt last) {
        const size_t prevSize = m_set.size();
        for (auto it = first; it != last; ++it)
            m_set.erase(*it);

        if (m_set.size() != prevSize)
            onBatchDeleted.Raise();

        return prevSize - m_set.size();
    }

    // --- 迭代 ---
    auto begin() const { return m_set.begin(); }
    auto end() const { return m_set.end(); }

private:
    std::unordered_set<T> m_set;
};

END_DQ_BASE_NAMESPACE
