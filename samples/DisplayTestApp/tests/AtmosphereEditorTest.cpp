// AtmosphereEditorTest — M-Z B 道 Atmosphere 编辑器子区锁
//（ViewSettingsPanel Environment 分区末节——Atmosphere.ts:11-321 的 Qt 承载）。
//
// Authored: no reference test exists in itwinjs-core for AtmosphereEditor
//           （display-test-app 无前端测试套件；场景逐行锚定 Atmosphere.ts——
//           控件清单 :49-255、写回环 :305-312、Enable 门 :64-69、is3d 门
//           :32/:257-268、展开态静态保持 :16/:40-44、Reset :314-316；
//           载体钉值 = dqCommon Atmosphere::Settings defaults）。
//
// RED（落地前）：atmosphere_menu/cbx_Atmosphere/atmosphere_* 控件不存在——
// findChild nullptr 运行期红（非编译红——锁面全走 objectName 查找）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QWidget>

#include "ViewSettingsPanel.h"

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqCommon/Environment.h>

namespace { struct QtEnvAtmo { QtEnvAtmo() { if (!qApp) { static int argc=1; static char n[]="t"; static char* av[]={n,nullptr}; new QApplication(argc,av);} } }; }
static QtEnvAtmo s_qtAtmo;

namespace {

// ViewSettingsPanelTest 同款夹具（blank connection + 无头 viewport + 注册）。
struct VpGuardAtmo {
    dqBase::RefPtr<dqApp::BlankConnection> conn;
    dqApp::Viewport* vp = nullptr;
    VpGuardAtmo() {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(-1000,-1000,-100, 1000,1000,100);
        conn = dqApp::BlankConnection::create(props);
        auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
        vp = dqApp::Viewport::Create(nullptr, view);
        dqApp::Application::Get().GetViewManager().AddViewport(vp);
    }
    ~VpGuardAtmo() {
        dqApp::Application::Get().GetViewManager().DropViewport(vp);
        delete vp;
    }
};

// 文本输入驱动 = 参考 input onchange（NumericInput.ts:34-42）——Qt6 信号是
// public 成员函数，直发 editingFinished 即用户提交路径。
void editField(Gui::ViewSettingsPanel& panel, char const* objectName, char const* text)
{
    auto* le = panel.findChild<QLineEdit*>(QString::fromLatin1(objectName));
    ASSERT_NE(le, nullptr) << objectName;
    le->setText(QString::fromLatin1(text));
    le->editingFinished();
}

dqCommon::Environment currentEnv(dqApp::Viewport* vp)
{
    return vp->GetView()->GetDisplayStyle().getEnvironment().clone();
}

}  // namespace

// 分区存在 + 控件清单（:49-255——Enable 复选 + 10 数值输入 + Reset）+ 初态
//（blank 3d 视图的载体缺省：displayAtmosphere=false → 控件体隐藏 :59-62）。
TEST(AtmosphereEditor, SectionPresentWithControlInventory)
{
    VpGuardAtmo g;
    Gui::ViewSettingsPanel panel;
    panel.syncFromViewport();

    auto* menu = panel.findChild<QWidget*>(QStringLiteral("atmosphere_menu"));
    ASSERT_NE(menu, nullptr);
    EXPECT_TRUE(menu->isVisibleTo(&panel));  // 3d 门（:32 isAtmosphereSupported）

    auto* cbx = panel.findChild<QCheckBox*>(QStringLiteral("cbx_Atmosphere"));
    ASSERT_NE(cbx, nullptr);
    EXPECT_FALSE(cbx->isChecked());  // blank 缺省 displayAtmosphere=false
    auto* body = panel.findChild<QWidget*>(QStringLiteral("atmosphere_controls"));
    ASSERT_NE(body, nullptr);
    EXPECT_FALSE(body->isVisibleTo(&panel));  // :265 showHideControls(false)

    // 10 数值输入 + Reset 全在场（:82-255 清单）。
    for (char const* id : {
             "atmosphere_exposure", "atmosphere_atmosphereHeightAboveEarth",
             "atmosphere_depthBelowEarthForMaxDensity", "atmosphere_scatteringStrength",
             "atmosphere_wavelengthR", "atmosphere_wavelengthG", "atmosphere_wavelengthB",
             "atmosphere_numViewRaySamples", "atmosphere_numSunRaySamples",
             "atmosphere_densityFalloff" })
        EXPECT_NE(panel.findChild<QLineEdit*>(QString::fromLatin1(id)), nullptr) << id;
    EXPECT_NE(panel.findChild<QPushButton*>(QStringLiteral("atmosphere_reset")), nullptr);

    // 回读 = 载体缺省值（:290-303 updateAtmosphereUI——参考编辑器构造字面量
    // [exposure 2.0 等]在首次 update 即被载体值覆写，此处断言载体面）。
    auto const& defs = dqCommon::Atmosphere::Settings::defaults();
    auto* exposure = panel.findChild<QLineEdit*>(QStringLiteral("atmosphere_exposure"));
    ASSERT_NE(exposure, nullptr);
    EXPECT_DOUBLE_EQ(exposure->text().toDouble(), defs.exposure);  // 20.0
}

