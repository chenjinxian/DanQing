// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/Base64UtilitiesTests.cpp
//              TEST_F(Base64UtilitiesTests, EncodeEmptyString)
//              TEST_F(Base64UtilitiesTests, EncodeEmptyBlob)
//              TEST_F(Base64UtilitiesTests, EncodeString)
//              TEST_F(Base64UtilitiesTests, DecodeEmptyString)
//              TEST_F(Base64UtilitiesTests, Decode)
//              TEST_F(Base64UtilitiesTests, EncodeDecodeString)
//              TEST_F(Base64UtilitiesTests, EncodeDecodeBlob)
//              TEST_F(Base64UtilitiesTests, EncodeDecodeByteStream)
//              TEST_F(Base64UtilitiesTests, AlphabetsMatching)
//
// Validates the full reference surface (imodel-native Base64Utilities.h:24-39):
//   - Encode(Utf8StringCR, header) / Encode(Utf8CP, size_t, header) /
//     Encode(Utf8StringR, Byte const*, size_t, header)
//   - Decode(bvector<Byte>&, ...) / Decode(Utf8String) / Decode(ByteStream&, ...)
//   - Alphabet() returns the canonical base64 alphabet
//   - MatchesAlphabet(Utf8CP)
//
// Reference-delegated Decode(ByteStream&, ...) is implemented as an overload
// taking DqVector<uint8_t>& (the DanQing-idiomatic byte buffer). DqByteStream is
// a §6 deferred item (audit row 20/54); the deviation is documented in
// Base64Utilities.h.
#include <gtest/gtest.h>

#include <dqBase/Base64Utilities.h>

#include <cstring>
#include <string>
#include <vector>

using dqBase::Base64Utilities;

// Ported from: imodel-native Base64UtilitiesTests.cpp
//              TEST_F(Base64UtilitiesTests, EncodeEmptyString) lines 19-24
TEST(Base64UtilitiesTest, EncodeEmptyString) {
    std::string input("");
    ASSERT_TRUE(Base64Utilities::Encode(input).empty());
    ASSERT_TRUE(Base64Utilities::Encode(input.c_str(), input.size()).empty());
}

// Ported from: imodel-native Base64UtilitiesTests.cpp
//              TEST_F(Base64UtilitiesTests, EncodeEmptyBlob) lines 29-35
TEST(Base64UtilitiesTest, EncodeEmptyBlob) {
    uint8_t const* blob = nullptr;
    std::string encoded;
    Base64Utilities::Encode(encoded, blob, 0);
    ASSERT_TRUE(encoded.empty());
}

// Ported from: imodel-native Base64UtilitiesTests.cpp
//              TEST_F(Base64UtilitiesTests, EncodeString) lines 40-54
TEST(Base64UtilitiesTest, EncodeString) {
    std::string input("Foo123!");
    const char* expected = "Rm9vMTIzIQ==";
    ASSERT_STREQ(expected, Base64Utilities::Encode(input).c_str());
    ASSERT_STREQ(expected, Base64Utilities::Encode(input.c_str(), input.size()).c_str());

    std::string encoded;
    Base64Utilities::Encode(encoded, reinterpret_cast<uint8_t const*>(input.data()), input.size());
    ASSERT_STREQ(expected, encoded.c_str());

    // encode empty
    std::string inputEmpty("");
    ASSERT_EQ(0u, Base64Utilities::Encode(inputEmpty).size());
}

// Ported from: imodel-native Base64UtilitiesTests.cpp
//              TEST_F(Base64UtilitiesTests, DecodeEmptyString) lines 59-70
TEST(Base64UtilitiesTest, DecodeEmptyString) {
    std::string encoded("");
    ASSERT_TRUE(Base64Utilities::Decode(encoded).empty());
    ASSERT_TRUE(Base64Utilities::Decode(nullptr, 0).empty());

    std::vector<uint8_t> blob;
    Base64Utilities::Decode(blob, encoded);
    ASSERT_TRUE(blob.empty());
    Base64Utilities::Decode(blob, nullptr, 0);
    ASSERT_TRUE(blob.empty());
}

// Ported from: imodel-native Base64UtilitiesTests.cpp
//              TEST_F(Base64UtilitiesTests, Decode) lines 75-89
TEST(Base64UtilitiesTest, Decode) {
    std::string expectedStr("Foo123!");
    std::string encoded("Rm9vMTIzIQ==");
    ASSERT_STREQ(expectedStr.c_str(), Base64Utilities::Decode(encoded).c_str());
    ASSERT_STREQ(expectedStr.c_str(), Base64Utilities::Decode(encoded.c_str(), encoded.size()).c_str());

    std::vector<uint8_t> byteArray;
    Base64Utilities::Decode(byteArray, encoded);
    ASSERT_EQ(expectedStr.size(), byteArray.size());
    for (size_t i = 0; i < expectedStr.size(); i++) {
        ASSERT_EQ(expectedStr[i], static_cast<char>(byteArray[i]));
    }
}

// Ported from: imodel-native Base64UtilitiesTests.cpp
//              TEST_F(Base64UtilitiesTests, EncodeDecodeString) lines 95-105
TEST(Base64UtilitiesTest, EncodeDecodeString) {
    std::string expectedString("Foo123!");

    std::string encoded = Base64Utilities::Encode(expectedString);
    ASSERT_STREQ("Rm9vMTIzIQ==", encoded.c_str());

    std::string actualString = Base64Utilities::Decode(encoded);

    ASSERT_STREQ(expectedString.c_str(), actualString.c_str());
}

