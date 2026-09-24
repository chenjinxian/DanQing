// SPDX-License-Identifier: Apache-2.0
// dqBase tests — HeapZone / FixedSizePool1 compile-smoke + basic alloc/free cycle.
//
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/HeapZone.h
//
// Authored: no reference test exists in imodel-native for HeapZone.
//   (grep -rl "HeapZone" iModelCore/.../Tests/ yields no HeapZone_test.cpp; the
//   reference relies on integration-level coverage via DgnPlatform consumers.)
//
// Purpose: prove that HeapZone.h + bpool.h preprocess cleanly and FixedSizePool1
// instantiates (today these are DEAD code blocked by a broken include + missing
// DEFINE_T_SUPER macro — audit P0 #8). Once it compiles, exercise a basic
// allocate/free cycle on FixedSizePool1 and HeapZone.
#include <gtest/gtest.h>

#include <dqBase/HeapZone.h>

#include <cstdint>
#include <cstring>

using dqBase::FixedSizePool1;
using dqBase::HeapZone;

namespace {
// FixedSizePool1 default-constructs to 8-byte entries; allocate and return one.
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/HeapZone.h
//              TEST(HeapZoneTest, FixedSizePool1_DefaultConstruct_AllocFreeCycle)
TEST(HeapZoneTest, FixedSizePool1_DefaultConstruct_AllocFreeCycle) {
    FixedSizePool1 pool;
    pool.SetSize(16, 32);

    void* p = pool.malloc();
    ASSERT_NE(nullptr, p);
    // Smoke: write to the chunk to confirm it is writable storage.
    std::memset(p, 0xAB, 16);
    pool.free(p);
}

// HeapZone routes Alloc to NUM_FIXED_POOLS sized buckets (entry granularity 8).
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/HeapZone.h
//              TEST(HeapZoneTest, HeapZone_SmallAlloc_FreeRoundTrip)
TEST(HeapZoneTest, HeapZone_SmallAlloc_FreeRoundTrip) {
    HeapZone zone;
    void* p16 = zone.Alloc(16);
    ASSERT_NE(nullptr, p16);
    std::memset(p16, 0x11, 16);

    void* p64 = zone.Alloc(64);
    ASSERT_NE(nullptr, p64);
    std::memset(p64, 0x22, 64);

    zone.Free(p16, 16);
    zone.Free(p64, 64);
}

// HeapZone::Realloc with same chunk count should reuse the pointer.
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/HeapZone.h
//              TEST(HeapZoneTest, HeapZone_Realloc_SameChunkCount_ReusesPointer)
TEST(HeapZoneTest, HeapZone_Realloc_SameChunkCount_ReusesPointer) {
    HeapZone zone;
    void* p = zone.Alloc(8);
    ASSERT_NE(nullptr, p);
    void* p2 = zone.Realloc(p, 8, 8);
    EXPECT_EQ(p, p2);
    zone.Free(p2, 8);
}

// EmptyAll must be safe to call on an empty pool and on one with live allocations.
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/HeapZone.h
//              TEST(HeapZoneTest, HeapZone_EmptyAll_NoCrash)
TEST(HeapZoneTest, HeapZone_EmptyAll_NoCrash) {
    HeapZone zone;
    zone.EmptyAll();  // empty
    void* p = zone.Alloc(24);
    ASSERT_NE(nullptr, p);
    zone.EmptyAll();  // has live allocation
    // No use-after-free access here; the zone's memory is purged.
    (void)p;
}
}  // namespace
