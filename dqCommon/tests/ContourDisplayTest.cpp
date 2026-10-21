// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — ContourDisplay tests
// Ported from: itwinjs-core core/common/src/test/ContourDisplay.test.ts
#include <dqCommon/ContourDisplay.h>
#include <dqCommon/DisplayStyleSettings.h>

#include <dqBase/CompressedId64Set.h>

#include <gtest/gtest.h>

using namespace dqCommon;

// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ContourDisplay.test.ts
//   it("Ensure ContourDisplay derives values properly from JSON, including
//      setting defaults and round-tripping through JSON")
// ---------------------------------------------------------------------------
TEST(ContourDisplayTest, DerivesDefaultsAndRoundTripsThroughJSON)
{
    auto verifyDefaults = [](ContourDisplay const& contourDisplay) {
        EXPECT_FALSE(contourDisplay.displayContours);
        EXPECT_TRUE(contourDisplay.groups.empty());
    };
    auto verifyBackAndForth = [](ContourDisplay const& a) {
        auto const json = a.toJSON();
        ContourDisplay const aCopy = ContourDisplay::fromJSON(&json);
        EXPECT_TRUE(aCopy.equals(a));
    };

    // create default ContourDisplay object and verify the default values are correct
    ContourDisplay const defaultContourDisplay = ContourDisplay::fromJSON();
    verifyDefaults(defaultContourDisplay);

    // check if the creation and back-and-forth via JSON works using the default object
    verifyBackAndForth(defaultContourDisplay);
}

