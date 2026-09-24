// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/GeomLibs/geom/src/polyface/BlockedVector.cpp
//              (no standalone BlockedVector test file; test scenarios derived from implementation)
// dqBase tests — BlockedVector / BlockedVectorInt 行为验证
#include <gtest/gtest.h>

#include <dqBase/BlockedVector.h>

// ---------------------------------------------------------------------------
// BlockedVector basic tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native BlockedVector.cpp default ctor (lines 157-166)
TEST(BlockedVectorTest, DefaultCtorInitializesMetadata) {
    dqBase::BlockedVector<int> bv;
    EXPECT_EQ(bv.NumPerStruct(), 1u);
    EXPECT_EQ(bv.StructsPerRow(), 1u);
    EXPECT_EQ(bv.Tag(), 0u);
    EXPECT_EQ(bv.IndexFamily(), 0u);
    EXPECT_EQ(bv.IndexedBy(), 0u);
    EXPECT_FALSE(bv.Active());
    EXPECT_EQ(bv.size(), 0u);
}

// Ported from: imodel-native BlockedVector.cpp parameterized ctor (lines 134-151)
TEST(BlockedVectorTest, ParameterizedCtorSetsMetadata) {
    dqBase::BlockedVector<double> bv(3, 10, 5, 1, 2, true);
    EXPECT_EQ(bv.NumPerStruct(), 3u);
    EXPECT_EQ(bv.StructsPerRow(), 10u);
    EXPECT_EQ(bv.Tag(), 5u);
    EXPECT_EQ(bv.IndexFamily(), 1u);
    EXPECT_EQ(bv.IndexedBy(), 2u);
    EXPECT_TRUE(bv.Active());
}

// Ported from: imodel-native BlockedVector.cpp SetTags (lines 69-86)
TEST(BlockedVectorTest, SetTagsUpdatesAllMetadata) {
    dqBase::BlockedVector<int> bv;
    bv.SetTags(2, 8, 3, 4, 5, true);
    EXPECT_EQ(bv.NumPerStruct(), 2u);
    EXPECT_EQ(bv.StructsPerRow(), 8u);
    EXPECT_EQ(bv.Tag(), 3u);
    EXPECT_EQ(bv.IndexFamily(), 4u);
    EXPECT_EQ(bv.IndexedBy(), 5u);
    EXPECT_TRUE(bv.Active());
}

// Ported from: imodel-native BlockedVector.cpp Append(T const&) (lines 257-263)
TEST(BlockedVectorTest, AppendSingleValueSetsSizeAndActive) {
    dqBase::BlockedVector<int> bv;
    EXPECT_FALSE(bv.Active());
    size_t newSize = bv.Append(42);
    EXPECT_EQ(newSize, 1u);
    EXPECT_EQ(bv[0], 42);
    EXPECT_TRUE(bv.Active());
}

// Ported from: imodel-native BlockedVector.cpp Append(T const*, size_t) (lines 244-255)
TEST(BlockedVectorTest, AppendFromArray) {
    dqBase::BlockedVector<int> bv;
    int data[] = {10, 20, 30};
    size_t newSize = bv.Append(data, 3);
    EXPECT_EQ(newSize, 3u);
    EXPECT_EQ(bv[0], 10);
    EXPECT_EQ(bv[1], 20);
    EXPECT_EQ(bv[2], 30);
    EXPECT_TRUE(bv.Active());
}

// Ported from: imodel-native BlockedVector.cpp AppendAndReturnIndex (lines 265-272)
TEST(BlockedVectorTest, AppendAndReturnIndex) {
    dqBase::BlockedVector<int> bv;
    EXPECT_EQ(bv.AppendAndReturnIndex(10), 0u);
    EXPECT_EQ(bv.AppendAndReturnIndex(20), 1u);
    EXPECT_EQ(bv.AppendAndReturnIndex(30), 2u);
}

// Ported from: imodel-native BlockedVector.cpp CopyVectorFrom (lines 120-128)
TEST(BlockedVectorTest, CopyVectorFromReplacesContent) {
    dqBase::BlockedVector<int> bv;
    bv.Append(99);
    std::vector<int> source = {1, 2, 3};
    bv.CopyVectorFrom(source);
    EXPECT_EQ(bv.size(), 3u);
    EXPECT_EQ(bv[0], 1);
    EXPECT_EQ(bv[1], 2);
    EXPECT_EQ(bv[2], 3);
}

// Ported from: imodel-native BlockedVector.cpp TryGetAt (lines 53-64)
TEST(BlockedVectorTest, TryGetAtReturnsTrueForValidIndex) {
    dqBase::BlockedVector<int> bv;
    bv.push_back(10);
    bv.push_back(20);
    int value = 0;
    EXPECT_TRUE(bv.TryGetAt(1, -1, value));
    EXPECT_EQ(value, 20);
}

