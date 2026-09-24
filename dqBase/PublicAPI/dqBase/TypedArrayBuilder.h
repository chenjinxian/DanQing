// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/TypedArrayBuilder.ts
// DanQing dqBase — TypedArrayBuilder<T> + Uint8/16/32ArrayBuilder + UintArrayBuilder
//
// 1:1 对齐 itwinjs-core TypedArrayBuilder。
//   - TypedArrayBuilder<T>: 泛型基类（capacity/at/ensureCapacity/toTypedArray/growthFactor）
//   - Uint8/16/32ArrayBuilder: 三种特化子类
//   - UintArrayBuilder: 按 push/append 时自动升级 bytesPerElement 的版本
//
// 命名：camelCase 方法名对齐 TS 参考与 SortedArray/OrderedSet 等同类容器。
// 容量/长度单位为元素个数（对齐 ref），byte-layout 由底层 typed buffer 体现。
#pragma once

#include "Export.h"
#include "DqTypes.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// TypedArrayBuilderOptions — 对齐 ref TypedArrayBuilderOptions
// Ported from: itwinjs-core core/bentley/src/TypedArrayBuilder.ts TypedArrayBuilderOptions
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT TypedArrayBuilderOptions {
    /// 控制扩容倍率。ensureCapacity 用 newCapacity * growthFactor 决定新容量。
    /// 默认 1.5；最小 1.0（精确分配）。
    double growthFactor = 1.5;

    /// 初始元素容量。若已知最小元素数，预分配可避免重分配。
    /// 默认 0；最小 0。
    size_t initialCapacity = 0;
};

// ---------------------------------------------------------------------------
// TypedArrayBuilder<T> — 泛型基类
// Ported from: itwinjs-core core/bentley/src/TypedArrayBuilder.ts TypedArrayBuilder<T>
//
// 模板参数 BytesPerElem = 1 / 2 / 4，对齐 ref 中 Uint8Array / Uint16Array / Uint32Array。
// 底层用 DqVector<uint8_t> 持有 byte buffer；length / capacity 均为元素个数。
// ---------------------------------------------------------------------------
template<unsigned BytesPerElem>
class TypedArrayBuilder {
    static_assert(BytesPerElem == 1 || BytesPerElem == 2 || BytesPerElem == 4,
                  "BytesPerElem must be 1, 2, or 4");
public:
    /// See [[TypedArrayBuilder]] constructor.
    explicit TypedArrayBuilder(const TypedArrayBuilderOptions& options = TypedArrayBuilderOptions{})
        : m_growthFactor(options.growthFactor < 1.0 ? 1.0 : options.growthFactor)
        , m_length(0)
        , m_capacity(options.initialCapacity) {
        m_bytes.resize(m_capacity * BytesPerElem, 0);
    }

    /// The number of elements currently in the array.
    size_t length() const noexcept { return m_length; }

    /// The number of elements that can fit into the memory currently allocated for the array.
    size_t capacity() const noexcept { return m_capacity; }

    /// Multiplier applied to required capacity by ensureCapacity.
    double growthFactor() const noexcept { return m_growthFactor; }

    /// Bytes per element of the underlying typed array (1, 2, or 4).
    static constexpr size_t bytesPerElement() noexcept { return BytesPerElem; }

    /// Like TypedArray.at — returns the element at index. Caller responsible for bounds.
    /// Ref: if (index < 0) index = this.length - index;
    uint32_t at(double index) const noexcept {
        if (index < 0.0)
            index = static_cast<double>(m_length) - index;
        const size_t i = static_cast<size_t>(index);
        return readElem(i);
    }

    /// Ensure that capacity is at least newCapacity. Returns resulting capacity.
    /// If newCapacity <= current capacity, no-op. Otherwise allocate
    /// newCapacity * growthFactor elements and copy contents.
    size_t ensureCapacity(size_t newCapacity) {
        if (m_capacity >= newCapacity)
            return m_capacity;
        // assert(growthFactor >= 1.0) enforced in ctor
        newCapacity = ceilSize(newCapacity * m_growthFactor);
        DqVector<uint8_t> newData(newCapacity * BytesPerElem, 0);
        if (!m_bytes.empty())
            std::memcpy(newData.data(), m_bytes.data(), m_bytes.size());
        m_bytes = std::move(newData);
        m_capacity = newCapacity;
        return m_capacity;
    }

    /// Append one value, resizing if necessary.
    void push(uint32_t value) {
        ensureCapacity(m_length + 1);
        writeElem(m_length, value);
        ++m_length;
    }

