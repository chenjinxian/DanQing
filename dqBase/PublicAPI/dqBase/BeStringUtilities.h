// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeStringUtilities.h
// DanQing dqBase — 字符串工具
//
// 1:1 对齐 imodel-native BeStringUtilities 的核心功能。
// §6: 自设计方法（ToLower/ToUpper/Trim/StartsWith/EndsWith/Contains/ReplaceAll/
//      UriEncode/UriDecode）已移除——参考项目无对应方法。
#pragma once

#include "Export.h"
#include "DqTypes.h"

#include <cstdint>
#include <cstdarg>
#include <cstddef>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// HexFormatOptions — 十六进制格式选项
// Ported from: imodel-native BeStringUtilities.h:23-31
// ---------------------------------------------------------------------------
enum class HexFormatOptions : int {
    None           = 0,
    LeftJustify    = 1 << 0,   // 类似 printf '-' 选项
    IncludePrefix  = 1 << 1,   // 添加 "0x" 或 "0X" 前缀
    Uppercase      = 1 << 2,   // 使用大写十六进制（类似 "%X"）
    LeadingZeros   = 1 << 3,   // 前导零填充（类似 printf '0'）
    UsePrecision   = 1 << 4,   // 使用精度（否则默认精度 1）
};

// ---------------------------------------------------------------------------
// BeStringUtilities — 字符串工具（对齐 imodel-native BeStringUtilities）
// Ported from: imodel-native BeStringUtilities.h:71-86
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT BeStringUtilities {
    /// NPOS — 表示缓冲区以 NULL 结尾
    /// Ported from: imodel-native BeStringUtilities.h:86
    static constexpr size_t NPOS = static_cast<size_t>(-1);

    /// AsManyAsPossible — 尽可能多的字符
    /// Ported from: imodel-native BeStringUtilities.h:87
    static constexpr size_t AsManyAsPossible = static_cast<size_t>(-1);

    /// 格式化字符串（printf 风格）
    /// Ported from: imodel-native BeStringUtilities.h (Sprintf pattern)
    static DqString Sprintf(const char* fmt, ...) noexcept;

    /// 格式化字符串（va_list 版本）
    /// Ported from: imodel-native BeStringUtilities.h (Vsprintf pattern)
    static DqString Vsprintf(const char* fmt, va_list args) noexcept;

    /// 安全格式化（带大小限制）
    /// Ported from: imodel-native BeStringUtilities.h (Snprintf pattern)
    static int Snprintf(char* buf, size_t size, const char* fmt, ...) noexcept;

    /// 大小写不敏感比较
    /// Ported from: imodel-native BeStringUtilities.h (Stricmp pattern)
    static int Stricmp(const char* a, const char* b) noexcept;

    /// 大小写不敏感比较（带长度限制）
    /// Ported from: imodel-native BeStringUtilities.h (Strnicmp pattern)
    static int Strnicmp(const char* a, const char* b, size_t count) noexcept;

    /// 解析 uint64（支持任意进制）
    /// Ported from: imodel-native BeStringUtilities.h (ParseUInt64 pattern)
    static uint64_t ParseUInt64(const char* str, int base = 10) noexcept;

    /// 格式化 uint64 到字符串
    /// Ported from: imodel-native BeStringUtilities.h (FormatUInt64 pattern)
    static void FormatUInt64(char* buf, size_t bufSize, uint64_t number, int base = 10) noexcept;

    /// 分割字符串
    /// Ported from: imodel-native BeStringUtilities.h (Split pattern)
    static DqVector<DqString> Split(const DqString& s, const char* delimiters);

    /// 连接字符串
    /// Ported from: imodel-native BeStringUtilities.h (Join pattern)
    static DqString Join(const DqVector<DqString>& parts, const char* separator);

    /// 词典序比较
    /// Ported from: imodel-native BeStringUtilities.h (LexicographicCompare pattern)
    static int LexicographicCompare(const char* a, const char* b) noexcept;

    /// 格式化十六进制字符串
    /// Ported from: imodel-native BeStringUtilities.h (FormatHex pattern)
    static DqString FormatHex(uint64_t value, HexFormatOptions options = HexFormatOptions::None, int minWidth = 0, int precision = 1);
};

END_DQ_BASE_NAMESPACE
