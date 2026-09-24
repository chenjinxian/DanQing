// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/SHA1_Test.cpp (sibling)
//              (No MD5_Test.cpp exists in imodel-native — see Authored note below.)
//
// Validates that MD5::operator() returns a lowercase-hex Utf8String digest,
// matching the reference declaration in imodel-native md5.h:74,77:
//     BENTLEYDLL_EXPORT Utf8String operator()(const void* data, size_t numBytes);
//     BENTLEYDLL_EXPORT Utf8String operator()(Utf8StringCR text);
// and the documented usage at md5.h:33-35:
//     Utf8String myHash = md5("Hello World");
//
// Canonical digest values come from RFC 1321 / NIST FIPS 180-4 test vectors
// (the same vectors any conformant MD5 implementation must produce).
#include <gtest/gtest.h>

#include <dqBase/MD5.h>

#include <string>

using dqBase::MD5;

// Authored: no reference test exists in imodel-native for MD5
// (iModelCore/Bentley/Tests/NonPublished has SHA1_Test.cpp but no MD5_Test.cpp).
// Canonical values are the well-known MD5 test vectors (RFC 1321), verified
// against the system `md5` / `md5sum` utilities:
//   MD5("")            = d41d8cd98f00b204e9800998ecf8427e
//   MD5("Hello World") = b10a8db164e0754105b7a99be72e3fe5
TEST(MD5Test, OperatorStringReturnsHexDigestMatchReferenceShape) {
    MD5 md5;
    // Ref shape: Utf8String operator()(Utf8StringCR text)
    std::string h = md5(std::string("Hello World"));
    EXPECT_EQ(std::string("b10a8db164e0754105b7a99be72e3fe5"), h);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/SHA1_Test.cpp (sibling)
//              TEST(MD5Test, OperatorBytesReturnsHexDigestMatchReferenceShape)
TEST(MD5Test, OperatorBytesReturnsHexDigestMatchReferenceShape) {
    MD5 md5;
    // Ref shape: Utf8String operator()(const void* data, size_t numBytes)
    std::string h = md5("Hello World", 11);
    EXPECT_EQ(std::string("b10a8db164e0754105b7a99be72e3fe5"), h);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/SHA1_Test.cpp (sibling)
//              TEST(MD5Test, EmptyStringDigest)
TEST(MD5Test, EmptyStringDigest) {
    MD5 md5;
    std::string h = md5(std::string(""));
    EXPECT_EQ(std::string("d41d8cd98f00b204e9800998ecf8427e"), h);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/SHA1_Test.cpp (sibling)
//              TEST(MD5Test, StreamingGetHashStringMatchesOperator)
TEST(MD5Test, StreamingGetHashStringMatchesOperator) {
    MD5 md5;
    md5.Add("Hello ", 6);
    md5.Add("World", 5);
    EXPECT_EQ(std::string("b10a8db164e0754105b7a99be72e3fe5"), md5.GetHashString());
}
