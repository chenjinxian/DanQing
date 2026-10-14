// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp tests — M-V V-c CameraPathsPanel（面板 UI + 状态机）
//
// Authored: no reference test exists in itwinjs-core / display-test-app for
//           CameraPathsMenu（无 .test.ts；行为锚 = CameraPaths.ts：
//           populateFromPathList :321-453 / _onStateChanged :434-448 /
//           setCreatePathButtonDisabled :421-427 / deletePath :461-474 /
//           appendNewPathToPathList :476-488 / populate :312-319 /
//           Viewer.ts:306-311 createDropDown 构造即 populate）。
//
// 覆盖面：
//  - 互斥矩阵全态（无选中四钮全禁+列表解禁 / 空路径选中 record 可 play 禁 /
//    实路径选中 play 可 record 禁 / delete 恒解禁——:434-448）。
//  - 名校验三态（空名/重名→Create 禁+红字；合法→解禁——:421-432）。
//  - Enter 追加链（appendNewPathToPathList + savePathsToExternalFile——
//    :328-335）+ Delete 键删（:350-353）。
//  - populate 从持久化装载（:312-319）+ 空路径灰字+tooltip（:358-361）。

#include <gtest/gtest.h>
// 保证 QApplication 存在（TimelinePanelTest 同款；进程级只建一次）
#include <QApplication>
namespace { struct QtEnvCP { QtEnvCP() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }

#include "Gui/CameraPathsPanel.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

#include <dqApp/BlankConnection.h>
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>

#include <dqGeom/Range3d.h>

#include <cstdlib>
#include <filesystem>

namespace {

// TimelinePanelTest 同形夹具
struct CameraPathsFixture {
    dqBase::RefPtr<dqApp::BlankConnection> conn;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    dqApp::Viewport* vp = nullptr;

    CameraPathsFixture()
    {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(dqGeom::Point3d::From(-100.0, -100.0, -100.0),
                                        dqGeom::Point3d::From(100.0, 100.0, 100.0));
        conn = dqApp::BlankConnection::create(props);
        view = dqApp::SpatialViewState::CreateBlank(conn.Get(), dqGeom::Point3d::FromZero(),
                                                    dqGeom::Vector3d::From(200.0, 200.0, 200.0));
        vp = dqApp::Viewport::Create(nullptr, view);
    }

    ~CameraPathsFixture()
    {
        delete vp;
    }
};

std::string freshCameraPathsDir(const char* sub)
{
    std::error_code ec;
    const std::string dir = (std::filesystem::current_path(ec) / sub).string();
    std::filesystem::remove_all(dir, ec);
    _putenv_s("DANQING_CAMERA_PATHS_DIR", dir.c_str());
    return dir;
}

// 面板子件查找（参考元素 id → objectName）
QLineEdit* nameEdit(Gui::CameraPathsPanel& p)
{
    return p.widget()->findChild<QLineEdit*>("txt_pathName");
}
QListWidget* pathsList(Gui::CameraPathsPanel& p)
{
    return p.widget()->findChild<QListWidget*>("pathsList");
}
QPushButton* createButton(Gui::CameraPathsPanel& p)
{
    return p.widget()->findChild<QPushButton*>("btn_createCameraPath");
}
QPushButton* deleteButton(Gui::CameraPathsPanel& p)
{
    return p.widget()->findChild<QPushButton*>("btn_deleteCameraPath");
}
QPushButton* recordButton(Gui::CameraPathsPanel& p)
{
    return p.widget()->findChild<QPushButton*>("btn_recordCameraPath");
}
QPushButton* playButton(Gui::CameraPathsPanel& p)
{
    return p.widget()->findChild<QPushButton*>("btn_playCameraPath");
}
QPushButton* stopButton(Gui::CameraPathsPanel& p)
{
    return p.widget()->findChild<QPushButton*>("btn_stopCameraPath");
}

// 预置一条"已录制"路径（duration>0 + 两帧）进持久化
void seedRecordedPath(const std::string& key, const char* name)
{
    Gui::CameraPath path;
    path.name = name;
    path.duration = 2000.0;
    Gui::Keyframe k0;
    k0.time = 0.0;
    k0.location = dqGeom::Point3d::From(0.0, 0.0, 10.0);
    k0.target = dqGeom::Point3d::From(0.0, 0.0, 0.0);
    k0.up = dqGeom::Vector3d::From(0.0, 1.0, 0.0);
    k0.lensAngle = dqGeom::Angle::FromDegrees(60.0);
    Gui::Keyframe k1 = k0;
    k1.time = 2000.0;
    k1.location = dqGeom::Point3d::From(10.0, 0.0, 10.0);
    path.keyframes = {k0, k1};
    Gui::writeExternalCameraPaths(key, Gui::cameraPathsToJsonString({path}));
}

}  // namespace

TEST(CameraPathsPanelTest, EmptySelectionDisablesAllActionButtons)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-panel-empty");
    CameraPathsFixture f;
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    // :436-437——无选中：play/record/stop/delete 全禁 + 列表解禁
    EXPECT_FALSE(playButton(panel)->isEnabled());
    EXPECT_FALSE(recordButton(panel)->isEnabled());
    EXPECT_FALSE(stopButton(panel)->isEnabled());
    EXPECT_FALSE(deleteButton(panel)->isEnabled());
    EXPECT_TRUE(pathsList(panel)->isEnabled());
}

