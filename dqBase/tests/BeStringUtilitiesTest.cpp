// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeStringUtilities.h
//              (no standalone BeStringUtilities test file; test scenarios derived from header contract)
// dqBase tests — BeStringUtilities 行为验证

#include <gtest/gtest.h>

#include <dqBase/BeStringUtilities.h>

#include <cstring>

// ---------------------------------------------------------------------------
// Sprintf / Vsprintf
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeStringUtilities Sprintf contract
TEST(BeStringUtilitiesTest, SprintfFormats) {
    auto result = dqBase::BeStringUtilities::Sprintf("hello %s %d", "world", 42);
    EXPECT_EQ(result, "hello world 42");
}

// Ported from: imodel-native BeStringUtilities Sprintf contract
TEST(BeStringUtilitiesTest, SprintfEmpty) {
    auto result = dqBase::BeStringUtilities::Sprintf("no args");
    EXPECT_EQ(result, "no args");
}

// ---------------------------------------------------------------------------
// Snprintf
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeStringUtilities Snprintf contract
TEST(BeStringUtilitiesTest, SnprintfTruncates) {
    char buf[6];
    int n = dqBase::BeStringUtilities::Snprintf(buf, sizeof(buf), "hello world");
    EXPECT_EQ(std::string(buf), "hello");  // truncated to 5 chars + null
    EXPECT_GT(n, 5);  // returns what would have been written
}

// ---------------------------------------------------------------------------
// Stricmp / Strnicmp
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeStringUtilities Stricmp contract
TEST(BeStringUtilitiesTest, StricmpCaseInsensitive) {
    EXPECT_EQ(dqBase::BeStringUtilities::Stricmp("hello", "HELLO"), 0);
    EXPECT_LT(dqBase::BeStringUtilities::Stricmp("abc", "def"), 0);
    EXPECT_GT(dqBase::BeStringUtilities::Stricmp("def", "abc"), 0);
}

// Ported from: imodel-native BeStringUtilities Strnicmp contract
TEST(BeStringUtilitiesTest, StrnicmpWithCount) {
    EXPECT_EQ(dqBase::BeStringUtilities::Strnicmp("hello world", "HELLO xyz", 5), 0);
    // First 6 chars: "hello " vs "HELLO " — case-insensitive equal
    EXPECT_EQ(dqBase::BeStringUtilities::Strnicmp("hello world", "HELLO xyz", 6), 0);
    // First 7 chars: "hello w" vs "HELLO x" — not equal
    EXPECT_NE(dqBase::BeStringUtilities::Strnicmp("hello world", "HELLO xyz", 7), 0);
}

// ---------------------------------------------------------------------------
// ParseUInt64 / FormatUInt64
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeStringUtilities ParseUInt64 contract
TEST(BeStringUtilitiesTest, ParseUInt64Decimal) {
    EXPECT_EQ(dqBase::BeStringUtilities::ParseUInt64("12345"), 12345u);
    EXPECT_EQ(dqBase::BeStringUtilities::ParseUInt64("0"), 0u);
}

// Ported from: imodel-native BeStringUtilities ParseUInt64 contract
TEST(BeStringUtilitiesTest, ParseUInt64Hex) {
    EXPECT_EQ(dqBase::BeStringUtilities::ParseUInt64("FF", 16), 255u);
    EXPECT_EQ(dqBase::BeStringUtilities::ParseUInt64("0x10", 16), 16u);
}

// Ported from: imodel-native BeStringUtilities FormatUInt64 contract
TEST(BeStringUtilitiesTest, FormatUInt64Decimal) {
    char buf[32];
    dqBase::BeStringUtilities::FormatUInt64(buf, sizeof(buf), 12345, 10);
    EXPECT_EQ(std::string(buf), "12345");
}

// Ported from: imodel-native BeStringUtilities FormatUInt64 contract
TEST(BeStringUtilitiesTest, FormatUInt64Hex) {
    char buf[32];
    dqBase::BeStringUtilities::FormatUInt64(buf, sizeof(buf), 255, 16);
    EXPECT_EQ(std::string(buf), "ff");
}

