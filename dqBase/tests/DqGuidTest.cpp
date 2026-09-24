// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native BeSQLite.h BeGuid contract
//              (BeGuid::IsValid / Invalidate semantics, BeSQLite.h:230)
#include <gtest/gtest.h>

#include <dqBase/DqGuid.h>

// Ported from: imodel-native BeSQLite.h BeGuid::IsValid() semantics (line 230)
//              (no standalone BeGuid test file; semantics derived from header contract)
TEST(DqGuidTest, IsValidRequiresBothHalvesNonzero) {
    // all-zero -> invalid (matches BeGuid default-constructed then Invalidate())
    EXPECT_FALSE(dqBase::DqGuid(0, 0).IsValid());
    // both nonzero -> valid
    EXPECT_TRUE(dqBase::DqGuid(1, 1).IsValid());
    // ONLY hi nonzero -> INVALID per reference (&&) - current DanQing wrongly returns true
    EXPECT_FALSE(dqBase::DqGuid(0x1234, 0).IsValid());
    // ONLY lo nonzero -> INVALID per reference (&&) - current DanQing wrongly returns true
    EXPECT_FALSE(dqBase::DqGuid(0, 0x5678).IsValid());
}

// Ported from: imodel-native BeSQLite.h BeGuid::IsValid() semantics (line 230)
//              IsNull is the exact logical inverse of IsValid.
TEST(DqGuidTest, IsNullIsLogicalInverseOfIsValid) {
    EXPECT_TRUE(dqBase::DqGuid(0, 0).IsNull());
    EXPECT_FALSE(dqBase::DqGuid(1, 1).IsNull());
    // one-half-zero GUIDs are "null/invalid" under &&- IsValid semantics
    EXPECT_TRUE(dqBase::DqGuid(0x1234, 0).IsNull());
    EXPECT_TRUE(dqBase::DqGuid(0, 0x5678).IsNull());
}
