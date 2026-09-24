// SPDX-License-Identifier: Apache-2.0
// DanQing dqBase — B.1.1 remaining items unit tests
//
// BeTest/BeDirectoryIterator/BeFileListIterator/ProcessDetector tests:
// Authored: no reference tests exist in imodel-native or itwinjs-core for these types
#include <dqBase/BeDirectoryIterator.h>
#include <dqBase/BeFileListIterator.h>
#include <dqBase/BeTest.h>
#include <dqBase/ProcessDetector.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <sys/stat.h>

using namespace dqBase;

// Authored: no reference test exists for BeTest
TEST(BeTest, GetTempDir)
{
    const auto dir = BeTest::GetTempDir();
    EXPECT_FALSE(dir.empty());
}

// Authored: no reference test exists for BeTest
TEST(BeTest, GetOutputRoot)
{
    const auto dir = BeTest::GetOutputRoot();
    EXPECT_FALSE(dir.empty());
}

// Authored: no reference test exists for BeTest
TEST(BeTest, GetDocumentsRoot)
{
    const auto dir = BeTest::GetDocumentsRoot();
    EXPECT_FALSE(dir.empty());
}

// Authored: no reference test exists for BeDirectoryIterator
TEST(BeDirectoryIterator, OpenNonExistent)
{
    BeDirectoryIterator iter;
    EXPECT_FALSE(iter.Open("/nonexistent_directory_path_12345"));
}

// Authored: no reference test exists for BeDirectoryIterator
TEST(BeDirectoryIterator, IterateTmp)
{
    BeDirectoryIterator iter;
    if (!iter.Open("/tmp"))
        return;  // /tmp might not exist on all platforms

    DqString name;
    bool isDir = false;
    bool isFile = false;
    int count = 0;
    while (iter.GetNextEntry(name, isDir, isFile) && count < 100) {
        EXPECT_FALSE(name.empty());
        // Note: some /tmp entries may have stat() failures (broken symlinks),
        // so we don't assert isDir || isFile for every entry.
        ++count;
    }
    // /tmp should have at least one entry
    EXPECT_GT(count, 0);
}

// Authored: no reference test exists for BeDirectoryIterator
TEST(BeDirectoryIterator, WalkDirsAndMatch)
{
    // Create a temp directory with files（Windows 无 /tmp，用当前目录下相对路径）
#ifdef _WIN32
    const char* testDir = "dqBIM_test_walkdir";
    _mkdir(testDir);
#else
    const char* testDir = "/tmp/dqBIM_test_walkdir";
    mkdir(testDir, 0755);
#endif

    // Create a test file
    DqString filePath = DqString(testDir) + "/testfile.txt";
    FILE* f = fopen(filePath.c_str(), "w");
    if (f) {
        fprintf(f, "test");
        fclose(f);
    }

    DqVector<DqString> results;
    BeDirectoryIterator::WalkDirsAndMatch(results, testDir, "*");
    EXPECT_GE(results.size(), 1u);

    // Cleanup
    remove(filePath.c_str());
    rmdir(testDir);
}

// Authored: no reference test exists for BeFileListIterator
TEST(BeFileListIterator, SingleFile)
{
    BeFileListIterator iter("/tmp/test.txt");
    DqString name;
    EXPECT_TRUE(iter.GetNextFileName(name));
    EXPECT_EQ(name, "/tmp/test.txt");
    EXPECT_FALSE(iter.GetNextFileName(name));
}

// Authored: no reference test exists for BeFileListIterator
TEST(BeFileListIterator, MultipleFiles)
{
    BeFileListIterator iter("/tmp/a.txt;/tmp/b.txt;/tmp/c.txt");
    DqString name;

    EXPECT_TRUE(iter.GetNextFileName(name));
    EXPECT_EQ(name, "/tmp/a.txt");

    EXPECT_TRUE(iter.GetNextFileName(name));
    EXPECT_EQ(name, "/tmp/b.txt");

    EXPECT_TRUE(iter.GetNextFileName(name));
    EXPECT_EQ(name, "/tmp/c.txt");

    EXPECT_FALSE(iter.GetNextFileName(name));
}

// Authored: no reference test exists for BeFileListIterator
TEST(BeFileListIterator, EmptyList)
{
    BeFileListIterator iter("");
    DqString name;
    EXPECT_FALSE(iter.GetNextFileName(name));
}

// Authored: no reference test exists for ProcessDetector
TEST(ProcessDetector, DesktopPlatform)
{
    // On macOS/Linux build machines, this should be true
#if defined(__APPLE__) || defined(__linux__)
    EXPECT_TRUE(ProcessDetector::IsDesktopPlatform());
#else
    // Windows or other
    EXPECT_TRUE(ProcessDetector::IsDesktopPlatform());
#endif
}

// Authored: no reference test exists for ProcessDetector
TEST(ProcessDetector, MobilePlatform)
{
    // On desktop build machines, this should be false
#if !defined(__ANDROID__) && !defined(__APPLE_IOS__)
    EXPECT_FALSE(ProcessDetector::IsMobilePlatform());
#endif
}

// Authored: no reference test exists for ProcessDetector
TEST(ProcessDetector, PlatformConsistency)
{
    // A platform cannot be both desktop and mobile
    EXPECT_FALSE(ProcessDetector::IsDesktopPlatform() && ProcessDetector::IsMobilePlatform());

    // At least one should be true (we're running somewhere)
    EXPECT_TRUE(ProcessDetector::IsDesktopPlatform() || ProcessDetector::IsMobilePlatform());
}

// Authored: no reference test exists for ProcessDetector
TEST(ProcessDetector, ApplePlatform)
{
#if defined(__APPLE__)
    EXPECT_TRUE(ProcessDetector::IsApplePlatform());
#else
    EXPECT_FALSE(ProcessDetector::IsApplePlatform());
#endif
}

// Authored: no reference test exists for ProcessDetector
TEST(ProcessDetector, WindowsPlatform)
{
#if defined(_WIN32)
    EXPECT_TRUE(ProcessDetector::IsWindowsPlatform());
#else
    EXPECT_FALSE(ProcessDetector::IsWindowsPlatform());
#endif
}

// Authored: no reference test exists for ProcessDetector
TEST(ProcessDetector, LinuxPlatform)
{
#if defined(__linux__) && !defined(__ANDROID__)
    EXPECT_TRUE(ProcessDetector::IsLinuxPlatform());
#else
    EXPECT_FALSE(ProcessDetector::IsLinuxPlatform());
#endif
}