// Ported from: imodel-native Base64UtilitiesTests.cpp
//              TEST_F(Base64UtilitiesTests, EncodeDecodeBlob) lines 110-128
TEST(Base64UtilitiesTest, EncodeDecodeBlob) {
    const int64_t expectedNumber = INT64_C(1234567890);
    uint8_t const* expectedBlob = reinterpret_cast<uint8_t const*>(&expectedNumber);
    const size_t expectedBlobSize = sizeof(expectedNumber);

    std::string encoded;
    Base64Utilities::Encode(encoded, expectedBlob, expectedBlobSize);

    ASSERT_STREQ("0gKWSQAAAAA=", encoded.c_str());

    std::vector<uint8_t> decodedBlob;
    Base64Utilities::Decode(decodedBlob, encoded);
    ASSERT_EQ(expectedBlobSize, decodedBlob.size());

    int64_t actualNumber = INT64_C(-1);
    memcpy(&actualNumber, decodedBlob.data(), sizeof(actualNumber));
    ASSERT_EQ(expectedNumber, actualNumber);
}

// Ported from: imodel-native Base64UtilitiesTests.cpp
//              TEST_F(Base64UtilitiesTests, EncodeDecodeByteStream) lines 133-156
//
// Deviation: ref uses ByteStream& dest; DanQing substitutes DqVector<uint8_t>&
// (DqByteStream is a §6 deferred item). Behaviour is equivalent: decoded bytes
// are appended into the destination container.
TEST(Base64UtilitiesTest, EncodeDecodeByteStream) {
    std::string expectedString("Foo123!");

    std::string encoded = Base64Utilities::Encode(expectedString);
    ASSERT_STREQ("Rm9vMTIzIQ==", encoded.c_str());

    std::vector<uint8_t> decodedStream;
    Base64Utilities::Decode(decodedStream, encoded.c_str(), encoded.size());
    std::string decodedString(reinterpret_cast<char const*>(decodedStream.data()), expectedString.size());
    ASSERT_STREQ(expectedString.c_str(), decodedString.c_str());

    // Decode empty
    std::vector<uint8_t> decodedEmpty;
    Base64Utilities::Decode(decodedEmpty, nullptr, 0);
    EXPECT_TRUE(0 == decodedEmpty.size());

    // Decode partial — decoder stops at first non-base64 char ('G' of "Garbage"
    // is actually base64, so the bound is the explicit length 12 = 4*3).
    std::vector<uint8_t> decodedPartial;
    Base64Utilities::Decode(decodedPartial, "Rm9vMTIzIQ==Garbage", 12);
    std::string decodedPartialString(reinterpret_cast<char const*>(decodedPartial.data()), expectedString.size());
    ASSERT_STREQ(expectedString.c_str(), decodedPartialString.c_str());
}

// Ported from: imodel-native Base64UtilitiesTests.cpp
//              TEST_F(Base64UtilitiesTests, AlphabetsMatching) lines 161-173
TEST(Base64UtilitiesTest, AlphabetsMatching) {
    const char* charStr = "";
    ASSERT_TRUE(1 == Base64Utilities::MatchesAlphabet(charStr));
    charStr = "FardhaBakKir";
    ASSERT_TRUE(1 == Base64Utilities::MatchesAlphabet(charStr));
    charStr = "945/56*a";
    ASSERT_TRUE(0 == Base64Utilities::MatchesAlphabet(charStr));
    charStr = "x+y=2a/z";
    ASSERT_TRUE(1 == Base64Utilities::MatchesAlphabet(charStr));
    std::string baseString = Base64Utilities::Alphabet();
    ASSERT_TRUE(1 == Base64Utilities::MatchesAlphabet(baseString.c_str()));
}

// Authored: no direct reference test for the header param (Encode overloads in
// Base64Utilities.h:24-26 carry `Utf8CP header = nullptr`). The header is
// prepended verbatim to the encoded output — verified against the reference
// implementation in Base64Utilities.cpp:61-62.
TEST(Base64UtilitiesTest, EncodeWithHeaderPrependsPrefix) {
    std::string input("Foo123!");
    const char* header = "data:image/png;base64,";
    std::string expected = std::string(header) + "Rm9vMTIzIQ==";

    // Encode(Utf8StringCR, header)
    ASSERT_STREQ(expected.c_str(), Base64Utilities::Encode(input, header).c_str());
    // Encode(Utf8CP, size_t, header)
    ASSERT_STREQ(expected.c_str(), Base64Utilities::Encode(input.c_str(), input.size(), header).c_str());
    // Encode(out-param) with header
    std::string encoded;
    Base64Utilities::Encode(encoded, reinterpret_cast<uint8_t const*>(input.data()), input.size(), header);
    ASSERT_STREQ(expected.c_str(), encoded.c_str());

    // Without header — must remain canonical (no prefix).
    ASSERT_STREQ("Rm9vMTIzIQ==", Base64Utilities::Encode(input).c_str());
}

// Authored: explicit Alphabet() content check. Reference returns the canonical
// base64 alphabet (Base64Utilities.cpp:16-18).
TEST(Base64UtilitiesTest, AlphabetReturnsCanonicalBase64Alphabet) {
    std::string a = Base64Utilities::Alphabet();
    ASSERT_EQ(std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"), a);
    ASSERT_EQ(64u, a.size());
}
