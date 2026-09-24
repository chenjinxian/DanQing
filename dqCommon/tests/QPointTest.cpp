// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — QPoint unit tests
// Ported from: itwinjs-core core/common/src/test/QPoint.test.ts (QuantizesToRange, ComputesRange)
//
// Reference: itwinjs-core core/common/src/test/QPoint.test.ts has exactly TWO cases:
//   describe("QPoint") { it("quantizes to range") ; it("computes range") }
// Both are ported verbatim below (QuantizesToRange, ComputesRange).
// All other DanQing cases exercise API surface NOT covered by the reference test file
// (Quantization helpers, QParams3d factories, QPoint3d equality/compare, QPoint3dList,
//  and the entire QPoint2d family which is a DanQing addition with no TS counterpart) —
// those are marked Authored honestly per §4.
#include "dqCommon/QPoint.h"

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqGeom;

// Ported from: itwinjs-core core/common/src/test/QPoint.test.ts
//              describe("QPoint") it("quantizes to range") (L23-42)
TEST(QPoint3d, QuantizesToRange)
{
    const auto range = Range3d::CreateXYZXYZ(0, -100, 200, 50, 100, 10000);
    const auto qparams = QParams3d::fromRange(range);

    // Origin should equal range.low
    EXPECT_TRUE(qparams.origin.AlmostEqual(range.low));

    // Low point should quantize to (0, 0, 0)
    {
        const auto qpt = QPoint3d::create(range.low, qparams);
        EXPECT_EQ(qpt.x, 0);
        EXPECT_EQ(qpt.y, 0);
        EXPECT_EQ(qpt.z, 0);
        const auto unq = qpt.unquantize(qparams);
        EXPECT_TRUE(unq.AlmostEqual(range.low, 0.0));
    }

    // High point should quantize to (0xffff, 0xffff, 0xffff)
    {
        const auto qpt = QPoint3d::create(range.high, qparams);
        EXPECT_EQ(qpt.x, 0xFFFF);
        EXPECT_EQ(qpt.y, 0xFFFF);
        EXPECT_EQ(qpt.z, 0xFFFF);
        const auto unq = qpt.unquantize(qparams);
        EXPECT_TRUE(unq.AlmostEqual(range.high, 0.0));
    }

    // Center should quantize to approximately (0x8000, 0x8000, 0x8000)
    {
        const auto center = Point3d::FromInterpolate(range.low, 0.5, range.high);
        const auto qpt = QPoint3d::create(center, qparams);
        // Allow some tolerance due to quantization
        EXPECT_NEAR(qpt.x, 0x8000, 2);
        EXPECT_NEAR(qpt.y, 0x8000, 2);
        EXPECT_NEAR(qpt.z, 0x8000, 2);
        const auto unq = qpt.unquantize(qparams);
        EXPECT_TRUE(unq.AlmostEqual(center, 0.08));
    }

    // Flat z range
    {
        auto flatRange = range;
        flatRange.low.z = 500;
        flatRange.high.z = 500;
        const auto flatParams = QParams3d::fromRange(flatRange);

        const auto qpt = QPoint3d::create(flatRange.low, flatParams);
        EXPECT_EQ(qpt.x, 0);
        EXPECT_EQ(qpt.y, 0);
        EXPECT_EQ(qpt.z, 0);
        const auto unq = qpt.unquantize(flatParams);
        EXPECT_TRUE(unq.AlmostEqual(flatRange.low, 0.0));
    }
}

// Ported from: itwinjs-core core/common/src/test/QPoint.test.ts
//              describe("QPoint") it("computes range") (L44-56)
TEST(QPoint3d, ComputesRange)
{
    auto roundTrip = [](const Range3d& range, double tolerance = 0.01) {
        const auto params = QParams3d::fromRange(range);
        EXPECT_TRUE(params.origin.AlmostEqual(range.low, 0.0));

        const auto result = params.computeRange();
        EXPECT_TRUE(result.low.AlmostEqual(range.low, 0.0));
        EXPECT_TRUE(result.high.AlmostEqual(range.high, tolerance));
    };

    roundTrip(Range3d::CreateXYZXYZ(0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF), 0.0);
    roundTrip(Range3d::CreateXYZXYZ(-100, 0, 100, 500, 10, 9999));
}

