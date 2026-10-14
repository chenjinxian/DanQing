// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Camera Paths 下拉面板
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/CameraPaths.ts
//              CameraPathsMenu（:249-685）
//
// 工具栏 item 8 Camera Paths 下拉（Viewer.ts:306-311——createDropDown 构造后
// populate()）。EQUIVALENCE / 裁决（§11.10）：
//  - DOM clearContent+全量重建（:455-459）→ Qt 持久件刷新（可观察态等价：
//    列表内容/显隐/钮态/名框文本保留；参考的 _onStateChanged 闭包重置
//    :458 是 DOM 重建的 JS 工件，Qt 无对应物）。
//  - <select size=N>（:342-346）→ QListWidget 行数策略（E3 装饰级等价）。
//  - textbox.style.color="red"（:425）→ QLineEdit styleSheet "color: red"。
//  - option 灰字+title（:359-360）→ QListWidgetItem foreground 灰 + toolTip。
//  - select onchange（:349）→ currentRowChanged；keyup Delete（:350-353）
//    → eventFilter Key_Delete（**不存盘**——对比 Delete 钮 :384-388 的
//    deletePath+savePathsToExternalFile，参考怪癖原样）。
//  - 持久化 = readExternalCameraPaths/writeExternalCameraPaths（V-b 本地
//    文件缝，:490-508 的 EQUIVALENCE）。
#pragma once

#include "CameraPaths.h"

#include <dqBase/DqEvent.h>  // DqEventScope（onViewChanged 订阅——:303-310）

#include <QElapsedTimer>
#include <QObject>
#include <optional>
#include <vector>

class QLineEdit;
class QListWidget;
class QPushButton;
class QTimer;
class QWidget;

namespace dqApp {
class Viewport;
class IModelConnection;
}

namespace Gui {

// CameraPathsPanel — CameraPathsMenu 的 Qt 承载。
// Ported from: CameraPaths.ts CameraPathsMenu（:249-685；录制/播放泵=V-d）。
class CameraPathsPanel final : public QObject {
    Q_OBJECT
public:
    // :275-297（ctor 只建容器；populate 由调用方——Viewer.ts:310）
    CameraPathsPanel(dqApp::Viewport* vp, QWidget* parentWidget);
    ~CameraPathsPanel() override;

    QWidget* widget() const noexcept { return m_widget; }

    // :312-319——isOpen 门 + loadPathsFromExternalFile + populateFromPathList
    void populate();

    // 测试缝（SectionsPanel/TimelinePanel 同族）
    const std::vector<CameraPath>& paths() const noexcept { return m_paths; }
    const CameraPath* selectedPath() const noexcept;  // :266-268

    // --- V-d 录制/播放泵（:516-684） ---
    void prepareView(bool record);  // :516-529
    void play(bool record);         // :531-561
    void stop();                    // :563-589
    // rAF 回调体（:543-553）——测试缝直驱（真实泵经 QTimer 16ms 喂
    // QElapsedTimer 毫秒时间戳——E2）。
    void animateFrame(double timestamp);

    bool isPlaying() const noexcept { return m_isPlaying; }      // :259
    bool isRecording() const noexcept { return m_isRecording; }  // :260
    double currentAnimationTime() const noexcept { return m_currentAnimationTime; }  // :264

    // QObject：列表 Delete 键（:350-353）
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void populateFromPathList();        // :321-453（Qt 持久件刷新形——EQUIVALENCE 头注）
    bool deletePath();                  // :461-474
    bool appendNewPathToPathList();     // :476-488
    void onStateChanged();              // :434-448 _onStateChanged
    void updateCreateButtonState();     // :421-427 setCreatePathButtonDisabled
    const CameraPath* findPath(const std::string& name) const;  // :510-514
    CameraPath* findPath(const std::string& name);
    void selectPath(const CameraPath* path);  // :270-273 setter（含 onStateChanged）

    void computeAnimationFrame();       // :591-606
    void playAnimationFrame();          // :608-615
    void recordAnimationFrame();        // :617-642
    void endAnimation();                // :644-646
    void setCameraPosition(const Keyframe& keyframe);  // :648-684
    void queueAnimationFrame();         // rAF → QTimer 单发链（:552/:556）

    dqApp::Viewport* m_vp;
    dqApp::IModelConnection* m_imodel = nullptr;     // :252/_imodel（:303-310 变更门）
    std::vector<CameraPath> m_paths;                 // :254 _paths
    std::optional<size_t> m_selectedIndex;           // :255 _selectedPath（对象→下标 EQUIVALENCE）
    std::string m_newPathName;                       // :257 _newPathName

    bool m_isPlaying = false;                        // :259（泵=V-d）
    bool m_isRecording = false;                      // :260（泵=V-d）
    double m_currentAnimationTime = 0.0;             // :264 _currentAnimationTime
    std::optional<double> m_previousAnimationFrame;  // :263（undefined=首帧）

    QTimer* m_animTimer = nullptr;                   // _animID 承载（可取消单发）
    QElapsedTimer m_elapsedTimer;                    // rAF timestamp 源（E2）

    dqBase::DqEventScope m_eventScope;               // onViewChanged（:303-310）

    QWidget* m_widget = nullptr;
    QLineEdit* m_nameEdit = nullptr;                 // txt_pathName
    QListWidget* m_pathsList = nullptr;              // pathsList
    QPushButton* m_createButton = nullptr;           // btn_createCameraPath
    QPushButton* m_deleteButton = nullptr;           // btn_deleteCameraPath
    QPushButton* m_recordButton = nullptr;           // btn_recordCameraPath
    QPushButton* m_playButton = nullptr;             // btn_playCameraPath
    QPushButton* m_stopButton = nullptr;             // btn_stopCameraPath
};

}  // namespace Gui