TEST(CameraPathsPanelTest, EmptyPathSelectedEnablesRecordDisablesPlay)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-panel-newpath");
    CameraPathsFixture f;
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    // Enter 追加（:330-333）
    nameEdit(panel)->setText("p1");
    emit nameEdit(panel)->returnPressed();
    ASSERT_EQ(pathsList(panel)->count(), 1);
    EXPECT_TRUE(pathsList(panel)->currentRow() == 0);
    // :439-443——canRecord=true：play 禁 / record 可 / stop 禁（未播放）/ delete 可
    EXPECT_FALSE(playButton(panel)->isEnabled());
    EXPECT_TRUE(recordButton(panel)->isEnabled());
    EXPECT_FALSE(stopButton(panel)->isEnabled());
    EXPECT_TRUE(deleteButton(panel)->isEnabled());
}

TEST(CameraPathsPanelTest, RecordedPathSelectedEnablesPlayDisablesRecord)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-panel-recorded");
    CameraPathsFixture f;
    seedRecordedPath(f.conn->GetKey(), "walk");
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    panel.populate();  // Viewer.ts:310——createDropDown 构造后 populate
    ASSERT_EQ(pathsList(panel)->count(), 1);
    pathsList(panel)->setCurrentRow(0);
    // :439-443——canRecord=false：play 可 / record 禁
    EXPECT_TRUE(playButton(panel)->isEnabled());
    EXPECT_FALSE(recordButton(panel)->isEnabled());
    EXPECT_TRUE(deleteButton(panel)->isEnabled());
}

TEST(CameraPathsPanelTest, InvalidNameDisablesCreateAndColorsRed)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-panel-names");
    CameraPathsFixture f;
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    // 空名 → 禁 + 红（:421-427）
    nameEdit(panel)->setText("");
    EXPECT_FALSE(createButton(panel)->isEnabled());
    // 合法名 → 解禁
    nameEdit(panel)->setText("alpha");
    EXPECT_TRUE(createButton(panel)->isEnabled());
    // 追加后重名 → 再禁
    emit nameEdit(panel)->returnPressed();
    nameEdit(panel)->setText("alpha");
    EXPECT_FALSE(createButton(panel)->isEnabled());
    // 非法态文本标红（:425 style.color="red"——Qt 侧 palette EQUIVALENCE）
    EXPECT_TRUE(nameEdit(panel)->styleSheet().contains("red"));
}

TEST(CameraPathsPanelTest, EnterAppendsPathAndPersists)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-panel-persist");
    CameraPathsFixture f;
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    nameEdit(panel)->setText("saved-path");
    emit nameEdit(panel)->returnPressed();
    // :331-332——追加即存盘
    const auto loaded = Gui::parseCameraPathsString(
        Gui::readExternalCameraPaths(f.conn->GetKey()));
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded[0].name, "saved-path");
    EXPECT_TRUE(loaded[0].keyframes.empty());
    // :482——追加后名框清空
    EXPECT_TRUE(nameEdit(panel)->text().isEmpty());
}

