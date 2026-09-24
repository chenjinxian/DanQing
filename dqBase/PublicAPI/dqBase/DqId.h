// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/Id.ts
//              imodel-native iModelCore/BeSQLite/PublicAPI/BeSQLite/BeId.h
// DanQing dqBase — 身份标识
//   DqId  : 64-bit 主标识（数据库主键、元素 ID）。对应 imodel BeInt64Id / itwinjs Id64。
//   DqGuid: 128-bit 全局唯一标识（跨文档/分布式）。在 DqGuid.h 中定义。
#pragma once

#include "DqBase.h"
#include "DqGuid.h"
#include "DqTypes.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

BEGIN_DQ_BASE_NAMESPACE

class DQ_BASE_EXPORT DqId {
public:
    static constexpr uint64_t kInvalidValue = 0;

    constexpr DqId() noexcept = default;
    explicit constexpr DqId(uint64_t v) noexcept
            : m_value(v) {} // NOLINT(google-explicit-constructor)

    constexpr bool isValid() const noexcept {
        return m_value != kInvalidValue;
    }
    constexpr bool isNull() const noexcept {
        return m_value == kInvalidValue;
    }
    constexpr uint64_t GetValue() const noexcept {
        return m_value;
    }

    // 十六进制字符串（带 0x 前缀），便于调试与序列化
    std::string ToString() const {
        char buf[20] = "0x";
        // 手动转 hex（C++ 标准库无直接 uint64→hex 格式化）
        constexpr char hex[] = "0123456789abcdef";
        int pos = 2;
        // 找到最高非零 nibble
        int startNibble = 0;
        for (int i = 60; i >= 0; i -= 4) {
            if ((m_value >> i) & 0xF) { startNibble = i; break; }
        }
        if (m_value == 0) {
            buf[pos++] = '0';
        } else {
            for (int i = startNibble; i >= 0; i -= 4)
                buf[pos++] = hex[(m_value >> i) & 0xF];
        }
        buf[pos] = '\0';
        return std::string(buf, static_cast<size_t>(pos));
    }

    // 解析十六进制（0x...）或十进制字符串
    static DqId FromString(std::string_view s) {
        // trim
        auto trimmed = s;
        while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
            trimmed.remove_prefix(1);
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t'))
            trimmed.remove_suffix(1);

        if (trimmed.empty())
            return DqId{kInvalidValue};

        uint64_t v = 0;
        int base = 10;
        std::string_view num = trimmed;
        if (trimmed.size() > 2 && trimmed[0] == '0' && (trimmed[1] == 'x' || trimmed[1] == 'X')) {
            base = 16;
            num = trimmed.substr(2);
        }
        if (num.empty())
            return DqId{kInvalidValue};

        auto [ptr, ec] = std::from_chars(num.data(), num.data() + num.size(), v, base);
        if (ec != std::errc{} || ptr != num.data() + num.size())
            return DqId{kInvalidValue};
        return DqId{v};
    }

    // 格式合法性检查（移植自 imodel BeIdTest / itwinjs Id.test.ts 的 goodIds/badIds 矩阵）。
    // 规则：""→false；"0"→true；十进制无前导零（"0"除外）；"0x"+小写十六进制，首数字非 0。
    static bool IsWellFormedString(std::string_view s) {
        if (s.empty())
            return false;
        if (s == "0")
            return true;
        if (s.size() > 2 && s[0] == '0' && (s[1] == 'x')) {
            auto hex = s.substr(2);
            if (hex.empty())
                return false; // "0x"
            if (hex[0] == '0')
                return false; // 前导零 "0x0..."
            for (size_t i = 0; i < hex.size(); ++i) {
                const char ch = hex[i];
                const bool ok = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
                if (!ok)
                    return false; // 拒绝大写 hex 与非法字符
            }
            return true;
        }
        // 十进制：禁前导零（"0" 已处理）、禁负号
        if (s[0] == '0' || s[0] == '-')
            return false;
        for (size_t i = 0; i < s.size(); ++i) {
            const char ch = s[i];
            if (!(ch >= '0' && ch <= '9'))
                return false;
        }
        return true;
    }

    friend constexpr bool operator==(DqId a, DqId b) noexcept {
        return a.m_value == b.m_value;
    }
    friend constexpr bool operator!=(DqId a, DqId b) noexcept {
        return a.m_value != b.m_value;
    }
    friend constexpr bool operator<(DqId a, DqId b) noexcept {
        return a.m_value < b.m_value;
    }
    friend constexpr bool operator!(DqId a) noexcept {
        return a.m_value == kInvalidValue;
    }

private:
    uint64_t m_value = kInvalidValue;
};

// 便捷类型别名
using DqIdSet    = DqSet<DqId>;
using DqIdVector = DqVector<DqId>;

