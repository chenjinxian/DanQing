// SPDX-License-Identifier: Apache-2.0
// dqBase tests — DqId / DqGuid 行为验证
// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//              + itwinjs-core core/bentley/src/test/Id.test.ts
//
// 参考来源：
//   - imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//     TEST(BeIdTests, ToFromString) / TEST(BeIdTests, IsWellFormed)
//   - itwinjs-core core/bentley/src/test/Id.test.ts
//     it("Id64 should construct properly") / it("should validate well-formed Id64Strings")
//     / it("Guids")
#include <gtest/gtest.h>

#include <dqBase/DqId.h>

#include <string>

// --- 默认 / 有效性 ---

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//              TEST(BeIdTests, ToFromString) — FromString("0") yields !isValid() (value 0 == invalid)
//              (also itwinjs-core Id.test.ts it("Id64 should construct properly"): fromJSON("0") → !isValidId64)
TEST(DqIdTest, DefaultConstructedIsInvalid) {
    dqBase::DqId id;
    EXPECT_FALSE(id.isValid());
    EXPECT_TRUE(id.isNull());
    EXPECT_EQ(id.GetValue(), 0u);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//              TEST(BeIdTests, ToFromString) — BeInt64Id(122321123) is valid
//              (also itwinjs-core Id.test.ts: fromJSON("0x123") → isValidId64)
TEST(DqIdTest, NonZeroIsValid) {
    dqBase::DqId id{ 12345u };
    EXPECT_TRUE(id.isValid());
    EXPECT_FALSE(id.isNull());
    EXPECT_EQ(id.GetValue(), 12345u);
}

// --- 比较 / 哈希 ---

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//              TEST(BeIdTests, ToFromString) — BeInt64Id(122321123) == received
//              (also itwinjs-core Id.test.ts it("Id64 should construct properly"): id1A === id1B === id1C)
TEST(DqIdTest, EqualityComparison) {
    dqBase::DqId a{ 100u }, b{ 100u }, c{ 101u };
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(a < c);
}

// Ported from: itwinjs-core core/bentley/src/test/Id.test.ts
//              it("Id64 should construct properly") — Id64.toIdSet([id1A, id6, i55, id8, i55]).size === 3 (dedup)
TEST(DqIdTest, UsableInUeSet) {
    dqBase::DqSet<dqBase::DqId> set;
    set.insert(dqBase::DqId{ 1u });
    set.insert(dqBase::DqId{ 2u });
    set.insert(dqBase::DqId{ 1u }); // 重复，应覆盖
    EXPECT_EQ(set.size(), 2u);
    EXPECT_TRUE(set.count(dqBase::DqId{ 1u }) > 0);
}

// --- 字符串往返 ---

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//              TEST(BeIdTests, ToFromString) — hexId.ToString(UseHex::Yes) == "0xabcd12345678"
TEST(DqIdTest, ToStringFromHex) {
    dqBase::DqId id{ 0x1a2bu };
    const std::string s = id.ToString();
    EXPECT_FALSE(s.empty());
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//              TEST(BeIdTests, ToFromString) — hex ToString→FromString roundtrip preserves value
TEST(DqIdTest, RoundtripStringPreservesValue) {
    const uint64_t original = 0xDEADBEEFu;
    dqBase::DqId id{ original };
    const std::string s = id.ToString();
    const dqBase::DqId parsed = dqBase::DqId::FromString(s);
    EXPECT_EQ(parsed.GetValue(), original);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//              TEST(BeIdTests, ToFromString) — FromString("122321123") == BeInt64Id(122321123)
TEST(DqIdTest, FromDecimalString) {
    const dqBase::DqId parsed = dqBase::DqId::FromString("12345");
    EXPECT_EQ(parsed.GetValue(), 12345u);
}

// --- DqGuid ---

// Ported from: itwinjs-core core/bentley/src/test/Id.test.ts
//              it("Guids") — Guid.createValue() then Guid.isGuid(id1) === true
TEST(DqGuidTest, CreateUuidIsNotNull) {
    dqBase::DqGuid g;
    g.Create();
    EXPECT_TRUE(g.IsValid());
}

// Authored: no reference test exists in itwinjs-core Id.test.ts for a default-constructed Guid object's isNull state
TEST(DqGuidTest, DefaultConstructedIsNull) {
    const dqBase::DqGuid g{};
    EXPECT_TRUE(g.IsNull());
}

// ===================== IsWellFormedString / FromString 边界 =====================

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//              TEST(BeIdTests, IsWellFormed)
TEST(DqIdTest, IsWellFormedStringAcceptsValidHexAndDecimal) {
    EXPECT_TRUE(dqBase::DqId::IsWellFormedString("0"));
    EXPECT_TRUE(dqBase::DqId::IsWellFormedString("1"));
    EXPECT_TRUE(dqBase::DqId::IsWellFormedString("122321123"));
    EXPECT_TRUE(dqBase::DqId::IsWellFormedString("0x1"));
    EXPECT_TRUE(dqBase::DqId::IsWellFormedString("0x12345678ab"));
    EXPECT_TRUE(dqBase::DqId::IsWellFormedString("0xabcd12345678"));
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//              TEST(BeIdTests, IsWellFormed) — bad format rejection
TEST(DqIdTest, IsWellFormedStringRejectsBadFormat) {
    EXPECT_FALSE(dqBase::DqId::IsWellFormedString(""));      // 空
    EXPECT_FALSE(dqBase::DqId::IsWellFormedString("0x0"));    // 0x0 不规范
    EXPECT_FALSE(dqBase::DqId::IsWellFormedString("0x01"));   // 前导零
    EXPECT_FALSE(dqBase::DqId::IsWellFormedString("0X1"));    // 大写前缀 X
    EXPECT_FALSE(dqBase::DqId::IsWellFormedString("0xg"));    // 非法数字 g
    EXPECT_FALSE(dqBase::DqId::IsWellFormedString("0x12345678AB")); // 大写十六进制数字
    EXPECT_FALSE(dqBase::DqId::IsWellFormedString("-4521"));  // 负数
    EXPECT_FALSE(dqBase::DqId::IsWellFormedString("45.21"));  // 小数
    EXPECT_FALSE(dqBase::DqId::IsWellFormedString("garbage"));
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//              TEST(BeIdTests, ToFromString)
TEST(DqIdTest, FromStringParsesDecimalAndHex) {
    EXPECT_EQ(dqBase::DqId::FromString("122321123").GetValue(), 122321123u);
    EXPECT_EQ(dqBase::DqId::FromString("0x1").GetValue(), 1u);
    EXPECT_EQ(dqBase::DqId::FromString("0xabcd12345678").GetValue(),
            0xabcd12345678u);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//              TEST(BeIdTests, ToFromString) — bad input handling
TEST(DqIdTest, FromStringReturnsInvalidOnBadInput) {
    EXPECT_FALSE(dqBase::DqId::FromString("").isValid());
    EXPECT_FALSE(dqBase::DqId::FromString("-4521").isValid());
    EXPECT_FALSE(dqBase::DqId::FromString("45.21").isValid());
    EXPECT_FALSE(dqBase::DqId::FromString("garbage").isValid());
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeIdTest.cpp
//              TEST(BeIdTests, ToFromString) — hex roundtrip
TEST(DqIdTest, ToStringHexRoundtrip) {
    const dqBase::DqId id{ 0xabcd12345678ull };
    EXPECT_EQ(id.ToString(), "0xabcd12345678");
    EXPECT_EQ(dqBase::DqId::FromString(id.ToString()).GetValue(), 0xabcd12345678ull);
}
