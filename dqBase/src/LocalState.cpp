// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/LocalState.h
// DanQing dqBase — 本地状态存储实现 (RuntimeLocalState::_SaveValue/_GetValue)
//
// 1:1 对齐 imodel-native RuntimeLocalState: (namespace,key) tuple-keyed
// bmap<bpair<DqString,DqString>,DqString>. 空串删除记录。
#include "dqBase/LocalState.h"

BEGIN_DQ_BASE_NAMESPACE

void RuntimeLocalState::_SaveValue(const char* nameSpace, const char* key, const DqString& value) {
    bpair<DqString, DqString> identifier(nameSpace ? nameSpace : "", key ? key : "");
    if (value.empty())
        m_values.erase(identifier);
    else
        m_values[identifier] = value;
}

DqString RuntimeLocalState::_GetValue(const char* nameSpace, const char* key) const {
    bpair<DqString, DqString> identifier(nameSpace ? nameSpace : "", key ? key : "");
    auto iterator = m_values.find(identifier);
    if (iterator == m_values.end())
        return "";
    return iterator->second;
}

END_DQ_BASE_NAMESPACE
