// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/NonCopyableClass.h
// DanQing dqBase — DqNonCopyable
// 禁止拷贝的混入基类。
#pragma once

#include "DqBase.h"

BEGIN_DQ_BASE_NAMESPACE

class DqNonCopyable {
protected:
    DqNonCopyable() = default;
    ~DqNonCopyable() = default;
    DqNonCopyable(DqNonCopyable&&) = default;
    DqNonCopyable& operator=(DqNonCopyable&&) = default;

private:
    DqNonCopyable(const DqNonCopyable&) = delete;
    DqNonCopyable& operator=(const DqNonCopyable&) = delete;
};

END_DQ_BASE_NAMESPACE
