// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeTextFile.h
//              and iModelCore/Bentley/src/BeTextFile.cpp
// DanQing dqBase — 文本文件 I/O 实现
#include "dqBase/BeTextFile.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

BEGIN_DQ_BASE_NAMESPACE

// Ported from: imodel-native BeTextFile.cpp — Open
RefPtr<BeTextFile> BeTextFile::Open(BeFileStatus& status, const char* path,
                                    TextFileOpenType openType,
                                    TextFileOptions options,
                                    TextFileEncoding encoding) {
    RefPtr<BeTextFile> file(new BeTextFile());
    file->m_encoding = encoding;
    file->m_options = options;

    BeFileAccess access = BeFileAccess::Read;
    switch (openType) {
    case TextFileOpenType::Read:   access = BeFileAccess::Read; break;
    case TextFileOpenType::Write:  access = BeFileAccess::Write; break;
    case TextFileOpenType::Append: access = BeFileAccess::Append; break;
    }

    status = file->m_file.Open(path, access);
    if (status != BeFileStatus::Success)
        return nullptr;

    return file;
}

// Ported from: imodel-native BeTextFile.cpp — Close
void BeTextFile::Close() {
    m_file.Close();
}

// Ported from: imodel-native BeTextFile.cpp — GetLine
TextFileReadStatus BeTextFile::GetLine(DqString& line) {
    line.clear();
    char ch;
    uint32_t bytesRead = 0;
    while (true) {
        if (m_file.Read(&ch, &bytesRead, 1) != BeFileStatus::Success || bytesRead == 0)
            return line.empty() ? TextFileReadStatus::Eof : TextFileReadStatus::Success;
        if (ch == '\n') {
            if (!(static_cast<uint32_t>(m_options) & static_cast<uint32_t>(TextFileOptions::KeepNewLine)))
                return TextFileReadStatus::Success;
        }
        if (ch == '\r') continue;
        line += ch;
    }
}

// Ported from: imodel-native BeTextFile.cpp — PutLine
TextFileWriteStatus BeTextFile::PutLine(const char* line, bool addNewLine) {
    if (!line) return TextFileWriteStatus::BadParameter;
    uint32_t len = static_cast<uint32_t>(std::strlen(line));
    uint32_t written = 0;
    if (m_file.Write(&written, line, len) != BeFileStatus::Success)
        return TextFileWriteStatus::Error;
    if (addNewLine) {
        const char nl = '\n';
        if (m_file.Write(&written, &nl, 1) != BeFileStatus::Success)
            return TextFileWriteStatus::Error;
    }
    return TextFileWriteStatus::Success;
}

// Ported from: imodel-native BeTextFile.cpp — GetChar
int BeTextFile::GetChar() {
    char ch;
    uint32_t bytesRead = 0;
    if (m_file.Read(&ch, &bytesRead, 1) != BeFileStatus::Success || bytesRead == 0)
        return -1;  // EOF
    return static_cast<unsigned char>(ch);
}

// Ported from: imodel-native BeTextFile.cpp — PrintfTo
TextFileWriteStatus BeTextFile::PrintfTo(bool toStdOutAlso, const char* format, ...) {
    if (!format) return TextFileWriteStatus::BadParameter;

    char buf[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (toStdOutAlso) {
        va_list args2;
        va_start(args2, format);
        vprintf(format, args2);
        va_end(args2);
    }

    return PutLine(buf, false);
}

// Ported from: imodel-native BeTextFile.cpp — Rewind
BeFileStatus BeTextFile::Rewind() {
    return m_file.SetPointer(0, BeFileSeekOrigin::Begin);
}

// Ported from: imodel-native BeTextFile.cpp — SetPointer
BeFileStatus BeTextFile::SetPointer(uint64_t position, BeFileSeekOrigin origin) {
    return m_file.SetPointer(static_cast<int64_t>(position), origin);
}

// Ported from: imodel-native BeTextFile.cpp — GetPointer
BeFileStatus BeTextFile::GetPointer(uint64_t& position) {
    position = static_cast<uint64_t>(m_file.GetPointer());
    return BeFileStatus::Success;
}

END_DQ_BASE_NAMESPACE
