// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeFile.h
// DanQing dqBase — 二进制文件 I/O 实现
#include "dqBase/BeFile.h"

#include <cstring>

BEGIN_DQ_BASE_NAMESPACE

BeFileStatus BeFile::Open(const char* path, BeFileAccess access) {
    if (m_handle) return BeFileStatus::FileAlreadyOpened;
    const char* mode = nullptr;
    switch (access) {
    case BeFileAccess::Read:      mode = "rb"; break;
    case BeFileAccess::Write:     mode = "wb"; break;
    case BeFileAccess::ReadWrite: mode = "r+b"; break;
    case BeFileAccess::Append:    mode = "ab"; break;
    }
    m_handle = fopen(path, mode);
    if (!m_handle) {
        if (errno == ENOENT) return BeFileStatus::FileNotFoundError;
        if (errno == EACCES) return BeFileStatus::AccessViolationError;
        return BeFileStatus::UnknownError;
    }
    return BeFileStatus::Success;
}

BeFileStatus BeFile::Create(const char* path) {
    if (m_handle) return BeFileStatus::FileAlreadyOpened;
    m_handle = fopen(path, "w+b");
    if (!m_handle) return BeFileStatus::UnknownError;
    return BeFileStatus::Success;
}

void BeFile::Close() {
    if (m_handle) {
        fclose(m_handle);
        m_handle = nullptr;
    }
}

// Ported from: imodel-native BeFile.h:149
//   BeFileStatus Read(void* buffer, uint32_t* bytesRead, uint32_t numBytes)
BeFileStatus BeFile::Read(void* buffer, uint32_t* bytesRead, uint32_t numBytes) {
    if (!m_handle) return BeFileStatus::FileNotOpenError;
    size_t count = fread(buffer, 1, static_cast<size_t>(numBytes), m_handle);
    if (bytesRead) *bytesRead = static_cast<uint32_t>(count);
    if (count < static_cast<size_t>(numBytes) && ferror(m_handle)) return BeFileStatus::ReadError;
    return BeFileStatus::Success;
}

// Ported from: imodel-native BeFile.h:156
//   BeFileStatus Write(uint32_t* bytesWritten, void const* buffer, uint32_t numBytes)
BeFileStatus BeFile::Write(uint32_t* bytesWritten, const void* buffer, uint32_t numBytes) {
    if (!m_handle) return BeFileStatus::FileNotOpenError;
    size_t count = fwrite(buffer, 1, static_cast<size_t>(numBytes), m_handle);
    if (bytesWritten) *bytesWritten = static_cast<uint32_t>(count);
    if (count < static_cast<size_t>(numBytes)) return BeFileStatus::DiskFull;
    return BeFileStatus::Success;
}

BeFileStatus BeFile::SetPointer(int64_t offset, BeFileSeekOrigin origin) {
    if (!m_handle) return BeFileStatus::FileNotOpenError;
    int whence = SEEK_SET;
    switch (origin) {
    case BeFileSeekOrigin::Begin:   whence = SEEK_SET; break;
    case BeFileSeekOrigin::Current: whence = SEEK_CUR; break;
    case BeFileSeekOrigin::End:     whence = SEEK_END; break;
    }
    if (fseek(m_handle, static_cast<long>(offset), whence) != 0)
        return BeFileStatus::UnknownError;
    return BeFileStatus::Success;
}

int64_t BeFile::GetPointer() {
    if (!m_handle) return -1;
    long pos = ftell(m_handle);
    return static_cast<int64_t>(pos);
}

uint64_t BeFile::GetSize() {
    if (!m_handle) return 0;
    long saved = ftell(m_handle);
    fseek(m_handle, 0, SEEK_END);
    long size = ftell(m_handle);
    fseek(m_handle, saved, SEEK_SET);
    return static_cast<uint64_t>(size);
}

void BeFile::Flush() {
    if (m_handle) fflush(m_handle);
}

BeFileStatus BeFile::ReadEntireFile(DqVector<uint8_t>& data) {
    if (!m_handle) return BeFileStatus::FileNotOpenError;
    uint64_t size = GetSize();
    data.resize(static_cast<size_t>(size));
    if (size == 0) return BeFileStatus::Success;
    uint32_t bytesRead = 0;
    return Read(data.data(), &bytesRead, static_cast<uint32_t>(size));
}

END_DQ_BASE_NAMESPACE
