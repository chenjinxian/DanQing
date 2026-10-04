// HiddenLineOverrideTest — M-O(4) P6 hline 编辑器引擎锁（HiddenLineSettings
// .override 聚合 + 编辑器写通道 + smooth 边选项权威源）。
//
// 锚定（真实读过的参考行号）：
//   - HiddenLine.ts:210-219 Settings.override（段级合并——显式段覆写、缺席段
//     保持现值）；
//   - ViewAttributes.ts:829-833 overrideEdgeSettings（settings.hiddenLineSettings
//     = current.override(props) + sync）；
//   - :865-869 Smooth Polyface Edges（tileAdmin.edgeOptions.smooth +
//     invalidateScene + sync）。
//
// RED（M-O(4) P6 落地前）：HiddenLineSettings::override / setSmoothPolyfaceEdges
// 不存在——编译期缺 API 红。
//
// Authored: no reference test exists in itwinjs-core for the hline editor
//          （HiddenLine.test.ts 无 override 用例；行为锚定如上）。
#include <gtest/gtest.h>

#include <dqCommon/ColorDef.h>
#include <dqCommon/HiddenLine.h>
#include <dqCommon/LinePixels.h>

#include <dqRender/tile/TileAdmin.h>

namespace {

dqCommon::HiddenLineStyleProps styleWith(int width, dqCommon::LinePixels pattern)
{
    dqCommon::HiddenLineStyleProps sp;
    sp.width = width;
    sp.pattern = pattern;
    return sp;
}

}  // namespace

// override 段级合并（:210-219——显式段覆写、缺席段保持现值）。
TEST(HiddenLineOverride, OverrideMergesSegmentsAndKeepsAbsent)
{
    dqCommon::HiddenLineSettings base;
    {
        dqCommon::HiddenLineSettingsProps p;
        p.visible = styleWith(3, dqCommon::LinePixels::Solid);
        p.hidden = styleWith(5, dqCommon::LinePixels::HiddenLine);
        p.transThreshold = 0.4;
        base = dqCommon::HiddenLineSettings::fromJSON(p);
    }

    // 只覆写 hidden 宽——visible 段与 transThreshold 保持。
    dqCommon::HiddenLineSettingsProps over;
    dqCommon::HiddenLineStyleProps hid;
    hid.width = 9;
    over.hidden = hid;
    dqCommon::HiddenLineSettings const merged = base.override(over);

    ASSERT_TRUE(merged.visible.width.has_value());
    EXPECT_EQ(3, *merged.visible.width);  // 缺席段保持
    ASSERT_TRUE(merged.visible.pattern.has_value());
    EXPECT_EQ(dqCommon::LinePixels::Solid, *merged.visible.pattern);
    ASSERT_TRUE(merged.hidden.width.has_value());
    EXPECT_EQ(9, *merged.hidden.width);  // 显式段覆写
    EXPECT_NEAR(0.4, merged.transparencyThreshold, 1e-12);
}

// HiddenLineStyle 三 override 工厂（:49-51——overrideColor/Width/Pattern 的
// 置/清双态）。
TEST(HiddenLineOverride, StyleOverrideFactoriesSetAndClear)
{
    dqCommon::HiddenLineStyle style;
    dqCommon::HiddenLineStyle const withColor =
        style.overrideColor(dqCommon::ColorDef::from(255, 0, 0));
    ASSERT_TRUE(withColor.color.has_value());
    EXPECT_EQ(dqCommon::ColorDef::from(255, 0, 0).getTbgr(),
              withColor.color->getTbgr());
    dqCommon::HiddenLineStyle const cleared = withColor.overrideColor(std::nullopt);
    EXPECT_FALSE(cleared.color.has_value());

    dqCommon::HiddenLineStyle const withWidth = style.overrideWidth(11);
    ASSERT_TRUE(withWidth.width.has_value());
    EXPECT_EQ(11, *withWidth.width);

    dqCommon::HiddenLineStyle const withPattern =
        style.overridePattern(dqCommon::LinePixels::Code1);
    ASSERT_TRUE(withPattern.pattern.has_value());
    EXPECT_EQ(dqCommon::LinePixels::Code1, *withPattern.pattern);
}

// TileAdmin edgeOptions 权威源（缺省 = TileOptions{}.edgeOptions——捕获键域
// 稳定；setEdgeOptions 可变面）。
TEST(HiddenLineOverride, TileAdminEdgeOptionsCarrier)
{
    auto& admin = dqRender::TileAdmin::instance();
    // 缺省（TileMetadata.ts:327-330 {compact, smooth:true}）。
    EXPECT_TRUE(admin.edgeOptions().smooth);
    EXPECT_EQ(dqRender::TileEdgeType::Compact, admin.edgeOptions().type);

    dqRender::EdgeOptions options;
    options.smooth = false;
    admin.setEdgeOptions(options);
    EXPECT_FALSE(admin.edgeOptions().smooth);

    // 复位（测试隔离）。
    dqRender::EdgeOptions restore;
    admin.setEdgeOptions(restore);
    EXPECT_TRUE(admin.edgeOptions().smooth);
}