TEST(CameraPathsPanelTest, DeleteKeyRemovesPathWithoutPersisting)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-panel-delete");
    CameraPathsFixture f;
    seedRecordedPath(f.conn->GetKey(), "doomed");
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    panel.populate();
    ASSERT_EQ(pathsList(panel)->count(), 1);
    pathsList(panel)->setCurrentRow(0);
    // :350-353——列表 Delete 键：仅 deletePath（:461-474 重填列表），
    // **不存盘**（对比 Delete 钮 :384-388 的 deletePath+save——参考怪癖原样）
    QKeyEvent delEv(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QCoreApplication::sendEvent(pathsList(panel), &delEv);
    EXPECT_EQ(pathsList(panel)->count(), 0);
    const auto loaded = Gui::parseCameraPathsString(
        Gui::readExternalCameraPaths(f.conn->GetKey()));
    ASSERT_EQ(loaded.size(), 1u);  // 文件仍在
    EXPECT_EQ(loaded[0].name, "doomed");
}

TEST(CameraPathsPanelTest, DeleteButtonRemovesPathAndPersists)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-panel-deletebtn");
    CameraPathsFixture f;
    seedRecordedPath(f.conn->GetKey(), "doomed2");
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    panel.populate();
    ASSERT_EQ(pathsList(panel)->count(), 1);
    pathsList(panel)->setCurrentRow(0);
    // :384-388——Delete 钮：deletePath + savePathsToExternalFile
    deleteButton(panel)->click();
    EXPECT_EQ(pathsList(panel)->count(), 0);
    const auto loaded = Gui::parseCameraPathsString(
        Gui::readExternalCameraPaths(f.conn->GetKey()));
    EXPECT_TRUE(loaded.empty());
}

TEST(CameraPathsPanelTest, EmptyPathEntryGreyedWithTooltip)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-panel-grey");
    CameraPathsFixture f;
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    panel.populate();
    nameEdit(panel)->setText("ghost");
    emit nameEdit(panel)->returnPressed();
    QListWidgetItem* item = pathsList(panel)->item(0);
    ASSERT_TRUE(item != nullptr);
    // :358-361——duration==0‖空帧 → 灰字 + 空路径提示 tooltip
    EXPECT_TRUE(item->toolTip().contains("empty"));
    EXPECT_NE(item->foreground().color(), QColor(Qt::black));
}

// --- V-d 录制/播放泵（CameraPaths.ts:516-684） ---

namespace {

// NavigateTest 同形：无窗视口建立真实 frustum（setupViewFromFrustum）
void setupFrustum(CameraPathsFixture& f)
{
    f.view->SetExtents(dqGeom::Vector3d::From(
        200.0, 200.0 * f.vp->height() / f.vp->width(), 200.0));
    f.vp->setupViewFromFrustum(f.vp->getFrustum(true));
}

// 预置两帧透视路径（t=0/t=2000，相机沿 +X 平移 0→100）
void seedFlyPath(const std::string& key)
{
    Gui::CameraPath path;
    path.name = "fly";
    path.duration = 2000.0;
    Gui::Keyframe k0;
    k0.time = 0.0;
    k0.location = dqGeom::Point3d::From(0.0, 0.0, 100.0);
    k0.target = dqGeom::Point3d::From(0.0, 0.0, 0.0);
    k0.up = dqGeom::Vector3d::From(0.0, 1.0, 0.0);
    k0.lensAngle = dqGeom::Angle::FromDegrees(60.0);
    Gui::Keyframe k1 = k0;
    k1.time = 2000.0;
    k1.location = dqGeom::Point3d::From(100.0, 0.0, 100.0);
    k1.target = dqGeom::Point3d::From(100.0, 0.0, 0.0);
    path.keyframes = {k0, k1};
    Gui::writeExternalCameraPaths(key, Gui::cameraPathsToJsonString({path}));
}

}  // namespace

