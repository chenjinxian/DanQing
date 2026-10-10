// SPDX-License-Identifier: Apache-2.0
// dqCommon tests — RenderSchedule
// Ported from: itwinjs-core core/common/src/test/RenderSchedule.test.ts
//              describe("RenderSchedule") — 全例直移（ScriptBuilder/"produces
//              expected JSON" 的 deep-equal → C++ 逐字段对账；TS 的 JSON.parse/
//              stringify 变体循环 → 同构 props 变换循环）。

#include <gtest/gtest.h>

#include <dqBase/CompressedId64Set.h>

#include <dqCommon/RenderSchedule.h>

#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Point4d.h>
#include <dqGeom/Transform.h>

#include <string>
#include <vector>

using namespace dqCommon;
using namespace dqCommon::RenderSchedule;

namespace {

// makeTransform(aa, bb, cc, dd) — RenderSchedule.test.ts:340-349。
// 参考 props 为 4 行矩阵；Transform.setFromJSON（Transform.ts:86-93）的
// isArrayOfNumberArray(json, 3, 4) 门只读前 3 行——第 4 行 [0,0,0,dd] 系
// 不可观察的红鲱鱼，C++ props 3×4 形态直接承载前 3 行。
TransformProps makeTransform(double aa, double bb, double cc, double /*dd*/)
{
    TransformProps props;
    props.transform = std::array<std::array<double, 4>, 3>{{
        {{aa, 0.0, 0.0, 0.0}},
        {{0.0, bb, 0.0, 0.0}},
        {{0.0, 0.0, cc, 0.0}},
    }};
    return props;
}

// makeTransformComponents(pos, rot, pivot) — RenderSchedule.test.ts:351-357
TransformProps makeTransformComponents(double pos, double rot, double pivot)
{
    TransformProps props = makeTransform(0, 0, 0, 0);
    props.position = std::array<double, 3>{{pos, 0.0, 0.0}};
    props.orientation = std::array<double, 4>{{0.0, rot, 0.0, 0.0}};
    props.pivot = std::array<double, 3>{{0.0, 0.0, pivot}};
    return props;
}

// makeCuttingPlane(pos, dir, vis) — RenderSchedule.test.ts:378-385
CuttingPlaneProps makeCuttingPlane(double pos, double dir, bool vis = false, bool hid = false)
{
    CuttingPlaneProps props;
    props.position = std::array<double, 3>{{pos, 0.0, 0.0}};
    props.direction = std::array<double, 3>{{0.0, dir, 0.0}};
    if (vis)
        props.visible = true;
    if (hid)
        props.hidden = true;
    return props;
}

dqBase::DqString compressIds(std::initializer_list<const char*> ids)
{
    dqBase::DqIdSet set;
    for (auto const* s : ids)
        set.insert(dqBase::DqId::FromString(s));
    return dqBase::CompressedId64Set::SortAndCompress(set);
}

// 指定初始化（C++20 designator）不可命名基类成员（TimelineEntryProps::time），
// 用助手函数承载。
VisibilityEntryProps visProps(double time)
{
    VisibilityEntryProps p;
    p.time = time;
    return p;
}

ColorEntryProps colorProps(double time)
{
    ColorEntryProps p;
    p.time = time;
    return p;
}

TransformEntryProps transformPropsTime(double time)
{
    TransformEntryProps p;
    p.time = time;
    return p;
}

CuttingPlaneEntryProps cuttingPropsTime(double time)
{
    CuttingPlaneEntryProps p;
    p.time = time;
    return p;
}

}  // namespace

