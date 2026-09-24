// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/SHA1.h
// DanQing dqBase — SHA1 哈希
//
// 1:1 对齐 imodel-native SHA1。
#pragma once

#include "Export.h"

#include <cstdint>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// SHA1 — SHA1 哈希计算
// Ported from: imodel-native SHA1.h
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT SHA1 {
public:
    static constexpr size_t BlockSize = 64;
    static constexpr size_t HashBytes = 20;

    struct HashVal {
        uint8_t bytes[HashBytes];
    };

    SHA1();

    /// 计算哈希 — 返回 40 字符小写 hex 摘要 (对齐 ref: Utf8String operator()(...))
    std::string operator()(const void* data, size_t size);
    std::string operator()(const std::string& s);

    /// 增量添加数据
    void Add(const void* data, size_t size);

    /// 获取哈希结果
    /// GetHashVal() 返回 20 原始字节；GetHashString() 返回 40 字符小写 hex
    HashVal GetHashVal();
    std::string GetHashString();

    /// 重置状态
    void Reset();

private:
    uint32_t m_state[5];
    uint64_t m_count;
    uint8_t m_buffer[BlockSize];
};

END_DQ_BASE_NAMESPACE