// ---------------------------------------------------------------------------
// Guid 命名空间 — GUID 工具函数
// Ported from: itwinjs-core Id.ts Guid namespace
// ---------------------------------------------------------------------------
namespace Guid {

/// 空 GUID 字符串
inline constexpr const char* kEmpty = "00000000-0000-0000-0000-000000000000";

/// 检查字符串是否为合法 GUID 格式 (8-4-4-4-12)
/// Ported from: itwinjs-core Guid.isGuid
inline bool IsGuid(const char* s) {
    if (!s) return false;
    // 期望格式: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx (36 字符)
    size_t len = std::strlen(s);
    if (len != 36) return false;
    static constexpr int groupLens[] = {8, 4, 4, 4, 12};
    int pos = 0;
    for (int g = 0; g < 5; ++g) {
        if (g > 0) {
            if (s[pos] != '-') return false;
            ++pos;
        }
        for (int i = 0; i < groupLens[g]; ++i, ++pos) {
            char c = s[pos];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                return false;
        }
    }
    return true;
}

/// 检查是否为 V4 GUID（第 13 位为 '4'，第 17 位为 '8','9','a','b'）
/// Ported from: itwinjs-core Guid.isV4Guid
inline bool IsV4Guid(const char* s) {
    if (!IsGuid(s)) return false;
    return s[14] == '4' && (s[19] == '8' || s[19] == '9' || s[19] == 'a' || s[19] == 'b');
}

/// 创建新的随机 GUID 字符串
/// Ported from: itwinjs-core Guid.createValue
inline std::string CreateValue() {
    DqGuid g;
    g.Create();
    return g.ToString();
}

/// 规范化 GUID 字符串（转小写）
/// Ported from: itwinjs-core Guid.normalize
inline std::string Normalize(const char* s) {
    if (!s || !IsGuid(s)) return "";
    std::string result(s, 36);
    for (auto& c : result) {
        if (c >= 'A' && c <= 'F')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return result;
}

} // namespace Guid

// ---------------------------------------------------------------------------
// Id64 命名空间 — 64-bit ID 工具函数
// Ported from: itwinjs-core Id.ts Id64 namespace
// ---------------------------------------------------------------------------
namespace Id64 {

/// 无效 ID 常量
inline constexpr uint64_t kInvalid = 0;

/// 获取本地 ID（低 32 位）
/// Ported from: itwinjs-core Id64.getLocalId
inline uint32_t GetLocalId(DqId id) noexcept {
    return static_cast<uint32_t>(id.GetValue() & 0xFFFFFFFF);
}

/// 获取 Briefcase ID（高 32 位）
/// Ported from: itwinjs-core Id64.getBriefcaseId
inline uint32_t GetBriefcaseId(DqId id) noexcept {
    return static_cast<uint32_t>(id.GetValue() >> 32);
}

/// 从本地 ID 和 Briefcase ID 构造 DqId
/// Ported from: itwinjs-core Id64.fromLocalAndBriefcaseIds
inline DqId FromLocalAndBriefcaseIds(uint32_t localId, uint32_t briefcaseId) noexcept {
    uint64_t v = (static_cast<uint64_t>(briefcaseId) << 32) | localId;
    return DqId{v};
}

/// 从 uint32 对构造 DqId
/// Ported from: itwinjs-core Id64.fromUint32Pair
inline DqId FromUint32Pair(uint32_t lowBytes, uint32_t highBytes) noexcept {
    return FromLocalAndBriefcaseIds(lowBytes, highBytes);
}

/// 获取低 32 位
/// Ported from: itwinjs-core Id64.getLowerUint32
inline uint32_t GetLowerUint32(DqId id) noexcept {
    return GetLocalId(id);
}

/// 获取高 32 位
/// Ported from: itwinjs-core Id64.getUpperUint32
inline uint32_t GetUpperUint32(DqId id) noexcept {
    return GetBriefcaseId(id);
}

/// 是否为有效 ID
/// Ported from: itwinjs-core Id64.isValid
inline bool isValid(DqId id) noexcept {
    return id.isValid();
}

/// 是否为无效 ID
/// Ported from: itwinjs-core Id64.isInvalid
inline bool IsInvalid(DqId id) noexcept {
    return id.isNull();
}

/// 是否为 transient ID（BriefcaseId == 0 且 LocalId != 0）
/// Ported from: itwinjs-core Id64.isTransient
inline bool IsTransient(DqId id) noexcept {
    return GetBriefcaseId(id) == 0 && GetLocalId(id) != 0;
}

} // namespace Id64

END_DQ_BASE_NAMESPACE

// std::hash 特化（供 DqHashMap<DqId, ...> / DqSet<DqId> 使用）
namespace std {
template<>
struct hash<dqBase::DqId> {
    size_t operator()(dqBase::DqId id) const noexcept {
        // FNV-1a 风格哈希
        size_t h = 14695981039346656037ULL;
        h ^= id.GetValue(); h *= 1099511628211ULL;
        return h;
    }
};
} // namespace std
