// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/TypedArrayBuilder.ts
// DanQing dqBase — TypedArrayBuilder 实现（UintArrayBuilder 非 inline 部分）
//
// TypedArrayBuilder<T> 完全模板化，在头文件中实现。
// UintArrayBuilder 含运行时 bytesPerElement 升级，逻辑在此文件实现。
#include "dqBase/TypedArrayBuilder.h"

#include <algorithm>
#include <cstring>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// UintArrayBuilder — 非 inline 实现
// ---------------------------------------------------------------------------

UintArrayBuilder::UintArrayBuilder(const UintArrayBuilderOptions& options)
    : m_growthFactor(options.growthFactor < 1.0 ? 1.0 : options.growthFactor)
    , m_length(0)
    , m_capacity(options.initialCapacity)
    , m_bytesPerElem(options.initialType == TypedArrayBuilderType::Uint8  ? 1
                   : options.initialType == TypedArrayBuilderType::Uint16 ? 2
                   : 4)
{
    m_bytes.resize(m_capacity * m_bytesPerElem, 0);
}

uint32_t UintArrayBuilder::at(double index) const noexcept {
    if (index < 0.0)
        index = static_cast<double>(m_length) - index;
    const size_t i = static_cast<size_t>(index);
    if (i >= m_length)
        return 0; // bounds-protected (no exceptions per §9)
    const uint8_t* p = m_bytes.data() + i * m_bytesPerElem;
    switch (m_bytesPerElem) {
        case 1:  return static_cast<uint32_t>(*p);
        case 2:  { uint16_t v = 0; std::memcpy(&v, p, 2); return static_cast<uint32_t>(v); }
        default: { uint32_t v = 0; std::memcpy(&v, p, 4); return v; }
    }
}

size_t UintArrayBuilder::ensureCapacity(size_t newCapacity) {
    if (m_capacity >= newCapacity)
        return m_capacity;
    // growthFactor >= 1.0 enforced in ctor
    double grown = static_cast<double>(newCapacity) * m_growthFactor;
    size_t whole = static_cast<size_t>(grown);
    if (static_cast<double>(whole) < grown) ++whole;
    newCapacity = whole;

    DqVector<uint8_t> newData(newCapacity * m_bytesPerElem, 0);
    if (!m_bytes.empty())
        std::memcpy(newData.data(), m_bytes.data(), m_bytes.size());
    m_bytes = std::move(newData);
    m_capacity = newCapacity;
    return m_capacity;
}

void UintArrayBuilder::upgradeTo(size_t newBytesPerElem) {
    if (newBytesPerElem <= m_bytesPerElem)
        return;
    DqVector<uint8_t> newData(m_capacity * newBytesPerElem, 0);
    // Copy existing length elements from the old (narrow) buffer to the new (wider) buffer.
    for (size_t i = 0; i < m_length; ++i) {
        uint32_t v = 0;
        const uint8_t* src = m_bytes.data() + i * m_bytesPerElem;
        switch (m_bytesPerElem) {
            case 1:  v = static_cast<uint32_t>(*src); break;
            case 2:  { uint16_t t = 0; std::memcpy(&t, src, 2); v = t; break; }
            default: { std::memcpy(&v, src, 4); break; }
        }
        uint8_t* dst = newData.data() + i * newBytesPerElem;
        switch (newBytesPerElem) {
            case 2:  { uint16_t t = static_cast<uint16_t>(v); std::memcpy(dst, &t, 2); break; }
            default: { std::memcpy(dst, &v, 4); break; }
        }
    }
    m_bytes = std::move(newData);
    m_bytesPerElem = newBytesPerElem;
}

void UintArrayBuilder::push(uint32_t value) {
    // 对齐 ref UintArrayBuilder.push → ensureBytesPerElement([value]) → super.push(value)
    if (m_bytesPerElem < 4) {
        if (value > 0xffff) {
            upgradeTo(4);
        } else if (value > 0xff && m_bytesPerElem < 2) {
            upgradeTo(2);
        }
    }
    ensureCapacity(m_length + 1);
    uint8_t* p = m_bytes.data() + m_length * m_bytesPerElem;
    switch (m_bytesPerElem) {
        case 1:  *p = static_cast<uint8_t>(value); break;
        case 2:  { uint16_t t = static_cast<uint16_t>(value); std::memcpy(p, &t, 2); break; }
        default: { std::memcpy(p, &value, 4); break; }
    }
    ++m_length;
}

void UintArrayBuilder::append(const uint8_t* data, size_t count) {
    // Ref UintArrayBuilder.append → ensureBytesPerElement(values) → super.append(values)
    if (m_bytesPerElem < 4) {
        size_t needed = computeNeededBytesPerElement(m_bytesPerElem, data, data + count);
        upgradeTo(needed);
    }
    const size_t newLength = m_length + count;
    ensureCapacity(newLength);
    // Note: source is uint8 — values in [0, 255] always fit current bpe (>= 1).
    for (size_t i = 0; i < count; ++i) {
        uint8_t* p = m_bytes.data() + (m_length + i) * m_bytesPerElem;
        const uint32_t v = static_cast<uint32_t>(data[i]);
        switch (m_bytesPerElem) {
            case 1:  *p = static_cast<uint8_t>(v); break;
            case 2:  { uint16_t t = static_cast<uint16_t>(v); std::memcpy(p, &t, 2); break; }
            default: { std::memcpy(p, &v, 4); break; }
        }
    }
    m_length = newLength;
}

void UintArrayBuilder::append(const uint16_t* data, size_t count) {
    // Values up to 0xffff — may force upgrade from 1 → 2 (or 1 → 4 if ref semantics ever extended).
    if (m_bytesPerElem < 4) {
        size_t needed = computeNeededBytesPerElement(m_bytesPerElem, data, data + count);
        upgradeTo(needed);
    }
    const size_t newLength = m_length + count;
    ensureCapacity(newLength);
    for (size_t i = 0; i < count; ++i) {
        uint8_t* p = m_bytes.data() + (m_length + i) * m_bytesPerElem;
        const uint32_t v = static_cast<uint32_t>(data[i]);
        switch (m_bytesPerElem) {
            case 1:  *p = static_cast<uint8_t>(v); break; // safe post-upgrade path
            case 2:  { uint16_t t = static_cast<uint16_t>(v); std::memcpy(p, &t, 2); break; }
            default: { std::memcpy(p, &v, 4); break; }
        }
    }
    m_length = newLength;
}

void UintArrayBuilder::append(const uint32_t* data, size_t count) {
    if (m_bytesPerElem < 4) {
        size_t needed = computeNeededBytesPerElement(m_bytesPerElem, data, data + count);
        upgradeTo(needed);
    }
    const size_t newLength = m_length + count;
    ensureCapacity(newLength);
    for (size_t i = 0; i < count; ++i) {
        uint8_t* p = m_bytes.data() + (m_length + i) * m_bytesPerElem;
        const uint32_t v = data[i];
        switch (m_bytesPerElem) {
            case 1:  *p = static_cast<uint8_t>(v); break;
            case 2:  { uint16_t t = static_cast<uint16_t>(v); std::memcpy(p, &t, 2); break; }
            default: { std::memcpy(p, &v, 4); break; }
        }
    }
    m_length = newLength;
}

END_DQ_BASE_NAMESPACE
