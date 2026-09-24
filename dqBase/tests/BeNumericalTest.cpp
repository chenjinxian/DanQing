// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeNumerical_Test.cpp
//              TEST(BeNumerical,Test1)
// dqBase tests — BeNumerical 浮点比较工具行为验证（TDD：先写测试）
//
// 参考来源：
//   - imodel-native iModelCore/Bentley/Tests/NonPublished/BeNumerical_Test.cpp
//     TEST(BeNumerical, Test1) — Compare 符号、compareToTolerance 辅助、容差单调性
//   - imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeNumerical.h
//     ComputeComparisonTolerance / NextafterDelta / Compare 算法定义
#include <gtest/gtest.h>

#include <dqBase/BeNumerical.h>

#include <cfloat>
#include <cmath>
#include <type_traits>

// ---------------------------------------------------------------------------
// 签名 / 返回类型断言（编译期）——对齐 imodel-native BeNumerical.h
// ---------------------------------------------------------------------------

// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeNumerical.h
//              BeIsnan / BeFinite return int (ref:23-24, 28-29); DanQing 曾误返回 bool
static_assert(std::is_same_v<decltype(dqBase::BeNumerical::BeIsnan(0.0)), int>,
              "BeIsnan must return int per imodel-native BeNumerical.h");
static_assert(std::is_same_v<decltype(dqBase::BeNumerical::BeFinite(0.0)), int>,
              "BeFinite must return int per imodel-native BeNumerical.h");

// Compare signature: int Compare(double, double) — 无 tol 参数（ref:57-62）
static_assert(std::is_same_v<decltype(dqBase::BeNumerical::Compare(0.0, 0.0)), int>,
              "Compare must return int per imodel-native BeNumerical.h");

// ---------------------------------------------------------------------------
// 参考移植：imodel-native BeNumerical_Test.cpp::compareToTolerance
// ---------------------------------------------------------------------------

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeNumerical_Test.cpp
//              static void compareToTolerance(double v) (lines 8-16)
static void compareToTolerance(double v) {
    double vTol = dqBase::BeNumerical::ComputeComparisonTolerance(v, v);
    EXPECT_EQ(dqBase::BeNumerical::Compare(v, v + vTol),     -1);
    EXPECT_EQ(dqBase::BeNumerical::Compare(v, v + vTol / 10), 0);
    EXPECT_EQ(dqBase::BeNumerical::Compare(v, v),             0);
    EXPECT_EQ(dqBase::BeNumerical::Compare(v, v - vTol / 10), 0);
    EXPECT_EQ(dqBase::BeNumerical::Compare(v, v - vTol),       1);
}

// ---------------------------------------------------------------------------
// 主测试：1:1 移植 imodel-native TEST(BeNumerical, Test1)
// ---------------------------------------------------------------------------

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/BeNumerical_Test.cpp
//              TEST(BeNumerical, Test1) (lines 21-40)
TEST(BeNumericalTest, CompareSignsAndToleranceBehavior) {
    using dqBase::BeNumerical;
    EXPECT_EQ(BeNumerical::Compare( 1.0,     0.0),  1);
    EXPECT_EQ(BeNumerical::Compare( 1.0e-13, 0.0),  1);
    EXPECT_EQ(BeNumerical::Compare( 0.0,     0.0),  0);
    EXPECT_EQ(BeNumerical::Compare(-1.0e-13, 0.0), -1);
    EXPECT_EQ(BeNumerical::Compare(-1.0,     0.0), -1);

    compareToTolerance(0.0);
    compareToTolerance(1.0e-13);
    compareToTolerance(0.5);
    compareToTolerance(0.51);
    compareToTolerance(1.0);
    compareToTolerance(-1.0);
    compareToTolerance(1.0e9);
    compareToTolerance(-1.0e9);
    compareToTolerance(1.0e17);

    // 容差单调性：1e9 的比较容差必须大于 1.0 的比较容差（ref:39）
    EXPECT_GT(BeNumerical::ComputeComparisonTolerance(1.0e9, 1.0e9),
              BeNumerical::ComputeComparisonTolerance(1.0,  1.0));
}

