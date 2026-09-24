// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeTextFile.h
//              (no standalone BeTextFile test file; test scenarios derived from header contract)
// dqBase tests — BeTextFile 行为验证

#include <gtest/gtest.h>

#include <dqBase/BeTextFile.h>

#include <cstdio>
#include <fstream>
#include <string>

static const char* kTestFile = "/tmp/dqBIM_test_textfile.txt";

// ---------------------------------------------------------------------------
// Open / Close
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeTextFile.h Open contract
TEST(BeTextFileTest, OpenForWrite) {
    dqBase::BeFileStatus status;
    auto file = dqBase::BeTextFile::Open(status, kTestFile,
        dqBase::TextFileOpenType::Write, dqBase::TextFileOptions::None, dqBase::TextFileEncoding::Utf8);
    EXPECT_EQ(status, dqBase::BeFileStatus::Success);
    EXPECT_TRUE(file.IsValid());
    file->Close();
    std::remove(kTestFile);
}

// Ported from: imodel-native BeTextFile.h Open contract
TEST(BeTextFileTest, OpenNonExistentForReadFails) {
    dqBase::BeFileStatus status;
    auto file = dqBase::BeTextFile::Open(status, "/tmp/nonexistent_file_xyz.txt",
        dqBase::TextFileOpenType::Read);
    EXPECT_NE(status, dqBase::BeFileStatus::Success);
    EXPECT_FALSE(file.IsValid());
}

// ---------------------------------------------------------------------------
// PutLine / GetLine
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeTextFile.h PutLine/GetLine contract
TEST(BeTextFileTest, PutLineAndGetLineRoundtrip) {
    dqBase::BeFileStatus status;
    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Write);
        EXPECT_EQ(status, dqBase::BeFileStatus::Success);
        EXPECT_EQ(file->PutLine("hello"), dqBase::TextFileWriteStatus::Success);
        EXPECT_EQ(file->PutLine("world"), dqBase::TextFileWriteStatus::Success);
    }

    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Read);
        EXPECT_EQ(status, dqBase::BeFileStatus::Success);

        dqBase::DqString line;
        EXPECT_EQ(file->GetLine(line), dqBase::TextFileReadStatus::Success);
        EXPECT_EQ(line, "hello");
        EXPECT_EQ(file->GetLine(line), dqBase::TextFileReadStatus::Success);
        EXPECT_EQ(line, "world");
        EXPECT_EQ(file->GetLine(line), dqBase::TextFileReadStatus::Eof);
    }
    std::remove(kTestFile);
}

// Ported from: imodel-native BeTextFile.h GetLine contract
TEST(BeTextFileTest, GetLineWithoutNewline) {
    dqBase::BeFileStatus status;
    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Write);
        file->PutLine("line1", false);
        file->PutLine("line2", false);
    }

    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Read);
        dqBase::DqString line;
        // 没有换行符，所有内容在一行
        EXPECT_EQ(file->GetLine(line), dqBase::TextFileReadStatus::Success);
        EXPECT_EQ(line, "line1line2");
    }
    std::remove(kTestFile);
}

// ---------------------------------------------------------------------------
// GetChar
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeTextFile.h GetChar contract
TEST(BeTextFileTest, GetCharReadsCharacterByCharacter) {
    dqBase::BeFileStatus status;
    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Write);
        file->PutLine("abc", false);
    }

    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Read);
        EXPECT_EQ(file->GetChar(), 'a');
        EXPECT_EQ(file->GetChar(), 'b');
        EXPECT_EQ(file->GetChar(), 'c');
        EXPECT_EQ(file->GetChar(), -1);  // EOF
    }
    std::remove(kTestFile);
}

// ---------------------------------------------------------------------------
// PrintfTo
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeTextFile.h PrintfTo contract
TEST(BeTextFileTest, PrintfToWritesFormatted) {
    dqBase::BeFileStatus status;
    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Write);
        EXPECT_EQ(file->PrintfTo(false, "value=%d", 42), dqBase::TextFileWriteStatus::Success);
    }

    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Read);
        dqBase::DqString line;
        file->GetLine(line);
        EXPECT_EQ(line, "value=42");
    }
    std::remove(kTestFile);
}

// ---------------------------------------------------------------------------
// Rewind
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeTextFile.h Rewind contract
TEST(BeTextFileTest, RewindAllowsReread) {
    dqBase::BeFileStatus status;
    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Write);
        file->PutLine("test");
    }

    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Read);
        dqBase::DqString line;
        file->GetLine(line);
        EXPECT_EQ(line, "test");

        // Rewind
        EXPECT_EQ(file->Rewind(), dqBase::BeFileStatus::Success);
        file->GetLine(line);
        EXPECT_EQ(line, "test");
    }
    std::remove(kTestFile);
}

// ---------------------------------------------------------------------------
// SetPointer / GetPointer
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeTextFile.h SetPointer/GetPointer contract
TEST(BeTextFileTest, SetPointerAndGetPointer) {
    dqBase::BeFileStatus status;
    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Write);
        file->PutLine("hello world");
    }

    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Read);
        uint64_t pos = 0;
        EXPECT_EQ(file->GetPointer(pos), dqBase::BeFileStatus::Success);
        EXPECT_EQ(pos, 0u);

        // Seek to byte 6 ("world")
        EXPECT_EQ(file->SetPointer(6, dqBase::BeFileSeekOrigin::Begin), dqBase::BeFileStatus::Success);
        EXPECT_EQ(file->GetPointer(pos), dqBase::BeFileStatus::Success);
        EXPECT_EQ(pos, 6u);
    }
    std::remove(kTestFile);
}

// ---------------------------------------------------------------------------
// Append mode
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeTextFile.h Append contract
TEST(BeTextFileTest, AppendMode) {
    dqBase::BeFileStatus status;
    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Write);
        file->PutLine("first");
    }
    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Append);
        file->PutLine("second");
    }
    {
        auto file = dqBase::BeTextFile::Open(status, kTestFile,
            dqBase::TextFileOpenType::Read);
        dqBase::DqString line;
        file->GetLine(line);
        EXPECT_EQ(line, "first");
        file->GetLine(line);
        EXPECT_EQ(line, "second");
    }
    std::remove(kTestFile);
}

// ---------------------------------------------------------------------------
// BUFFER_SIZE constant
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeTextFile.h:63
TEST(BeTextFileTest, BufferSizeConstant) {
    EXPECT_EQ(dqBase::BeTextFile::BUFFER_SIZE, 100u);
}
