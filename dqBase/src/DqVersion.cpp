// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeVersion.h
// DanQing dqBase — DqVersion 实现（4-digit BeVersion，零 Qt 依赖）
//
// 实现 ref BeVersion::ToString / ToMajorMinorString / FromString 的字符串 IO。
// ref 使用 Utf8PrintfString + Utf8String::Sscanf_safe；DanQing 用 std::snprintf + std::sscanf
// 实现等价语义（同样要求 %d 仅作为解析格式说明符）。
#include "dqBase/Version.h"

#include <cstdio>
#include <cstring>

BEGIN_DQ_BASE_NAMESPACE

std::string DqVersion::ToString(const char* format) const {
    char buf[32];
    std::snprintf(buf, sizeof(buf), format, m_major, m_minor, m_sub1, m_sub2);
    return std::string(buf);
}

std::string DqVersion::ToMajorMinorString(const char* format) const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), format, m_major, m_minor);
    return std::string(buf);
}

DqStatus DqVersion::FromString(const char* versionStr, const char* format) noexcept {
    if (nullptr == versionStr)
        return DqStatus::Error;

    int major = 0, minor = 0, sub1 = 0, sub2 = 0;
    const int result = std::sscanf(versionStr, format, &major, &minor, &sub1, &sub2);
    if (result >= 1)
        m_major = static_cast<uint16_t>(0xFFFF & major);
    if (result >= 2)
        m_minor = static_cast<uint16_t>(0xFFFF & minor);
    if (result >= 3)
        m_sub1 = static_cast<uint16_t>(0xFFFF & sub1);
    if (result >= 4)
        m_sub2 = static_cast<uint16_t>(0xFFFF & sub2);

    // 即使使用自定义格式串，仍要求至少匹配 1 个数字（对齐 ref BeVersion.h:107）
    return (result >= 1 ? DqStatus::Success : DqStatus::Error);
}

END_DQ_BASE_NAMESPACE
