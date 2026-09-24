// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/BeSQLite/PublicAPI/BeSQLite/BeSQLite.h (BeGuid)
// DanQing dqBase — DqGuid 实现（零 Qt 依赖）
#include "dqBase/DqGuid.h"

#include <cstdio>
#include <cstring>
#include <random>

BEGIN_DQ_BASE_NAMESPACE

void DqGuid::Create() noexcept {
    // 生成随机 128-bit GUID（对齐 imodel BeGuid::Create）
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    m_hi = rng();
    m_lo = rng();
    // 设置版本 4 (random) 和变体 10xx 的标志位
    m_hi = (m_hi & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL; // version 4
    m_lo = (m_lo & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL; // variant 1
}

std::string DqGuid::ToString() const {
    // 格式: 8-4-4-4-12（如 "12345678-1234-1234-1234-123456789abc"）
    char buf[37];
    uint32_t a = static_cast<uint32_t>(m_hi >> 32);
    uint16_t b = static_cast<uint16_t>((m_hi >> 16) & 0xFFFF);
    uint16_t c = static_cast<uint16_t>(m_hi & 0xFFFF);
    uint16_t d = static_cast<uint16_t>(m_lo >> 48);
    uint64_t e = m_lo & 0x0000FFFFFFFFFFFFULL;
    snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012llx",
             a, b, c, d, static_cast<unsigned long long>(e));
    return std::string(buf, 36);
}

bool DqGuid::FromString(const char* str) noexcept {
    if (!str || std::strlen(str) != 36)
        return false;
    // 解析 8-4-4-4-12 格式
    uint32_t a; unsigned int b, c, d; unsigned long long e;
    if (sscanf(str, "%08x-%04x-%04x-%04x-%012llx", &a, &b, &c, &d, &e) != 5)
        return false;
    m_hi = (static_cast<uint64_t>(a) << 32) | (static_cast<uint64_t>(b) << 16) | c;
    m_lo = (static_cast<uint64_t>(d) << 48) | e;
    return true;
}

END_DQ_BASE_NAMESPACE