    /// Append an array of values (typed by bytesPerElement), resizing at most once.
    /// `data` points to `count` typed elements of BytesPerElem bytes each.
    void append(const void* data, size_t count) {
        const size_t newLength = m_length + count;
        ensureCapacity(newLength);
        std::memcpy(m_bytes.data() + m_length * BytesPerElem, data, count * BytesPerElem);
        m_length = newLength;
    }

    /// Read-only access to the underlying byte buffer (length * BytesPerElem bytes used).
    const uint8_t* data() const noexcept { return m_bytes.data(); }

protected:
    /// Reset and re-seat the underlying buffer to a different element type's bytes.
    /// Used by UintArrayBuilder upgrade: `bytes` is a byte buffer holding `elemCount`
    /// elements of `BytesPerElem` bytes each, in host byte order.
    void resetBuffer(DqVector<uint8_t> bytes, size_t elemCount, size_t newCapacity) {
        m_bytes = std::move(bytes);
        m_length = elemCount;
        m_capacity = newCapacity;
    }
    DqVector<uint8_t>& buffer() noexcept { return m_bytes; }
    const DqVector<uint8_t>& buffer() const noexcept { return m_bytes; }

private:
    static size_t ceilSize(double v) noexcept {
        size_t whole = static_cast<size_t>(v);
        if (static_cast<double>(whole) < v)
            ++whole;
        return whole;
    }

    uint32_t readElem(size_t index) const noexcept {
        const uint8_t* p = m_bytes.data() + index * BytesPerElem;
        // 三分支全覆盖（MSVC 对早返回后 fallthrough 报 C4702 误报，else-if 结构规避）
        if constexpr (BytesPerElem == 1) {
            return static_cast<uint32_t>(*p);
        } else if constexpr (BytesPerElem == 2) {
            uint16_t v = 0;
            std::memcpy(&v, p, 2);
            return static_cast<uint32_t>(v);
        } else {
            uint32_t v = 0;
            std::memcpy(&v, p, 4);
            return v;
        }
    }

    void writeElem(size_t index, uint32_t value) noexcept {
        uint8_t* p = m_bytes.data() + index * BytesPerElem;
        if constexpr (BytesPerElem == 1) {
            *p = static_cast<uint8_t>(value);
        } else if constexpr (BytesPerElem == 2) {
            uint16_t v = static_cast<uint16_t>(value);
            std::memcpy(p, &v, 2);
        } else {
            std::memcpy(p, &value, 4);
        }
    }

protected:
    double m_growthFactor;
    size_t m_length;
    size_t m_capacity;
    DqVector<uint8_t> m_bytes;
};

// ---------------------------------------------------------------------------
// Uint8ArrayBuilder — TypedArrayBuilder<1>
// Ported from: itwinjs-core TypedArrayBuilder.ts Uint8ArrayBuilder
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT Uint8ArrayBuilder : public TypedArrayBuilder<1> {
public:
    using Base = TypedArrayBuilder<1>;
    explicit Uint8ArrayBuilder(const TypedArrayBuilderOptions& options = TypedArrayBuilderOptions{})
        : Base(options) {}
};

// ---------------------------------------------------------------------------
// Uint16ArrayBuilder — TypedArrayBuilder<2>
// Ported from: itwinjs-core TypedArrayBuilder.ts Uint16ArrayBuilder
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT Uint16ArrayBuilder : public TypedArrayBuilder<2> {
public:
    using Base = TypedArrayBuilder<2>;
    explicit Uint16ArrayBuilder(const TypedArrayBuilderOptions& options = TypedArrayBuilderOptions{})
        : Base(options) {}

    /// Append a typed array of uint16 elements, resizing at most once.
    void append(const uint16_t* data, size_t count) { Base::append(data, count); }
};

// ---------------------------------------------------------------------------
// Uint32ArrayBuilder — TypedArrayBuilder<4>
// Ported from: itwinjs-core TypedArrayBuilder.ts Uint32ArrayBuilder
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT Uint32ArrayBuilder : public TypedArrayBuilder<4> {
public:
    using Base = TypedArrayBuilder<4>;
    explicit Uint32ArrayBuilder(const TypedArrayBuilderOptions& options = TypedArrayBuilderOptions{})
        : Base(options) {}

    /// Append a typed array of uint32 elements, resizing at most once.
    void append(const uint32_t* data, size_t count) { Base::append(data, count); }

    /// Obtain a view of the finished array as an array of bytes.
    /// If includeUnusedCapacity is true, returns capacity * 4 bytes (extra bytes zero-initialized);
    /// otherwise returns length * 4 bytes.
    std::vector<uint8_t> toUint8Array(bool includeUnusedCapacity = false) const {
        const size_t elems = includeUnusedCapacity ? m_capacity : m_length;
        std::vector<uint8_t> out(elems * 4, 0);
        if (!m_bytes.empty())
            std::memcpy(out.data(), m_bytes.data(), m_length * 4);
        return out;
    }
};