// ---------------------------------------------------------------------------
// Ported from: itwinjs-core ContourDisplay.test.ts
//   it("Ensure ContourDisplay can use API for partial reconfigurations")
//
// §3.4 适配：参考 clone(changedProps) 携**值对象段**（{...this, ...changedProps}
// 的 NonFunctionPropertiesOf 形）；DanQing 以对应 JSON Props 段承载变更
//（toJSON/fromJSON 往返无损——省略段回落默认=现值当且仅当现值==默认；非默认
// 现值恒写出恒回读）。
// ---------------------------------------------------------------------------
TEST(ContourDisplayTest, PartialReconfigurations)
{
    // create a ContourDisplay object with some configured values
    ContourStyleProps minorStyle;
    minorStyle.pattern = LinePixels::Code1;
    ContourStyleProps majorStyle;
    majorStyle.pattern = LinePixels::Code6;
    ContourProps contourDef;
    contourDef.minorStyle = minorStyle;
    contourDef.majorStyle = majorStyle;
    contourDef.minorInterval = 10.0;
    contourDef.majorIntervalCount = 2;
    contourDef.showGeometry = false;
    ContourGroupProps group;
    group.name = std::string("test");
    group.subCategories = std::string("");  // 参考 props `subCategories: ""`
    group.contourDef = contourDef;
    ContourDisplayProps displayProps;
    displayProps.displayContours = false;
    displayProps.groups = std::vector<ContourGroupProps>{group};
    ContourDisplay const contourDisplayA = ContourDisplay::fromJSON(&displayProps);

    // ensure objects match if we modify it to have the same value for displayContours
    ContourDisplay contourDisplayB = contourDisplayA.withDisplayContours(false);
    EXPECT_TRUE(contourDisplayA.equals(contourDisplayB));

    // ensure objects do not match if we modify it to have a different value for displayContours
    contourDisplayB = contourDisplayA.withDisplayContours(true);
    EXPECT_FALSE(contourDisplayA.equals(contourDisplayB));

    // modify a group
    auto const& groupsA = contourDisplayA.groups;
    ContourGroupProps renameGroup;
    renameGroup.name = std::string("test 2");
    ContourGroup modifiedGroup = groupsA[0].clone(renameGroup);
    EXPECT_FALSE(groupsA[0].equals(modifiedGroup));  // ensure old group and modified group do not match
    ContourDisplayProps modifiedGroups;
    modifiedGroups.groups = std::vector<ContourGroupProps>{modifiedGroup.toJSON()};
    contourDisplayB = contourDisplayA.clone(modifiedGroups);
    EXPECT_FALSE(contourDisplayA.equals(contourDisplayB));  // ensure the main object itself no longer matches

    // modify a contour
    ContourProps showGeometryOn;
    showGeometryOn.showGeometry = true;
    Contour modifiedContour = groupsA[0].contourDef.clone(showGeometryOn);
    EXPECT_FALSE(modifiedContour.equals(groupsA[0].contourDef));  // ensure the old contour and modified contour do not match
    ContourGroupProps modifiedContourGroup;
    modifiedContourGroup.contourDef = modifiedContour.toJSON();
    modifiedGroup = groupsA[0].clone(modifiedContourGroup);
    EXPECT_FALSE(groupsA[0].equals(modifiedGroup));  // ensure old group and modified group do not match
    ContourDisplayProps modifiedGroups2;
    modifiedGroups2.groups = std::vector<ContourGroupProps>{modifiedGroup.toJSON()};
    contourDisplayB = contourDisplayA.clone(modifiedGroups2);
    EXPECT_FALSE(contourDisplayA.equals(contourDisplayB));  // ensure the main object itself no longer matches

    // modify a contour style
    ContourStyleProps solidPattern;
    solidPattern.pattern = LinePixels::Solid;
    ContourStyle modifiedContourStyle = groupsA[0].contourDef.minorStyle.clone(solidPattern);
    EXPECT_FALSE(modifiedContourStyle.equals(groupsA[0].contourDef.minorStyle));  // ensure old contour style and modified contour style do not match
    ContourProps minorStyleSwap;
    minorStyleSwap.minorStyle = modifiedContourStyle.toJSON();
    modifiedContour = groupsA[0].contourDef.clone(minorStyleSwap);
    EXPECT_FALSE(modifiedContour.equals(groupsA[0].contourDef));  // ensure the old contour and modified contour do not match
    ContourGroupProps contourSwapGroup;
    contourSwapGroup.contourDef = modifiedContour.toJSON();
    modifiedGroup = groupsA[0].clone(contourSwapGroup);
    EXPECT_FALSE(groupsA[0].equals(modifiedGroup));  // ensure old group and modified group do not match
    ContourDisplayProps modifiedGroups3;
    modifiedGroups3.groups = std::vector<ContourGroupProps>{modifiedGroup.toJSON()};
    contourDisplayB = contourDisplayA.clone(modifiedGroups3);
    EXPECT_FALSE(contourDisplayA.equals(contourDisplayB));  // ensure the main object itself no longer matches

    // unmodify the contour style
    ContourStyleProps restorePattern;
    restorePattern.pattern = LinePixels::Code1;
    modifiedContourStyle = contourDisplayB.groups[0].contourDef.minorStyle.clone(restorePattern);
    EXPECT_TRUE(modifiedContourStyle.equals(groupsA[0].contourDef.minorStyle));  // ensure old contour style and modified contour style do match
    ContourProps restoreMinor;
    restoreMinor.minorStyle = modifiedContourStyle.toJSON();
    modifiedContour = groupsA[0].contourDef.clone(restoreMinor);
    EXPECT_TRUE(modifiedContour.equals(groupsA[0].contourDef));  // ensure the old contour and modified contour do match
    ContourGroupProps restoreGroup;
    restoreGroup.contourDef = modifiedContour.toJSON();
    modifiedGroup = groupsA[0].clone(restoreGroup);
    EXPECT_TRUE(groupsA[0].equals(modifiedGroup));  // ensure old group and modified group do match
    ContourDisplayProps restoreGroups;
    restoreGroups.groups = std::vector<ContourGroupProps>{modifiedGroup.toJSON()};
    contourDisplayB = contourDisplayA.clone(restoreGroups);
    EXPECT_TRUE(contourDisplayA.equals(contourDisplayB));  // ensure the main object itself matches
}

// ---------------------------------------------------------------------------
// 五组截断（计划 X-a1 锁）：equals 只比对前 ContourDisplay.maxContourGroups
// 组（ContourDisplay.ts:410 的 `index < ContourDisplay.maxContourGroups` 界）。
// Authored: 参考无针对截断界的测试（equals 行为面锚=ContourDisplay.ts:405-416
//           源码逐行）。
// ---------------------------------------------------------------------------
TEST(ContourDisplayTest, EqualsComparesOnlyFirstMaxContourGroups)
{
    auto makeGroup = [](char const* name) {
        ContourGroup g;
        g.name = name;
        return g;
    };

    ContourDisplay a;
    ContourDisplay b;
    for (int i = 0; i < ContourDisplay::MaxContourGroups; ++i) {
        a.groups.push_back(makeGroup("same"));
        b.groups.push_back(makeGroup("same"));
    }
    // 第 6 组（越过 maxContourGroups=5 的界）：a/b 不同——equals 不看它。
    a.groups.push_back(makeGroup("sixth-a"));
    b.groups.push_back(makeGroup("sixth-b"));
    EXPECT_TRUE(a.equals(b)) << "groups beyond maxContourGroups are ignored by equals";

    // 界内（第 4 组）不同 → 不等。
    b.groups[3].name = "different";
    EXPECT_FALSE(a.equals(b)) << "a difference within the first maxContourGroups compares unequal";
}

