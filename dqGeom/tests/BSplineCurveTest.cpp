// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — BSplineCurve3d/KnotVector 子集测试（M-W W-b2）
//
// 测试出处（§5）——参考 core/geometry/src/test/bspline/BsplineCurve.test.ts
// 与消费面对应的用例子集（AspectRatioSkew 直构三极点 2 次 clamped bcurve +
// evaluate/EmitStrokes 容差细分 + range + clone）。锁内断言值全部来自参考测试
// 或参考实现语义（无自造场景）。各 TEST 上方逐条标注出处。
#include <gtest/gtest.h>

#include "dqGeom/BSplineCurve.h"
#include "dqGeom/BezierCurve3d.h"
#include "dqGeom/KnotVector.h"
#include "dqGeom/LineString3d.h"
#include "dqGeom/Point3d.h"
#include "dqGeom/Range3d.h"
#include "dqGeom/StrokeOptions.h"

#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE
namespace {

// Checker.testPoint3d 的最小对应（参考 Checker.ts——assert 两点 within tolerance）。
::testing::AssertionResult PointNear(Point3d const& a, Point3d const& b,
                                     double tol = 1.0e-9, char const* msg = "")
{
    if (a.Distance(b) <= tol)
        return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure()
        << msg << " (" << a.x << "," << a.y << "," << a.z << ") vs ("
        << b.x << "," << b.y << "," << b.z << ")";
}

}  // namespace

// Ported from: core/geometry/src/test/bspline/BsplineCurve.test.ts
//              it("evaluatePointInSpan1")（:219-240）——四极点 ×[0,0,0,1,1,1]
// 的 clamped 三次曲线单 span 内分点钉值（参考断言值原样；order=degree+1）。
TEST(BSplineCurveTest, EvaluatePointInSpan1)
{
    int const degree = 3;
    std::vector<Point3d> const poleArray = {
        Point3d::From(0, 0), Point3d::From(1, 2), Point3d::From(3, 2), Point3d::From(4, 0)};
    std::vector<double> const knotArray = {0, 0, 0, 1, 1, 1};
    auto bspline = BSplineCurve3d::create(poleArray, knotArray, degree + 1);
    ASSERT_TRUE(bspline.IsValid());

    Point3d const p0 = bspline->evaluatePointInSpan(0, 0.0);
    Point3d const p1 = bspline->evaluatePointInSpan(0, 0.5);
    Point3d const p2 = bspline->evaluatePointInSpan(0, 1.0);
    EXPECT_TRUE(PointNear(p0, Point3d::From(0, 0, 0)));
    EXPECT_TRUE(PointNear(p1, Point3d::From(2, 1.5, 0)));
    EXPECT_TRUE(PointNear(p2, Point3d::From(4, 0, 0)));
}

// Ported from: core/geometry/src/test/bspline/BsplineCurve.test.ts
//              it("evaluatePointInSpan2")（:241-261）——相邻 span 在重结处同点。
TEST(BSplineCurveTest, EvaluatePointInSpan2)
{
    int const degree = 3;
    std::vector<Point3d> const poleArray = {
        Point3d::From(0, 0), Point3d::From(1, 2), Point3d::From(3, 2),
        Point3d::From(4, 0), Point3d::From(5, 2)};
    std::vector<double> const knotArray = {0, 0, 0, 0.5, 1, 1, 1};
    auto bspline = BSplineCurve3d::create(poleArray, knotArray, degree + 1);
    ASSERT_TRUE(bspline.IsValid());

    Point3d const p0 = bspline->evaluatePointInSpan(0, 1.0);
    Point3d const p1 = bspline->evaluatePointInSpan(1, 0.0);
    EXPECT_TRUE(PointNear(p0, p1, 1.0e-9,
                          "both adjacent spans yield the same Point3d at spanFraction equal to a knot"));
}

