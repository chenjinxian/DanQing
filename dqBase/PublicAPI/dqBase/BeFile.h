// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeFile.h
// DanQing dqBase — 二进制文件 I/O
//
// 1:1 对齐 imodel-native BeFile。
// 基于 C 标准库 stdio，零外部依赖。
#pragma once

#include "Export.h"
#include "NonCopyable.h"
#include "DqTypes.h"

#include <cstdint>
#include <cstdio>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeFileStatus — 文件操作状态
// Ported from: imodel-native BeFile.h BeFileStatus
//
// Numeric layout MUST match the reference exactly (§5 numeric precision):
//   - Success  = SUCCESS = 0          (Bentley.h:291)
//   - UnknownError = ERROR = 0x8000   (Bentley.h:293, aliased at BeFile.h:36)
//   - the named errors between them take the reference's implicit numbering
//     1..9 (BeFile.h:27-35).
// Code that compares status against ERROR / 0x8000 depends on these values.
// ---------------------------------------------------------------------------
enum class BeFileStatus : int {
    Success               = 0,
    FileNotFoundError,        // = 1
    AccessViolationError,    // = 2
    SharingViolationError,   // = 3
    TooManyOpenFilesError,   // = 4
    FileNotOpenError,        // = 5
    NotLockedError,          // = 6
    ReadError,               // = 7
    DiskFull,                // = 8
    FileAlreadyOpened,       // = 9
    UnknownError          = 0x8000,
};

// ---------------------------------------------------------------------------
// BeFileAccess — 文件访问模式
// Ported from: imodel-native BeFile.h BeFileAccess
// ---------------------------------------------------------------------------
enum class BeFileAccess : int {
    Read      = 1,
    Write     = 2,
    ReadWrite = 3,
    Append    = 4,  // Ported from: imodel-native BeFile.h (append mode)
};

// ---------------------------------------------------------------------------
// BeFileSeekOrigin — 文件定位原点
// Ported from: imodel-native BeFile.h BeFileSeekOrigin
// ---------------------------------------------------------------------------
enum class BeFileSeekOrigin : int {
    Begin   = 0,
    Current = 1,
    End     = 2,
};

// ---------------------------------------------------------------------------
// BeFile — 二进制文件 I/O
// Ported from: imodel-native BeFile.h
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT BeFile : public DqNonCopyable {
public:
    BeFile() = default;
    ~BeFile() { Close(); }

    BeFile(BeFile&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    BeFile& operator=(BeFile&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    /// 打开文件
    BeFileStatus Open(const char* path, BeFileAccess access);

    /// 创建文件（覆盖已存在的）
    BeFileStatus Create(const char* path);

    /// 关闭文件
    void Close();

    /// 是否已打开
    bool IsOpen() const noexcept { return m_handle != nullptr; }

    /// 读取数据
    /// Ported from: imodel-native BeFile.h:149
    ///   Read(void* buffer, uint32_t* bytesRead, uint32_t numBytes)
    /// Argument order + types match the reference exactly (§5 interface
    /// signature): buffer, &bytesRead, numBytes.
    BeFileStatus Read(void* buffer, uint32_t* bytesRead, uint32_t numBytes);

    /// 写入数据
    /// Ported from: imodel-native BeFile.h:156
    ///   Write(uint32_t* bytesWritten, void const* buffer, uint32_t numBytes)
    /// Argument order + types match the reference exactly (§5 interface
    /// signature): &bytesWritten, buffer, numBytes.
    BeFileStatus Write(uint32_t* bytesWritten, const void* buffer, uint32_t numBytes);

    /// 定位
    BeFileStatus SetPointer(int64_t offset, BeFileSeekOrigin origin);

    /// 获取当前位置
    int64_t GetPointer();

    /// 获取文件大小
    uint64_t GetSize();

    /// 刷新缓冲区
    void Flush();

    /// 读取整个文件
    BeFileStatus ReadEntireFile(DqVector<uint8_t>& data);

private:
    FILE* m_handle = nullptr;
};

END_DQ_BASE_NAMESPACE
