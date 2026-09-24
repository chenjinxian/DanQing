// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/test/render/GraphicPrimitive.test.ts
//              describe("GraphicPrimitive") / it("forwards to appropriate method with appropriate arguments")
// DanQing dqRender — addPrimitive dispatch test: each GraphicPrimitive variant forwards to the matching
// GraphicBuilder addXXX method with the appropriate (defaulted) arguments.
//
// 保真依据（CLAUDE.md §5）：逐行移植 GraphicPrimitive.test.ts。测试场景、边界条件、断言值全部来自参考；
// 仅适配类型（TS union→std::variant、RefPtr<const T> 持几何、span 签名）。CaptureBuilder 1:1 覆写全部 12 个
// addXXX（与参考 Builder 一致），测试数组 12 行覆盖 8 个类型（6 point + arc×3 + arc2d×3），path/loop/
// polyface/solidPrimitive 覆写存在但参考数组未行使其分支（1:1 不发明场景）。
#include "dqRender/GraphicBuilder.h"
#include "dqRender/GraphicPrimitive.h"
#include "dqRender/RenderGraphic.h"

#include <dqBase/RefCounted.h>
#include <dqCommon/ColorDef.h>
#include <dqCommon/LinePixels.h>
#include <dqGeom/Arc3d.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Path.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Polyface.h>
#include <dqGeom/SolidPrimitive.h>
#include <dqGeom/Transform.h>

#include <gtest/gtest.h>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

using namespace dqRender;
using namespace dqGeom;

namespace {

constexpr double kTol = 1.0e-9;

// toEqual equivalent: same variant type + per-alternative field comparison.
void expectEqual(const GraphicPrimitive& actual, const GraphicPrimitive& expected) {
    ASSERT_EQ(actual.index(), expected.index()) << "GraphicPrimitive variant type mismatch";
    std::visit([&](const auto& a) {
        using T = std::decay_t<decltype(a)>;
        const T* b = std::get_if<T>(&expected);
        ASSERT_NE(b, nullptr);
        if constexpr (std::is_same_v<T, GraphicLineString> ||
                      std::is_same_v<T, GraphicPointString> ||
                      std::is_same_v<T, GraphicShape>) {
            ASSERT_EQ(a.points.size(), b->points.size());
            for (size_t i = 0; i < a.points.size(); ++i) {
                EXPECT_NEAR(a.points[i].x, b->points[i].x, kTol);
                EXPECT_NEAR(a.points[i].y, b->points[i].y, kTol);
                EXPECT_NEAR(a.points[i].z, b->points[i].z, kTol);
            }
        } else if constexpr (std::is_same_v<T, GraphicLineString2d> ||
                             std::is_same_v<T, GraphicPointString2d> ||
                             std::is_same_v<T, GraphicShape2d>) {
            EXPECT_NEAR(a.zDepth, b->zDepth, kTol);
            ASSERT_EQ(a.points.size(), b->points.size());
            for (size_t i = 0; i < a.points.size(); ++i) {
                EXPECT_NEAR(a.points[i].x, b->points[i].x, kTol);
                EXPECT_NEAR(a.points[i].y, b->points[i].y, kTol);
            }
        } else if constexpr (std::is_same_v<T, GraphicArc>) {
            EXPECT_EQ(a.arc.Get(), b->arc.Get());
            EXPECT_EQ(a.isEllipse, b->isEllipse);
            EXPECT_EQ(a.filled, b->filled);
        } else if constexpr (std::is_same_v<T, GraphicArc2d>) {
            EXPECT_EQ(a.arc.Get(), b->arc.Get());
            EXPECT_EQ(a.isEllipse, b->isEllipse);
            EXPECT_EQ(a.filled, b->filled);
            EXPECT_NEAR(a.zDepth, b->zDepth, kTol);
        } else if constexpr (std::is_same_v<T, GraphicPath>) {
            EXPECT_EQ(a.path.Get(), b->path.Get());
        } else if constexpr (std::is_same_v<T, GraphicLoop>) {
            EXPECT_EQ(a.loop.Get(), b->loop.Get());
        } else if constexpr (std::is_same_v<T, GraphicPolyface>) {
            EXPECT_EQ(a.polyface.Get(), b->polyface.Get());
            EXPECT_EQ(a.filled, b->filled);
        } else if constexpr (std::is_same_v<T, GraphicSolidPrimitive>) {
            EXPECT_EQ(a.solidPrimitive.Get(), b->solidPrimitive.Get());
        }
    }, actual);
}

// Ported from: GraphicPrimitive.test.ts class Builder (lines 14-53).
// Overrides every addXXX to capture the reconstructed GraphicPrimitive (1:1 with the TS overrides that
// bypass the default geometry building and record the forwarded arguments).
class CaptureBuilder : public GraphicBuilder {
public:
    CaptureBuilder()
        : GraphicBuilder(GraphicBuilderOptions{.type = GraphicType::Scene}) {}

    std::optional<GraphicPrimitive> m_last;

    // set primitive(p): expect undefined before set (1:1 test setter guard against double-dispatch).
    void set(GraphicPrimitive p) {
        ASSERT_FALSE(m_last.has_value()) << "addPrimitive dispatched more than once before expectPrimitive";
        m_last = std::move(p);
    }