// Ported from: core/geometry/src/test/bspline/BsplineCurve.test.ts
//              it("HelloWorld")（:262-318）的标量子集——greville 极点的常数
// y=b 特性（x 与结值精确一致[均匀钳位结]）+ curveLength ≤ quickLength。
// 3dH/rational 臂未移植（消费面缺席）；两 span 相邻同点腿保留。
TEST(BSplineCurveTest, HelloWorldConstantYAndLengthBound)
{
    for (int order = 2; order <= 5; ++order) {
        double const b = 2.0;
        int const degree = order - 1;
        int const numPoles = 5;
        auto knots = KnotVector::createUniformClamped(numPoles, degree, 0.0, 1.0);
        std::vector<Point3d> points;
        for (int i = 0; i < numPoles; ++i) {
            double const x = knots.grevilleKnot(i);
            points.push_back(Point3d::From(x, b, 0));
        }
        auto curve = BSplineCurve3d::createUniformKnots(points, order);
        ASSERT_TRUE(curve.IsValid());
        double const arcLength = curve->CurveLength();
        EXPECT_LE(arcLength, curve->QuickLength() + 1.0e-6) << "order " << order;
        for (int span = 0; span < curve->numSpan(); ++span) {
            Point3d const p1 = curve->evaluatePointInSpan(span, 0.5);
            EXPECT_NEAR(b, p1.y, 1.0e-9) << "constant bspline y, order " << order;
            if (span + 1 < curve->numSpan()) {
                Point3d const p2 = curve->evaluatePointInSpan(span, 1.0);
                Point3d const q2 = curve->evaluatePointInSpan(span + 1, 0.0);
                EXPECT_TRUE(PointNear(p2, q2, 1.0e-9, "span match")) << "order " << order;
            }
        }
    }
}

// Ported from: core/geometry/src/test/bspline/BsplineCurve.test.ts
//              it("KnotVector")（:356-375）——createUniformClamped 的
// 基数组长度/knotLength01/逐 span 左结与 span 长度对账（断言面原样）。
TEST(KnotVectorTest, UniformClampedSpanAccounting)
{
    int const numPoles = 10;
    int const degree = 3;
    double const a0 = 1.0;
    double const a1 = 22.0;
    auto knots = KnotVector::createUniformClamped(numPoles, degree, a0, a1);
    auto basisValues = knots.createBasisArray();
    EXPECT_EQ(basisValues.size(), static_cast<size_t>(degree) + 1);
    EXPECT_DOUBLE_EQ(knots.knotLength01(), a1 - a0);
    for (int spanIndex = 0; spanIndex < knots.numSpans(); ++spanIndex) {
        double const leftKnotFromSpan = knots.spanFractionToKnot(spanIndex, 0.0);
        double const rightKnotFromSpan = knots.spanFractionToKnot(spanIndex, 1.0);
        double const leftKnotFromArray =
            knots.knots[static_cast<size_t>(spanIndex + knots.leftKnotIndex())];
        EXPECT_DOUBLE_EQ(leftKnotFromArray, leftKnotFromSpan);
        EXPECT_DOUBLE_EQ(knots.spanIndexToSpanLength(spanIndex),
                         rightKnotFromSpan - leftKnotFromSpan);
    }
}

// Ported from: core/geometry/src/test/bspline/BsplineCurve.test.ts
//              it("StrokeParabola")（:764-812）的判定子集——孤立 Bezier 与
// B 样条包络的同一抛物线在相同 options 下**描点数精确一致**（options 七态
// 裁剪至消费面：undefined/createForCurves/chordTol 0.2/chordTol 0.02/
// maxEdgeLength 10——angle 5°/2.5° 两态依赖 Angle.createDegrees 面，语义同
// chord 态已覆盖，TODO 随 Angle 备齐回加）；极点集三组原样。
TEST(BSplineStrokeTest, StrokeParabolaMatchesIsolatedBezier)
{
    double const x2 = 100.0;
    std::vector<StrokeOptions> allOptions;
    allOptions.emplace_back();                       // i===0: undefined（默认构造）
    allOptions.push_back(StrokeOptions::CreateForCurves());  // i===1
    {
        StrokeOptions o = StrokeOptions::CreateForCurves();
        o.chordTol = 0.2;                            // i===5
        allOptions.push_back(o);
    }
    {
        StrokeOptions o = StrokeOptions::CreateForCurves();
        o.chordTol = 0.02;                           // i===6
        allOptions.push_back(o);
    }
    {
        StrokeOptions o = StrokeOptions::CreateForCurves();
        o.maxEdgeLength = 10.0;                      // i===7
        allOptions.push_back(o);
    }

    for (double x1 : {30.0, 50.0, 80.0, 95.0}) {
        for (double y2 : {10.0, 40.0, 80.0}) {
            std::vector<Point3d> const poles = {
                Point3d::From(0, 0, 0), Point3d::From(x1, 0, 0), Point3d::From(x2, y2, 0)};
            auto bcurve = BSplineCurve3d::createUniformKnots(poles, 3);
            auto bezier = BezierCurve3d::create(poles);
            ASSERT_TRUE(bcurve.IsValid());
            ASSERT_TRUE(bezier.IsValid());
            for (auto const& options : allOptions) {
                LineString3d strokes;
                LineString3d bezierStrokes;
                bcurve->EmitStrokes(strokes, options);
                bezier->EmitStrokes(bezierStrokes, options);
                EXPECT_EQ(strokes.PointCount(), bezierStrokes.PointCount())
                    << "bezier stroke counts the same for isolated and bspline"
                    << " (x1=" << x1 << " y2=" << y2 << ")";
            }
        }
    }
}

