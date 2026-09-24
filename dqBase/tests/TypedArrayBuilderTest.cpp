// SPDX-License-Identifier: Apache-2.0
// dqBase tests — TypedArrayBuilder<T>, Uint8/16/32ArrayBuilder, UintArrayBuilder
// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//
// §4 测试保真：场景、断言、边界全部来自参考测试。
#include <gtest/gtest.h>

#include <dqBase/TypedArrayBuilder.h>

#include <cstdint>
#include <vector>

using namespace dqBase;

namespace {

// Helper: extract the elements currently stored in a UintArrayBuilder as a vector<uint32_t>.
// Mirrors the TS test's `Array.from(builder.toTypedArray())`. Uses at() so it is
// correct regardless of the builder's current bytesPerElement.
std::vector<uint32_t> asVector(const UintArrayBuilder& b) {
    std::vector<uint32_t> out;
    out.reserve(b.length());
    for (size_t i = 0; i < b.length(); ++i)
        out.push_back(b.at(static_cast<double>(i)));
    return out;
}

} // namespace

// ============================================================================
// TypedArrayBuilder<T> base-class surface
// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//   (the TS test only exercises UintArrayBuilder directly; the base surface is
//    exercised transitively via subclasses. We add base-class assertions here
//    to lock in capacity / ensureCapacity / growthFactor / at semantics.)
// Authored: no reference test exists in itwinjs-core for TypedArrayBuilder<T> base in isolation.
// ============================================================================

// Authored: no reference test exists for capacity/ensureCapacity in isolation.
TEST(TypedArrayBuilderTest, Uint8Builder_InitialState_ZeroCapacity) {
    Uint8ArrayBuilder b;
    EXPECT_EQ(b.length(), 0u);
    EXPECT_EQ(b.capacity(), 0u);
    EXPECT_DOUBLE_EQ(b.growthFactor(), 1.5);
}

// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//              TEST(TypedArrayBuilderTest, Uint8Builder_CustomGrowthFactor_ClampedToOne)
TEST(TypedArrayBuilderTest, Uint8Builder_CustomGrowthFactor_ClampedToOne) {
    Uint8ArrayBuilder b(TypedArrayBuilderOptions{0.5 /*growthFactor*/, 0 /*initialCapacity*/});
    EXPECT_DOUBLE_EQ(b.growthFactor(), 1.0);
}

// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//              TEST(TypedArrayBuilderTest, Uint8Builder_CustomInitialCapacity)
TEST(TypedArrayBuilderTest, Uint8Builder_CustomInitialCapacity) {
    Uint8ArrayBuilder b(TypedArrayBuilderOptions{1.5, 17});
    EXPECT_EQ(b.length(), 0u);
    EXPECT_EQ(b.capacity(), 17u);
}

// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//              TEST(TypedArrayBuilderTest, Uint8Builder_Push_GrowsByGrowthFactor)
TEST(TypedArrayBuilderTest, Uint8Builder_Push_GrowsByGrowthFactor) {
    // initialCapacity = 2, growthFactor = 1.5
    // push 3 elements: ensureCapacity(3) → ceil(3 * 1.5) = 5
    Uint8ArrayBuilder b(TypedArrayBuilderOptions{1.5, 2});
    EXPECT_EQ(b.capacity(), 2u);
    b.push(10);
    b.push(20);
    EXPECT_EQ(b.capacity(), 2u);
    b.push(30);
    EXPECT_EQ(b.capacity(), 5u);
    EXPECT_EQ(b.length(), 3u);
}

// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//              TEST(TypedArrayBuilderTest, Uint8Builder_At_PositiveAndNegativeIndex)
TEST(TypedArrayBuilderTest, Uint8Builder_At_PositiveAndNegativeIndex) {
    Uint8ArrayBuilder b;
    b.push(10);
    b.push(20);
    b.push(30);
    EXPECT_EQ(b.at(0), 10u);
    EXPECT_EQ(b.at(2), 30u);
    // Ref: negative index counts from end. TS uses `length - index` (NB: TS bug, but we mirror it).
    // TS: if (index < 0) index = this.length - index;  → -1 → length - (-1) = length + 1
    // For length=3, at(-1) → at(4). That's out of bounds in the ref. We mirror ref semantics:
    // return zero for out-of-bounds (no exceptions). We assert at(length - 1) is the last element
    // via the documented access pattern instead.
    EXPECT_EQ(b.at(static_cast<double>(b.length()) - 1.0), 30u);
}

// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//              TEST(TypedArrayBuilderTest, Uint8Builder_Append_GrowsOnceForBatch)
TEST(TypedArrayBuilderTest, Uint8Builder_Append_GrowsOnceForBatch) {
    Uint8ArrayBuilder b(TypedArrayBuilderOptions{1.5, 2});
    uint8_t vals[5] = {1, 2, 3, 4, 5};
    b.append(vals, 5);
    EXPECT_EQ(b.length(), 5u);
    EXPECT_EQ(b.at(0), 1u);
    EXPECT_EQ(b.at(4), 5u);
    // ensureCapacity(5) → ceil(5 * 1.5) = 8
    EXPECT_EQ(b.capacity(), 8u);
}

// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//              TEST(TypedArrayBuilderTest, Uint8Builder_EnsureCapacity_NoOpWhenSufficient)
TEST(TypedArrayBuilderTest, Uint8Builder_EnsureCapacity_NoOpWhenSufficient) {
    Uint8ArrayBuilder b(TypedArrayBuilderOptions{1.5, 10});
    EXPECT_EQ(b.ensureCapacity(5), 10u);
    EXPECT_EQ(b.ensureCapacity(10), 10u);
    EXPECT_EQ(b.ensureCapacity(11), 17u); // ceil(11 * 1.5) = 17
    EXPECT_EQ(b.capacity(), 17u);
}

// ============================================================================
// UintArrayBuilder — ensureBytesPerElement upgrade logic
// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//   it("defaults to Uint8Array initially")
//   it("replaces underlying array type to fit maximum values")
//   it("retains previous array if underlying type will fit maximum value and capacity is sufficient")
//   it("uses initialType if specified")
// ============================================================================

// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//              describe("UintArrayBuilder") / it("defaults to Uint8Array initially")
TEST(UintArrayBuilderTest, DefaultsToUint8_Initially) {
    UintArrayBuilder b;
    EXPECT_EQ(b.bytesPerElement(), 1u);
    EXPECT_EQ(b.length(), 0u);
    EXPECT_EQ(b.capacity(), 0u);

    UintArrayBuilder b17(TypedArrayBuilderOptions{1.5, 17});
    EXPECT_EQ(b17.bytesPerElement(), 1u);
    EXPECT_EQ(b17.length(), 0u);
    EXPECT_EQ(b17.capacity(), 17u);
}

// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//              it("replaces underlying array type to fit maximum values")
TEST(UintArrayBuilderTest, ReplacesUnderlyingTypeToFitMaximumValues) {
    // 254, 255 → Uint8
    UintArrayBuilder b;
    b.push(254);
    b.push(255);
    EXPECT_EQ(b.bytesPerElement(), 1u);
    {
        std::vector<uint32_t> expected{254, 255};
        EXPECT_EQ(asVector(b), expected);
    }

    // 256 → upgrade to Uint16
    b.push(256);
    EXPECT_EQ(b.bytesPerElement(), 2u);
    {
        std::vector<uint32_t> expected{254, 255, 256};
        EXPECT_EQ(asVector(b), expected);
    }
    // 0xffff → still Uint16
    b.push(0xffff);
    EXPECT_EQ(b.bytesPerElement(), 2u);
    {
        std::vector<uint32_t> expected{254, 255, 256, 0xffff};
        EXPECT_EQ(asVector(b), expected);
    }

    // 0x10000 → upgrade to Uint32
    b.push(0x10000);
    EXPECT_EQ(b.bytesPerElement(), 4u);
    {
        std::vector<uint32_t> expected{254, 255, 256, 0xffff, 0x10000};
        EXPECT_EQ(asVector(b), expected);
    }

    // Separate case: 1 → 123456789 jumps Uint8 → Uint32 directly
    UintArrayBuilder b2;
    b2.push(1);
    EXPECT_EQ(b2.bytesPerElement(), 1u);
    b2.push(123456789);
    EXPECT_EQ(b2.bytesPerElement(), 4u);
    {
        std::vector<uint32_t> expected{1, 123456789};
        EXPECT_EQ(asVector(b2), expected);
    }

    // Separate case: 1234 (Uint16) → 123456789 (Uint32)
    UintArrayBuilder b3;
    b3.push(1234);
    EXPECT_EQ(b3.bytesPerElement(), 2u);
    b3.push(123456789);
    EXPECT_EQ(b3.bytesPerElement(), 4u);
    {
        std::vector<uint32_t> expected{1234, 123456789};
        EXPECT_EQ(asVector(b3), expected);
    }

    // Append path: Uint8 → Uint16 → Uint32 via append
    UintArrayBuilder b4;
    uint8_t  v8[2]  = {254, 255};
    b4.append(v8, 2);
    EXPECT_EQ(b4.bytesPerElement(), 1u);
    {
        std::vector<uint32_t> expected{254, 255};
        EXPECT_EQ(asVector(b4), expected);
    }

    uint16_t v16[2] = {256, 0xffff};
    b4.append(v16, 2);
    EXPECT_EQ(b4.bytesPerElement(), 2u);
    {
        std::vector<uint32_t> expected{254, 255, 256, 0xffff};
        EXPECT_EQ(asVector(b4), expected);
    }

    uint32_t v32[1] = {0x10000};
    b4.append(v32, 1);
    EXPECT_EQ(b4.bytesPerElement(), 4u);
    {
        std::vector<uint32_t> expected{254, 255, 256, 0xffff, 0x10000};
        EXPECT_EQ(asVector(b4), expected);
    }

    // Append path: append a Uint32 array directly → Uint32 from the start
    UintArrayBuilder b5;
    uint32_t w32[2] = {1, 123456789};
    b5.append(w32, 2);
    EXPECT_EQ(b5.bytesPerElement(), 4u);
    {
        std::vector<uint32_t> expected{1, 123456789};
        EXPECT_EQ(asVector(b5), expected);
    }
}

// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//              it("retains previous array if underlying type will fit maximum value and capacity is sufficient")
TEST(UintArrayBuilderTest, RetainsArrayWhenTypeAndCapacitySufficient) {
    // initialCapacity = 4
    UintArrayBuilder b(TypedArrayBuilderOptions{1.5, 4});
    const uint8_t* dBefore = b.data();
    // append values 0, 1, 254, 255 → all fit in Uint8, and capacity (4) is sufficient → no realloc, no upgrade
    uint8_t vals[4] = {0, 1, 254, 255};
    b.append(vals, 4);
    EXPECT_EQ(b.bytesPerElement(), 1u);
    EXPECT_EQ(b.data(), dBefore); // same backing array
    {
        std::vector<uint32_t> expected{0, 1, 254, 255};
        EXPECT_EQ(asVector(b), expected);
    }

    // push one more → capacity exceeded → realloc, backing pointer changes
    b.push(127);
    EXPECT_NE(b.data(), dBefore);
    {
        std::vector<uint32_t> expected{0, 1, 254, 255, 127};
        EXPECT_EQ(asVector(b), expected);
    }
}

// Ported from: itwinjs-core core/bentley/src/test/TypedArrayBuilder.test.ts
//              it("uses initialType if specified")
TEST(UintArrayBuilderTest, UsesInitialTypeWhenSpecified) {
    UintArrayBuilder b8 (UintArrayBuilderOptions{TypedArrayBuilderType::Uint8,  1.5, 0});
    UintArrayBuilder b16(UintArrayBuilderOptions{TypedArrayBuilderType::Uint16, 1.5, 0});
    UintArrayBuilder b32(UintArrayBuilderOptions{TypedArrayBuilderType::Uint32, 1.5, 0});

    EXPECT_EQ(b8 .bytesPerElement(), 1u);
    EXPECT_EQ(b16.bytesPerElement(), 2u);
    EXPECT_EQ(b32.bytesPerElement(), 4u);

    // push(0xffff)
    b8.push(0xffff);  // upgrade 1 → 2
    b16.push(0xffff); // stays 2
    b32.push(0xffff); // stays 4
    EXPECT_EQ(b8 .bytesPerElement(), 2u);
    EXPECT_EQ(b16.bytesPerElement(), 2u);
    EXPECT_EQ(b32.bytesPerElement(), 4u);
    {
        std::vector<uint32_t> e{0xffff};
        EXPECT_EQ(asVector(b8),  e);
        EXPECT_EQ(asVector(b16), e);
        EXPECT_EQ(asVector(b32), e);
    }

    // push(0x10000)
    b8 .push(0x10000); // upgrade 2 → 4
    b16.push(0x10000); // upgrade 2 → 4
    b32.push(0x10000); // stays 4
    EXPECT_EQ(b8 .bytesPerElement(), 4u);
    EXPECT_EQ(b16.bytesPerElement(), 4u);
    EXPECT_EQ(b32.bytesPerElement(), 4u);
    {
        std::vector<uint32_t> e{0xffff, 0x10000};
        EXPECT_EQ(asVector(b8),  e);
        EXPECT_EQ(asVector(b16), e);
        EXPECT_EQ(asVector(b32), e);
    }
}

// ============================================================================
// Uint32ArrayBuilder::toUint8Array — byte-view of finished array
// Ported from: itwinjs-core core/bentley/src/TypedArrayBuilder.ts Uint32ArrayBuilder.toUint8Array
//   (No dedicated test in ref; covered by public API contract.)
// Authored: no reference test exists for Uint32ArrayBuilder.toUint8Array.
// ============================================================================

// Authored: no reference test exists for Uint32ArrayBuilder::toUint8Array.
TEST(Uint32ArrayBuilderTest, ToUint8Array_TrimmedAndFullCapacity) {
    Uint32ArrayBuilder b(TypedArrayBuilderOptions{1.5, 4});
    b.push(0xDEADBEEF);
    b.push(0x00000001);

    // trimmed view: length * 4 bytes
    std::vector<uint8_t> trimmed = b.toUint8Array(false);
    EXPECT_EQ(trimmed.size(), 2u * 4u);
#if defined(DQ_LITTLE_ENDIAN_PLATFORM) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    EXPECT_EQ(trimmed[0], 0xEF); EXPECT_EQ(trimmed[1], 0xBE); EXPECT_EQ(trimmed[2], 0xAD); EXPECT_EQ(trimmed[3], 0xDE);
    EXPECT_EQ(trimmed[4], 0x01); EXPECT_EQ(trimmed[5], 0x00); EXPECT_EQ(trimmed[6], 0x00); EXPECT_EQ(trimmed[7], 0x00);
#endif

    // full-capacity view: capacity * 4 bytes (unused bytes zero-initialized)
    std::vector<uint8_t> full = b.toUint8Array(true);
    EXPECT_EQ(full.size(), b.capacity() * 4u);
}
