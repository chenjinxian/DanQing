// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeFileName.h
//              (no standalone BeFileName test file; test scenarios derived from header contract)
// dqBase tests — BeFileName 值类型行为验证

#include <gtest/gtest.h>

#include <dqBase/BeFileName.h>

#include <cstdio>
#include <fstream>

// ---------------------------------------------------------------------------
// 平台分隔符 helper：参考 imodel-native Windows 行为（Append/Combine/FixPathName
// 输出 '\'，IsAbsolutePath 需盘符前缀）。断言按平台构造期望值，场景不变（§5）。
// ---------------------------------------------------------------------------
namespace {
dqBase::DqString Sep() {
    return dqBase::DqString(1, dqBase::BeFileName::kDirSeparator);
}
} // namespace

// ---------------------------------------------------------------------------
// 构造函数测试
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.h default ctor (line 98)
TEST(BeFileNameTest, DefaultCtorIsEmpty) {
    dqBase::BeFileName fn;
    EXPECT_TRUE(fn.IsEmpty());
    EXPECT_EQ(fn.size(), 0u);
}

// Ported from: imodel-native BeFileName.h Utf8String ctor (line 107)
TEST(BeFileNameTest, StringCtor) {
    dqBase::BeFileName fn("/tmp/test.txt");
    EXPECT_EQ(fn, "/tmp/test.txt");
    EXPECT_FALSE(fn.IsEmpty());
}

// Ported from: imodel-native BeFileName.h copy ctor (line 137)
TEST(BeFileNameTest, CopyCtor) {
    dqBase::BeFileName fn("/tmp/test.txt");
    dqBase::BeFileName copy(fn);
    EXPECT_EQ(copy, "/tmp/test.txt");
}

// Ported from: imodel-native BeFileName.h 4-part ctor (line 134)
TEST(BeFileNameTest, FourPartCtor) {
    dqBase::BeFileName fn("", "/tmp/", "test", ".txt");
    EXPECT_EQ(fn, "/tmp/test.txt");
}

// ---------------------------------------------------------------------------
// 路径修改测试
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.h Clear (line 143)
TEST(BeFileNameTest, Clear) {
    dqBase::BeFileName fn("/tmp/test.txt");
    fn.Clear();
    EXPECT_TRUE(fn.IsEmpty());
}

// Ported from: imodel-native BeFileName.h SetName (line 147)
TEST(BeFileNameTest, SetName) {
    dqBase::BeFileName fn;
    fn.SetName("/tmp/new.txt");
    EXPECT_EQ(fn, "/tmp/new.txt");
}

// Ported from: imodel-native BeFileName.h AppendToPath (line 160)
TEST(BeFileNameTest, AppendToPath) {
    dqBase::BeFileName fn("/tmp");
    fn.AppendToPath("test.txt");
    EXPECT_EQ(fn, "/tmp" + Sep() + "test.txt");
}

// Ported from: imodel-native BeFileName.h AppendToPath (line 160)
TEST(BeFileNameTest, AppendToPathAddsSeparator) {
    dqBase::BeFileName fn("/tmp");
    fn.AppendToPath("subdir");
    EXPECT_EQ(fn, "/tmp" + Sep() + "subdir");
}

// Ported from: imodel-native BeFileName.h AppendExtension (line 163)
TEST(BeFileNameTest, AppendExtensionAddsDot) {
    dqBase::BeFileName fn("/tmp/test");
    fn.AppendExtension("txt");
    EXPECT_EQ(fn, "/tmp/test.txt");
}

// Ported from: imodel-native BeFileName.h AppendExtension (line 163)
TEST(BeFileNameTest, AppendExtensionAlreadyHasDot) {
    dqBase::BeFileName fn("/tmp/test");
    fn.AppendExtension(".txt");
    EXPECT_EQ(fn, "/tmp/test.txt");
}

// Ported from: imodel-native BeFileName.h AppendSeparator (line 172)
TEST(BeFileNameTest, AppendSeparator) {
    dqBase::BeFileName fn("/tmp");
    fn.AppendSeparator();
    EXPECT_EQ(fn.back(), dqBase::BeFileName::kDirSeparator);
}

