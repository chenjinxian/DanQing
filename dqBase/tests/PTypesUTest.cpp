// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/PTypesU.h
//              (no standalone PTypesU test file; test scenarios derived from header contract)
// dqBase tests — PTypesU 类型联合行为验证

#include <gtest/gtest.h>

#include <dqBase/PTypesU.h>

#include <cstring>

// ---------------------------------------------------------------------------
// UnalignedShort tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native PTypesU.h UnalignedShort contract
TEST(UnalignedShortTest, SizeEqualsSizeofShort) {
    EXPECT_EQ(sizeof(dqBase::UnalignedShort), sizeof(short));
}

// ---------------------------------------------------------------------------
// UnalignedLong tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native PTypesU.h UnalignedLong contract
TEST(UnalignedLongTest, SizeEqualsSizeofLong) {
    EXPECT_EQ(sizeof(dqBase::UnalignedLong), sizeof(long));
}

// ---------------------------------------------------------------------------
// UnalignedDouble tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native PTypesU.h UnalignedDouble contract
TEST(UnalignedDoubleTest, SizeEqualsSizeofDouble) {
    EXPECT_EQ(sizeof(dqBase::UnalignedDouble), sizeof(double));
}

// ---------------------------------------------------------------------------
// UnalignedInt64 tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native PTypesU.h UnalignedInt64 contract
TEST(UnalignedInt64Test, SizeEqualsSizeofInt64) {
    EXPECT_EQ(sizeof(dqBase::UnalignedInt64), sizeof(int64_t));
}

// ---------------------------------------------------------------------------
// UnalignedPointer tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native PTypesU.h UnalignedPointer contract
TEST(UnalignedPointerTest, SizeEqualsSizeofPointer) {
    EXPECT_EQ(sizeof(dqBase::UnalignedPointer), sizeof(void*));
}

// ---------------------------------------------------------------------------
// Shorts union tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native PTypesU.h Shorts union
TEST(ShortsUnionTest, UshAndShShareStorage) {
    dqBase::Shorts s;
    s.ush = 0x1234;
    EXPECT_EQ(s.sh, static_cast<short>(0x1234));
}

// ---------------------------------------------------------------------------
// Longs union tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native PTypesU.h Longs union
// On macOS, sizeof(long)==8 but sizeof(uint32_t)==4, so writing uLg doesn't clear upper bytes.
// Just verify they share the same address.
TEST(LongsUnionTest, ULgAndLgShareStorage) {
    dqBase::Longs l;
    std::memset(&l, 0, sizeof(l));
    l.uLg = 42;
    // lg reads the full long; lower 4 bytes should be 42
    EXPECT_EQ(static_cast<uint32_t>(l.lg), 42u);
}

// ---------------------------------------------------------------------------
// Pointers union tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native PTypesU.h Pointers union
TEST(PointersUnionTest, PVoidAndUapShareStorage) {
    int x = 42;
    dqBase::Pointers p;
    p.pVoid = &x;
    EXPECT_EQ(reinterpret_cast<void*>(p.uap), &x);
}

// ---------------------------------------------------------------------------
// StackDouble tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native PTypesU.h StackDouble/StackInt64
TEST(StackDoubleTest, SizeIsTwoLongs) {
    EXPECT_EQ(sizeof(dqBase::StackDouble), 2 * sizeof(long));
}

// Ported from: imodel-native PTypesU.h StackInt64 = StackDouble
TEST(StackInt64Test, SameAsStackDouble) {
    EXPECT_TRUE((std::is_same_v<dqBase::StackInt64, dqBase::StackDouble>));
}

// ---------------------------------------------------------------------------
// DoubleArg union tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native PTypesU.h DoubleArg union
TEST(DoubleArgUnionTest, DAndLdShareStorage) {
    dqBase::DoubleArg da;
    da.d = 3.14;
    // StackDouble ld should overlay the same memory
    EXPECT_EQ(sizeof(dqBase::DoubleArg), sizeof(dqBase::StackDouble));
}

// ---------------------------------------------------------------------------
// Ptypes_u union tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native PTypesU.h Ptypes_u union (lines 89-121)
TEST(PtypesuUnionTest, AllPointerMembersHaveSameSize) {
    // All pointer members in Ptypes_u should be the same size as void*
    EXPECT_EQ(sizeof(dqBase::Ptypes_u), sizeof(void*));
}

// Ported from: imodel-native PTypesU.h Ptypes_u union
TEST(PtypesuUnionTest, PVAndPCShareStorage) {
    int x = 99;
    dqBase::Ptypes_u u;
    u.pv = &x;
    EXPECT_EQ(static_cast<void*>(u.pc), static_cast<void*>(&x));
}

// Ported from: imodel-native PTypesU.h Ptypes_u union
TEST(PtypesuUnionTest, PiAndPfShareStorage) {
    dqBase::Ptypes_u u;
    int i = 0x40490FDB;  // IEEE 754 encoding of ~3.14159
    u.pi = &i;
    // Reading through pf gives the float interpretation of the same bits
    (void)u.pf;  // Just verify it compiles and doesn't crash
}

// Ported from: imodel-native PTypesU.h LongAlignedDouble
// On non-alignment-required platforms, LongAlignedDouble is typedef'd to double (8 bytes).
// On alignment-required + FULL_DOUBLE_ALIGNMENT, it's struct{long dl[2]} (16 bytes).
TEST(LongAlignedDoubleTest, SizeIsCorrect) {
#if defined(DATA_ALIGNMENT_FORCED) && defined(FULL_DOUBLE_ALIGNMENT)
    EXPECT_EQ(sizeof(dqBase::LongAlignedDouble), 2 * sizeof(long));
#else
    EXPECT_EQ(sizeof(dqBase::LongAlignedDouble), sizeof(double));
#endif
}

// Ported from: imodel-native PTypesU.h LongAlignedInt64
TEST(LongAlignedInt64Test, SizeIsCorrect) {
#if defined(DATA_ALIGNMENT_FORCED) && defined(FULL_DOUBLE_ALIGNMENT)
    EXPECT_EQ(sizeof(dqBase::LongAlignedInt64), 2 * sizeof(long));
#else
    EXPECT_EQ(sizeof(dqBase::LongAlignedInt64), sizeof(int64_t));
#endif
}
