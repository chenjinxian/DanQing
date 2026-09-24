// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeVersion.h
// DanQing dqBase — 4-digit 版本（对齐 ref BeVersion：uint16_t × 4 + Mask + GetInt64 + CompareTo）
//
// 1:1 移植 BeVersion 的字段布局（m_major/m_minor/m_sub1/m_sub2 全为 uint16_t）、
// Mask 位掩码枚举、GetInt64(Mask)、CompareTo(BeVersionCR, Mask)、IsEmpty、
// ToString/ToMajorMinorString、FromString 返回 DqStatus 且 mutate this。
// DqStatus 对齐 imodel-native BentleyStatus（Success/Error）。
#pragma once

#include "DqBase.h"
#include "DqStatus.h"

#include <cinttypes>
#include <cstdint>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

// 格式串对齐 ref BeVersion.h 的 VERSION_FORMAT / VERSION_FORMAT_MAJOR_MINOR / VERSION_PARSE_FORMAT
// （%d 用于解析、PRIu16 用于格式化，因 uint16_t 在 printf 路径上需用正确宽度说明符）
#define DQ_VERSION_FORMAT             "%" PRIu16 ".%" PRIu16 ".%" PRIu16 ".%" PRIu16
#define DQ_VERSION_FORMAT_MAJOR_MINOR "%" PRIu16 ".%" PRIu16
#define DQ_VERSION_PARSE_FORMAT       "%d.%d.%d.%d"

struct DqVersion;
typedef DqVersion& DqVersionR;
typedef DqVersion const& DqVersionCR;

//=======================================================================================
//! A 4-digit number that specifies version number. 1:1 with imodel-native BeVersion.
// 对齐 ref: struct BeVersion (BeVersion.h:23-109)
//=======================================================================================
struct DQ_BASE_EXPORT DqVersion {
private:
    uint16_t m_major;
    uint16_t m_minor;
    uint16_t m_sub1;
    uint16_t m_sub2;

public:
    // 对齐 ref BeVersion::Mask (BeVersion.h:32-41) —— 哪些字段参与比较/打包
    enum Mask : uint64_t {
        Major          = (((uint64_t) 0xffff) << 48),
        Minor          = (((uint64_t) 0xffff) << 32),
        Sub1           = (((uint64_t) 0xffff) << 16),
        Sub2           = ((uint64_t) 0xffff),
        MajorMinor     = (Major | Minor),
        MajorMinorSub1 = (Major | Minor | Sub1),
        All            = (Major | Minor | Sub1 | Sub2),
    };

    // 0/2/4-arg ctors 对齐 ref (BeVersion.h:43-45)
    constexpr DqVersion() noexcept : DqVersion(0, 0, 0, 0) {}
    constexpr DqVersion(uint16_t major, uint16_t minor) noexcept : DqVersion(major, minor, 0, 0) {}
    constexpr DqVersion(uint16_t major, uint16_t minor, uint16_t sub1, uint16_t sub2) noexcept
        : m_major(major), m_minor(minor), m_sub1(sub1), m_sub2(sub2) {}

    // int 重载对齐 ref BeVersion(int, int) (BeVersion.h:46) —— 截断为 uint16_t
    constexpr DqVersion(int major, int minor) noexcept
        : DqVersion(static_cast<uint16_t>(major), static_cast<uint16_t>(minor)) {}

    //! 字符串构造：内部调用 FromString（对齐 ref BeVersion.h:51）
    DqVersion(const char* versionStr, const char* format = DQ_VERSION_PARSE_FORMAT) noexcept
        : DqVersion() { FromString(versionStr, format); }

    // Getters 对齐 ref BeVersion.h:55-66 —— uint16_t 返回（非 int）
    uint16_t GetMajor() const noexcept { return m_major; }
    uint16_t GetMinor() const noexcept { return m_minor; }
    uint16_t GetSub1() const noexcept { return m_sub1; }
    uint16_t GetSub2() const noexcept { return m_sub2; }

    //! 按 Mask 打包为 uint64_t（对齐 ref BeVersion.h:68）。
    //! m_major<<48 | m_minor<<32 | m_sub1<<16 | m_sub2，然后 & mask。
    constexpr uint64_t GetInt64(Mask mask) const noexcept {
        return mask & ((static_cast<uint64_t>(m_major) << 48) | (static_cast<uint64_t>(m_minor) << 32)
                       | (static_cast<uint64_t>(m_sub1) << 16) | static_cast<uint64_t>(m_sub2));
    }

    //! 按 Mask 比较：0=相等 / 1=greater / -1=less（对齐 ref BeVersion.h:69）
    constexpr int CompareTo(DqVersionCR other, Mask mask = Mask::All) const noexcept {
        const uint64_t a = GetInt64(mask);
        const uint64_t b = other.GetInt64(mask);
        return (a == b) ? 0 : (a > b ? 1 : -1);
    }

    constexpr bool operator==(DqVersionCR rhs) const noexcept { return CompareTo(rhs) == 0; }
    constexpr bool operator!=(DqVersionCR rhs) const noexcept { return CompareTo(rhs) != 0; }
    constexpr bool operator<(DqVersionCR rhs) const noexcept  { return CompareTo(rhs) < 0; }
    constexpr bool operator<=(DqVersionCR rhs) const noexcept { return CompareTo(rhs) <= 0; }
    constexpr bool operator>(DqVersionCR rhs) const noexcept  { return CompareTo(rhs) > 0; }
    constexpr bool operator>=(DqVersionCR rhs) const noexcept { return CompareTo(rhs) >= 0; }

    //! 全部字段为 0 即视为空（对齐 ref BeVersion.h:78）
    constexpr bool IsEmpty() const noexcept { return 0 == GetInt64(Mask::All); }

    //! 4 段字符串 "major.minor.sub1.sub2"（对齐 ref BeVersion.h:83）
    std::string ToString(const char* format = DQ_VERSION_FORMAT) const;

    //! 仅 major.minor（对齐 ref BeVersion.h:87）
    std::string ToMajorMinorString(const char* format = DQ_VERSION_FORMAT_MAJOR_MINOR) const;

    //! 解析版本串；至少匹配 1 个数字返回 Success，否则 Error（对齐 ref BeVersion.h:93-108）。
    //! 直接 mutate this，而非返回新对象。
    DqStatus FromString(const char* versionStr, const char* format = DQ_VERSION_PARSE_FORMAT) noexcept;
};

END_DQ_BASE_NAMESPACE