// Ported from: imodel-native BlockedVector.cpp TryGetAt (lines 53-64)
TEST(BlockedVectorTest, TryGetAtReturnsFalseForInvalidIndex) {
    dqBase::BlockedVector<int> bv;
    bv.push_back(10);
    int value = 0;
    EXPECT_FALSE(bv.TryGetAt(5, -1, value));
    EXPECT_EQ(value, -1);
}

// Ported from: imodel-native BlockedVector.cpp GetPtr/GetCP (lines 92-114)
TEST(BlockedVectorTest, GetPtrReturnsNullForEmpty) {
    dqBase::BlockedVector<int> bv;
    EXPECT_EQ(bv.GetPtr(), nullptr);
    EXPECT_EQ(bv.GetCP(), nullptr);
}

// Ported from: imodel-native BlockedVector.cpp GetPtr/GetCP (lines 92-114)
TEST(BlockedVectorTest, GetPtrReturnsDataPointer) {
    dqBase::BlockedVector<int> bv;
    bv.push_back(10);
    bv.push_back(20);
    EXPECT_NE(bv.GetPtr(), nullptr);
    EXPECT_EQ(bv.GetPtr()[0], 10);
    EXPECT_EQ(bv.GetCP()[1], 20);
}

// Ported from: imodel-native BlockedVector.cpp ReverseInRange (lines 292-302)
TEST(BlockedVectorTest, ReverseInRange) {
    dqBase::BlockedVector<int> bv;
    for (int i = 1; i <= 5; ++i) bv.push_back(i);
    bv.ReverseInRange(1, 3);  // reverse [2,3,4] → [4,3,2]
    EXPECT_EQ(bv[0], 1);
    EXPECT_EQ(bv[1], 4);
    EXPECT_EQ(bv[2], 3);
    EXPECT_EQ(bv[3], 2);
    EXPECT_EQ(bv[4], 5);
}

// Ported from: imodel-native BlockedVector.cpp CopyData (lines 308-314)
TEST(BlockedVectorTest, CopyDataCopiesValue) {
    dqBase::BlockedVector<int> bv;
    bv.push_back(10);
    bv.push_back(20);
    bv.push_back(30);
    bv.CopyData(0, 2);
    EXPECT_EQ(bv[2], 10);
}

// Ported from: imodel-native BlockedVector.cpp Trim (lines 320-334)
TEST(BlockedVectorTest, TrimKeepsSubset) {
    dqBase::BlockedVector<int> bv;
    for (int i = 1; i <= 5; ++i) bv.push_back(i);
    bv.Trim(2, 2);  // keep [3,4]
    EXPECT_EQ(bv.size(), 2u);
    EXPECT_EQ(bv[0], 3);
    EXPECT_EQ(bv[1], 4);
}

// Ported from: imodel-native BlockedVector.cpp NumCompleteRows (lines 340-346)
TEST(BlockedVectorTest, NumCompleteRows) {
    dqBase::BlockedVector<int> bv(3, 4);  // numPerStruct=3, structsPerRow=4
    for (int i = 0; i < 10; ++i) bv.push_back(i);
    EXPECT_EQ(bv.NumCompleteRows(), 2u);  // 10 / 4 = 2 complete rows
}

// Ported from: imodel-native BlockedVector.cpp SetStructsPerRow (line 23)
TEST(BlockedVectorTest, SetStructsPerRow) {
    dqBase::BlockedVector<int> bv;
    bv.SetStructsPerRow(16);
    EXPECT_EQ(bv.StructsPerRow(), 16u);
}

// Ported from: imodel-native BlockedVector.cpp ClearAndAppendBlock (lines 172-199)
TEST(BlockedVectorTest, ClearAndAppendBlockWithWrap) {
    dqBase::BlockedVector<int> source;
    source.SetActive(true);
    for (int i = 1; i <= 5; ++i) source.push_back(i);

    dqBase::BlockedVector<int> bv;
    uint32_t count = bv.ClearAndAppendBlock(source, 1, 3, 2);  // [2,3,4] + wrap [2,3] = 5
    EXPECT_EQ(count, 5u);
    EXPECT_EQ(bv[0], 2);
    EXPECT_EQ(bv[1], 3);
    EXPECT_EQ(bv[2], 4);
    EXPECT_EQ(bv[3], 2);  // wrap
    EXPECT_EQ(bv[4], 3);  // wrap
}

// Ported from: imodel-native BlockedVector.cpp ClearAndAppend (lines 201-207)
TEST(BlockedVectorTest, ClearAndAppendReplacesContent) {
    dqBase::BlockedVector<int> bv;
    bv.push_back(99);
    std::vector<int> source = {1, 2, 3};
    bv.ClearAndAppend(source);
    EXPECT_EQ(bv.size(), 3u);
    EXPECT_EQ(bv[0], 1);
}