TEST(CameraPathsPumpTest, PlayDrivesCameraAlongPathAndStopsAtDuration)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-pump-play");
    CameraPathsFixture f;
    setupFrustum(f);
    seedFlyPath(f.conn->GetKey());
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    panel.populate();
    pathsList(panel)->setCurrentRow(0);

    panel.play(false);  // :531-561
    // :558-560 + :440-443——播放中：play 禁 / stop 可 / 列表禁
    EXPECT_TRUE(panel.isPlaying());
    EXPECT_FALSE(panel.isRecording());
    EXPECT_FALSE(playButton(panel)->isEnabled());
    EXPECT_TRUE(stopButton(panel)->isEnabled());
    EXPECT_FALSE(pathsList(panel)->isEnabled());

    // 首帧 elapsed=0（:544 的 previousAnimationFrame=undefined 怪癖）→ t=0 帧
    panel.animateFrame(1000.0);
    EXPECT_NEAR(f.view->AsViewState3d()->getEyePoint().x, 0.0, 1e-6);
    EXPECT_NEAR(f.view->AsViewState3d()->getEyePoint().z, 100.0, 1e-6);

    // 第二帧 elapsed=1000 → t=1000 → 中点插值（:191-203）
    panel.animateFrame(2000.0);
    EXPECT_NEAR(f.view->AsViewState3d()->getEyePoint().x, 50.0, 1e-6);
    EXPECT_NEAR(f.view->AsViewState3d()->getEyePoint().z, 100.0, 1e-6);

    // 越过 duration → endAnimation（:602-603）
    panel.animateFrame(4500.0);
    EXPECT_FALSE(panel.isPlaying());
    EXPECT_TRUE(playButton(panel)->isEnabled());
    EXPECT_FALSE(stopButton(panel)->isEnabled());
    EXPECT_TRUE(pathsList(panel)->isEnabled());
}

TEST(CameraPathsPumpTest, RecordCapturesCameraKeyframes)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-pump-record");
    CameraPathsFixture f;
    setupFrustum(f);
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    panel.populate();
    nameEdit(panel)->setText("rec");
    emit nameEdit(panel)->returnPressed();

    panel.play(true);  // :531-561 record 形
    EXPECT_TRUE(panel.isPlaying());
    EXPECT_TRUE(panel.isRecording());

    panel.animateFrame(100.0);   // t=0
    panel.animateFrame(600.0);   // t=500
    const auto& frames = panel.selectedPath()->keyframes;
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_DOUBLE_EQ(frames[0].time, 0.0);
    EXPECT_DOUBLE_EQ(frames[1].time, 500.0);
    // :626-627——location=npcToWorld(0.5,0.5,1)、target=npcToWorld(0.5,0.5,0)
    const dqGeom::Point3d expectLoc = f.vp->NpcToWorld(dqGeom::Point3d::From(0.5, 0.5, 1.0));
    const dqGeom::Point3d expectTgt = f.vp->NpcToWorld(dqGeom::Point3d::From(0.5, 0.5, 0.0));
    EXPECT_NEAR(frames[0].location.x, expectLoc.x, 1e-6);
    EXPECT_NEAR(frames[0].location.y, expectLoc.y, 1e-6);
    EXPECT_NEAR(frames[0].location.z, expectLoc.z, 1e-6);
    EXPECT_NEAR(frames[0].target.x, expectTgt.x, 1e-6);

    panel.stop();  // :563-589 录制收尾
    EXPECT_FALSE(panel.isPlaying());
    // :574——duration 落账 = currentAnimationTime
    EXPECT_DOUBLE_EQ(panel.selectedPath()->duration, 500.0);
    // :579——存盘
    const auto loaded = Gui::parseCameraPathsString(
        Gui::readExternalCameraPaths(f.conn->GetKey()));
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_DOUBLE_EQ(loaded[0].duration, 500.0);
    EXPECT_EQ(loaded[0].keyframes.size(), 2u);
}

