// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeTextFile.h
// DanQing dqBase — 文本文件 I/O
//
// 1:1 对齐 imodel-native BeTextFile。
// 补全: GetChar、PrintfTo、Rewind/SetPointer/GetPointer 返回 BeFileStatus。
// 使用 DqString（窄字符串）代替参考的 WString（宽字符串），适配 DanQing 架构。
#pragma once

#include "Export.h"
#include "BeFile.h"
#include "RefCounted.h"
#include "DqTypes.h"

#include <cstdint>
#include <cstdarg>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// 文本文件枚举
// Ported from: imodel-native BeTextFile.h:15-52
// ---------------------------------------------------------------------------
enum class TextFileOpenType : int {
    Read   = 0,
    Write  = 1,
    Append = 2,
};

enum class TextFileEncoding : int {
    CurrentLocale = 0,
    Utf8          = 1,
    Utf16         = 2,
};

enum class TextFileOptions : uint32_t {
    None          = 0,
    KeepNewLine   = 0x1,
    NewLinesToSpace = 0x5,
};

enum class TextFileReadStatus : int {
    Success      = 0,
    Eof          = 1,
    BadParameter = 2,
};

enum class TextFileWriteStatus : int {
    Success      = 0,
    Error        = 1,
    BadParameter = 2,
};

// ---------------------------------------------------------------------------
// BeTextFile — 文本文件读写
// Ported from: imodel-native BeTextFile.h:61-153
//
// 使用 DqString（窄字符串）代替参考的 WString。
// 参考使用 WString 是因为 imodel-native 内部统一用 UTF-16；
// DanQing 使用 UTF-8 窄字符串作为内部表示。
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT BeTextFile : public RefCounted<BeTextFile> {
public:
    /// 缓冲区大小
    /// Ported from: imodel-native BeTextFile.h:63
    static constexpr uint32_t BUFFER_SIZE = 100;

    /// 打开文本文件
    /// Ported from: imodel-native BeTextFile.h:105
    static RefPtr<BeTextFile> Open(BeFileStatus& status, const char* path,
                                    TextFileOpenType openType,
                                    TextFileOptions options = TextFileOptions::None,
                                    TextFileEncoding encoding = TextFileEncoding::Utf8);

    /// 关闭文件
    /// Ported from: imodel-native BeTextFile.h:109
    void Close();

    /// 读取一行
    /// Ported from: imodel-native BeTextFile.h:116
    TextFileReadStatus GetLine(DqString& line);

    /// 写入一行
    /// Ported from: imodel-native BeTextFile.h:123
    TextFileWriteStatus PutLine(const char* line, bool addNewLine = true);

    /// 读取下一个字符
    /// Ported from: imodel-native BeTextFile.h:127
    int GetChar();

    /// 格式化写入
    /// Ported from: imodel-native BeTextFile.h:133
    TextFileWriteStatus PrintfTo(bool toStdOutAlso, const char* format, ...);

    /// 回到文件开头（跳过 BOM）
    /// Ported from: imodel-native BeTextFile.h:138
    BeFileStatus Rewind();

    /// 移动文件读写位置
    /// Ported from: imodel-native BeTextFile.h:146
    BeFileStatus SetPointer(uint64_t position, BeFileSeekOrigin origin);

    /// 获取文件读写位置
    /// Ported from: imodel-native BeTextFile.h:151
    BeFileStatus GetPointer(uint64_t& position);

private:
    BeTextFile() = default;

    BeFile m_file;
    TextFileEncoding m_encoding = TextFileEncoding::Utf8;
    TextFileOptions m_options = TextFileOptions::None;
};

using BeTextFilePtr = RefPtr<BeTextFile>;

END_DQ_BASE_NAMESPACE
