// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/SHA1.h
// DanQing dqBase — SHA1 实现
#include "dqBase/SHA1.h"

#include <cstring>

BEGIN_DQ_BASE_NAMESPACE

namespace {
inline uint32_t rotl(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

void processBlock(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i)
        w[i] = (uint32_t(block[i*4]) << 24) | (uint32_t(block[i*4+1]) << 16) |
               (uint32_t(block[i*4+2]) << 8) | uint32_t(block[i*4+3]);
    for (int i = 16; i < 80; ++i)
        w[i] = rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6; }
        uint32_t tmp = rotl(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rotl(b, 30); b = a; a = tmp;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}
} // namespace

SHA1::SHA1() { Reset(); }

void SHA1::Reset() {
    m_state[0] = 0x67452301; m_state[1] = 0xEFCDAB89;
    m_state[2] = 0x98BADCFE; m_state[3] = 0x10325476;
    m_state[4] = 0xC3D2E1F0;
    m_count = 0;
    memset(m_buffer, 0, BlockSize);
}

void SHA1::Add(const void* data, size_t size) {
    auto* p = static_cast<const uint8_t*>(data);
    size_t bufPos = static_cast<size_t>(m_count % BlockSize);
    m_count += size;
    while (size > 0) {
        size_t copy = std::min(size, BlockSize - bufPos);
        memcpy(m_buffer + bufPos, p, copy);
        bufPos += copy;
        p += copy;
        size -= copy;
        if (bufPos == BlockSize) {
            processBlock(m_state, m_buffer);
            bufPos = 0;
        }
    }
}

SHA1::HashVal SHA1::GetHashVal() {
    // Padding
    size_t bufPos = static_cast<size_t>(m_count % BlockSize);
    m_buffer[bufPos++] = 0x80;
    if (bufPos > 56) {
        while (bufPos < BlockSize) m_buffer[bufPos++] = 0;
        processBlock(m_state, m_buffer);
        bufPos = 0;
    }
    while (bufPos < 56) m_buffer[bufPos++] = 0;
    uint64_t bits = m_count * 8;
    for (int i = 7; i >= 0; --i) m_buffer[56 + (7 - i)] = static_cast<uint8_t>(bits >> (i * 8));
    processBlock(m_state, m_buffer);

    HashVal result;
    for (int i = 0; i < 5; ++i) {
        result.bytes[i*4] = static_cast<uint8_t>(m_state[i] >> 24);
        result.bytes[i*4+1] = static_cast<uint8_t>(m_state[i] >> 16);
        result.bytes[i*4+2] = static_cast<uint8_t>(m_state[i] >> 8);
        result.bytes[i*4+3] = static_cast<uint8_t>(m_state[i]);
    }
    return result;
}

std::string SHA1::GetHashString() {
    auto h = GetHashVal();
    char buf[41];
    for (int i = 0; i < 20; ++i)
        snprintf(buf + i*2, 3, "%02x", h.bytes[i]);
    return std::string(buf, 40);
}

std::string SHA1::operator()(const void* data, size_t size) {
    Reset();
    Add(data, size);
    return GetHashString();
}

std::string SHA1::operator()(const std::string& s) {
    return operator()(s.data(), s.size());
}

END_DQ_BASE_NAMESPACE