// ---------------------------------------------------------------------------
// BlockedVectorInt tests
// ---------------------------------------------------------------------------

// Ported from: imodel-native BlockedVector.cpp AddSequentialBlock (lines 542-571)
TEST(BlockedVectorIntTest, AddSequentialBlock) {
    dqBase::BlockedVectorInt bvi(1, 0, 0, 0, 0, true);
    bvi.AddSequentialBlock(10, 3, 2);  // [10,11,12] + wrap [10,11]
    EXPECT_EQ(bvi.size(), 5u);
    EXPECT_EQ(bvi[0], 10);
    EXPECT_EQ(bvi[1], 11);
    EXPECT_EQ(bvi[2], 12);
    EXPECT_EQ(bvi[3], 10);
    EXPECT_EQ(bvi[4], 11);
}

// Ported from: imodel-native BlockedVector.cpp AddSequentialBlock (lines 542-571)
TEST(BlockedVectorIntTest, AddSequentialBlockWithTrailingZeros) {
    dqBase::BlockedVectorInt bvi(1, 0, 0, 0, 0, true);
    bvi.AddSequentialBlock(1, 3, 0, 2);  // [1,2,3] + 2 trailing zeros
    EXPECT_EQ(bvi.size(), 5u);
    EXPECT_EQ(bvi[3], 0);
    EXPECT_EQ(bvi[4], 0);
}

// Ported from: imodel-native BlockedVector.cpp AddSteppedBlock (lines 508-538)
TEST(BlockedVectorIntTest, AddSteppedBlock) {
    dqBase::BlockedVectorInt bvi(1, 0, 0, 0, 0, true);
    bvi.AddSteppedBlock(0, 10, 4, 0);  // [0,10,20,30]
    EXPECT_EQ(bvi.size(), 4u);
    EXPECT_EQ(bvi[0], 0);
    EXPECT_EQ(bvi[1], 10);
    EXPECT_EQ(bvi[2], 20);
    EXPECT_EQ(bvi[3], 30);
}

// Ported from: imodel-native BlockedVector.cpp CountZeros (lines 466-476)
TEST(BlockedVectorIntTest, CountZeros) {
    dqBase::BlockedVectorInt bvi;
    int data[] = {1, 0, 3, 0, 5};
    bvi.Append(data, 5);
    EXPECT_EQ(bvi.CountZeros(), 2u);
}

// Ported from: imodel-native BlockedVector.cpp MinMax (lines 702-718)
TEST(BlockedVectorIntTest, MinMax) {
    dqBase::BlockedVectorInt bvi;
    int data[] = {3, 1, 4, 1, 5, 9, 2, 6};
    bvi.Append(data, 8);
    int minVal = 0, maxVal = 0;
    EXPECT_TRUE(bvi.MinMax(minVal, maxVal));
    EXPECT_EQ(minVal, 1);
    EXPECT_EQ(maxVal, 9);
}

// Ported from: imodel-native BlockedVector.cpp MinMax (lines 702-718)
TEST(BlockedVectorIntTest, MinMaxEmptyReturnsFalse) {
    dqBase::BlockedVectorInt bvi;
    int minVal = 0, maxVal = 0;
    EXPECT_FALSE(bvi.MinMax(minVal, maxVal));
}

// Ported from: imodel-native BlockedVector.cpp AddAndTerminate (lines 482-506)
TEST(BlockedVectorIntTest, AddAndTerminate) {
    dqBase::BlockedVectorInt bvi(1, 4);  // structsPerRow=4
    int data[] = {1, 2, 3};
    EXPECT_TRUE(bvi.AddAndTerminate(data, 3));
    // 3 values + 1 zero pad to fill row of 4
    EXPECT_EQ(bvi.size(), 4u);
    EXPECT_EQ(bvi[0], 1);
    EXPECT_EQ(bvi[1], 2);
    EXPECT_EQ(bvi[2], 3);
    EXPECT_EQ(bvi[3], 0);
}

// Ported from: imodel-native BlockedVector.cpp AllNegativeInRange (lines 725-731)
TEST(BlockedVectorIntTest, AllNegativeInRange) {
    dqBase::BlockedVectorInt bvi;
    int data[] = {-1, -2, -3, 4, -5};
    bvi.Append(data, 5);
    EXPECT_TRUE(bvi.AllNegativeInRange(0, 2));
    EXPECT_FALSE(bvi.AllNegativeInRange(0, 3));
}

