// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Base64Utilities.h
//              imodel-native iModelCore/Bentley/Bentley/nonport/Base64Utilities.cpp
// DanQing dqBase — Base64 编解码实现
//
// Algorithm, lookup tables, and control flow are ported verbatim from the
// reference (CLAUDE.md §3). The decode-index lookup is O(1) by ASCII code; the
// static_assert mirrors the reference assertion at Base64Utilities.cpp:36.
#include "dqBase/Base64Utilities.h"

#include <cstring>

BEGIN_DQ_BASE_NAMESPACE

// Ported from: Base64Utilities.cpp:16-18
static const Utf8String s_base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

// Ported from: Base64Utilities.cpp:23-34
// base64 char → blob char lookup. For a given base64 char, its ASCII code is
// the index; the value at that index is the 6-bit blob value. O(1) vs O(n)
// find() on s_base64_chars.
static const Byte s_base64_decodeindex[] = {
    // ASCII code of base64 char:    0 1 2                                                    41 42
    (Byte) -1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // 43 44 45 46  47  48                                  57 58                64
    // +           /   0   1   2 ...                       9
      62, 0, 0, 0, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0, 0,
    // 65 66                                                                                     90 91             96
    // A  B  ...                                                                                 Z
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 0, 0,
    // 97  98                                                                                             122
    // a   b  ...                                                                                          z
      26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

static_assert(sizeof(s_base64_decodeindex) / sizeof(Byte) == 123,
              "base64decodelookup is expected to have (int) 'z' + 1 elements");

// Ported from: Base64Utilities.cpp:41
static bool is_base64(Byte c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

// Ported from: Base64Utilities.cpp:46-51
Utf8String Base64Utilities::Encode(Utf8CP bytesToEncode, size_t byteCount, Utf8CP header) {
    Utf8String encodedString;
    Encode(encodedString, reinterpret_cast<Byte const*>(bytesToEncode), byteCount, header);
    return encodedString;
}

// Ported from: Base64Utilities.cpp:56-105
void Base64Utilities::Encode(Utf8StringR encodedString, Byte const* bytesToEncode, size_t byteCount, Utf8CP header) {
    if (bytesToEncode == nullptr || byteCount == 0)
        return;

    if (header != nullptr && header[0] != '\0')
        encodedString.assign(header);

    size_t nEncodedBytes = (size_t)(4.0 * ((byteCount + 2) / 3.0)) + encodedString.size();
    encodedString.reserve(nEncodedBytes);

    Byte byte_array_3[3];
    Byte byte_array_4[4];

    int i = 0;
    int j = 0;
    while (byteCount--) {
        byte_array_3[i++] = *(bytesToEncode++);
        if (i == 3) {
            byte_array_4[0] = (byte_array_3[0] & 0xfc) >> 2;
            byte_array_4[1] = ((byte_array_3[0] & 0x03) << 4) + ((byte_array_3[1] & 0xf0) >> 4);
            byte_array_4[2] = ((byte_array_3[1] & 0x0f) << 2) + ((byte_array_3[2] & 0xc0) >> 6);
            byte_array_4[3] = byte_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                encodedString += s_base64_chars[byte_array_4[i]];

            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            byte_array_3[j] = '\0';

        byte_array_4[0] = (byte_array_3[0] & 0xfc) >> 2;
        byte_array_4[1] = ((byte_array_3[0] & 0x03) << 4) + ((byte_array_3[1] & 0xf0) >> 4);
        byte_array_4[2] = ((byte_array_3[1] & 0x0f) << 2) + ((byte_array_3[2] & 0xc0) >> 6);
        byte_array_4[3] = byte_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            encodedString += s_base64_chars[byte_array_4[j]];

        while ((i++ < 3))
            encodedString += '=';
    }
}

// Ported from: Base64Utilities.cpp:110-118
Utf8String Base64Utilities::Decode(Utf8CP encodedString, size_t encodedStringLength) {
    DqVector<Byte> byteStream;
    Decode(byteStream, encodedString, encodedStringLength);
    if (byteStream.empty())
        return Utf8String();

    return Utf8String(reinterpret_cast<Utf8CP>(byteStream.data()), byteStream.size());
}

// Ported from: Base64Utilities.cpp:123-174 (base64_decode template)
template<typename T>
static void base64_decode(T& byteArray, Utf8CP encodedString, size_t encodedStringLength) {
    if (encodedString == nullptr || encodedString[0] == '\0' || encodedStringLength == 0)
        return;

    int i = 0;
    int j = 0;
    int in_ = 0;

    Byte byte_array_4[4], byte_array_3[3];

    while (encodedStringLength-- && (encodedString[in_] != '=') && is_base64(encodedString[in_])) {
        byte_array_4[i++] = encodedString[in_];
        in_++;
        if (i != 4)
            continue;

        for (i = 0; i < 4; i++) {
            byte_array_4[i] = s_base64_decodeindex[byte_array_4[i]];
        }

        byte_array_3[0] = (byte_array_4[0] << 2) + ((byte_array_4[1] & 0x30) >> 4);
        byte_array_3[1] = ((byte_array_4[1] & 0xf) << 4) + ((byte_array_4[2] & 0x3c) >> 2);
        byte_array_3[2] = ((byte_array_4[2] & 0x3) << 6) + byte_array_4[3];

        for (i = 0; (i < 3); i++)
            byteArray.push_back(byte_array_3[i]);

        i = 0;
    }

    if (i == 0)
        return;

    for (j = i; j < 4; j++)
        byte_array_4[j] = 0;

    for (j = 0; j < 4; j++) {
        byte_array_4[j] = s_base64_decodeindex[byte_array_4[j]];
    }

    byte_array_3[0] = (byte_array_4[0] << 2) + ((byte_array_4[1] & 0x30) >> 4);
    byte_array_3[1] = ((byte_array_4[1] & 0xf) << 4) + ((byte_array_4[2] & 0x3c) >> 2);
    byte_array_3[2] = 0xFF & (((byte_array_4[2] & 0x3) << 6) + byte_array_4[3]);

    for (j = 0; (j < i - 1); j++)
        byteArray.push_back(byte_array_3[j]);
}

// Ported from: Base64Utilities.cpp:179-182 (bvector<Byte>) and :209-216
// (ByteStream). In DanQing both ref destinations collapse onto DqVector<Byte>&
// — see header note. The reference's ByteStreamAdapter grow/Reserve logic
// collapses to std::vector::push_back; identical output bytes, equivalent
// amortised O(1) append.
void Base64Utilities::Decode(DqVector<Byte>& byteArray, Utf8CP encodedString, size_t encodedStringLength) {
    base64_decode(byteArray, encodedString, encodedStringLength);
}

// Ported from: Base64Utilities.cpp:221
Utf8StringCR Base64Utilities::Alphabet() {
    return s_base64_chars;
}

// Ported from: Base64Utilities.cpp:226-236
bool Base64Utilities::MatchesAlphabet(Utf8CP input) {
    if (input == nullptr || input[0] == '\0')
        return true;

    for (Utf8CP ch = input; 0 != *ch; ++ch)
        if (!is_base64(*ch) && *ch != '=')
            return false;

    return true;
}

END_DQ_BASE_NAMESPACE