// ---------------------------------------------------------------------------
// ComputeComparisonTolerance 精确值断言
// 公式（ref:47-51）：uv = max(|sv1|,|sv2|); uv<1.0 ? DBL_EPSILON : NextafterDelta(uv)
// NextafterDelta(sv) = BeNextafter(|sv|, DBL_MAX) - |sv|  （ref:37-41，带 fabs）
// 预期值由参考公式手算得出（std::nextafter 等价于 BeNextafter 的非 Windows 路径）
// ---------------------------------------------------------------------------

// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeNumerical.h
//              ComputeComparisonTolerance (lines 47-51)
TEST(BeNumericalTest, ComputeComparisonToleranceMatchesReferenceFormula) {
    using dqBase::BeNumerical;

    // uv = 0.0 < 1.0 → DBL_EPSILON
    EXPECT_DOUBLE_EQ(BeNumerical::ComputeComparisonTolerance(0.0, 0.0), DBL_EPSILON);

    // uv = 0.5 < 1.0 → DBL_EPSILON
    EXPECT_DOUBLE_EQ(BeNumerical::ComputeComparisonTolerance(0.5, 0.5), DBL_EPSILON);

    // uv = 0.51 < 1.0 → DBL_EPSILON（边界附近）
    EXPECT_DOUBLE_EQ(BeNumerical::ComputeComparisonTolerance(0.51, 0.51), DBL_EPSILON);

    // uv = 1.0 NOT < 1.0 → NextafterDelta(1.0) = nextafter(1.0, DBL_MAX) - 1.0 = DBL_EPSILON
    EXPECT_DOUBLE_EQ(BeNumerical::ComputeComparisonTolerance(1.0, 1.0),
                     std::nextafter(1.0, DBL_MAX) - 1.0);

    // uv = 2.0 → NextafterDelta(2.0) = nextafter(2.0, DBL_MAX) - 2.0 = 2*DBL_EPSILON
    EXPECT_DOUBLE_EQ(BeNumerical::ComputeComparisonTolerance(2.0, 2.0),
                     std::nextafter(2.0, DBL_MAX) - 2.0);

    // uv = 1e9 → NextafterDelta(1e9) = nextafter(1e9, DBL_MAX) - 1e9
    EXPECT_DOUBLE_EQ(BeNumerical::ComputeComparisonTolerance(1.0e9, 1.0e9),
                     std::nextafter(1.0e9, DBL_MAX) - 1.0e9);

    // max(|sv1|,|sv2|) 取较大者：一个为 0 一个为 1e9 → uv = 1e9
    EXPECT_DOUBLE_EQ(BeNumerical::ComputeComparisonTolerance(0.0, 1.0e9),
                     std::nextafter(1.0e9, DBL_MAX) - 1.0e9);

    // uv = 1e17 → NextafterDelta(1e17) = 16.0（double ULP at 1e17）
    EXPECT_DOUBLE_EQ(BeNumerical::ComputeComparisonTolerance(1.0e17, 1.0e17),
                     std::nextafter(1.0e17, DBL_MAX) - 1.0e17);
}

// ---------------------------------------------------------------------------
// NextafterDelta 算法断言（含 fabs 与 DBL_MAX 方向）
// ---------------------------------------------------------------------------

// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeNumerical.h
//              NextafterDelta (lines 37-41): double v = fabs(sv); return BeNextafter(v, DBL_MAX) - v;
TEST(BeNumericalTest, NextafterDeltaMatchesReferenceFormula) {
    using dqBase::BeNumerical;

    // fabs(sv) 与 DBL_MAX 方向：正值与负值必须得到相同结果
    EXPECT_DOUBLE_EQ(BeNumerical::NextafterDelta( 2.0),
                     std::nextafter(2.0, DBL_MAX) - 2.0);
    EXPECT_DOUBLE_EQ(BeNumerical::NextafterDelta(-2.0),
                     std::nextafter(2.0, DBL_MAX) - 2.0);

    // |sv| = 1e9 → 与正向 nextafter 一致
    EXPECT_DOUBLE_EQ(BeNumerical::NextafterDelta(-1.0e9),
                     std::nextafter(1.0e9, DBL_MAX) - 1.0e9);
}