// Ported from: imodel-native BeFileName.h AppendSeparator (line 172)
TEST(BeFileNameTest, AppendSeparatorAlreadyHasSeparator) {
    dqBase::BeFileName fn("/tmp/");
    fn.AppendSeparator();
    EXPECT_EQ(fn, "/tmp/");
}

// Ported from: imodel-native BeFileName.h PopDir (line 262)
TEST(BeFileNameTest, PopDir) {
    dqBase::BeFileName fn("/tmp/subdir/test.txt");
    fn.PopDir();
    EXPECT_EQ(fn, "/tmp/subdir/");
}

// Ported from: imodel-native BeFileName.h RemoveQuotes (line 178)
TEST(BeFileNameTest, RemoveQuotes) {
    dqBase::BeFileName fn("\"/tmp/test.txt\"");
    fn.RemoveQuotes();
    EXPECT_EQ(fn, "/tmp/test.txt");
}

// ---------------------------------------------------------------------------
// 路径解析测试
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.h ParseName (line 235)
TEST(BeFileNameTest, ParseName) {
    dqBase::DqString dev, dir, name, ext;
    dqBase::BeFileName::ParseName(&dev, &dir, &name, &ext, "/tmp/test.txt");
    EXPECT_EQ(dir, "/tmp/");
    EXPECT_EQ(name, "test");
    EXPECT_EQ(ext, ".txt");
}

// Ported from: imodel-native BeFileName.h ParseName (line 235)
TEST(BeFileNameTest, ParseNameNoExtension) {
    dqBase::DqString dev, dir, name, ext;
    dqBase::BeFileName::ParseName(&dev, &dir, &name, &ext, "/tmp/Makefile");
    EXPECT_EQ(dir, "/tmp/");
    EXPECT_EQ(name, "Makefile");
    EXPECT_EQ(ext, "");
}

// ---------------------------------------------------------------------------
// 路径获取测试
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.h GetDirectoryName (line 270)
TEST(BeFileNameTest, GetDirectoryName) {
    dqBase::BeFileName fn("/tmp/subdir/test.txt");
    dqBase::BeFileName dir = fn.GetDirectoryName();
    EXPECT_EQ(dir, "/tmp/subdir/");
}

// Ported from: imodel-native BeFileName.h GetExtension (line 286)
TEST(BeFileNameTest, GetExtension) {
    dqBase::BeFileName fn("/tmp/test.txt");
    EXPECT_EQ(fn.GetExtension(), "txt");
}

// Ported from: imodel-native BeFileName.h GetFileNameAndExtension (line 294)
TEST(BeFileNameTest, GetFileNameAndExtension) {
    dqBase::BeFileName fn("/tmp/test.txt");
    EXPECT_EQ(fn.GetFileNameAndExtension(), "test.txt");
}

// Ported from: imodel-native BeFileName.h GetFileNameWithoutExtension (line 305)
TEST(BeFileNameTest, GetFileNameWithoutExtension) {
    dqBase::BeFileName fn("/tmp/test.txt");
    EXPECT_EQ(fn.GetFileNameWithoutExtension(), "test");
}

// Ported from: imodel-native BeFileName.h GetDevice (line 315)
TEST(BeFileNameTest, GetDeviceUnix) {
    dqBase::BeFileName fn("/tmp/test.txt");
    EXPECT_EQ(fn.GetDevice(), "");
}

// ---------------------------------------------------------------------------
// 属性查询测试
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.h IsAbsolutePath (line 387)
TEST(BeFileNameTest, IsAbsolutePath) {
#ifdef _WIN32
    // Windows：绝对路径需盘符前缀（ref Windows 分支行为）
    EXPECT_FALSE(dqBase::BeFileName("/tmp/test.txt").IsAbsolutePath());
    EXPECT_TRUE(dqBase::BeFileName("C:/tmp/test.txt").IsAbsolutePath());
#else
    EXPECT_TRUE(dqBase::BeFileName("/tmp/test.txt").IsAbsolutePath());
#endif
    EXPECT_FALSE(dqBase::BeFileName("test.txt").IsAbsolutePath());
    EXPECT_FALSE(dqBase::BeFileName("").IsAbsolutePath());
}