// Ported from: core/geometry/src/test/bspline/BsplineCurve.test.ts
//              it("StrokeWithKnotsA")（:719-731）——10 极点 × 重结向量
// [0,0,0,1,1,2,2,3,3,4,4,4]（order 4）的 chordTol=0.01 描线存活 + 点数下限
// （参考仅存档描点，无计数断言——DanQing 以 ≥3 点存活为最小存活判定面，
// Authored 判据注记）。
TEST(BSplineStrokeTest, StrokeWithKnotsA)
{
    std::vector<Point3d> poles;
    for (int i = 0; i < 10; ++i)
        poles.push_back(Point3d::From(i, i * i, 0));
    std::vector<double> const knots = {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4};
    auto bcurve = BSplineCurve3d::create(poles, knots, 4);
    ASSERT_TRUE(bcurve.IsValid());
    StrokeOptions options;
    options.chordTol = 0.01;
    LineString3d strokes;
    bcurve->EmitStrokes(strokes, options);
    EXPECT_GE(strokes.PointCount(), 3u);
}

// Authored: no reference test exists in itwinjs-core for the AspectRatioSkew
// consumption shape (AspectRatioSkewDecorator.ts:23-40 构造形态的几何合同——
// 参考装饰器为交互会话消费面，无离线对应测试)。三极点 [l,c]/[c,h]/[h,c] ×
// knots [0,0,0,1,1,1] × order 3：**EmitStrokes 容差细分语义**——chordTol 减半
// → 描点数增（AspectRatioSkew 演示"细节层次随 skew/精度调整"的引擎侧保证）；
// 端点 = 首/末极点（clamped 曲线过端极点）；range = 极点保守超集。
TEST(BSplineStrokeTest, AspectRatioSkewShapeChordToleranceRefines)
{
    // AspectRatioSkewDecorator.ts:29-33 的极点形态（无量纲归一——
    // l=(0,0,0) h=(100,100,100) c=(50,50,50)）。
    std::vector<Point3d> const poles = {
        Point3d::From(0.0, 0.0, 50.0),
        Point3d::From(50.0, 100.0, 50.0),
        Point3d::From(100.0, 50.0, 50.0),
    };
    std::vector<double> const knots = {0, 0, 0, 1, 1, 1};
    auto bcurve = BSplineCurve3d::create(poles, knots, 3);
    ASSERT_TRUE(bcurve.IsValid());

    // 端点 = 首末极点（clamped）。
    EXPECT_TRUE(PointNear(bcurve->StartPoint(), poles.front(), 1.0e-9, "clamped start = first pole"));
    EXPECT_TRUE(PointNear(bcurve->EndPoint(), poles.back(), 1.0e-9, "clamped end = last pole"));

    // range = 极点保守超集（BSplineCurve3d.extendRange 的极点面语义）。
    Range3d range = bcurve->Range();
    EXPECT_TRUE(range.ContainsXYZ(poles[1].x, poles[1].y, poles[1].z));

    // 容差细分：chordTol 收紧 → 描点数增。判据窗取 chord 主导域（探针实测
    // 本形态 angle/默认步长在 chordTol>0.5 时先饱和于 13 点——0.5→0.1 为
    // 13→25 的干净严格增长；判据域选择记录）。
    StrokeOptions coarse;
    coarse.chordTol = 0.5;
    StrokeOptions fine;
    fine.chordTol = 0.1;
    LineString3d coarseStrokes;
    LineString3d fineStrokes;
    bcurve->EmitStrokes(coarseStrokes, coarse);
    bcurve->EmitStrokes(fineStrokes, fine);
    EXPECT_GT(fineStrokes.PointCount(), coarseStrokes.PointCount())
        << "tightening chordTol must refine the stroke count";
}