// Authored: no equivalent reference test in itwinjs-core for Quantization.computeScale
//   (QPoint.test.ts has no Quantization describe-block; the Quantization helper is exercised
//    only transitively via the quantizes-to-range case); behavior verified against QPoint.ts contract.
TEST(Quantization, computeScale)
{
    EXPECT_DOUBLE_EQ(Quantization::computeScale(0.0), 0.0);
    EXPECT_NEAR(Quantization::computeScale(100.0), 655.35, 0.01);
    EXPECT_NEAR(Quantization::computeScale(1.0), 65535.0, 0.01);
}

// Authored: no equivalent reference test in itwinjs-core for Quantization.isInRange
TEST(Quantization, isInRange)
{
    EXPECT_TRUE(Quantization::isInRange(0.0));
    EXPECT_TRUE(Quantization::isInRange(32768.0));
    EXPECT_TRUE(Quantization::isInRange(65535.0));
    EXPECT_FALSE(Quantization::isInRange(-1.0));
    EXPECT_FALSE(Quantization::isInRange(65536.0));
}

// Authored: no equivalent reference test in itwinjs-core for Quantization.quantize/unquantize round-trip
TEST(Quantization, QuantizeUnquantize)
{
    const double origin = 0.0;
    const double scale = 65535.0;  // maps [0, 1] to [0, 0xffff]

    // quantize 0 → 0
    EXPECT_EQ(Quantization::quantize(0.0, origin, scale), 0);
    // quantize 1 → 0xffff
    EXPECT_EQ(Quantization::quantize(1.0, origin, scale), 0xFFFF);

    // Roundtrip
    const uint16_t q = Quantization::quantize(0.5, origin, scale);
    const double unq = Quantization::unquantize(q, origin, scale);
    EXPECT_NEAR(unq, 0.5, 1.0 / 65535.0);
}

// Authored: no equivalent reference test in itwinjs-core for Quantization.isQuantized
TEST(Quantization, isQuantized)
{
    EXPECT_TRUE(Quantization::isQuantized(0.0));
    EXPECT_TRUE(Quantization::isQuantized(32768.0));
    EXPECT_TRUE(Quantization::isQuantized(65535.0));
    EXPECT_FALSE(Quantization::isQuantized(0.5));
    EXPECT_FALSE(Quantization::isQuantized(-1.0));
}

// Authored: no equivalent reference test in itwinjs-core for QParams3d.fromNormalizedRange
TEST(QParams3d, fromNormalizedRange)
{
    const auto params = QParams3d::fromNormalizedRange();
    EXPECT_TRUE(params.origin.AlmostEqual(Point3d::From(-1, -1, -1)));
}

// Authored: no equivalent reference test in itwinjs-core for QParams3d.fromZeroToOne
TEST(QParams3d, fromZeroToOne)
{
    const auto params = QParams3d::fromZeroToOne();
    EXPECT_TRUE(params.origin.AlmostEqual(Point3d::From(0, 0, 0)));
}

// Authored: no equivalent reference test in itwinjs-core for QParams3d.rangeDiagonal
TEST(QParams3d, rangeDiagonal)
{
    const auto range = Range3d::CreateXYZXYZ(0, 0, 0, 100, 200, 300);
    const auto params = QParams3d::fromRange(range);
    const auto diag = params.rangeDiagonal();
    EXPECT_NEAR(diag.x, 100.0, 0.01);
    EXPECT_NEAR(diag.y, 200.0, 0.01);
    EXPECT_NEAR(diag.z, 300.0, 0.01);
}