// Ported from: imodel-native BeFileName.h DoesPathExist (line 377)
TEST(BeFileNameTest, DoesPathExist) {
    dqBase::BeFileName tmpDir("/tmp");
    EXPECT_TRUE(tmpDir.DoesPathExist());
    EXPECT_FALSE(dqBase::BeFileName("/nonexistent/path/xyz").DoesPathExist());
}

// Ported from: imodel-native BeFileName.h IsDirectory (line 411)
TEST(BeFileNameTest, IsDirectory) {
    EXPECT_TRUE(dqBase::BeFileName("/tmp").IsDirectory());
    EXPECT_FALSE(dqBase::BeFileName("/nonexistent").IsDirectory());
}

// Ported from: imodel-native BeFileName.h IsEquivalentTo (line 403)
TEST(BeFileNameTest, IsEquivalentTo) {
    dqBase::BeFileName fn("/tmp/test.txt");
    EXPECT_TRUE(fn.IsEquivalentTo("/tmp/test.txt"));
    EXPECT_FALSE(fn.IsEquivalentTo("/tmp/other.txt"));
}

// ---------------------------------------------------------------------------
// 文件操作测试
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.h CreateNewDirectory (line 424)
TEST(BeFileNameTest, CreateNewDirectory) {
    dqBase::DqString testDir = "/tmp/dqBIM_test_dir";
    auto status = dqBase::BeFileName::CreateNewDirectory(testDir);
    EXPECT_EQ(status, dqBase::BeFileNameStatus::Success);
    EXPECT_TRUE(dqBase::BeFileName(testDir).IsDirectory());

    // Already exists
    status = dqBase::BeFileName::CreateNewDirectory(testDir);
    EXPECT_EQ(status, dqBase::BeFileNameStatus::AlreadyExists);

    // Cleanup
    rmdir(testDir.c_str());
}

// Ported from: imodel-native BeFileName.h BeDeleteFile (line 468)
TEST(BeFileNameTest, BeDeleteFile) {
    dqBase::DqString testFile = "/tmp/dqBIM_test_file.txt";
    std::ofstream(testFile) << "test";
    EXPECT_TRUE(dqBase::BeFileName(testFile).DoesPathExist());

    auto status = dqBase::BeFileName::BeDeleteFileStatic(testFile);
    EXPECT_EQ(status, dqBase::BeFileNameStatus::Success);
    EXPECT_FALSE(dqBase::BeFileName(testFile).DoesPathExist());
}

// Ported from: imodel-native BeFileName.h BeCopyFile (line 452)
TEST(BeFileNameTest, BeCopyFile) {
    dqBase::DqString srcFile = "/tmp/dqBIM_test_src.txt";
    dqBase::DqString dstFile = "/tmp/dqBIM_test_dst.txt";
    std::ofstream(srcFile) << "hello";

    auto status = dqBase::BeFileName::BeCopyFile(
        dqBase::BeFileName(srcFile), dqBase::BeFileName(dstFile));
    EXPECT_EQ(status, dqBase::BeFileNameStatus::Success);
    EXPECT_TRUE(dqBase::BeFileName(dstFile).DoesPathExist());

    // Cleanup
    unlink(srcFile.c_str());
    unlink(dstFile.c_str());
}

// Ported from: imodel-native BeFileName.h GetFileSize (line 526)
TEST(BeFileNameTest, GetFileSize) {
    dqBase::DqString testFile = "/tmp/dqBIM_test_size.txt";
    std::ofstream(testFile) << "hello world";

    dqBase::BeFileName fn(testFile);
    uint64_t sz = 0;
    auto status = fn.GetFileSize(sz);
    EXPECT_EQ(status, dqBase::BeFileNameStatus::Success);
    EXPECT_EQ(sz, 11u);  // "hello world" = 11 bytes

    unlink(testFile.c_str());
}

