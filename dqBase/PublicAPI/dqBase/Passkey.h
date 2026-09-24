// SPDX-License-Identifier: Apache-2.0
// Authored: no reference exists in itwinjs-core or imodel-native
// DanQing dqBase — Passkey 访问控制惯用法
//
// 仅声明为友元的类型能默认构造 Passkey<Friend>，从而限制某些方法只能由该友元调用。
#pragma once

#include "DqBase.h"

BEGIN_DQ_BASE_NAMESPACE

template<typename Friend>
class Passkey {
private:
    friend Friend;
    Passkey() = default;
};

END_DQ_BASE_NAMESPACE
