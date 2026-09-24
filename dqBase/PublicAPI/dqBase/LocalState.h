// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/LocalState.h
// DanQing dqBase — 本地状态存储
//
// 1:1 对齐 imodel-native ILocalState / RuntimeLocalState:
//   - ILocalState: protected pure-virtual _SaveValue/_GetValue + public non-virtual
//     SaveValue/GetValue wrappers (the protected-virtual + public-wrapper pattern).
//   - RuntimeLocalState::Values = bmap<bpair<DqString,DqString>,DqString> (the nested
//     pair-keyed bmap; pair = (namespace, key)). CLAUDE.md §7.3 mandates btree/bmap
//     over std::unordered_map; the (namespace,key) tuple keying matches the reference
//     contract.
#pragma once

#include "Export.h"
#include "RefCounted.h"
#include "DqTypes.h"
#include "bmap.h"
#include "bpair.h"

#include <string>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// ILocalState — 本地状态接口
// Ported from: imodel-native LocalState.h ILocalState
//
// Vtable shape (per ref): protected pure-virtual _SaveValue/_GetValue +
// public non-virtual SaveValue/GetValue wrappers. The base owns the public
// entry points so it can inject behavior around the subclass's override.
// ---------------------------------------------------------------------------
struct ILocalState : public RefCountedBase {
protected:
    /// 保存值（子类实现）。空字符串删除记录。
    /// @param nameSpace 命名空间，标识负责维护该值的代码。
    /// @param key 在命名空间上下文中标识值的键。
    /// @param value 要保存的字符串；空串删除记录。
    virtual void _SaveValue(const char* nameSpace, const char* key, const DqString& value) = 0;

    /// 获取值（子类实现）。
    /// @param nameSpace 命名空间，标识负责维护该值的代码。
    /// @param key 在命名空间上下文中标识值的键。
    /// @return 给定 namespace+key 的字符串；记录不存在时返回空串。
    virtual DqString _GetValue(const char* nameSpace, const char* key) const = 0;

public:
    /// 保存值的公开入口（非虚，委托给 _SaveValue）。
    void SaveValue(const char* nameSpace, const char* key, const DqString& value) { _SaveValue(nameSpace, key, value); }

    /// 获取值的公开入口（非虚，委托给 _GetValue）。
    DqString GetValue(const char* nameSpace, const char* key) const { return _GetValue(nameSpace, key); }
};

using ILocalStatePtr = RefPtr<ILocalState>;

// ---------------------------------------------------------------------------
// RuntimeLocalState — 运行时本地状态（内存中，无文件持久化）
// Ported from: imodel-native LocalState.h RuntimeLocalState
//
// Storage (per ref, §7.3): bmap<bpair<DqString,DqString>,DqString> —
// (namespace,key) tuple → value, btree-backed.
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT RuntimeLocalState : public ILocalState {
public:
    /// {(namespace, key) → value} 存储。对齐 ref RuntimeLocalState::Values。
    typedef bmap<bpair<DqString, DqString>, DqString> Values;

    /// 获取内部 values map — {(namespace, key) → value}
    const Values& GetValues() const { return m_values; }
    /// 获取内部 values map — {(namespace, key) → value}
    Values& GetValues() { return m_values; }

protected:
    void _SaveValue(const char* nameSpace, const char* key, const DqString& value) override;
    DqString _GetValue(const char* nameSpace, const char* key) const override;

private:
    Values m_values;
};

END_DQ_BASE_NAMESPACE
