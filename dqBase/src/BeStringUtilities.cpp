// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeStringUtilities.h
// DanQing dqBase — 字符串工具实现
#include "dqBase/BeStringUtilities.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sstream>

BEGIN_DQ_BASE_NAMESPACE

// Ported from: imodel-native BeStringUtilities — Sprintf
DqString BeStringUtilities::Sprintf(const char* fmt, ...) noexcept {
    va_list args;
    va_start(args, fmt);
    DqString result = Vsprintf(fmt, args);
    va_end(args);
    return result;
}

// Ported from: imodel-native BeStringUtilities — Vsprintf
DqString BeStringUtilities::Vsprintf(const char* fmt, va_list args) noexcept {
    char buf[4096];
    vsnprintf(buf, sizeof(buf), fmt, args);
    return DqString(buf);
}

// Ported from: imodel-native BeStringUtilities — Snprintf
int BeStringUtilities::Snprintf(char* buf, size_t size, const char* fmt, ...) noexcept {
    va_list args;
    va_start(args, fmt);
    int result = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return result;
}

// Ported from: imodel-native BeStringUtilities — Stricmp
int BeStringUtilities::Stricmp(const char* a, const char* b) noexcept {
#if defined(_WIN32)
    return _stricmp(a, b);
#else
    return strcasecmp(a, b);
#endif
}

// Ported from: imodel-native BeStringUtilities — Strnicmp
int BeStringUtilities::Strnicmp(const char* a, const char* b, size_t count) noexcept {
#if defined(_WIN32)
    return _strnicmp(a, b, count);
#else
    return strncasecmp(a, b, count);
#endif
}

// Ported from: imodel-native BeStringUtilities — ParseUInt64
uint64_t BeStringUtilities::ParseUInt64(const char* str, int base) noexcept {
    if (!str) return 0;
    return static_cast<uint64_t>(strtoull(str, nullptr, base));
}

// Ported from: imodel-native BeStringUtilities — FormatUInt64
void BeStringUtilities::FormatUInt64(char* buf, size_t bufSize, uint64_t number, int base) noexcept {
    if (!buf || bufSize == 0) return;
    if (base == 10) {
        snprintf(buf, bufSize, "%llu", static_cast<unsigned long long>(number));
    } else if (base == 16) {
        snprintf(buf, bufSize, "%llx", static_cast<unsigned long long>(number));
    } else if (base == 8) {
        snprintf(buf, bufSize, "%llo", static_cast<unsigned long long>(number));
    } else {
        // 通用进制转换
        char tmp[65];
        int pos = 64;
        tmp[pos] = '\0';
        if (number == 0) {
            tmp[--pos] = '0';
        } else {
            while (number > 0 && pos > 0) {
                int digit = static_cast<int>(number % base);
                tmp[--pos] = digit < 10 ? static_cast<char>('0' + digit)
                                        : static_cast<char>('a' + digit - 10);
                number /= base;
            }
        }
        snprintf(buf, bufSize, "%s", tmp + pos);
    }
}

// Ported from: imodel-native BeStringUtilities — Split
DqVector<DqString> BeStringUtilities::Split(const DqString& s, const char* delimiters) {
    DqVector<DqString> parts;
    if (!delimiters) return parts;
    if (s.empty()) { parts.push_back(s); return parts; }

    size_t start = 0;
    size_t pos = s.find_first_of(delimiters, start);
    while (pos != DqString::npos) {
        parts.push_back(s.substr(start, pos - start));
        start = pos + 1;
        pos = s.find_first_of(delimiters, start);
    }
    parts.push_back(s.substr(start));
    return parts;
}

// Ported from: imodel-native BeStringUtilities — Join
DqString BeStringUtilities::Join(const DqVector<DqString>& parts, const char* separator) {
    if (parts.empty()) return "";
    DqString result = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        if (separator) result += separator;
        result += parts[i];
    }
    return result;
}

// Ported from: imodel-native BeStringUtilities — LexicographicCompare
int BeStringUtilities::LexicographicCompare(const char* a, const char* b) noexcept {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a, b);
}

// Ported from: imodel-native BeStringUtilities — FormatHex
DqString BeStringUtilities::FormatHex(uint64_t value, HexFormatOptions options, int minWidth, int precision) {
    DqString result;
    bool uppercase = (static_cast<int>(options) & static_cast<int>(HexFormatOptions::Uppercase)) != 0;
    bool includePrefix = (static_cast<int>(options) & static_cast<int>(HexFormatOptions::IncludePrefix)) != 0;
    bool leadingZeros = (static_cast<int>(options) & static_cast<int>(HexFormatOptions::LeadingZeros)) != 0;
    bool leftJustify = (static_cast<int>(options) & static_cast<int>(HexFormatOptions::LeftJustify)) != 0;
    bool usePrecision = (static_cast<int>(options) & static_cast<int>(HexFormatOptions::UsePrecision)) != 0;

    // 格式化十六进制数字
    char digits[32];
    int digitCount = 0;
    uint64_t tmp = value;
    if (tmp == 0) {
        digits[0] = '0';
        digitCount = 1;
    } else {
        while (tmp > 0 && digitCount < 32) {
            int d = static_cast<int>(tmp & 0xF);
            digits[digitCount++] = uppercase ? "0123456789ABCDEF"[d] : "0123456789abcdef"[d];
            tmp >>= 4;
        }
    }

    // 精度 = 最小数字位数
    int effectivePrecision = usePrecision ? precision : 1;
    int totalDigits = std::max(digitCount, effectivePrecision);

    // 前缀
    if (includePrefix) {
        result += uppercase ? "0X" : "0x";
    }

    // 前导零填充（精度）
    int totalLen = totalDigits;
    if (leadingZeros) {
        for (int i = digitCount; i < totalDigits; ++i)
            result += '0';
    }
    for (int i = digitCount - 1; i >= 0; --i)
        result += digits[i];

    // 宽度填充（前导零或空格）
    if (leadingZeros && totalLen < minWidth) {
        // 前导零填充到 minWidth（在前缀之后、数字之前）
        DqString zeros;
        while (totalLen < minWidth) { zeros += '0'; ++totalLen; }
        // 插入到前缀之后
        size_t prefixLen = includePrefix ? 2 : 0;
        result = result.substr(0, prefixLen) + zeros + result.substr(prefixLen);
    } else if (leftJustify) {
        while (totalLen < minWidth) { result += ' '; ++totalLen; }
    } else {
        DqString padded;
        while (totalLen < minWidth) { padded += ' '; ++totalLen; }
        result = padded + result;
    }

    return result;
}

END_DQ_BASE_NAMESPACE
