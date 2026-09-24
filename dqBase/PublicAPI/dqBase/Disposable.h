// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/Disposable.ts
// DanQing dqBase — 资源释放接口
//
// 1:1 对齐 itwinjs-core IDisposable / DisposableList。
// C++ 用 RAII 为主，但保留接口用于跨模块资源管理。
#pragma once

#include "Export.h"

#include <functional>
#include <vector>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// IDisposable — 资源释放接口
// Ported from: itwinjs-core Disposable.ts IDisposable
// ---------------------------------------------------------------------------
struct IDisposable {
    virtual ~IDisposable() = default;
    virtual void Dispose() = 0;
};

// ---------------------------------------------------------------------------
// DisposeFunc — 释放函数类型
// Ported from: itwinjs-core Disposable.ts DisposeFunc
// ---------------------------------------------------------------------------
using DisposeFunc = std::function<void()>;

// ---------------------------------------------------------------------------
// DisposableList — 可释放资源列表（RAII 风格）
// Ported from: itwinjs-core Disposable.ts DisposableList
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT DisposableList : public IDisposable {
public:
    DisposableList() = default;
    ~DisposableList() override { Dispose(); }

    DisposableList(const DisposableList&) = delete;
    DisposableList& operator=(const DisposableList&) = delete;

    /// 添加释放函数
    void add(DisposeFunc func) {
        m_funcs.push_back(std::move(func));
    }

    /// 释放所有资源（顺序释放，对齐参考实现）
    // Ported from: itwinjs-core Disposable.ts DisposableList.dispose() (185-187)
    //              — iterates _disposables FORWARD (registration order).
    void Dispose() override {
        for (auto it = m_funcs.begin(); it != m_funcs.end(); ++it) {
            if (*it) (*it)();
        }
        m_funcs.clear();
    }

    /// 是否为空
    bool isEmpty() const noexcept { return m_funcs.empty(); }

private:
    std::vector<DisposeFunc> m_funcs;
};

END_DQ_BASE_NAMESPACE
