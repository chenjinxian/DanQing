// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/BeSQLite/PublicAPI/BeSQLite/BeSQLite.h (BeGuid)
// DanQing dqBase — 128-bit 全局唯一标识
//
// 替代 Qt QUuid。对齐 imodel-native BeGuid 的接口风格。
// 底层用两个 uint64_t 存储，紧凑且高效。
#pragma once

#include "Export.h"

#include <cstdint>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

struct DQ_BASE_EXPORT DqGuid {
    uint64_t m_hi = 0;
    uint64_t m_lo = 0;

    constexpr DqGuid() noexcept = default;
    constexpr DqGuid(uint64_t hi, uint64_t lo) noexcept : m_hi(hi), m_lo(lo) {}
    constexpr DqGuid(uint32_t a, uint32_t b, uint32_t c, uint32_t d) noexcept
        : m_hi((static_cast<uint64_t>(a) << 32) | b)
        , m_lo((static_cast<uint64_t>(c) << 32) | d) {}

    // Ported from: imodel-native BeSQLite.h BeGuid::IsValid() (line 230) — BOTH halves nonzero.
    constexpr bool IsValid() const noexcept { return m_hi != 0 && m_lo != 0; }
    // Inverse of IsValid: true iff either half is zero.
    constexpr bool IsNull() const noexcept { return m_hi == 0 || m_lo == 0; }

    // 生成新的随机 GUID（实现文件中调用平台 API）
    void Create() noexcept;

    // 格式: 8-4-4-4-12（如 "12345678-1234-1234-1234-123456789abc"）
    std::string ToString() const;

    // 从字符串解析
    bool FromString(const char* str) noexcept;

    constexpr bool operator==(const DqGuid& rhs) const noexcept {
        return m_hi == rhs.m_hi && m_lo == rhs.m_lo;
    }
    constexpr bool operator!=(const DqGuid& rhs) const noexcept {
        return !(*this == rhs);
    }
    constexpr bool operator<(const DqGuid& rhs) const noexcept {
        return m_hi < rhs.m_hi || (m_hi == rhs.m_hi && m_lo < rhs.m_lo);
    }
};

END_DQ_BASE_NAMESPACE

// std::hash 特化（供 DqHashMap<DqGuid, ...> 使用）
namespace std {
template<>
struct hash<dqBase::DqGuid> {
    size_t operator()(const dqBase::DqGuid& g) const noexcept {
        // 简单的 FNV-1a 风格混合
        size_t h = 14695981039346656037ULL;
        h ^= g.m_hi; h *= 1099511628211ULL;
        h ^= g.m_lo; h *= 1099511628211ULL;
        return h;
    }
};
} // namespace std