// ---------------------------------------------------------------------------
// Split / Join
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeStringUtilities Split contract
TEST(BeStringUtilitiesTest, SplitByComma) {
    auto parts = dqBase::BeStringUtilities::Split("a,b,c", ",");
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

// Ported from: imodel-native BeStringUtilities Split contract
TEST(BeStringUtilitiesTest, SplitByMultipleDelimiters) {
    auto parts = dqBase::BeStringUtilities::Split("a:b;c", ":;");
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

// Ported from: imodel-native BeStringUtilities Split contract
TEST(BeStringUtilitiesTest, SplitEmpty) {
    auto parts = dqBase::BeStringUtilities::Split("", ",");
    ASSERT_EQ(parts.size(), 1u);
    EXPECT_EQ(parts[0], "");
}

// Ported from: imodel-native BeStringUtilities Join contract
TEST(BeStringUtilitiesTest, JoinWithSeparator) {
    dqBase::DqVector<dqBase::DqString> parts = {"a", "b", "c"};
    auto result = dqBase::BeStringUtilities::Join(parts, ", ");
    EXPECT_EQ(result, "a, b, c");
}

// Ported from: imodel-native BeStringUtilities Join contract
TEST(BeStringUtilitiesTest, JoinEmpty) {
    dqBase::DqVector<dqBase::DqString> parts;
    auto result = dqBase::BeStringUtilities::Join(parts, ", ");
    EXPECT_EQ(result, "");
}

// Ported from: imodel-native BeStringUtilities Join contract
TEST(BeStringUtilitiesTest, JoinSingle) {
    dqBase::DqVector<dqBase::DqString> parts = {"hello"};
    auto result = dqBase::BeStringUtilities::Join(parts, ", ");
    EXPECT_EQ(result, "hello");
}

// ---------------------------------------------------------------------------
// LexicographicCompare
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeStringUtilities LexicographicCompare contract
TEST(BeStringUtilitiesTest, LexicographicCompare) {
    EXPECT_EQ(dqBase::BeStringUtilities::LexicographicCompare("abc", "abc"), 0);
    EXPECT_LT(dqBase::BeStringUtilities::LexicographicCompare("abc", "def"), 0);
    EXPECT_GT(dqBase::BeStringUtilities::LexicographicCompare("def", "abc"), 0);
    EXPECT_LT(dqBase::BeStringUtilities::LexicographicCompare("ab", "abc"), 0);
}

// ---------------------------------------------------------------------------
// HexFormatOptions / FormatHex
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeStringUtilities HexFormatOptions contract
TEST(BeStringUtilitiesTest, FormatHexBasic) {
    auto result = dqBase::BeStringUtilities::FormatHex(255);
    EXPECT_EQ(result, "ff");
}

// Ported from: imodel-native BeStringUtilities HexFormatOptions contract
TEST(BeStringUtilitiesTest, FormatHexUppercase) {
    auto result = dqBase::BeStringUtilities::FormatHex(255, dqBase::HexFormatOptions::Uppercase);
    EXPECT_EQ(result, "FF");
}

// Ported from: imodel-native BeStringUtilities HexFormatOptions contract
TEST(BeStringUtilitiesTest, FormatHexWithPrefix) {
    auto result = dqBase::BeStringUtilities::FormatHex(255, dqBase::HexFormatOptions::IncludePrefix);
    EXPECT_EQ(result, "0xff");
}

// Ported from: imodel-native BeStringUtilities HexFormatOptions contract
TEST(BeStringUtilitiesTest, FormatHexLeadingZeros) {
    auto result = dqBase::BeStringUtilities::FormatHex(0xF, dqBase::HexFormatOptions::LeadingZeros, 4);
    EXPECT_EQ(result, "000f");
}

// Ported from: imodel-native BeStringUtilities HexFormatOptions contract
TEST(BeStringUtilitiesTest, FormatHexZero) {
    auto result = dqBase::BeStringUtilities::FormatHex(0);
    EXPECT_EQ(result, "0");
}

// ---------------------------------------------------------------------------
// NPOS / AsManyAsPossible
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeStringUtilities.h:86-87
TEST(BeStringUtilitiesTest, Constants) {
    EXPECT_EQ(dqBase::BeStringUtilities::NPOS, static_cast<size_t>(-1));
    EXPECT_EQ(dqBase::BeStringUtilities::AsManyAsPossible, static_cast<size_t>(-1));
}