// Enable 复选（:49-69——environment.displayAtmosphere 位 + 控件显隐 + toJSON
// 携带 display 键）。
TEST(AtmosphereEditor, EnableToggleWritesDisplayAtmosphereAndShowsControls)
{
    VpGuardAtmo g;
    Gui::ViewSettingsPanel panel;
    panel.syncFromViewport();

    auto* cbx = panel.findChild<QCheckBox*>(QStringLiteral("cbx_Atmosphere"));
    ASSERT_NE(cbx, nullptr);
    auto* body = panel.findChild<QWidget*>(QStringLiteral("atmosphere_controls"));
    ASSERT_NE(body, nullptr);
    // 展开菜单（嵌套组合：控件区可见 ⟺ Enable ∧ 展开——参考同构：controlsDiv
    // 在 menu body 内，菜单收拢时体隐则控件隐）。
    auto* header = panel.findChild<QToolButton*>(QStringLiteral("atmosphere_menu_header"));
    ASSERT_NE(header, nullptr);
    header->setChecked(true);

    cbx->setChecked(true);
    EXPECT_TRUE(currentEnv(g.vp).displayAtmosphere);
    EXPECT_TRUE(body->isVisibleTo(&panel));
    // toJSON 携带（Environment.toJSON :100-106——atmosphere 恒携 display 写出）。
    auto const props = currentEnv(g.vp).toJSON();
    ASSERT_TRUE(props.atmosphere.has_value());
    ASSERT_TRUE(props.atmosphere->display.has_value());
    EXPECT_TRUE(*props.atmosphere->display);

    cbx->setChecked(false);
    EXPECT_FALSE(currentEnv(g.vp).displayAtmosphere);
    EXPECT_FALSE(body->isVisibleTo(&panel));
}

// 十字段写回环（:82-255 逐 handler → updateAtmosphere :305-312——
// toJSON 现值+改字段→fromJSON→environment 替换；断言 = environment.atmosphere
// 字段对账）。
TEST(AtmosphereEditor, TenFieldsWriteThrough)
{
    VpGuardAtmo g;
    Gui::ViewSettingsPanel panel;
    panel.syncFromViewport();

    editField(panel, "atmosphere_exposure", "3.5");
    EXPECT_DOUBLE_EQ(currentEnv(g.vp).atmosphere.exposure, 3.5);
    editField(panel, "atmosphere_atmosphereHeightAboveEarth", "250000");
    EXPECT_DOUBLE_EQ(currentEnv(g.vp).atmosphere.atmosphereHeightAboveEarth, 250000.0);
    editField(panel, "atmosphere_depthBelowEarthForMaxDensity", "-500");
    EXPECT_DOUBLE_EQ(currentEnv(g.vp).atmosphere.depthBelowEarthForMaxDensity, -500.0);
    editField(panel, "atmosphere_densityFalloff", "7.25");
    EXPECT_DOUBLE_EQ(currentEnv(g.vp).atmosphere.densityFalloff, 7.25);
    editField(panel, "atmosphere_scatteringStrength", "42.5");
    EXPECT_DOUBLE_EQ(currentEnv(g.vp).atmosphere.scatteringStrength, 42.5);
    editField(panel, "atmosphere_wavelengthR", "610");
    EXPECT_DOUBLE_EQ(currentEnv(g.vp).atmosphere.wavelengths.r, 610.0);
    editField(panel, "atmosphere_wavelengthG", "555");
    EXPECT_DOUBLE_EQ(currentEnv(g.vp).atmosphere.wavelengths.g, 555.0);
    editField(panel, "atmosphere_wavelengthB", "475");
    EXPECT_DOUBLE_EQ(currentEnv(g.vp).atmosphere.wavelengths.b, 475.0);
    // 两采样数为整面（参考无 parseAsFloat——parseInt 前导整数语义；
    // "12.9" → 12）。
    editField(panel, "atmosphere_numViewRaySamples", "12");
    EXPECT_EQ(currentEnv(g.vp).atmosphere.numViewRaySamples, 12);
    editField(panel, "atmosphere_numSunRaySamples", "8");
    EXPECT_EQ(currentEnv(g.vp).atmosphere.numSunRaySamples, 8);

    // 段合并守恒（updateAtmosphere 只动单字段——其余字段保持既有写入）。
    EXPECT_DOUBLE_EQ(currentEnv(g.vp).atmosphere.exposure, 3.5);
    EXPECT_DOUBLE_EQ(currentEnv(g.vp).atmosphere.wavelengths.g, 555.0);

    // NaN 不写穿（NumericInput.ts:37 isNaN 门——"abc" 解析 NaN 落 handler 外）。
    editField(panel, "atmosphere_exposure", "abc");
    EXPECT_DOUBLE_EQ(currentEnv(g.vp).atmosphere.exposure, 3.5);
}

