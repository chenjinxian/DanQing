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