// ---------------------------------------------------------------------------
// BeIsnan / BeFinite 返回 int（对齐 ref:23-24, 28-29）
// imodel-native 在 _WIN32 用 _isnan/_finite（返回 int），非 Win 用 isnan/isfinite
// 两者均返回 int（true→非零，false→0）
// ---------------------------------------------------------------------------

// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeNumerical.h
//              BeIsnan / BeFinite return int (lines 23-24, 28-29)
TEST(BeNumericalTest, BeIsnanAndBeFiniteReturnInt) {
    using dqBase::BeNumerical;

    // BeIsnan: NaN → 非零；非 NaN → 0
    EXPECT_NE(BeNumerical::BeIsnan(std::nan("0")), 0);
    EXPECT_EQ(BeNumerical::BeIsnan(0.0), 0);
    EXPECT_EQ(BeNumerical::BeIsnan(1.0), 0);

    // BeFinite: 有限值 → 非零；NaN/Inf → 0
    EXPECT_NE(BeNumerical::BeFinite(0.0), 0);
    EXPECT_NE(BeNumerical::BeFinite(1.0e300), 0);
    EXPECT_EQ(BeNumerical::BeFinite(std::nan("0")), 0);
    EXPECT_EQ(BeNumerical::BeFinite(HUGE_VAL), 0);
    EXPECT_EQ(BeNumerical::BeFinite(-HUGE_VAL), 0);
}

// ---------------------------------------------------------------------------
// IsGreater/IsLess/IsEqual 系列：对齐 ref:65-92（语义断言）
// ---------------------------------------------------------------------------

// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeNumerical.h
//              IsGreater/IsLess/IsEqual... (lines 65-92) — 语义断言
// 注意：在参考公式下 CCT(0,0)=CCT(1,1)=DBL_EPSILON≈2.22e-16，因此 1e-13 已远超容差，
// Compare(1.0e-13, 0.0) 返回 1（参考 Test1 line 24），Compare(-1.0e-13, 0.0) 返回 -1。
TEST(BeNumericalTest, ComparisonPredicatesUseStrictSignatures) {
    using dqBase::BeNumerical;

    // IsGreater 仅当严格大于（== 1）；1e-13 已超出 0 处容差 DBL_EPSILON → greater
    EXPECT_TRUE (BeNumerical::IsGreater( 1.0, 0.0));
    EXPECT_TRUE (BeNumerical::IsGreater( 1.0e-13, 0.0)); // 参考语义：1e-13 > 2.22e-16
    EXPECT_FALSE(BeNumerical::IsGreater( 0.0, 0.0));

    // IsLess 仅当严格小于（== -1）
    EXPECT_TRUE (BeNumerical::IsLess(-1.0, 0.0));
    EXPECT_TRUE (BeNumerical::IsLess(-1.0e-13, 0.0)); // 参考语义：-1e-13 < -2.22e-16
    EXPECT_FALSE(BeNumerical::IsLess( 0.0, 0.0));

    // IsEqual：完全相等；以及半 ULP 内视为相等，多倍 ULP 外视为不等
    EXPECT_TRUE (BeNumerical::IsEqual(0.0, 0.0));
    EXPECT_TRUE (BeNumerical::IsEqual(1.0, 1.0 + DBL_EPSILON / 2.0));   // 半 ULP：在容差内
    EXPECT_FALSE(BeNumerical::IsEqual(1.0, 1.0 + 10.0 * DBL_EPSILON));  // 10 ULP：超出容差

    // 零判断系列
    EXPECT_TRUE (BeNumerical::IsEqualToZero(0.0));
    EXPECT_FALSE(BeNumerical::IsEqualToZero(1.0e-13));
    EXPECT_TRUE (BeNumerical::IsGreaterThanZero(1.0e-13));
    EXPECT_TRUE (BeNumerical::IsLessThanZero(-1.0e-13));
}
