// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/BeEvent.ts
// DanQing dqBase — 轻量事件（非 QObject 类型用）
//
// 双轨事件策略（D3）：
//   - QObject 派生类（dqApp 层）用 Qt signals/slots。
//   - 非 QObject（RefCounted 对象、值类型，如仓外数据层 TxnManager）用本 DqEvent。
// 无 moc 开销，线程安全（std::mutex），返回退订令牌 + DqEventScope RAII 批量退订。
// 对应 imodel BeEvent / itwinjs BeEvent。
// 替代 Qt QMutex/QMutexLocker，用 std::mutex/std::lock_guard。
#pragma once

#include "DqBase.h"
#include "DqSync.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

BEGIN_DQ_BASE_NAMESPACE

// 退订令牌：调用即移除监听器（BeEvent 的 cancel_callback 模式）
using DqEventDisconnect = std::function<void()>;

// RAII 批量退订：绑定到作用域，析构时移除该作用域内注册的所有监听。
// 适用于把一组订阅绑定到 Db/会话生命周期的场景。
class DQ_BASE_EXPORT DqEventScope {
public:
    DqEventScope() = default;
    ~DqEventScope() {
        DisconnectAll();
    }
    DqEventScope(const DqEventScope&) = delete;
    DqEventScope& operator=(const DqEventScope&) = delete;
    void add(DqEventDisconnect d) {
        m_disconnectors.push_back(std::move(d));
    }
    void DisconnectAll() {
        for (auto& d: m_disconnectors) {
            if (d)
                d();
        }
        m_disconnectors.clear();
    }

private:
    std::vector<DqEventDisconnect> m_disconnectors;
};

// 轻量观察者（非 QObject 类型用）
template<typename... Args>
class DqEvent {
public:
    using Callback = std::function<void(Args...)>;

    // 添加持久监听器，返回退订令牌
    DqEventDisconnect AddListener(Callback cb) {
        DqLockGuard<DqMutex> lock(m_mutex);
        const std::size_t token = m_nextToken++;
        m_listeners.push_back({ token, std::move(cb), /*once=*/false });
        return [this, token]() {
            this->RemoveListener(token);
        };
    }

    // 添加一次性监听器：触发一次后自动移除（对应 imodel BeEvent::AddOnce）
    DqEventDisconnect AddOnce(Callback cb) {
        DqLockGuard<DqMutex> lock(m_mutex);
        const std::size_t token = m_nextToken++;
        m_listeners.push_back({ token, std::move(cb), /*once=*/true });
        return [this, token]() {
            this->RemoveListener(token);
        };
    }

    // 清空所有监听器
    void clear() {
        DqLockGuard<DqMutex> lock(m_mutex);
        m_listeners.clear();
    }

    // 触发（按注册顺序）；触发后移除 once 监听器
    void Raise(Args... args) {
        // 拷贝回调快照 + 记录 once token，避免回调中增删监听器导致迭代器失效
        std::vector<Callback> snapshot;
        std::vector<std::size_t> onceTokens;
        {
            DqLockGuard<DqMutex> lock(m_mutex);
            snapshot.reserve(m_listeners.size());
            for (auto& l: m_listeners) {
                snapshot.push_back(l.cb);
                if (l.once)
                    onceTokens.push_back(l.token);
            }
        }
        for (auto& cb: snapshot)
            cb(args...);
        if (!onceTokens.empty()) {
            DqLockGuard<DqMutex> lock(m_mutex);
            m_listeners.erase(std::remove_if(m_listeners.begin(), m_listeners.end(),
                                      [&onceTokens](const Entry& e) {
                                          return std::find(onceTokens.begin(), onceTokens.end(),
                                                         e.token) != onceTokens.end();
                                      }),
                    m_listeners.end());
        }
    }

    std::size_t ListenerCount() const {
        DqLockGuard<DqMutex> lock(m_mutex);
        return m_listeners.size();
    }

private:
    void RemoveListener(std::size_t token) {
        DqLockGuard<DqMutex> lock(m_mutex);
        m_listeners.erase(std::remove_if(m_listeners.begin(), m_listeners.end(),
                                  [token](const Entry& e) {
                                      return e.token == token;
                                  }),
                m_listeners.end());
    }
    struct Entry {
        std::size_t token;
        Callback cb;
        bool once;
    };
    mutable DqMutex m_mutex;
    std::vector<Entry> m_listeners;
    std::size_t m_nextToken = 0;
};

END_DQ_BASE_NAMESPACE