TEST(CameraPathsPumpTest, StopRecordingSimplifiesCollinearFrames)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-pump-simplify");
    CameraPathsFixture f;
    setupFrustum(f);
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    panel.populate();
    nameEdit(panel)->setText("rec4");
    emit nameEdit(panel)->returnPressed();

    panel.play(true);
    // 静止录制 4 帧（t=0/500/1000/1500——位姿全同 → 中帧皆可插值重建）
    panel.animateFrame(100.0);
    panel.animateFrame(600.0);
    panel.animateFrame(1100.0);
    panel.animateFrame(1600.0);
    ASSERT_EQ(panel.selectedPath()->keyframes.size(), 4u);
    panel.stop();
    // :577——simplifies(0.005, 1°, 0.01)：中间两帧删
    const auto& frames = panel.selectedPath()->keyframes;
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_DOUBLE_EQ(frames[0].time, 0.0);
    EXPECT_DOUBLE_EQ(frames[1].time, 1500.0);
    EXPECT_DOUBLE_EQ(panel.selectedPath()->duration, 1500.0);
}

TEST(CameraPathsPumpTest, PlayWithoutSelectionDoesNotStart)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-pump-nosel");
    CameraPathsFixture f;
    setupFrustum(f);
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    panel.populate();
    panel.play(false);  // :538-539 早退
    EXPECT_FALSE(panel.isPlaying());
}

TEST(CameraPathsPumpTest, OrthoKeyframeAppliesExtentsAndTurnsCameraOff)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-pump-ortho");
    CameraPathsFixture f;
    setupFrustum(f);
    // 正交路径（extents 形态）
    Gui::CameraPath path;
    path.name = "ortho";
    path.duration = 1000.0;
    Gui::Keyframe k0;
    k0.time = 0.0;
    k0.location = dqGeom::Point3d::From(0.0, 0.0, 100.0);
    k0.target = dqGeom::Point3d::From(0.0, 0.0, 0.0);
    k0.up = dqGeom::Vector3d::From(0.0, 1.0, 0.0);
    k0.extents = dqGeom::Vector3d::From(111.0, 55.5, 1.0);
    Gui::Keyframe k1 = k0;
    k1.time = 1000.0;
    path.keyframes = {k0, k1};
    Gui::writeExternalCameraPaths(f.conn->GetKey(), Gui::cameraPathsToJsonString({path}));

    Gui::CameraPathsPanel panel(f.vp, nullptr);
    panel.populate();
    pathsList(panel)->setCurrentRow(0);
    panel.play(false);  // prepareView :525 turnCameraOn（注释：非透视首帧会关相机）
    panel.animateFrame(100.0);
    // :668-680——正交 lookAt；lookAt 的 isPerspective=false → TurnCameraOff
    // （ViewState.cpp lookAt :1983-1986——参考注释 :523-525 的机制）
    EXPECT_FALSE(f.view->IsCameraOn());
    EXPECT_NEAR(f.view->GetExtents().x, 111.0, 1e-6);
    // lookAt 的 newExtents 经视口纵横比校正（x 保持、y=x/aspect——参考
    // fixAspectRatio[Viewport.ts:2042，synchWithView→doSetupFromView 链] 同机制）；
    // 钉住实际语义而非原样 extents.y（容差 1e-4——viewRect aspect 亚像素噪声）。
    EXPECT_NEAR(f.view->GetExtents().y,
                111.0 * f.vp->height() / f.vp->width(), 1e-4);
    panel.stop();
}

TEST(CameraPathsPumpTest, StopDuringPlayDoesNotPersist)
{
    QtEnvCP qt;
    freshCameraPathsDir("cp-pump-stopplay");
    CameraPathsFixture f;
    setupFrustum(f);
    seedFlyPath(f.conn->GetKey());
    Gui::CameraPathsPanel panel(f.vp, nullptr);
    panel.populate();
    pathsList(panel)->setCurrentRow(0);
    panel.play(false);
    panel.animateFrame(1000.0);
    panel.stop();
    EXPECT_FALSE(panel.isPlaying());
    // :570——录制收尾门（isPlaying && isRecording）不满足 → 不动盘
    const auto loaded = Gui::parseCameraPathsString(
        Gui::readExternalCameraPaths(f.conn->GetKey()));
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_DOUBLE_EQ(loaded[0].duration, 2000.0);
    EXPECT_EQ(loaded[0].keyframes.size(), 2u);
}
