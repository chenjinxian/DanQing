// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/SHA1_Test.cpp
//              TEST(SHA1Test, String)   lines 11-24
//              TEST(SHA1Test, Bytes)    lines 28-37
//              TEST(SHA1Test, Stream)   lines 41-50
//
// Validates that SHA1::operator() returns a lowercase-hex Utf8String digest,
// matching the reference declaration in imodel-native SHA1.h:76,78:
//     BENTLEYDLL_EXPORT Utf8String operator()(const void* data, size_t numBytes);
//     BENTLEYDLL_EXPORT Utf8String operator()(Utf8StringCR text);
// and the documented usage at SHA1.h:35:
//     Utf8String myHash = sha1("Hello World");
//
// Canonical digest values are taken verbatim from the reference test
// (CLAUDE.md §4: imodel-native C++ tests take priority).
#include <gtest/gtest.h>

#include <dqBase/SHA1.h>

#include <string>

using dqBase::SHA1;

// Ported from: imodel-native SHA1_Test.cpp TEST(SHA1Test, String) lines 11-24
TEST(SHA1Test, String) {
    SHA1 sha1;
    // Utf8String
    EXPECT_STREQ("0a4d55a8d778e5022fab701977c5d840bbc486d0", sha1(std::string("Hello World")).c_str());
    // empty string
    EXPECT_STREQ("da39a3ee5e6b4b0d3255bfef95601890afd80709", sha1(std::string("")).c_str());

    std::string longStr;
    for (int i = 0; i < 8; ++i)
        longStr += "12345678";
    EXPECT_STREQ("8c3697a6c16f22ee9a7871ff6cd06ca1e8868216", sha1(longStr).c_str());
}

// Ported from: imodel-native SHA1_Test.cpp TEST(SHA1Test, Bytes) lines 28-37
TEST(SHA1Test, Bytes) {
    SHA1 sha1;
    // arbitrary data, 11 bytes
    EXPECT_STREQ("0a4d55a8d778e5022fab701977c5d840bbc486d0", sha1("Hello World", 11).c_str());
    // arbitrary data, 11 bytes, sub string
    EXPECT_STREQ("0a4d55a8d778e5022fab701977c5d840bbc486d0", sha1("Hello World!", 11).c_str());
    // Empty
    EXPECT_STREQ("da39a3ee5e6b4b0d3255bfef95601890afd80709", sha1("", 0).c_str());
}

// Ported from: imodel-native SHA1_Test.cpp TEST(SHA1Test, Stream) lines 41-50
TEST(SHA1Test, Stream) {
    SHA1 sha1;
    sha1.Add("Hello ", 6);
    sha1.Add("World", 5);
    EXPECT_STREQ("0a4d55a8d778e5022fab701977c5d840bbc486d0", sha1.GetHashString().c_str());
}