// Ported from: imodel-native BeFileName.h SetFileReadOnly / IsFileReadOnly (lines 499/512)
TEST(BeFileNameTest, SetFileReadOnly) {
    dqBase::DqString testFile = "/tmp/dqBIM_test_ro.txt";
    std::ofstream(testFile) << "test";

    dqBase::BeFileName fn(testFile);
    auto status = fn.SetFileReadOnly(true);
    EXPECT_EQ(status, dqBase::BeFileNameStatus::Success);
    EXPECT_TRUE(fn.IsFileReadOnly());

    status = fn.SetFileReadOnly(false);
    EXPECT_EQ(status, dqBase::BeFileNameStatus::Success);
    EXPECT_FALSE(fn.IsFileReadOnly());

    unlink(testFile.c_str());
}

// ---------------------------------------------------------------------------
// 路径规范化测试
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.h FixPathName (line 345)
TEST(BeFileNameTest, FixPathName) {
    dqBase::DqString result;
    auto status = dqBase::BeFileName::FixPathName(result, "/tmp//test.txt");
    EXPECT_EQ(status, dqBase::BeFileNameStatus::Success);
    // Windows 分支把所有 '/' 规范化为 '\'（含前导，ref FixPathName Windows 行为）
    EXPECT_EQ(result, "\\tmp" + Sep() + "test.txt");
}

// ---------------------------------------------------------------------------
// 静态方法兼容性测试
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.h Combine (line 185)
TEST(BeFileNameTest, CombineStatic) {
    dqBase::DqString result = dqBase::BeFileName::CombineStatic("/tmp", "test.txt");
    EXPECT_EQ(result, "/tmp" + Sep() + "test.txt");
}

// Ported from: imodel-native BeFileName.h AppendSeparator (line 169)
TEST(BeFileNameTest, AppendSeparatorStatic) {
    dqBase::DqString result = dqBase::BeFileName::AppendSeparatorStatic("/tmp");
    EXPECT_EQ(result, "/tmp" + Sep());
}

// Ported from: imodel-native BeFileName.h GetExtension (line 283)
TEST(BeFileNameTest, GetExtensionStatic) {
    EXPECT_EQ(dqBase::BeFileName::GetExtensionStatic("/tmp/test.txt"), "txt");
    EXPECT_EQ(dqBase::BeFileName::GetExtensionStatic("/tmp/Makefile"), "");
}

// Ported from: imodel-native BeFileName.h GetFileNameWithoutExtension (line 302)
TEST(BeFileNameTest, GetFileNameWithoutExtensionStatic) {
    EXPECT_EQ(dqBase::BeFileName::GetFileNameWithoutExtensionStatic("/tmp/test.txt"), "test");
    EXPECT_EQ(dqBase::BeFileName::GetFileNameWithoutExtensionStatic("test.txt"), "test");
}

// Ported from: imodel-native BeFileName.h IsAbsolutePath (line 387)
TEST(BeFileNameTest, IsAbsolutePathStatic) {
#ifdef _WIN32
    // Windows：绝对路径需盘符前缀（ref Windows 分支行为）
    EXPECT_FALSE(dqBase::BeFileName::IsAbsolutePathStatic("/tmp/test.txt"));
    EXPECT_TRUE(dqBase::BeFileName::IsAbsolutePathStatic("C:/tmp/test.txt"));
#else
    EXPECT_TRUE(dqBase::BeFileName::IsAbsolutePathStatic("/tmp/test.txt"));
#endif
    EXPECT_FALSE(dqBase::BeFileName::IsAbsolutePathStatic("test.txt"));
}

// ---------------------------------------------------------------------------
// FileNameParts 构造测试
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.h FileNameParts ctor (line 131)
TEST(BeFileNameTest, FileNamePartsNameAndExt) {
    dqBase::BeFileName fn(dqBase::NameAndExt, "/tmp/test.txt");
    EXPECT_EQ(fn, "test.txt");
}

// Ported from: imodel-native BeFileName.h FileNameParts ctor (line 131)
TEST(BeFileNameTest, FileNamePartsDevAndDir) {
    dqBase::BeFileName fn(dqBase::DevAndDir, "/tmp/test.txt");
    EXPECT_EQ(fn, "/tmp/");
}