// ---------------------------------------------------------------------------
// TypedArrayBuilderType — 对齐 ref UintArrayBuilderOptions.initialType
// ---------------------------------------------------------------------------
enum class TypedArrayBuilderType : uint8_t {
    Uint8,
    Uint16,
    Uint32,
};

// ---------------------------------------------------------------------------
// UintArrayBuilderOptions — 对齐 ref UintArrayBuilderOptions
// Ported from: itwinjs-core core/bentley/src/TypedArrayBuilder.ts UintArrayBuilderOptions
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT UintArrayBuilderOptions {
    TypedArrayBuilderType initialType = TypedArrayBuilderType::Uint8;
    double growthFactor = 1.5;
    size_t initialCapacity = 0;

    /// Implicit up-conversion so callers can pass TypedArrayBuilderOptions
    /// and get the default Uint8 initial type (matches ref ergonomics where
    /// UintArrayBuilderOptions extends TypedArrayBuilderOptions).
    UintArrayBuilderOptions(const TypedArrayBuilderOptions& base) // NOLINT(google-explicit-constructor)
        : initialType(TypedArrayBuilderType::Uint8)
        , growthFactor(base.growthFactor)
        , initialCapacity(base.initialCapacity) {}

    UintArrayBuilderOptions() = default;
    UintArrayBuilderOptions(TypedArrayBuilderType t, double gf, size_t cap)
        : initialType(t), growthFactor(gf), initialCapacity(cap) {}
};

// ---------------------------------------------------------------------------
// UintArrayBuilder — 自动升级 bytesPerElement 的 TypedArrayBuilder
// Ported from: itwinjs-core core/bentley/src/TypedArrayBuilder.ts UintArrayBuilder
//
// 当 push/append 遇到大于当前 bytesPerElement 可容纳的值时，整体迁移到更大的类型：
//   value > 0xffff → Uint32 (4 bytes)
//   value > 0xff   → Uint16 (2 bytes)
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT UintArrayBuilder {
public:
    explicit UintArrayBuilder(const UintArrayBuilderOptions& options = UintArrayBuilderOptions{});

    /// The number of elements currently in the array.
    size_t length() const noexcept { return m_length; }

    /// The number of elements that can fit into the memory currently allocated for the array.
    size_t capacity() const noexcept { return m_capacity; }

    /// Multiplier applied to required capacity by ensureCapacity.
    double growthFactor() const noexcept { return m_growthFactor; }

    /// The number of bytes (1, 2, or 4) currently allocated per element.
    /// May change as larger values are added.
    size_t bytesPerElement() const noexcept { return m_bytesPerElem; }

    /// Read-only access to the underlying byte buffer.
    const uint8_t* data() const noexcept { return m_bytes.data(); }

    /// Like TypedArray.at — negative index counts from end (per ref semantics).
    uint32_t at(double index) const noexcept;

    /// Ensure capacity is at least newCapacity. Returns resulting capacity.
    size_t ensureCapacity(size_t newCapacity);

    /// Append one value, upgrading type if necessary and resizing if necessary.
    void push(uint32_t value);

    /// Append typed array of values (byte layout given by the pointer's type).
    /// Ref TS uses a `UintArray` argument (any of Uint8/16/32Array). DanQing exposes
    /// three typed overloads plus a generic raw-byte path.
    void append(const uint8_t*  data, size_t count);
    void append(const uint16_t* data, size_t count);
    void append(const uint32_t* data, size_t count);

private:
    /// 对齐 ref UintArrayBuilder.ensureBytesPerElement
    /// Compute the smallest bytes-per-element (1, 2, or 4) required to hold the
    /// maximum value in [first, last) without shrinking below current.
    template<typename Iter>
    static size_t computeNeededBytesPerElement(size_t curBytesPerElem, Iter first, Iter last) {
        if (curBytesPerElem >= 4)
            return 4;
        size_t needed = curBytesPerElem;
        for (Iter it = first; it != last; ++it) {
            const uint32_t v = static_cast<uint32_t>(*it);
            if (v > 0xffff) {
                needed = 4;
                break;
            } else if (v > 0xff) {
                needed = 2;
            }
        }
        return needed;
    }

    /// Upgrade underlying buffer from current bytes-per-element to newBytesPerElem.
    /// Copies existing length elements into the new wider buffer.
    void upgradeTo(size_t newBytesPerElem);

    double m_growthFactor;
    size_t m_length;
    size_t m_capacity;
    size_t m_bytesPerElem; // 1, 2, or 4
    DqVector<uint8_t> m_bytes;
};

END_DQ_BASE_NAMESPACE