// Ported from: imodel-native BlockedVector.cpp NegateInRange (lines 737-741)
TEST(BlockedVectorIntTest, NegateInRange) {
    dqBase::BlockedVectorInt bvi;
    int data[] = {1, -2, 3, -4};
    bvi.Append(data, 4);
    bvi.NegateInRange(0, 2);
    EXPECT_EQ(bvi[0], -1);
    EXPECT_EQ(bvi[1], 2);
    EXPECT_EQ(bvi[2], -3);
    EXPECT_EQ(bvi[3], -4);  // untouched
}

// Ported from: imodel-native BlockedVector.cpp Abs (lines 757-760)
TEST(BlockedVectorIntTest, AbsAll) {
    dqBase::BlockedVectorInt bvi;
    int data[] = {-1, 2, -3, 4};
    bvi.Append(data, 4);
    bvi.Abs();
    EXPECT_EQ(bvi[0], 1);
    EXPECT_EQ(bvi[1], 2);
    EXPECT_EQ(bvi[2], 3);
    EXPECT_EQ(bvi[3], 4);
}

// Ported from: imodel-native BlockedVector.cpp AddTerminatedGridBlocks (lines 648-696)
// Grid layout: rowStep=1 (col stride), colStep=3 (row stride)
// Vertices: row0=[0,1,2], row1=[3,4,5]
TEST(BlockedVectorIntTest, AddTerminatedGridBlocksQuad) {
    dqBase::BlockedVectorInt bvi(1, 0, 0, 0, 0, true);
    bvi.AddTerminatedGridBlocks(2, 3, 1, 3, false, true, 0, -1);
    // (2-1)*(3-1) = 2 quads, each 4 indices + terminator = 10 values
    EXPECT_EQ(bvi.size(), 10u);
    // Quad (row0=0,row1=1) x (col0=0,col1=1): i00=0, i01=1, i11=4, i10=3, terminator=-1
    EXPECT_EQ(bvi[0], 0);
    EXPECT_EQ(bvi[1], 1);
    EXPECT_EQ(bvi[2], 4);
    EXPECT_EQ(bvi[3], 3);
    EXPECT_EQ(bvi[4], -1);
    // Quad (row0=0,row1=1) x (col0=1,col1=2): i00=1, i01=2, i11=5, i10=4, terminator=-1
    EXPECT_EQ(bvi[5], 1);
    EXPECT_EQ(bvi[6], 2);
    EXPECT_EQ(bvi[7], 5);
    EXPECT_EQ(bvi[8], 4);
    EXPECT_EQ(bvi[9], -1);
}

// Ported from: imodel-native BlockedVector.cpp AddTerminatedGridBlocks (lines 648-696)
TEST(BlockedVectorIntTest, AddTerminatedGridBlocksTriangulated) {
    dqBase::BlockedVectorInt bvi(1, 0, 0, 0, 0, true);
    // 2x2 grid, rowStep=1, colStep=2. Vertices: row0=[0,1], row1=[2,3]
    bvi.AddTerminatedGridBlocks(2, 2, 1, 2, true, true, 0, -1);
    // (2-1)*(2-1) = 1 cell, 2 triangles * 4 values = 8
    EXPECT_EQ(bvi.size(), 8u);
    // Triangle 1: i00=0, i01=1, i10=2, terminator=-1
    EXPECT_EQ(bvi[0], 0);
    EXPECT_EQ(bvi[1], 1);
    EXPECT_EQ(bvi[2], 2);
    EXPECT_EQ(bvi[3], -1);
    // Triangle 2: i01=1, i11=3, i10=2, terminator=-1
    EXPECT_EQ(bvi[4], 1);
    EXPECT_EQ(bvi[5], 3);
    EXPECT_EQ(bvi[6], 2);
    EXPECT_EQ(bvi[7], -1);
}

// Ported from: imodel-native BlockedVector.cpp ShiftSignsFromCyclicPredecessorsInRange (lines 777-787)
TEST(BlockedVectorIntTest, ShiftSignsFromCyclicPredecessorsInRange) {
    dqBase::BlockedVectorInt bvi;
    int data[] = {1, -2, 3, -4};
    bvi.Append(data, 4);
    bvi.ShiftSignsFromCyclicPredecessorsInRange(0, 3);
    // last=−4 → sign0=−1. k=0: value=1>0 → sign1=+1, at(0)=−1*|1|=−1. sign0=+1
    // k=1: value=−2<0 → sign1=−1, at(1)=+1*|−2|=+2. sign0=−1
    // k=2: value=3>0 → sign1=+1, at(2)=−1*|3|=−3. sign0=+1
    // k=3: value=−4<0 → sign1=−1, at(3)=+1*|−4|=+4. sign0=−1
    EXPECT_EQ(bvi[0], -1);
    EXPECT_EQ(bvi[1], 2);
    EXPECT_EQ(bvi[2], -3);
    EXPECT_EQ(bvi[3], 4);
}