    // --- 3d virtuals (1:1 addLineString/addPointString/addShape overrides) ---
    void addLineString(const Point3d* pts, size_t n) override {
        set(GraphicLineString{ std::vector<Point3d>(pts, pts + n) });
    }
    void addPointString(const Point3d* pts, size_t n) override {
        set(GraphicPointString{ std::vector<Point3d>(pts, pts + n) });
    }
    void addShape(const Point3d* pts, size_t n) override {
        set(GraphicShape{ std::vector<Point3d>(pts, pts + n) });
    }
    // --- 2d virtuals (1:1 addLineString2d/addPointString2d/addShape2d overrides; bypass the lift) ---
    void addLineString2d(const Point2d* pts, size_t n, double zDepth) override {
        set(GraphicLineString2d{ GraphicPrimitive2d{zDepth}, std::vector<Point2d>(pts, pts + n) });
    }
    void addPointString2d(const Point2d* pts, size_t n, double zDepth) override {
        set(GraphicPointString2d{ GraphicPrimitive2d{zDepth}, std::vector<Point2d>(pts, pts + n) });
    }
    void addShape2d(const Point2d* pts, size_t n, double zDepth) override {
        set(GraphicShape2d{ GraphicPrimitive2d{zDepth}, std::vector<Point2d>(pts, pts + n) });
    }
    // --- arc virtuals (1:1 addArc/addArc2d overrides; bypass Loop/Path building) ---
    void addArc(const Arc3d& ellipse, bool isEllipse, bool filled) override {
        set(GraphicArc{ dqBase::RefPtr<const Arc3d>(&ellipse), isEllipse, filled });
    }
    void addArc2d(const Arc3d& ellipse, bool isEllipse, bool filled, double zDepth) override {
        set(GraphicArc2d{ dqBase::RefPtr<const Arc3d>(&ellipse), isEllipse, filled, zDepth });
    }
    // --- path/loop/polyface/solid virtuals (1:1 overrides present; not exercised by the reference array) ---
    void addPath(const Path& path) override {
        set(GraphicPath{ dqBase::RefPtr<const Path>(&path) });
    }
    void addLoop(const Loop& loop) override {
        set(GraphicLoop{ dqBase::RefPtr<const Loop>(&loop) });
    }
    void addPolyface(const Polyface& polyface, bool filled) override {
        set(GraphicPolyface{ dqBase::RefPtr<const Polyface>(&polyface), filled });
    }
    void addSolidPrimitive(const SolidPrimitive& solidPrimitive) override {
        set(GraphicSolidPrimitive{ dqBase::RefPtr<const SolidPrimitive>(&solidPrimitive) });
    }
    // setSymbology/activateGraphicParams/setBlankingFill inherited (concrete on the GraphicBuilder base).

    RenderGraphic* finish() override { return nullptr; }

    // expectPrimitive(expected): assert last captured + structural equality, then reset (1:1 test).
    void expectPrimitive(const GraphicPrimitive& expected) {
        ASSERT_TRUE(m_last.has_value()) << "no primitive was forwarded by addPrimitive";
        expectEqual(*m_last, expected);
        m_last.reset();
    }
};

}  // namespace

// Ported from: GraphicPrimitive.test.ts it("forwards to appropriate method with appropriate arguments") (55-83).
TEST(GraphicPrimitive, ForwardsToAppropriateMethodWithAppropriateArguments) {
    const std::vector<Point3d> points = { Point3d::From(0, 1, 2) };
    const std::vector<Point2d> pts2d = { Point2d::From(3, 4) };
    const double zDepth = 42.0;
    // Reference uses Arc3d.createXYZXYZXYZ(1..9); DanQing exposes CreateXY (geometry is irrelevant to this
    // dispatch test — only pointer identity + forwarded bools/zDepth are asserted).
    const auto arc = Arc3d::CreateXY(Point3d::FromZero(), 1.0);

    // Each row: (input, expected). When expected is engaged it is the default-normalized form addPrimitive
    // must forward (arc/arc2d with omitted bools → isEllipse=false, filled=false); when disengaged the input
    // itself is expected verbatim (test[1] ?? test[0] in the reference).
    struct Row {
        GraphicPrimitive input;
        std::optional<GraphicPrimitive> expected;
    };
    const std::vector<Row> tests = {
        { GraphicLineString{ points }, {} },
        { GraphicLineString2d{ GraphicPrimitive2d{zDepth}, pts2d }, {} },
        { GraphicPointString{ points }, {} },
        { GraphicPointString2d{ GraphicPrimitive2d{zDepth}, pts2d }, {} },
        { GraphicShape{ points }, {} },
        { GraphicShape2d{ GraphicPrimitive2d{zDepth}, pts2d }, {} },
        { GraphicArc{ dqBase::RefPtr<const Arc3d>(arc) },
          GraphicArc{ dqBase::RefPtr<const Arc3d>(arc), false, false } },
        { GraphicArc{ dqBase::RefPtr<const Arc3d>(arc), true, true }, {} },
        { GraphicArc{ dqBase::RefPtr<const Arc3d>(arc), false, false }, {} },
        { GraphicArc2d{ dqBase::RefPtr<const Arc3d>(arc), false, false, zDepth },
          GraphicArc2d{ dqBase::RefPtr<const Arc3d>(arc), false, false, zDepth } },
        { GraphicArc2d{ dqBase::RefPtr<const Arc3d>(arc), true, true, zDepth }, {} },
        { GraphicArc2d{ dqBase::RefPtr<const Arc3d>(arc), false, false, zDepth }, {} },
    };

    CaptureBuilder builder;
    for (const auto& test : tests) {
        builder.addPrimitive(test.input);
        builder.expectPrimitive(test.expected ? *test.expected : test.input);
    }
}