// ---------------------------------------------------------------------------
// 默认组语义（计划 X-a1 锁）：空 subCategories = default group
//（ContourGroup.isDefaultGroup——ContourDisplay.ts:298-300
//  `OrderedId64Iterable.isEmptySet(this._subCategories)`）。
// Authored: 参考无 isDefaultGroup 单测（行为面锚=源码 + Contours.ts:85-86
//           buildLookupTable 的 defaultNdx 切换消费面）。
// ---------------------------------------------------------------------------
TEST(ContourDisplayTest, EmptySubCategoriesMeansDefaultGroup)
{
    ContourGroup def;
    EXPECT_TRUE(def.isDefaultGroup()) << "empty subCategories = default group";
    EXPECT_TRUE(def.subCategories.empty());

    // 非空压缩集 → 非默认组。
    dqBase::DqIdSet ids;
    ids.insert(dqBase::DqId(uint64_t{0x123}));
    ContourGroupProps props;
    props.subCategories = dqBase::CompressedId64Set::SortAndCompress(ids);
    ContourGroup const g = ContourGroup::fromJSON(&props);
    EXPECT_FALSE(g.isDefaultGroup());
    EXPECT_FALSE(g.subCategories.empty());

    // subCategories 压缩串往返（fromJSON 经 sortAndCompress 归一化——
    // ContourDisplay.ts:319）。
    ContourGroupProps roundTrip = g.toJSON();
    ASSERT_TRUE(roundTrip.subCategories.has_value());
    EXPECT_EQ(*roundTrip.subCategories, *props.subCategories);

    // subCategoriesEqual（Contours.ts:50 的 LUT 失效比对消费面）。
    ContourGroup const g2 = ContourGroup::fromJSON(&props);
    EXPECT_TRUE(g.subCategoriesEqual(g2));
    ContourGroup other;
    EXPECT_FALSE(g.subCategoriesEqual(other));
}

// ---------------------------------------------------------------------------
// DisplayStyle3dSettings.contours 载体（X-a1 计划锁：props 往返 +
// applyOverrides 消费——DisplayStyleSettings.ts:165 字段 + :1103 ctor
// fromJSON + :1181 toJSON + :1213-1214 applyOverrides 经 setter）。
// Authored: 参考无 settings.contours 的独立单测（DisplayStyle.test.ts:361-364
//           的 baseProps 携带 contours 段进入大 roundTrip 用例——本锁=该段的
//           独立提取；DanQing 无 toOverrides 面）。
// ---------------------------------------------------------------------------
TEST(ContourDisplayTest, SettingsContoursRoundTripAndOverrides)
{
    // props 往返：DisplayStyle3dSettingsProps.contours → settings → toJSON3d。
    ContourGroupProps group;
    group.name = std::string("grp");
    ContourProps def;
    def.minorInterval = 2.5;
    group.contourDef = def;
    ContourDisplayProps contours;
    contours.displayContours = true;
    contours.groups = std::vector<ContourGroupProps>{group};

    DisplayStyle3dSettingsProps props;
    props.contours = contours;

    DisplayStyle3dSettings settings;
    settings.applyOverrides3d(props);

    EXPECT_TRUE(settings.getContours().displayContours);
    ASSERT_EQ(1u, settings.getContours().groups.size());
    EXPECT_EQ("grp", settings.getContours().groups[0].name);
    EXPECT_NEAR(2.5, settings.getContours().groups[0].contourDef.minorInterval, 1e-12);

    auto const out = settings.toJSON3d();
    ASSERT_TRUE(out.contours.has_value());
    EXPECT_TRUE(out.contours->displayContours.has_value() && *out.contours->displayContours);
    ASSERT_TRUE(out.contours->groups.has_value());
    ASSERT_EQ(1u, out.contours->groups->size());
    ASSERT_TRUE((*out.contours->groups)[0].name.has_value());
    EXPECT_EQ("grp", *(*out.contours->groups)[0].name);

    // applyOverrides 缺席段不触碰既有值（参考 :1213 的 `if (overrides.contours)` 门）。
    DisplayStyle3dSettingsProps empty;
    settings.applyOverrides3d(empty);
    EXPECT_TRUE(settings.getContours().displayContours);
    ASSERT_EQ(1u, settings.getContours().groups.size());
}