// Authored: no equivalent reference test in itwinjs-core for QParams3d.isQuantizable
TEST(QParams3d, isQuantizable)
{
    const auto range = Range3d::CreateXYZXYZ(0, 0, 0, 100, 100, 100);
    const auto params = QParams3d::fromRange(range);
    EXPECT_TRUE(params.isQuantizable(Point3d::From(50, 50, 50)));
    EXPECT_TRUE(params.isQuantizable(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(params.isQuantizable(Point3d::From(100, 100, 100)));
}

// Authored: no equivalent reference test in itwinjs-core for QPoint3d equality
TEST(QPoint3d, Equality)
{
    const QPoint3d a(100, 200, 300);
    const QPoint3d b(100, 200, 300);
    const QPoint3d c(100, 200, 301);
    EXPECT_TRUE(a.equals(b));
    EXPECT_FALSE(a.equals(c));
}

// Authored: no equivalent reference test in itwinjs-core for QPoint3d.Compare
TEST(QPoint3d, Compare)
{
    const QPoint3d a(100, 200, 300);
    const QPoint3d b(100, 200, 300);
    const QPoint3d c(101, 200, 300);
    EXPECT_EQ(a.compare(b), 0);
    EXPECT_LT(a.compare(c), 0);
    EXPECT_GT(c.compare(a), 0);
}

// Authored: no equivalent reference test in itwinjs-core for QPoint3dList
TEST(QPoint3dList, fromPoints)
{
    const Point3d points[] = {
        Point3d::From(0, 0, 0),
        Point3d::From(100, 200, 300),
        Point3d::From(50, 50, 50),
    };
    auto list = QPoint3dList::fromPoints(points, 3);
    EXPECT_EQ(list.length(), 3);

    // unquantize and check approximate equality
    for (int i = 0; i < 3; ++i) {
        const auto unq = list.unquantize(i);
        EXPECT_TRUE(unq.AlmostEqual(points[i], 0.01));
    }
}

// Authored: no equivalent reference test in itwinjs-core for QPoint3dList.add/clear
TEST(QPoint3dList, AddAndClear)
{
    QParams3d params;
    params.setFromRange(Range3d::CreateXYZXYZ(0, 0, 0, 1000, 1000, 1000));
    QPoint3dList list(params);

    list.add(Point3d::From(100, 200, 300));
    EXPECT_EQ(list.length(), 1);

    list.add(Point3d::From(400, 500, 600));
    EXPECT_EQ(list.length(), 2);

    list.clear();
    EXPECT_EQ(list.length(), 0);
}

// Authored: no equivalent reference test in itwinjs-core for QPoint3dList.toTypedArray
TEST(QPoint3dList, toTypedArray)
{
    const Point3d points[] = {
        Point3d::From(0, 0, 0),
        Point3d::From(100, 200, 300),
    };
    auto list = QPoint3dList::fromPoints(points, 2);
    const auto array = list.toTypedArray();
    EXPECT_EQ(array.size(), 6u);  // 2 points * 3 components
}

// Authored: no reference test exists in itwinjs-core for QPoint2d (DanQing addition)
TEST(QParams2d, fromRange)
{
    const auto range = Range2d::CreateXYXY(0.0, -100.0, 50.0, 100.0);
    const auto params = QParams2d::fromRange(range);

    // Origin should equal range.low
    EXPECT_NEAR(params.origin.x, 0.0, 1e-10);
    EXPECT_NEAR(params.origin.y, -100.0, 1e-10);
}

// Authored: no reference test exists in itwinjs-core for QPoint2d (DanQing addition)
TEST(QParams2d, fromNormalizedRange)
{
    const auto params = QParams2d::fromNormalizedRange();
    EXPECT_NEAR(params.origin.x, -1.0, 1e-10);
    EXPECT_NEAR(params.origin.y, -1.0, 1e-10);
}

// Authored: no reference test exists in itwinjs-core for QPoint2d (DanQing addition)
TEST(QParams2d, fromZeroToOne)
{
    const auto params = QParams2d::fromZeroToOne();
    EXPECT_NEAR(params.origin.x, 0.0, 1e-10);
    EXPECT_NEAR(params.origin.y, 0.0, 1e-10);
}

// Authored: no reference test exists in itwinjs-core for QPoint2d (DanQing addition)
TEST(QParams2d, rangeDiagonal)
{
    const auto range = Range2d::CreateXYXY(0.0, 0.0, 100.0, 200.0);
    const auto params = QParams2d::fromRange(range);
    const auto diag = params.rangeDiagonal();
    EXPECT_NEAR(diag.x, 100.0, 0.01);
    EXPECT_NEAR(diag.y, 200.0, 0.01);
}

// Authored: no reference test exists in itwinjs-core for QPoint2d (DanQing addition)
TEST(QPoint2d, CreateAndUnquantize)
{
    const auto range = Range2d::CreateXYXY(0.0, 0.0, 100.0, 200.0);
    const auto params = QParams2d::fromRange(range);

    // Low point should quantize to (0, 0)
    {
        const Point2d lowPt = Point2d::from(range.low.x, range.low.y);
        const auto qpt = QPoint2d::create(lowPt, params);
        EXPECT_EQ(qpt.x, 0);
        EXPECT_EQ(qpt.y, 0);
        const auto unq = qpt.unquantize(params);
        EXPECT_NEAR(unq.x, 0.0, 1e-10);
        EXPECT_NEAR(unq.y, 0.0, 1e-10);
    }

    // High point should quantize to (0xffff, 0xffff)
    {
        const Point2d highPt = Point2d::from(range.high.x, range.high.y);
        const auto qpt = QPoint2d::create(highPt, params);
        EXPECT_EQ(qpt.x, 0xFFFF);
        EXPECT_EQ(qpt.y, 0xFFFF);
        const auto unq = qpt.unquantize(params);
        EXPECT_NEAR(unq.x, 100.0, 0.01);
        EXPECT_NEAR(unq.y, 200.0, 0.01);
    }
}

// Authored: no reference test exists in itwinjs-core for QPoint2d (DanQing addition)
TEST(QPoint2d, Equality)
{
    const QPoint2d a(100, 200);
    const QPoint2d b(100, 200);
    const QPoint2d c(100, 201);
    EXPECT_TRUE(a.equals(b));
    EXPECT_FALSE(a.equals(c));
}

// Authored: no reference test exists in itwinjs-core for QPoint2d (DanQing addition)
TEST(QPoint2dList, fromPoints)
{
    const Point2d points[] = {
        Point2d::from(0, 0),
        Point2d::from(100, 200),
        Point2d::from(50, 50),
    };
    auto list = QPoint2dList::fromPoints(points, 3);
    EXPECT_EQ(list.length(), 3);

    // unquantize and check approximate equality
    for (int i = 0; i < 3; ++i) {
        const auto unq = list.unquantize(i);
        EXPECT_NEAR(unq.x, points[i].x, 0.01);
        EXPECT_NEAR(unq.y, points[i].y, 0.01);
    }
}

// Authored: no reference test exists in itwinjs-core for QPoint2d (DanQing addition)
TEST(QPoint2dList, AddAndClear)
{
    QParams2d params;
    params.setFromRange(Range2d::CreateXYXY(0.0, 0.0, 1000.0, 1000.0));
    QPoint2dList list(params);

    list.add(Point2d::from(100, 200));
    EXPECT_EQ(list.length(), 1);

    list.add(Point2d::from(400, 500));
    EXPECT_EQ(list.length(), 2);

    list.clear();
    EXPECT_EQ(list.length(), 0);
}

// Authored: no reference test exists in itwinjs-core for QPoint2d (DanQing addition)
TEST(QPoint2dList, toTypedArray)
{
    const Point2d points[] = {
        Point2d::from(0, 0),
        Point2d::from(100, 200),
    };
    auto list = QPoint2dList::fromPoints(points, 2);
    const auto array = list.toTypedArray();
    EXPECT_EQ(array.size(), 4u);  // 2 points * 2 components
}
