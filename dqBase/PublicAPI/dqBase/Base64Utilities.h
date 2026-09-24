// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Base64Utilities.h
// DanQing dqBase — Base64 编解码
//
// 1:1 对齐 imodel-native Base64Utilities.h:24-39 (full surface):
//   Encode(Utf8StringCR, header)               inline → Encode(c_str, size, header)
//   Encode(Utf8CP, size_t, header)             core
//   Encode(Utf8StringR, Byte const*, size_t, header)  core (out-param)
//   Decode(bvector<Byte>&, Utf8StringCR)       inline → Decode(c_str, size)
//   Decode(bvector<Byte>&, Utf8CP, size_t)     core
//   Decode(Utf8StringCR)                       inline → Decode(c_str, size)
//   Decode(Utf8CP, size_t)                     core
//   Decode(ByteStream&, Utf8StringCR)          inline → Decode(c_str, size)
//   Decode(ByteStream&, Utf8CP, size_t)        core
//   Alphabet()                                 canonical base64 alphabet
//   MatchesAlphabet(Utf8CP)
//
// Deviation (documented per CLAUDE.md §6): the reference's Decode(ByteStream&, …)
// overloads are emitted here as Decode(DqVector<uint8_t>&, …) — i.e. the
// bvector<Byte> shape already on the other Decode overloads. DqByteStream is a
// §6 deferred item (audit row 20/54); collapsing the two ref overloads into the
// single DqVector<uint8_t>& shape preserves the full input/output semantics
// (decoded bytes appended into a caller-supplied buffer) with zero behaviour
// drift. The two distinct ref entry-points are still callable because
// bvector<Byte> and ByteStream both map onto DqVector<uint8_t> at the SDK
// boundary — there is exactly one DanQing overload where the ref has two.
#pragma once

#include "Export.h"
#include "DqTypes.h"

#include <cstdint>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

// --- Reference-faithful type aliases (scoped to dqBase) ---------------------
// Ported from: imodel-native WString.h / bvector.h — Utf8String/bvector<Byte>.
// Local to this header so the Base64 surface reads 1:1 with the reference
// without leaking WString dependencies into the rest of dqBase.
using Byte        = uint8_t;
using Utf8CP      = const char*;
using Utf8String  = std::string;
using Utf8StringR = std::string&;
using Utf8StringCR = const std::string&;

// ---------------------------------------------------------------------------
// Base64Utilities — Base64 编解码
// Ported from: imodel-native Base64Utilities.h
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT Base64Utilities {
private:
    Base64Utilities();
    ~Base64Utilities();

public:
    // --- Encode ----------------------------------------------------------
    // Ported from: Base64Utilities.h:24
    static Utf8String Encode(Utf8StringCR stringToEncode, Utf8CP header = nullptr) {
        return Encode(stringToEncode.c_str(), stringToEncode.size(), header);
    }
    // Ported from: Base64Utilities.h:25
    static Utf8String Encode(Utf8CP bytesToEncode, size_t byteCount, Utf8CP header = nullptr);
    // Ported from: Base64Utilities.h:26
    static void Encode(Utf8StringR encodedString, Byte const* bytesToEncode, size_t byteCount, Utf8CP header = nullptr);

    // --- Decode → byte buffer (bvector<Byte> / ByteStream) --------------
    // Ported from: Base64Utilities.h:28-29 AND :34-35. The reference exposes
    // two destination types — `bvector<Byte>&` and `ByteStream&` — each with a
    // Utf8StringCR inline delegate and a (Utf8CP, size_t) core overload. In
    // DanQing, Byte aliases uint8_t (see above) and DqByteStream is a §6 deferred
    // item (audit row 20/54), so both ref destinations collapse onto
    // DqVector<Byte>&. Behaviour (decoded bytes appended into dest) is
    // identical; only the entry-point count differs (2 overloads here vs 4 in
    // ref). When DqByteStream lands, the distinct ByteStream& overloads will be
    // reinstated 1:1.
    static void Decode(DqVector<Byte>& byteArray, Utf8StringCR encodedString) {
        Decode(byteArray, encodedString.c_str(), encodedString.size());
    }
    // Ported from: Base64Utilities.h:29 (and :35 ByteStream core)
    static void Decode(DqVector<Byte>& byteArray, Utf8CP encodedString, size_t encodedStringLength);

    // --- Decode → Utf8String --------------------------------------------
    // Ported from: Base64Utilities.h:31
    static Utf8String Decode(Utf8StringCR encodedString) {
        return Decode(encodedString.c_str(), encodedString.size());
    }
    // Ported from: Base64Utilities.h:32
    static Utf8String Decode(Utf8CP encodedString, size_t encodedStringLength);

    // Ported from: Base64Utilities.h:37
    static Utf8StringCR Alphabet();

    // Ported from: Base64Utilities.h:39
    static bool MatchesAlphabet(Utf8CP str);
};

END_DQ_BASE_NAMESPACE