// Ported from: itwinjs-core RenderSchedule.test.ts
//              it("interpolates transforms")
TEST(RenderScheduleTest, InterpolatesTransforms)
{
    // 参考 props：7 个 transform entry，每个同时带 transform 行 + components
    // （pivot/orientation/position 全在场——插值走 components 四元数路径；
    // fraction==0 的端点走 value=Transform.fromJSON(transform 行) 路径）。
    auto const entry = [](double time, std::array<double, 4> const& orientation,
                          std::array<double, 3> const& position,
                          std::array<std::array<double, 4>, 3> const& rows,
                          Interpolation interp) {
        TransformEntryProps p;
        p.time = time;
        p.interpolation = interp;
        TransformProps val;
        val.pivot = std::array<double, 3>{{63533.371094, -16860.751953, 22829.71875}};
        val.orientation = orientation;
        val.position = position;
        val.transform = rows;
        p.value = val;
        return p;
    };

    using Rows = std::array<std::array<double, 4>, 3>;
    std::vector<TransformEntryProps> timeline;
    timeline.push_back(entry(1439798400,
                             std::array<double, 4>{{0, 0, 0, 1}},
                             std::array<double, 3>{{-55672.671875, 25451.609375, -22869.257813}},
                             Rows{{{1, 0, 0, 7860.699219}, {0, 1, 0, 8590.857422}, {0, 0, 1, -39.539063}}},
                             Interpolation::Linear));
    timeline.push_back(entry(1439798460,
                             std::array<double, 4>{{0, 0, 0, 1}},
                             std::array<double, 3>{{-55652.878906, 25445.414063, -22869.257813}},
                             Rows{{{1, 0, 0, 7880.492188}, {0, 1, 0, 8584.662109}, {0, 0, 1, -39.539063}}},
                             Interpolation::Linear));
    timeline.push_back(entry(1439890920,
                             std::array<double, 4>{{0, 0, 0, 1}},
                             std::array<double, 3>{{-25152.789063, 15897.631836, -22869.257813}},
                             Rows{{{1, 0, 0, 38380.582031}, {0, 1, 0, -963.120117}, {0, 0, 1, -39.539063}}},
                             Interpolation::Linear));
    timeline.push_back(entry(1439904180,
                             std::array<double, 4>{{-1.109122e-8, -3.543056e-8, -0.228533, 0.973536}},
                             std::array<double, 3>{{-20778.666016, 14528.351563, -22869.257813}},
                             Rows{{{0.895545, -0.444971, 7.405529e-8, 43620.875},
                                   {0.444971, 0.895545, -5.401287e-9, 27699.283203},
                                   {-6.391644e-8, 3.778954e-8, 1, -39.543762}}},
                             Interpolation::Linear));
    timeline.push_back(entry(1439982060,
                             std::array<double, 4>{{-1.113305e-8, -3.556418e-8, -0.229395, 0.973333}},
                             std::array<double, 3>{{5798.800781, 18796.800781, -22869.259766}},
                             Rows{{{0.894756, -0.446556, 7.433935e-8, 70174.921875},
                                   {0.446556, 0.894756, -5.35584e-9, 32081.748047},
                                   {-6.412388e-8, 3.798884e-8, 1, -39.545731}}},
                             Interpolation::Linear));
    timeline.push_back(entry(1439996640,
                             std::array<double, 4>{{0.005562, 0.077206, 0.104044, 0.991556}},
                             std::array<double, 3>{{10774.503906, 19597.402344, -22869.259766}},
                             Rows{{{0.966428, 0.20719, -0.15195, 65212.59375},
                                   {-0.205472, 0.978288, 0.027096, -9333.005859},
                                   {0.154265, 0.005035, 0.988017, 9402.956055}}},
                             Interpolation::Linear));
    timeline.push_back(entry(1440172740,
                             std::array<double, 4>{{0.005575, 0.077382, 0.104837, 0.991459}},
                             std::array<double, 3>{{63098.109375, -10204.736328, -13983.59082}},
                             Rows{{{0.966043, 0.208746, -0.152273, 117478.09375},
                                   {-0.207021, 0.977956, 0.02728, -39223.738281},
                                   {0.15461, 0.00517, 0.987962, 18307.048828}}},
                             Interpolation::Step));

    ScriptProps props;
    ModelTimelineProps model;
    model.modelId = "0x123";
    ElementTimelineProps element;
    element.batchId = 1;
    element.elementIds = dqBase::DqIdVector();
    element.transformTimeline = timeline;
    model.elementTimelines = std::vector<ElementTimelineProps>{element};
    props.push_back(model);

    auto script = Script::fromJSON(props);
    ASSERT_TRUE(script);

    auto expectTransform = [&](double timepoint,
                               std::array<std::array<double, 4>, 3> const& expected) {
        auto const actual = script->getTransform("0x123", 1, timepoint);
        ASSERT_TRUE(actual.has_value());
        EXPECT_TRUE(actual->IsAlmostEqual(dqGeom::Transform::FromRows(expected), 1.0e-6))
            << "timepoint=" << timepoint;
    };

    expectTransform(script->duration.low, {{
        {{1, 0, 0, 7860.699219}},
        {{0, 1, 0, 8590.857422}},
        {{0, 0, 1, -39.539063}},
    }});

    expectTransform(script->duration.high, {{
        {{0.966043, 0.208746, -0.152273, 117478.09375}},
        {{-0.207021, 0.977956, 0.02728, -39223.738281}},
        {{0.15461, 0.00517, 0.987962, 18307.048828}},
    }});

    expectTransform(script->duration.FractionToPoint(0.5), {{
        {{0.9542084100696944, -0.29673855635648877, -0.037848901222266286, 71759.88767870933}},
        {{0.29684190212797457, 0.9549219611634173, -0.002988850758880856, 21679.967974600604}},
        {{0.03702965424240229, -0.008383153301704048, 0.9992790038059481, 2437.9638765390373}},
    }});
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              it("interpolates visibility")
TEST(RenderScheduleTest, InterpolatesVisibility)
{
    TimelineProps props;
    std::vector<VisibilityEntryProps> timeline;
    auto const entry = [](double time, std::optional<double> value, Interpolation interp) {
        VisibilityEntryProps p;
        p.time = time;
        p.value = value;
        p.interpolation = interp;
        return p;
    };
    timeline.push_back(entry(1254330000, std::nullopt, Interpolation::Linear));
    timeline.push_back(entry(1369123200, 70, Interpolation::Linear));
    timeline.push_back(entry(1369123260, 69, Interpolation::Linear));
    timeline.push_back(entry(1370710740, 30, Interpolation::Step));
    props.visibilityTimeline = timeline;

    Timeline const tl(props);
    ASSERT_TRUE(tl.visibility.has_value());
    auto const& vis = *tl.visibility;
    for (size_t i = 0; i < vis.length(); i++)
        EXPECT_DOUBLE_EQ(tl.getVisibility(vis.getEntry(i)->time), vis.getValue(i));

    EXPECT_DOUBLE_EQ(tl.getVisibility(tl.duration.low), 100.0);
    EXPECT_DOUBLE_EQ(tl.getVisibility(tl.duration.high), 30.0);
    EXPECT_DOUBLE_EQ(tl.getVisibility(tl.duration.low * 0.5), 100.0);
    EXPECT_DOUBLE_EQ(tl.getVisibility(tl.duration.high * 2.0), 30.0);

    for (size_t j = 0; j < 3; j++) {
        auto const& a = (*props.visibilityTimeline)[j];
        auto const& b = (*props.visibilityTimeline)[j + 1];
        double const time = a.time + (b.time - a.time) / 2.0;
        double const value = (a.value.value_or(100.0)) +
                             ((b.value.value_or(100.0)) - (a.value.value_or(100.0))) / 2.0;
        EXPECT_DOUBLE_EQ(tl.getVisibility(time), value);
    }
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              describe("ScriptBuilder") / it("sorts and compresses element Ids")
TEST(RenderScheduleScriptBuilderTest, SortsAndCompressesElementIds)
{
    auto const expectIds = [](std::variant<dqBase::DqIdVector, dqBase::DqString> input,
                              char const* expected) {
        ScriptBuilder script;
        auto& model = script.addModelTimeline("0x123");
        ElementTimelineBuilder* element = nullptr;
        if (auto const* str = std::get_if<dqBase::DqString>(&input))
            element = &model.addElementTimeline(*str);
        else
            element = &model.addElementTimeline(*std::get_if<dqBase::DqIdVector>(&input));
        EXPECT_EQ(element->elementIds, expected);
    };

    expectIds(dqBase::DqString("+1+4"), "+1+4");
    expectIds(dqBase::DqIdVector{dqBase::DqId::FromString("0x1"), dqBase::DqId::FromString("0x5")},
              "+1+4");
    expectIds(dqBase::DqIdVector{dqBase::DqId::FromString("0x5"), dqBase::DqId::FromString("0x1")},
              "+1+4");
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              it("assigns unique consecutive batch Ids")
TEST(RenderScheduleScriptBuilderTest, AssignsUniqueConsecutiveBatchIds)
{
    ScriptBuilder script;
    auto& modelA = script.addModelTimeline("0xa");
    auto& modelB = script.addModelTimeline("0xb");

    auto& elemA1 = modelA.addElementTimeline(dqBase::DqIdVector{dqBase::DqId::FromString("0xa1")});
    auto& elemB1 = modelB.addElementTimeline(dqBase::DqIdVector{dqBase::DqId::FromString("0xb1")});
    auto& elemB2 = modelB.addElementTimeline(dqBase::DqIdVector{dqBase::DqId::FromString("0xb2")});
    auto& elemA2 = modelA.addElementTimeline(dqBase::DqIdVector{dqBase::DqId::FromString("0xa2")});

    EXPECT_EQ(elemA1.batchId, 1u);
    EXPECT_EQ(elemB1.batchId, 2u);
    EXPECT_EQ(elemB2.batchId, 3u);
    EXPECT_EQ(elemA2.batchId, 4u);
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              it("produces expected JSON")——deep.equal → 逐字段对账。
TEST(RenderScheduleScriptBuilderTest, ProducesExpectedJson)
{
    ScriptBuilder script;
    auto& model1 = script.addModelTimeline("0x1");
    model1.addVisibility(100, std::nullopt);
    model1.addVisibility(200, 50);

    auto& elem1 = model1.addElementTimeline(dqBase::DqString("+1+4"));
    elem1.addColor(300, RgbColor(1, 2, 3), Interpolation::Step);

    auto& model2 = script.addModelTimeline("0x2");
    model2.realityModelUrl = "https://google.com";

    auto& elem2 = model2.addElementTimeline(dqBase::DqIdVector{dqBase::DqId::FromString("0xabc")});
    elem2.addTransform(400, dqGeom::Transform::CreateTranslation(4, 5, 6));
    elem2.addTransform(400 + 100,
                       dqGeom::Transform::CreateTranslation(4, 5, 6),
                       TransformComponentsProps{
                           std::array<double, 3>{{8, 9, 0}},
                           std::array<double, 4>{{4, 5, 6, 7}},
                           std::array<double, 3>{{1, 2, 3}},
                       });

    auto& elem3 = model2.addElementTimeline(dqBase::DqIdVector{dqBase::DqId::FromString("0xdef")});
    // 参考：addCuttingPlane(600, { position: Point3d(1,2,3), direction: Point3d(0,0,-1), visible: true })
    CuttingPlaneProps plane600;
    plane600.position = std::array<double, 3>{{1, 2, 3}};
    plane600.direction = std::array<double, 3>{{0, 0, -1}};
    plane600.visible = true;
    elem3.addCuttingPlane(600, plane600);

    auto const props = script.finish();

    ASSERT_EQ(props.size(), 2u);

    // ── model1 ──
    auto const& m1 = props[0];
    EXPECT_EQ(m1.modelId, "0x1");
    ASSERT_TRUE(m1.visibilityTimeline.has_value());
    ASSERT_EQ(m1.visibilityTimeline->size(), 2u);
    EXPECT_DOUBLE_EQ((*m1.visibilityTimeline)[0].time, 100.0);
    EXPECT_EQ((*m1.visibilityTimeline)[0].interpolation, Interpolation::Linear);
    EXPECT_FALSE((*m1.visibilityTimeline)[0].value.has_value());
    EXPECT_DOUBLE_EQ((*m1.visibilityTimeline)[1].time, 200.0);
    EXPECT_EQ((*m1.visibilityTimeline)[1].interpolation, Interpolation::Linear);
    ASSERT_TRUE((*m1.visibilityTimeline)[1].value.has_value());
    EXPECT_DOUBLE_EQ(*(*m1.visibilityTimeline)[1].value, 50.0);
    EXPECT_FALSE(m1.colorTimeline.has_value());
    EXPECT_FALSE(m1.transformTimeline.has_value());
    EXPECT_FALSE(m1.cuttingPlaneTimeline.has_value());
    ASSERT_TRUE(m1.elementTimelines.has_value());
    ASSERT_EQ(m1.elementTimelines->size(), 1u);
    auto const& e1 = (*m1.elementTimelines)[0];
    EXPECT_EQ(e1.batchId, 1u);
    ASSERT_TRUE(std::holds_alternative<dqBase::DqString>(e1.elementIds));
    EXPECT_EQ(std::get<dqBase::DqString>(e1.elementIds), "+1+4");
    ASSERT_TRUE(e1.colorTimeline.has_value());
    ASSERT_EQ(e1.colorTimeline->size(), 1u);
    EXPECT_DOUBLE_EQ((*e1.colorTimeline)[0].time, 300.0);
    EXPECT_EQ((*e1.colorTimeline)[0].interpolation, Interpolation::Step);
    ASSERT_TRUE((*e1.colorTimeline)[0].value.has_value());
    EXPECT_EQ((*e1.colorTimeline)[0].value->r, 1);
    EXPECT_EQ((*e1.colorTimeline)[0].value->g, 2);
    EXPECT_EQ((*e1.colorTimeline)[0].value->b, 3);
    EXPECT_FALSE(e1.visibilityTimeline.has_value());

    // ── model2 ──
    auto const& m2 = props[1];
    EXPECT_EQ(m2.modelId, "0x2");
    ASSERT_TRUE(m2.realityModelUrl.has_value());
    EXPECT_EQ(*m2.realityModelUrl, "https://google.com");
    EXPECT_FALSE(m2.visibilityTimeline.has_value());
    ASSERT_TRUE(m2.elementTimelines.has_value());
    ASSERT_EQ(m2.elementTimelines->size(), 2u);

    auto const& e2 = (*m2.elementTimelines)[0];
    EXPECT_EQ(e2.batchId, 2u);
    EXPECT_EQ(std::get<dqBase::DqString>(e2.elementIds), "+ABC");
    ASSERT_TRUE(e2.transformTimeline.has_value());
    ASSERT_EQ(e2.transformTimeline->size(), 2u);
    EXPECT_DOUBLE_EQ((*e2.transformTimeline)[0].time, 400.0);
    EXPECT_EQ((*e2.transformTimeline)[0].interpolation, Interpolation::Linear);
    ASSERT_TRUE((*e2.transformTimeline)[0].value.has_value());
    auto const& v0 = *(*e2.transformTimeline)[0].value;
    ASSERT_TRUE(v0.transform.has_value());
    EXPECT_DOUBLE_EQ((*v0.transform)[0][3], 4.0);
    EXPECT_DOUBLE_EQ((*v0.transform)[1][3], 5.0);
    EXPECT_DOUBLE_EQ((*v0.transform)[2][3], 6.0);
    EXPECT_FALSE(v0.pivot.has_value());
    EXPECT_FALSE(v0.orientation.has_value());
    EXPECT_FALSE(v0.position.has_value());

    EXPECT_DOUBLE_EQ((*e2.transformTimeline)[1].time, 500.0);
    auto const& v1 = *(*e2.transformTimeline)[1].value;
    ASSERT_TRUE(v1.pivot.has_value());
    EXPECT_DOUBLE_EQ((*v1.pivot)[0], 1.0);
    EXPECT_DOUBLE_EQ((*v1.pivot)[1], 2.0);
    EXPECT_DOUBLE_EQ((*v1.pivot)[2], 3.0);
    ASSERT_TRUE(v1.orientation.has_value());
    EXPECT_DOUBLE_EQ((*v1.orientation)[0], 4.0);
    EXPECT_DOUBLE_EQ((*v1.orientation)[1], 5.0);
    EXPECT_DOUBLE_EQ((*v1.orientation)[2], 6.0);
    EXPECT_DOUBLE_EQ((*v1.orientation)[3], 7.0);
    ASSERT_TRUE(v1.position.has_value());
    EXPECT_DOUBLE_EQ((*v1.position)[0], 8.0);
    EXPECT_DOUBLE_EQ((*v1.position)[1], 9.0);
    EXPECT_DOUBLE_EQ((*v1.position)[2], 0.0);
    ASSERT_TRUE(v1.transform.has_value());
    EXPECT_DOUBLE_EQ((*v1.transform)[0][3], 4.0);
    EXPECT_DOUBLE_EQ((*v1.transform)[1][3], 5.0);
    EXPECT_DOUBLE_EQ((*v1.transform)[2][3], 6.0);

    auto const& e3 = (*m2.elementTimelines)[1];
    EXPECT_EQ(e3.batchId, 3u);
    EXPECT_EQ(std::get<dqBase::DqString>(e3.elementIds), "+DEF");
    ASSERT_TRUE(e3.cuttingPlaneTimeline.has_value());
    ASSERT_EQ(e3.cuttingPlaneTimeline->size(), 1u);
    EXPECT_DOUBLE_EQ((*e3.cuttingPlaneTimeline)[0].time, 600.0);
    EXPECT_EQ((*e3.cuttingPlaneTimeline)[0].interpolation, Interpolation::Linear);
    ASSERT_TRUE((*e3.cuttingPlaneTimeline)[0].value.has_value());
    auto const& cp = *(*e3.cuttingPlaneTimeline)[0].value;
    EXPECT_DOUBLE_EQ(cp.position[0], 1.0);
    EXPECT_DOUBLE_EQ(cp.position[1], 2.0);
    EXPECT_DOUBLE_EQ(cp.position[2], 3.0);
    EXPECT_DOUBLE_EQ(cp.direction[0], 0.0);
    EXPECT_DOUBLE_EQ(cp.direction[1], 0.0);
    EXPECT_DOUBLE_EQ(cp.direction[2], -1.0);
    ASSERT_TRUE(cp.visible.has_value());
    EXPECT_TRUE(*cp.visible);
    EXPECT_FALSE(cp.hidden.has_value());
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              describe("VisibilityEntry") / it("compares for equality")
TEST(RenderScheduleVisibilityEntryTest, ComparesForEquality)
{
    EXPECT_TRUE(VisibilityEntry(visProps(1))
                    .equals(VisibilityEntry(visProps(1))));
    EXPECT_FALSE(VisibilityEntry(visProps(1))
                     .equals(VisibilityEntry(visProps(2))));
    {
        VisibilityEntryProps p1;
        p1.time = 1;
        p1.interpolation = Interpolation::Step;
        VisibilityEntryProps p2 = p1;
        EXPECT_TRUE(VisibilityEntry(p1).equals(VisibilityEntry(p2)));
        p2.interpolation = Interpolation::Linear;
        EXPECT_FALSE(VisibilityEntry(p1).equals(VisibilityEntry(p2)));
    }
    {
        VisibilityEntryProps p1;
        p1.time = 1;
        p1.value = 2;
        VisibilityEntryProps p2 = p1;
        EXPECT_TRUE(VisibilityEntry(p1).equals(VisibilityEntry(p2)));
        p2.value = 3;
        EXPECT_FALSE(VisibilityEntry(p1).equals(VisibilityEntry(p2)));
    }
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              describe("ColorEntry") / it("compares for equality")
TEST(RenderScheduleColorEntryTest, ComparesForEquality)
{
    EXPECT_TRUE(ColorEntry(colorProps(1)).equals(ColorEntry(colorProps(1))));
    EXPECT_FALSE(ColorEntry(colorProps(1)).equals(ColorEntry(colorProps(2))));
    {
        ColorEntryProps p1;
        p1.time = 1;
        p1.interpolation = Interpolation::Step;
        ColorEntryProps p2 = p1;
        EXPECT_TRUE(ColorEntry(p1).equals(ColorEntry(p2)));
        p2.interpolation = Interpolation::Linear;
        EXPECT_FALSE(ColorEntry(p1).equals(ColorEntry(p2)));
    }
    {
        ColorEntryProps p1;
        p1.time = 1;
        RgbColorProps rgb1{1, 2, 3};
        p1.value = rgb1;
        ColorEntryProps p2 = p1;
        EXPECT_TRUE(ColorEntry(p1).equals(ColorEntry(p2)));
        RgbColorProps rgb2{3, 2, 1};
        p2.value = rgb2;
        EXPECT_FALSE(ColorEntry(p1).equals(ColorEntry(p2)));
    }
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              describe("TransformEntry") / it("compares for equality")
TEST(RenderScheduleTransformEntryTest, ComparesForEquality)
{
    EXPECT_TRUE(TransformEntry(transformPropsTime(1))
                    .equals(TransformEntry(transformPropsTime(1))));
    EXPECT_FALSE(TransformEntry(transformPropsTime(1))
                     .equals(TransformEntry(transformPropsTime(2))));
    {
        TransformEntryProps p1;
        p1.time = 1;
        p1.interpolation = Interpolation::Step;
        TransformEntryProps p2 = p1;
        EXPECT_TRUE(TransformEntry(p1).equals(TransformEntry(p2)));
        p2.interpolation = Interpolation::Linear;
        EXPECT_FALSE(TransformEntry(p1).equals(TransformEntry(p2)));
    }
    {
        TransformEntryProps p1;
        p1.time = 1;
        p1.value = makeTransform(1, 2, 3, 4);
        TransformEntryProps p2 = p1;
        EXPECT_TRUE(TransformEntry(p1).equals(TransformEntry(p2)));
        p2.value = makeTransform(5, 6, 7, 8);
        EXPECT_FALSE(TransformEntry(p1).equals(TransformEntry(p2)));
    }
    {
        TransformEntryProps p1;
        p1.time = 1;
        p1.value = makeTransformComponents(1, 2, 3);
        TransformEntryProps p2 = p1;
        EXPECT_TRUE(TransformEntry(p1).equals(TransformEntry(p2)));
        p2.value = makeTransformComponents(3, 2, 1);
        EXPECT_FALSE(TransformEntry(p1).equals(TransformEntry(p2)));
    }
    {
        TransformEntryProps p1;
        p1.time = 1;
        p1.value = makeTransform(1, 2, 3, 4);
        TransformEntryProps p2;
        p2.time = 1;
        p2.value = makeTransformComponents(1, 2, 3);
        EXPECT_FALSE(TransformEntry(p1).equals(TransformEntry(p2)));
    }
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              describe("CuttingPlaneEntry") / it("compares for equality")
TEST(RenderScheduleCuttingPlaneEntryTest, ComparesForEquality)
{
    EXPECT_TRUE(CuttingPlaneEntry(cuttingPropsTime(1))
                    .equals(CuttingPlaneEntry(cuttingPropsTime(1))));
    EXPECT_FALSE(CuttingPlaneEntry(cuttingPropsTime(1))
                     .equals(CuttingPlaneEntry(cuttingPropsTime(2))));
    {
        CuttingPlaneEntryProps p1;
        p1.time = 1;
        p1.interpolation = Interpolation::Step;
        CuttingPlaneEntryProps p2 = p1;
        EXPECT_TRUE(CuttingPlaneEntry(p1).equals(CuttingPlaneEntry(p2)));
        p2.interpolation = Interpolation::Linear;
        EXPECT_FALSE(CuttingPlaneEntry(p1).equals(CuttingPlaneEntry(p2)));
    }
    {
        CuttingPlaneEntryProps p1;
        p1.time = 1;
        p1.value = makeCuttingPlane(1, 2, true);
        CuttingPlaneEntryProps p2 = p1;
        EXPECT_TRUE(CuttingPlaneEntry(p1).equals(CuttingPlaneEntry(p2)));
        p2.value = makeCuttingPlane(3, 2, true);
        EXPECT_FALSE(CuttingPlaneEntry(p1).equals(CuttingPlaneEntry(p2)));
        p2.value = makeCuttingPlane(1, 3, false, true);
        EXPECT_FALSE(CuttingPlaneEntry(p1).equals(CuttingPlaneEntry(p2)));
        p2.value = makeCuttingPlane(1, 2, false, true);
        EXPECT_FALSE(CuttingPlaneEntry(p1).equals(CuttingPlaneEntry(p2)));
    }
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              describe("ElementTimeline") / it("compares for equality")
TEST(RenderScheduleElementTimelineTest, ComparesForEquality)
{
    auto const ids = dqBase::DqIdVector{dqBase::DqId::FromString("0x1"),
                                        dqBase::DqId::FromString("0x5"),
                                        dqBase::DqId::FromString("0xabc")};
    auto const compressed = compressIds({"0x1", "0x5", "0xabc"});

    auto const makeEl = [&](uint32_t batchId, std::variant<dqBase::DqIdVector, dqBase::DqString> elIds) {
        ElementTimelineProps p;
        p.batchId = batchId;
        p.elementIds = std::move(elIds);
        return ElementTimeline::fromJSON(p);
    };

    EXPECT_TRUE(makeEl(4, ids).equals(makeEl(4, ids)));
    EXPECT_TRUE(makeEl(4, compressed).equals(makeEl(4, compressed)));
    EXPECT_TRUE(makeEl(4, ids).equals(makeEl(4, compressed)));
    EXPECT_FALSE(makeEl(4, ids).equals(makeEl(5, ids)));
    EXPECT_FALSE(makeEl(4, ids).equals(makeEl(4, dqBase::DqIdVector{dqBase::DqId::FromString("0xabc")})));
    EXPECT_FALSE(makeEl(4, ids).equals(makeEl(4, dqBase::DqIdVector{dqBase::DqId::FromString("0x1"),
                                                                    dqBase::DqId::FromString("0x5")})));
    EXPECT_FALSE(makeEl(4, ids).equals(makeEl(4, dqBase::DqIdVector{
        dqBase::DqId::FromString("0x1"), dqBase::DqId::FromString("0x5"),
        dqBase::DqId::FromString("0xdef")})));

    auto const v1 = [] {
        VisibilityEntryProps p;
        p.time = 3;
        p.interpolation = Interpolation::Step;
        p.value = 1;
        return p;
    }();
    auto const v2 = [] {
        VisibilityEntryProps p;
        p.time = 4;
        p.interpolation = Interpolation::Linear;
        p.value = 2;
        return p;
    }();
    ColorEntryProps c1;
    c1.time = 3;
    c1.interpolation = Interpolation::Linear;
    c1.value = RgbColorProps{0, 1, 2};
    ColorEntryProps c2;
    c2.time = 4;
    c2.interpolation = Interpolation::Linear;
    c2.value = RgbColorProps{255, 254, 253};
    auto const t1 = [] {
        TransformEntryProps p;
        p.time = 3;
        p.value = makeTransform(-1, 0, 1, 2);
        return p;
    }();
    auto const t2 = [] {
        TransformEntryProps p;
        p.time = 4;
        p.value = makeTransformComponents(8, 9, -10);
        return p;
    }();
    auto const p1 = [] {
        CuttingPlaneEntryProps p;
        p.time = 3;
        p.interpolation = Interpolation::Linear;
        p.value = makeCuttingPlane(1, 2);
        return p;
    }();
    auto const p2 = [] {
        CuttingPlaneEntryProps p;
        p.time = 4;
        p.interpolation = Interpolation::Step;
        p.value = makeCuttingPlane(-1, -2, false, true);
        return p;
    }();

    auto const makeTimelineProps = [&](std::optional<std::vector<VisibilityEntryProps>> vis,
                                       std::optional<std::vector<ColorEntryProps>> col,
                                       std::optional<std::vector<TransformEntryProps>> tf,
                                       std::optional<std::vector<CuttingPlaneEntryProps>> cp) {
        ElementTimelineProps p;
        p.batchId = 1;
        p.elementIds = ids;
        p.visibilityTimeline = std::move(vis);
        p.colorTimeline = std::move(col);
        p.transformTimeline = std::move(tf);
        p.cuttingPlaneTimeline = std::move(cp);
        return p;
    };

    auto const baseline = makeTimelineProps(
        std::vector<VisibilityEntryProps>{v1, v2}, std::vector<ColorEntryProps>{c1, c2},
        std::vector<TransformEntryProps>{t1, t2},
        std::vector<CuttingPlaneEntryProps>{p1, p2});
    auto const timeline = ElementTimeline::fromJSON(baseline);
    EXPECT_TRUE(timeline.equals(ElementTimeline::fromJSON(baseline)));

    // 参考的 keys 循环：四条时间线逐一缺席/弹尾/加 Duplicate 都必须判不等；
    // 变换后的 props 自身相等（对称性）。
    auto const mutateCheck = [&](auto const& absentOrAltered) {
        EXPECT_FALSE(timeline.equals(ElementTimeline::fromJSON(absentOrAltered)));
        EXPECT_TRUE(ElementTimeline::fromJSON(absentOrAltered)
                        .equals(ElementTimeline::fromJSON(absentOrAltered)));
    };
    {
        auto p = baseline;
        p.visibilityTimeline = std::nullopt;
        mutateCheck(p);
        p = baseline;
        p.visibilityTimeline->pop_back();
        mutateCheck(p);
        p = baseline;
        p.visibilityTimeline->push_back(p.visibilityTimeline->at(1));
        mutateCheck(p);
    }
    {
        auto p = baseline;
        p.colorTimeline = std::nullopt;
        mutateCheck(p);
        p = baseline;
        p.colorTimeline->pop_back();
        mutateCheck(p);
        p = baseline;
        p.colorTimeline->push_back(p.colorTimeline->at(1));
        mutateCheck(p);
    }
    {
        auto p = baseline;
        p.transformTimeline = std::nullopt;
        mutateCheck(p);
        p = baseline;
        p.transformTimeline->pop_back();
        mutateCheck(p);
        p = baseline;
        p.transformTimeline->push_back(p.transformTimeline->at(1));
        mutateCheck(p);
    }
    {
        auto p = baseline;
        p.cuttingPlaneTimeline = std::nullopt;
        mutateCheck(p);
        p = baseline;
        p.cuttingPlaneTimeline->pop_back();
        mutateCheck(p);
        p = baseline;
        p.cuttingPlaneTimeline->push_back(p.cuttingPlaneTimeline->at(1));
        mutateCheck(p);
    }
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              describe("ElementTimeline") /
//              it("considers the same element Ids equal only if in the same order")
TEST(RenderScheduleElementTimelineTest, SameElementIdsEqualOnlyIfInSameOrder)
{
    auto const sorted = dqBase::DqIdVector{dqBase::DqId::FromString("0x1"),
                                           dqBase::DqId::FromString("0x5"),
                                           dqBase::DqId::FromString("0xabc")};
    auto const unsorted = dqBase::DqIdVector{dqBase::DqId::FromString("0x5"),
                                             dqBase::DqId::FromString("0xabc"),
                                             dqBase::DqId::FromString("0x1")};
    auto const compressed = compressIds({"0x1", "0x5", "0xabc"});

    auto const make = [&](std::variant<dqBase::DqIdVector, dqBase::DqString> elIds) {
        ElementTimelineProps p;
        p.batchId = 0;
        p.elementIds = std::move(elIds);
        return ElementTimeline::fromJSON(p);
    };

    auto const sortedT = make(sorted);
    auto const unsortedT = make(unsorted);
    auto const compressedT = make(compressed);

    EXPECT_TRUE(sortedT.equals(compressedT));
    EXPECT_FALSE(sortedT.equals(unsortedT));
    EXPECT_FALSE(compressedT.equals(unsortedT));
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              describe("ModelTimeline") / it("compares for equality")
TEST(RenderScheduleModelTimelineTest, ComparesForEquality)
{
    auto const make = [&](char const* modelId, std::optional<std::string> url = std::nullopt,
                          std::optional<std::vector<ElementTimelineProps>> els = std::nullopt) {
        ModelTimelineProps p;
        p.modelId = modelId;
        p.realityModelUrl = url;
        p.elementTimelines = std::move(els);
        return ModelTimeline::fromJSON(p);
    };

    EXPECT_TRUE(make("0x1", "blah").equals(make("0x1", "blah")));
    EXPECT_TRUE(make("0x2").equals(make("0x2")));
    EXPECT_FALSE(make("0x1", "blah").equals(make("0x1")));
    EXPECT_FALSE(make("0x1", "blah").equals(make("0x2", "blah")));

    ElementTimelineProps el;
    el.batchId = 0;
    el.elementIds = dqBase::DqIdVector{dqBase::DqId::FromString("0x2")};
    EXPECT_FALSE(make("0x1").equals(make("0x1", std::nullopt,
                                        std::vector<ElementTimelineProps>{el})));
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              describe("Script") / it("considers the same model Ids equal only if in the same order")
TEST(RenderScheduleScriptTest, SameModelIdsEqualOnlyIfInSameOrder)
{
    ModelTimelineProps m1;
    m1.modelId = "0x1";
    m1.elementTimelines = std::vector<ElementTimelineProps>{};
    ModelTimelineProps m2;
    m2.modelId = "0x2";
    m2.elementTimelines = std::vector<ElementTimelineProps>{};
    ModelTimelineProps m3;
    m3.modelId = "0x3";
    m3.elementTimelines = std::vector<ElementTimelineProps>{};

    auto const s123 = Script::fromJSON(ScriptProps{m1, m2, m3});
    auto const s321 = Script::fromJSON(ScriptProps{m3, m2, m1});
    ASSERT_TRUE(s123);
    ASSERT_TRUE(s321);
    EXPECT_TRUE(s123->equals(*s123));
    EXPECT_FALSE(s123->equals(*s321));
}

// Ported from: itwinjs-core RenderSchedule.test.ts
//              describe("Script") / it("weakly caches previous comparisons")
// C++ 适配（EQUIVALENCE E6）：JS WeakMap 缓存不可移植、删除——本例只锁
// 可观察的比较结果（compareTo 值与互逆性）；参考的 spy 断言（m1.compared
// 猴子补丁）系 JS 专属，不移植。
TEST(RenderScheduleScriptTest, ComparisonResultsAreConsistent)
{
    ModelTimelineProps m1;
    m1.modelId = "0x1";
    m1.elementTimelines = std::vector<ElementTimelineProps>{};
    ModelTimelineProps m2;
    m2.modelId = "0x2";
    m2.elementTimelines = std::vector<ElementTimelineProps>{};

    auto const s1 = Script::fromJSON(ScriptProps{m1});
    auto const s2 = Script::fromJSON(ScriptProps{m2});
    ASSERT_TRUE(s1);
    ASSERT_TRUE(s2);

    EXPECT_EQ(s1->compareTo(*s2), -1);
    EXPECT_EQ(s2->compareTo(*s1), 1);
    // 重复比较结果稳定（直比语义）。
    EXPECT_EQ(s1->compareTo(*s2), -1);
}