// Reset（:71-77/:314-316——Atmosphere.Settings.defaults 整替 + UI 回读）。
TEST(AtmosphereEditor, ResetRestoresCarrierDefaults)
{
    VpGuardAtmo g;
    Gui::ViewSettingsPanel panel;
    panel.syncFromViewport();

    editField(panel, "atmosphere_exposure", "9.5");
    editField(panel, "atmosphere_wavelengthB", "123");
    ASSERT_DOUBLE_EQ(currentEnv(g.vp).atmosphere.exposure, 9.5);

    auto* reset = panel.findChild<QPushButton*>(QStringLiteral("atmosphere_reset"));
    ASSERT_NE(reset, nullptr);
    reset->click();

    auto const env = currentEnv(g.vp);
    EXPECT_TRUE(env.atmosphere.equals(dqCommon::Atmosphere::Settings::defaults()));
    // UI 回读（updateAtmosphereUI——defaults.exposure=20 回写输入框）。
    auto* exposure = panel.findChild<QLineEdit*>(QStringLiteral("atmosphere_exposure"));
    ASSERT_NE(exposure, nullptr);
    EXPECT_DOUBLE_EQ(exposure->text().toDouble(),
                     dqCommon::Atmosphere::Settings::defaults().exposure);
    auto* waveB = panel.findChild<QLineEdit*>(QStringLiteral("atmosphere_wavelengthB"));
    ASSERT_NE(waveB, nullptr);
    EXPECT_DOUBLE_EQ(waveB->text().toDouble(),
                     dqCommon::Atmosphere::Settings::defaults().wavelengths.b);
}

// 展开态静态保持（:16/:40-44——_expandAtmosphereEditor 静态：关/重开下拉后
// 保持；Qt 承载 = 面板级静态，新面板实例读同一态）。
TEST(AtmosphereEditor, ExpandStateStaticPersistsAcrossPanels)
{
    VpGuardAtmo g;

    auto expandOf = [](Gui::ViewSettingsPanel& p) {
        auto* header = p.findChild<QToolButton*>(QStringLiteral("atmosphere_menu_header"));
        EXPECT_NE(header, nullptr);
        return header != nullptr && header->isChecked();
    };

    Gui::ViewSettingsPanel p1;
    p1.syncFromViewport();
    bool const initial = expandOf(p1);
    auto* header1 = p1.findChild<QToolButton*>(QStringLiteral("atmosphere_menu_header"));
    ASSERT_NE(header1, nullptr);
    // 翻转 → 写静态。
    header1->setChecked(!initial);
    Gui::ViewSettingsPanel p2;
    p2.syncFromViewport();
    EXPECT_EQ(expandOf(p2), !initial);  // 静态保持（跨实例）
    // 恢复初态（测试后清场——静态不越测试污染）。
    p2.findChild<QToolButton*>(QStringLiteral("atmosphere_menu_header"))
        ->setChecked(initial);
    Gui::ViewSettingsPanel p3;
    p3.syncFromViewport();
    EXPECT_EQ(expandOf(p3), initial);
}

// 回读链（syncFromViewport → updateAtmosphereUI——载体直改后面板同步）。
TEST(AtmosphereEditor, ReadbackFromCarrierOnSync)
{
    VpGuardAtmo g;
    // 载体直改（绕面板——DisplayStyle 面）。
    {
        auto env = currentEnv(g.vp);
        env.atmosphere.exposure = 7.5;
        env.atmosphere.wavelengths.g = 501.0;
        env.displayAtmosphere = true;
        g.vp->GetView()->GetDisplayStyle().setEnvironment(env);
    }
    Gui::ViewSettingsPanel panel;
    panel.syncFromViewport();
    auto* exposure = panel.findChild<QLineEdit*>(QStringLiteral("atmosphere_exposure"));
    ASSERT_NE(exposure, nullptr);
    EXPECT_DOUBLE_EQ(exposure->text().toDouble(), 7.5);
    auto* waveG = panel.findChild<QLineEdit*>(QStringLiteral("atmosphere_wavelengthG"));
    ASSERT_NE(waveG, nullptr);
    EXPECT_DOUBLE_EQ(waveG->text().toDouble(), 501.0);
    auto* cbx = panel.findChild<QCheckBox*>(QStringLiteral("cbx_Atmosphere"));
    ASSERT_NE(cbx, nullptr);
    EXPECT_TRUE(cbx->isChecked());
    // 回读含控件显隐（展开后可见——嵌套组合见 EnableToggle 注）。
    auto* header = panel.findChild<QToolButton*>(QStringLiteral("atmosphere_menu_header"));
    ASSERT_NE(header, nullptr);
    header->setChecked(true);
    EXPECT_TRUE(panel.findChild<QWidget*>(QStringLiteral("atmosphere_controls"))
                    ->isVisibleTo(&panel));
}
