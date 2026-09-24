// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/md5.h
// DanQing dqBase — MD5 实现
#include "dqBase/MD5.h"

#include <cstring>

BEGIN_DQ_BASE_NAMESPACE

namespace {
inline uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
inline uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
inline uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
inline uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }
inline uint32_t rotl(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

const uint32_t T[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};

const int S[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5, 9,14,20,5, 9,14,20,5, 9,14,20,5, 9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};

void processBlock(uint32_t state[4], const uint8_t block[64]) {
    uint32_t M[16];
    for (int i = 0; i < 16; ++i)
        M[i] = uint32_t(block[i*4]) | (uint32_t(block[i*4+1]) << 8) |
               (uint32_t(block[i*4+2]) << 16) | (uint32_t(block[i*4+3]) << 24);
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    for (int i = 0; i < 64; ++i) {
        uint32_t f;
        int g;
        if (i < 16) { f = F(b,c,d); g = i; }
        else if (i < 32) { f = G(b,c,d); g = (5*i+1)%16; }
        else if (i < 48) { f = H(b,c,d); g = (3*i+5)%16; }
        else { f = I(b,c,d); g = (7*i)%16; }
        uint32_t tmp = d;
        d = c; c = b;
        b = b + rotl(a + f + T[i] + M[g], S[i]);
        a = tmp;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}
} // namespace

MD5::MD5() { Reset(); }

void MD5::Reset() {
    m_state[0] = 0x67452301; m_state[1] = 0xefcdab89;
    m_state[2] = 0x98badcfe; m_state[3] = 0x10325476;
    m_count = 0;
    memset(m_buffer, 0, BlockSize);
}

void MD5::Add(const void* data, size_t size) {
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

MD5::HashVal MD5::GetHashVal() {
    size_t bufPos = static_cast<size_t>(m_count % BlockSize);
    m_buffer[bufPos++] = 0x80;
    if (bufPos > 56) {
        while (bufPos < BlockSize) m_buffer[bufPos++] = 0;
        processBlock(m_state, m_buffer);
        bufPos = 0;
    }
    while (bufPos < 56) m_buffer[bufPos++] = 0;
    uint64_t bits = m_count * 8;
    memcpy(m_buffer + 56, &bits, 8);
    processBlock(m_state, m_buffer);

    HashVal result;
    memcpy(result.bytes, m_state, 16);
    return result;
}

std::string MD5::GetHashString() {
    auto h = GetHashVal();
    char buf[33];
    for (int i = 0; i < 16; ++i)
        snprintf(buf + i*2, 3, "%02x", h.bytes[i]);
    return std::string(buf, 32);
}

std::string MD5::operator()(const void* data, size_t size) {
    Reset();
    Add(data, size);
    return GetHashString();
}

std::string MD5::operator()(const std::string& s) {
    return operator()(s.data(), s.size());
}

END_DQ_BASE_NAMESPACE
