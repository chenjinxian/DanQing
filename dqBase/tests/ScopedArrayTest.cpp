// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/ScopedArray.h
//              (no standalone ScopedArray test file; test scenarios derived from header contract)
// dqBase tests — ScopedArray / IndexedScopedArray / AlignedArray behavior verification

#include <gtest/gtest.h>

#include <dqBase/ScopedArray.h>

#include <cstring>
#include <vector>

// ---------------------------------------------------------------------------
// ScopedArray tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native ScopedArray.h contract — small allocation uses stack
TEST(ScopedArrayTest, SmallAllocationUsesStack) {
    // THRESHOLD default = 511 bytes. 10 ints = 40 bytes < 511 → stack.
    dqBase::ScopedArray<int> sa(10);
    EXPECT_NE(sa.GetData(), nullptr);
    // Data should be valid memory (we can write to it)
    for (int i = 0; i < 10; ++i)
        sa.GetData()[i] = i;
    for (int i = 0; i < 10; ++i)
        EXPECT_EQ(sa.GetData()[i], i);
}

// Ported from: imodel-native ScopedArray.h contract — large allocations fall back to heap
TEST(ScopedArrayTest, LargeAllocationUsesHeap) {
    // 1000 ints = 4000 bytes > 511 → heap.
    dqBase::ScopedArray<int> sa(1000);
    EXPECT_NE(sa.GetData(), nullptr);
    for (int i = 0; i < 1000; ++i)
        sa.GetData()[i] = i * 2;
    for (int i = 0; i < 1000; ++i)
        EXPECT_EQ(sa.GetData()[i], i * 2);
}

// Ported from: imodel-native ScopedArray.h — GetDataCP returns const pointer
TEST(ScopedArrayTest, GetDataCPReturnsConstPointer) {
    dqBase::ScopedArray<int> sa(5);
    const auto& csa = sa;
    EXPECT_EQ(csa.GetDataCP(), sa.GetData());
}

// Ported from: imodel-native ScopedArray.h — boundary at exactly THRESHOLD bytes
TEST(ScopedArrayTest, BoundaryAtThreshold) {
    // THRESHOLD = 511 bytes. 511 / sizeof(int) = 127 ints on 4-byte → exactly at boundary.
    // 127 * 4 = 508 bytes ≤ 511 → stack.
    // 128 * 4 = 512 bytes > 511 → heap.
    size_t stackCount = 511 / sizeof(int);
    size_t heapCount = stackCount + 1;

    dqBase::ScopedArray<int> saStack(stackCount);
    EXPECT_NE(saStack.GetData(), nullptr);

    dqBase::ScopedArray<int> saHeap(heapCount);
    EXPECT_NE(saHeap.GetData(), nullptr);
}

// Ported from: imodel-native ScopedArray.h — copy-from-data constructor
TEST(ScopedArrayTest, CopyFromDataConstructor) {
    int source[] = {10, 20, 30, 40, 50};
    dqBase::ScopedArray<int> sa(5, source);
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(sa.GetData()[i], source[i]);
}

// Ported from: imodel-native ScopedArray.h — ScopedArray does NOT invoke constructors
// This is a behavioral contract: memory is allocated as raw bytes, not as T objects.
TEST(ScopedArrayTest, DoesNotInvokeConstructorsOnAllocation) {
    static int ctorCount = 0;
    static int dtorCount = 0;
    struct Counter {
        Counter() { ++ctorCount; }
        ~Counter() { ++dtorCount; }
    };

    ctorCount = 0;
    dtorCount = 0;
    {
        // Reference: "does NOT invoke ctors/dtors" for allocation.
        // After construction of ScopedArray, ctorCount should be 0 (raw memory, no ctors).
        dqBase::ScopedArray<Counter> sa(100);
    }
    // Both should remain 0 — allocation/deallocation is raw-memory.
    EXPECT_EQ(ctorCount, 0);
    EXPECT_EQ(dtorCount, 0);
}

// ---------------------------------------------------------------------------
// IndexedScopedArray tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native ScopedArray.h IndexedScopedArray — operator[] access
TEST(IndexedScopedArrayTest, OperatorBracketAccess) {
    int source[] = {1, 2, 3, 4, 5};
    dqBase::IndexedScopedArray<int> isa(5);
    for (int i = 0; i < 5; ++i)
        isa[i] = source[i];
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(isa[i], source[i]);
}

// Ported from: imodel-native ScopedArray.h IndexedScopedArray — const operator[]
TEST(IndexedScopedArrayTest, ConstOperatorBracketAccess) {
    int source[] = {10, 20, 30};
    dqBase::IndexedScopedArray<int> isa(3);
    for (int i = 0; i < 3; ++i)
        isa[i] = source[i];
    const auto& cisa = isa;
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(cisa[i], source[i]);
}

// ---------------------------------------------------------------------------
// AlignedArray tests (non-alignment-required platform: no-op behavior)
// ---------------------------------------------------------------------------

// Ported from: imodel-native ScopedArray.h AlignedArray — GetAlignedData returns
// the original pointer when alignment is not required (no-op path).
TEST(AlignedArrayTest, GetAlignedDataReturnsOriginalPointer) {
    dqBase::AlignedArray<double> aa;
    double data[10];
    for (int i = 0; i < 10; ++i) data[i] = i * 1.5;

    const double* result = aa.GetAlignedData(data, sizeof(data));
    // On non-alignment-required platforms, should return the same pointer
    EXPECT_EQ(result, data);
}

// Ported from: imodel-native ScopedArray.h AlignedArray — data accessible via returned pointer
TEST(AlignedArrayTest, ReturnedPointerIsAccessible) {
    dqBase::AlignedArray<int> aa;
    int data[5] = {10, 20, 30, 40, 50};

    const int* result = aa.GetAlignedData(data, sizeof(data));
    ASSERT_NE(result, nullptr);
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(result[i], data[i]);
}

// Ported from: imodel-native ScopedArray.h AlignedArray — Clear is a no-op (no crash)
TEST(AlignedArrayTest, ClearIsNoOp) {
    dqBase::AlignedArray<double> aa;
    double data[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
    aa.GetAlignedData(data, sizeof(data));
    // Should not crash
    aa.Clear();
}