// Ported from: core/geometry/src/test/bspline/BsplineCurve.test.ts
//              it("CreateBspline")（:150-217）的构造形态子集——两计数形态
// 识别（4 极点 × order 4：[0,0,0,1,1,1] 六结=modern[8==6+2]；
// [0,1/5..1] 六结同合法[参考第一组双构造全成]；八结=classic[8==8 skip]；
// 七结=无效[8!=7 且 8!=9]）+ 极点数/阶/span 对账。参考的 saveGeometry
// 目检面不移植；Checker 零错误判定以构造成功性 + 度量对账承载。
TEST(BSplineCurveTest, CreateBsplineFormRecognition)
{
    std::vector<Point3d> const poleArray = {
        Point3d::From(0, 0), Point3d::From(1, 2), Point3d::From(3, 2), Point3d::From(4, 0)};
    int const order = 4;  // 参考 create(poleArray, knotArray, degree + 1)

    // 现代形态：numPoles + order === numKnots + 2（4+4==6+2 ✓）。
    auto modern = BSplineCurve3d::create(poleArray, {0, 0, 0, 1, 1, 1}, order);
    ASSERT_TRUE(modern.IsValid());
    EXPECT_EQ(modern->numPoles(), 4);
    EXPECT_EQ(modern->order(), 4);
    EXPECT_EQ(modern->degree(), 3);
    EXPECT_EQ(modern->numSpan(), 1);  // numPoles - degree
    auto const knotsCopied = modern->copyKnots(false);
    ASSERT_EQ(knotsCopied.size(), static_cast<size_t>(6));
    EXPECT_DOUBLE_EQ(knotsCopied[0], 0.0);
    EXPECT_DOUBLE_EQ(knotsCopied[5], 1.0);

    // 参考第一组 knotArray1（均匀六结）同参合法（8==6+2 modern——参考
    // CreateBspline 首段双构造均存活的原样）。
    auto uniform = BSplineCurve3d::create(
        poleArray, {0, 1.0 / 5.0, 2.0 / 5.0, 3.0 / 5.0, 4.0 / 5.0, 1.0}, order);
    ASSERT_TRUE(uniform.IsValid());
    // 参考语义：numSpan = numPoles - degree = 1——span 计数是极点派生量，
    // 与结内部分段无关（不在此断言面）。
    EXPECT_EQ(uniform->numSpan(), modern->numSpan());

    // 经典过钳位形态：numPoles + order === numKnots（4+4==8 ✓——首末外结剥除）。
    auto classic = BSplineCurve3d::create(
        poleArray, {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0}, order);
    ASSERT_TRUE(classic.IsValid());
    EXPECT_EQ(classic->numPoles(), 4);  // 极点数不随结剥除变（参考 :612-616 语义）
    EXPECT_EQ(classic->numSpan(), 1);
    // 剥除后存储结与现代形态同（classic copyKnots 回 8 值）。
    auto const classicKnots = classic->copyKnots(true);
    ASSERT_EQ(classicKnots.size(), static_cast<size_t>(8));

    // 无效：numPoles + order 与 numKnots 两形态均不合（8 != 7 且 8 != 9）。
    auto invalid = BSplineCurve3d::create(
        poleArray, {0, 0, 0, 0.5, 1, 1, 1}, order);
    EXPECT_FALSE(invalid.IsValid());

    // 无效：numPoles < order（:609-610）。
    std::vector<Point3d> const twoPoles = {Point3d::From(0, 0), Point3d::From(1, 1)};
    EXPECT_FALSE(BSplineCurve3d::create(twoPoles, {0, 0, 0, 1, 1, 1}, 3).IsValid());
}

END_DQ_GEOM_NAMESPACE
