// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/ByteStream.ts
// DanQing dqBase — 顺序字节流（读写统一，std::vector<uint8_t> 底层）
//
// 对应 imodel ByteStream / itwinjs ByteStream。用于 GeometryStream 等紧凑自描述格式。
// 替代 Qt QByteArray。底层用 std::vector<uint8_t>。
#pragma once

#include "DqBase.h"
#include "DqTypes.h"

#include <cstdint>
#include <cstring>
#include <vector>

BEGIN_DQ_BASE_NAMESPACE

class DQ_BASE_EXPORT DqByteStream {
public:
    DqByteStream() = default;
    explicit DqByteStream(DqVector<uint8_t> data)
            : m_data(std::move(data)) {}

    // 用既有字节构造一个定位在 0 的（主读）流
    static DqByteStream FromReadable(DqVector<uint8_t> data) {
        DqByteStream s;
        s.m_data = std::move(data);
        s.m_pos = 0;
        return s;
    }

    // --- 写（追加，推进 Position）---
    void WriteBytes(const uint8_t* data, size_t size) {
        m_data.insert(m_data.end(), data, data + size);
        m_pos = m_data.size();
    }
    void WriteUint8(uint8_t v) {
        WriteBytes(&v, 1);
    }
    void WriteUint16(uint16_t v) {
        WriteBytes(reinterpret_cast<uint8_t*>(&v), 2);
    }
    void WriteUint32(uint32_t v) {
        WriteBytes(reinterpret_cast<uint8_t*>(&v), 4);
    }
    void WriteUint64(uint64_t v) {
        WriteBytes(reinterpret_cast<uint8_t*>(&v), 8);
    }
    void WriteFloat(float v) {
        WriteBytes(reinterpret_cast<uint8_t*>(&v), 4);
    }
    void WriteDouble(double v) {
        WriteBytes(reinterpret_cast<uint8_t*>(&v), 8);
    }

    // --- 读（推进 Position）---
    bool ReadBytes(uint8_t* out, size_t size) {
        if (m_pos + size > m_data.size())
            return false;
        std::memcpy(out, m_data.data() + m_pos, size);
        m_pos += size;
        return true;
    }
    uint8_t ReadUint8() {
        uint8_t v{};
        ReadBytes(&v, 1);
        return v;
    }
    uint16_t ReadUint16() {
        uint16_t v{};
        ReadBytes(reinterpret_cast<uint8_t*>(&v), 2);
        return v;
    }
    uint32_t ReadUint32() {
        uint32_t v{};
        ReadBytes(reinterpret_cast<uint8_t*>(&v), 4);
        return v;
    }
    uint64_t ReadUint64() {
        uint64_t v{};
        ReadBytes(reinterpret_cast<uint8_t*>(&v), 8);
        return v;
    }
    float ReadFloat() {
        float v{};
        ReadBytes(reinterpret_cast<uint8_t*>(&v), 4);
        return v;
    }
    double ReadDouble() {
        double v{};
        ReadBytes(reinterpret_cast<uint8_t*>(&v), 8);
        return v;
    }

    // --- 游标 ---
    size_t Position() const noexcept {
        return m_pos;
    }
    size_t Size() const noexcept {
        return m_data.size();
    }
    bool AtEnd() const noexcept {
        return m_pos >= m_data.size();
    }
    void Seek(size_t pos) {
        m_pos = pos;
    }

    DqVector<uint8_t> ToByteArray() const {
        return m_data;
    }

private:
    DqVector<uint8_t> m_data;
    size_t m_pos = 0;
};

END_DQ_BASE_NAMESPACE
