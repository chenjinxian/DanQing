// EnvironmentEditorTest — M-O(4) P7 Environment 编辑器引擎锁（渐变段合并/
// Reset/withDisplay + OnEnvironmentChanged → 天空消费链）。
//
// 锚定（真实读过的参考行号）：
//   - EnvironmentEditor.ts:258-271 updateEnvironment（{...current, ...newEnv}
//     段合并 → SkyBox.createGradient → environment 替换）；
//   - :296-304 resetEnvironmentEditor（Environment.defaults().withDisplay
//     ({sky:true})）；
//   - :306-316 addEnvAttribute 的 withDisplay（sky/ground 显隐位）。
//
// RED（M-O(4) P7 落地前）：宿主槽不存在——面板域锁延后；本锁钉引擎面
//（SkyBoxProps 合并语义 + Environment 载体 + DisplayStyle.setEnvironment 事件）。
//
// Authored: no reference test exists in itwinjs-core for EnvironmentEditor
//          （display-test-app 无前端测试；行为锚定如上）。
#include <gtest/gtest.h>

#include <dqApp/BlankConnection.h>
#include <dqApp/DisplayStyle.h>
#include <dqApp/ViewState.h>

#include <dqCommon/ColorDef.h>
#include <dqCommon/Environment.h>
#include <dqCommon/SkyBox.h>

namespace {

// EnvironmentEditor.updateEnvironment 的段合并语义（:265-266 展开合并——
// SkyBoxProps 全 optional 字段缺席保持现值）。
dqCommon::SkyGradient mergeSkyProps(dqCommon::SkyBoxProps const& current,
                                    dqCommon::SkyBoxProps const& newEnv)
{
    dqCommon::SkyBoxProps merged = current;
    if (newEnv.twoColor.has_value())
        merged.twoColor = newEnv.twoColor;
    if (newEnv.skyColor.has_value())
        merged.skyColor = newEnv.skyColor;
    if (newEnv.groundColor.has_value())
        merged.groundColor = newEnv.groundColor;
    if (newEnv.zenithColor.has_value())
        merged.zenithColor = newEnv.zenithColor;
    if (newEnv.nadirColor.has_value())
        merged.nadirColor = newEnv.nadirColor;
    if (newEnv.skyExponent.has_value())
        merged.skyExponent = newEnv.skyExponent;
    if (newEnv.groundExponent.has_value())
        merged.groundExponent = newEnv.groundExponent;
    return dqCommon::SkyGradient::fromJSON(&merged);
}

}  // namespace

// 段合并（缺席字段保持现值——参考 {...current, ...newEnv} 展开）。
TEST(EnvironmentEditor, SkyMergeKeepsAbsentFields)
{
    dqCommon::SkyGradient base = dqCommon::SkyGradient::defaults();
    base.twoColor = false;
    base.skyExponent = 8.0;
    base.zenithColor = dqCommon::ColorDef::from(1, 2, 3);

    // 只改 twoColor——zenith/skyExponent 保持。
    dqCommon::SkyBoxProps edit;
    edit.twoColor = true;
    dqCommon::SkyGradient const merged =
        mergeSkyProps(base.toJSON(), edit);
    EXPECT_TRUE(merged.twoColor);
    EXPECT_NEAR(8.0, merged.skyExponent, 1e-12);
    EXPECT_EQ(dqCommon::ColorDef::from(1, 2, 3).getTbgr(),
              merged.zenithColor.getTbgr());
}

// Reset 语义（:301——Environment.defaults + displaySky=true）。
TEST(EnvironmentEditor, ResetRestoresDefaultsWithSkyOn)
{
    dqCommon::Environment env = dqCommon::Environment::defaults().clone();
    env.displaySky = true;
    EXPECT_TRUE(env.displaySky);
    EXPECT_FALSE(env.displayGround);
    // 渐变回到默认四色。
    EXPECT_TRUE(env.sky.gradient.equals(dqCommon::SkyGradient::defaults()));
}

// DisplayStyle.setEnvironment 事件面（:252-254 onEnvironmentChanged 的引擎
// 对应物——DisplayStyle.h:108-111）。
TEST(EnvironmentEditor, SetEnvironmentRaisesEvent)
{
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(
            dqGeom::Point3d::From(-10, -10, -10),
            dqGeom::Point3d::From(10, 10, 10));
        imodel = dqApp::BlankConnection::create(props);
    }
    dqBase::RefPtr<dqApp::SpatialViewState> view = dqApp::SpatialViewState::CreateBlank(
        imodel.Get(), dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(20, 20, 20));
    ASSERT_TRUE(view.IsValid());

    int events = 0;
    auto scope = view->GetDisplayStyle().OnEnvironmentChanged.AddListener(
        [&events]() { ++events; });

    dqCommon::Environment env = view->GetDisplayStyle().getEnvironment().clone();
    env.displaySky = true;
    view->GetDisplayStyle().setEnvironment(env);
    EXPECT_EQ(1, events);
    EXPECT_TRUE(view->GetDisplayStyle().getEnvironment().displaySky);
}
