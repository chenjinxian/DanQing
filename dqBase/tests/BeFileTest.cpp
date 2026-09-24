// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeFile_Test.cpp
//              TEST(BeFileTests, Write)             lines 295-315
//              TEST(BeFileTests, Read)              lines 645-676
//              TEST(BeFileTests, WriteWithPointerSet) lines 434-481 (Read/Write arg order)
//
// Validates two P0 conformance fixes vs imodel-native BeFile.h:
//   1. BeFileStatus::UnknownError must alias ERROR (= 0x8000) per ref
//      Bentley/BeFile.h:36 (Bentley.h:293 `ERROR = 0x8000`). DanQing previously
//      had UnknownError = 10, breaking any code testing `status == ERROR` or
//      comparing against reference numerics.
//   2. BeFile::Read / BeFile::Write argument order+types must match ref
//      BeFile.h:149 / :156 exactly:
//        Read (void* buffer, uint32_t* bytesRead, uint32_t numBytes)
//        Write(uint32_t* bytesWritten, void const* buffer, uint32_t numBytes)
//      DanQing previously had size/count reversed and used size_t instead of
//      uint32_t for the count / out-params.
#include <gtest/gtest.h>

#include <dqBase/BeFile.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#ifdef _WIN32
    #include <io.h> // _getcwd 等（Windows 无 unistd.h）
    #include <windows.h> // GetTempPathA / GetTempFileNameA（MakeTempPath）
#else
    #include <unistd.h>
#endif

using dqBase::BeFile;
using dqBase::BeFileAccess;
using dqBase::BeFileSeekOrigin;
using dqBase::BeFileStatus;
using dqBase::DqString;

namespace {

// Unique temp path per process to avoid collisions with parallel test runs.
#ifdef _WIN32
DqString MakeTempPath(const char* suffix) {
    char dir[MAX_PATH], path[MAX_PATH];
    GetTempPathA(MAX_PATH, dir);
    if (0 == GetTempFileNameA(dir, "zok", 0, path)) return DqString();
    // 对齐 POSIX 分支的 mkstemp+unlink 语义：创建后删除，BeFile::Create 从干净状态开始
    DeleteFileA(path);
    return DqString(path) + suffix;
}
#else
DqString MakeTempPath(const char* suffix) {
    char tmpl[] = "/tmp/danqing_befile_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return DqString();
    close(fd);
    // mkstemp creates the file; unlink so BeFile::Create starts clean.
    unlink(tmpl);
    return DqString(tmpl) + suffix;
}
#endif

}  // namespace

// Ported from: imodel-native BeFile_Test.cpp (enum value derived from
//              Bentley.h:293 `ERROR = 0x8000` aliased by BeFile.h:36
//              `UnknownError = ERROR`).
// Authored: no single TEST in BeFile_Test.cpp asserts the raw numeric; this
// lock-in is required for conformance (§5 numeric precision) since callers
// may compare against ERROR / 0x8000.
TEST(BeFileTest, UnknownErrorAliasesErrorAt0x8000) {
    // Reference: enum class BeFileStatus { ... UnknownError = ERROR, };
    // with ERROR = 0x8000 (Bentley.h:293).
    EXPECT_EQ(static_cast<int>(BeFileStatus::UnknownError), 0x8000);
    // Sanity: Success stays 0 (SUCCESS = 0, Bentley.h:291).
    EXPECT_EQ(static_cast<int>(BeFileStatus::Success), 0);
    // And the named siblings must keep their reference implicit numbering
    // (BeFile.h:27-35): FileNotFoundError=1 ... FileAlreadyOpened=9.
    EXPECT_EQ(static_cast<int>(BeFileStatus::FileNotFoundError),     1);
    EXPECT_EQ(static_cast<int>(BeFileStatus::AccessViolationError),  2);
    EXPECT_EQ(static_cast<int>(BeFileStatus::SharingViolationError), 3);
    EXPECT_EQ(static_cast<int>(BeFileStatus::TooManyOpenFilesError), 4);
    EXPECT_EQ(static_cast<int>(BeFileStatus::FileNotOpenError),      5);
    EXPECT_EQ(static_cast<int>(BeFileStatus::NotLockedError),        6);
    EXPECT_EQ(static_cast<int>(BeFileStatus::ReadError),             7);
    EXPECT_EQ(static_cast<int>(BeFileStatus::DiskFull),              8);
    EXPECT_EQ(static_cast<int>(BeFileStatus::FileAlreadyOpened),     9);
}

// Ported from: imodel-native BeFile_Test.cpp TEST(BeFileTests, Write) lines 295-315
//              + TEST(BeFileTests, Read) lines 645-676
//
// Round-trip asserting the ref argument order compiles and works:
//   Write(uint32_t* bytesWritten, void const* buffer, uint32_t numBytes)
//   Read   (void* buffer,         uint32_t* bytesRead, uint32_t numBytes)
TEST(BeFileTest, ReadWriteRoundTripMatchesReferenceArgOrder) {
    DqString path = MakeTempPath(".txt");

    BeFile file;
    ASSERT_EQ(file.Create(path.c_str()), BeFileStatus::Success);

    // Ref Write signature: Write(&bytesWritten, buffer, numBytes).
    const char* buf = "QWERTYUIOP QWERTYUJHG !@#$%^&*() 1234567890";
    uint32_t byteCountToCopy = static_cast<uint32_t>(std::strlen(buf));
    uint32_t bytesWritten = 0;
    BeFileStatus status = file.Write(&bytesWritten, buf, byteCountToCopy);
    EXPECT_EQ(status, BeFileStatus::Success);
    EXPECT_EQ(bytesWritten, byteCountToCopy);
    file.Close();

    // Reopen for read.
    ASSERT_EQ(file.Open(path.c_str(), BeFileAccess::Read), BeFileStatus::Success);
    file.SetPointer(0, BeFileSeekOrigin::Begin);

    // Ref Read signature: Read(buffer, &bytesRead, numBytes).
    char readBuf[128] = {0};
    uint32_t bytesRead = 0;
    status = file.Read(readBuf, &bytesRead, byteCountToCopy);
    EXPECT_EQ(status, BeFileStatus::Success);
    EXPECT_EQ(bytesRead, byteCountToCopy);
    EXPECT_STREQ(readBuf, buf);
    file.Close();

    unlink(path.c_str());
}
