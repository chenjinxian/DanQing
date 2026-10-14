// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Camera Paths 下拉面板实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/CameraPaths.ts
#include "CameraPathsPanel.h"

#include <dqApp/IModelConnection.h>
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>

#include <dqCommon/Camera.h>  // Camera::validateLensAngle（:656）

#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <limits>

namespace Gui {

namespace {

QFrame* makeHRule(QWidget* parent)
{
    auto* rule = new QFrame(parent);
    rule->setFrameShape(QFrame::HLine);
    rule->setFrameShadow(QFrame::Sunken);
    return rule;
}

}  // namespace

CameraPathsPanel::CameraPathsPanel(dqApp::Viewport* vp, QWidget* parentWidget)
    : QObject(parentWidget), m_vp(vp)
{
    // :290-296——toolMenu 容器（300px；className "toolMenu" → objectName）
    m_widget = new QWidget(parentWidget);
    m_widget->setObjectName(QStringLiteral("cameraPathsMenu"));
    m_widget->setMinimumWidth(300);
    auto* layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(3, 3, 3, 3);

    // :324-339——path 名 textbox（tooltip=placeholder；size=36 字符宽）
    m_nameEdit = new QLineEdit(m_widget);
    m_nameEdit->setObjectName(QStringLiteral("txt_pathName"));
    m_nameEdit->setPlaceholderText(QStringLiteral("Name of new camera path to create"));
    layout->addWidget(m_nameEdit);
    layout->addWidget(makeHRule(m_widget));

    // :342-353——路径列表（空隐藏；Delete 键删——不存盘，参考怪癖 :350-353）
    m_pathsList = new QListWidget(m_widget);
    m_pathsList->setObjectName(QStringLiteral("pathsList"));
    m_pathsList->setVisible(false);
    m_pathsList->installEventFilter(this);
    layout->addWidget(m_pathsList);

    // :365-419——Create/Delete + Record/Play/Stop（buttonDiv 居中两段）
    auto* row1 = new QHBoxLayout();
    m_createButton = new QPushButton(QStringLiteral("Create"), m_widget);
    m_createButton->setObjectName(QStringLiteral("btn_createCameraPath"));
    m_createButton->setToolTip(QStringLiteral("Create a new camera path"));
    m_deleteButton = new QPushButton(QStringLiteral("Delete"), m_widget);
    m_deleteButton->setObjectName(QStringLiteral("btn_deleteCameraPath"));
    m_deleteButton->setToolTip(QStringLiteral("Delete selected camera path"));
    row1->addStretch(1);
    row1->addWidget(m_createButton);
    row1->addWidget(m_deleteButton);
    row1->addStretch(1);
    layout->addLayout(row1);
    layout->addWidget(makeHRule(m_widget));

    auto* row2 = new QHBoxLayout();
    m_recordButton = new QPushButton(QStringLiteral("Record"), m_widget);
    m_recordButton->setObjectName(QStringLiteral("btn_recordCameraPath"));
    m_recordButton->setToolTip(QStringLiteral("Record a camera path"));
    m_playButton = new QPushButton(QStringLiteral("Play"), m_widget);
    m_playButton->setObjectName(QStringLiteral("btn_playCameraPath"));
    m_playButton->setToolTip(QStringLiteral("Play selected camera path"));
    m_stopButton = new QPushButton(QStringLiteral("Stop"), m_widget);
    m_stopButton->setObjectName(QStringLiteral("btn_stopCameraPath"));
    m_stopButton->setToolTip(QStringLiteral("Stop running camera path"));
    row2->addStretch(1);
    row2->addWidget(m_recordButton);
    row2->addWidget(m_playButton);
    row2->addWidget(m_stopButton);
    row2->addStretch(1);
    layout->addLayout(row2);

    // --- handler 接线 ---
    // :328-335——Enter：追加 + 存盘
    QObject::connect(m_nameEdit, &QLineEdit::returnPressed, this, [this]() {
        if (appendNewPathToPathList())
            writeExternalCameraPaths(m_vp->GetView()->GetIModel()->GetKey(),
                                     cameraPathsToJsonString(m_paths));
    });
    // :429-432——onkeyup：_newPathName 同步 + Create 校验
    QObject::connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_newPathName = text.toStdString();
        updateCreateButtonState();
    });
    // :349——onchange：selectedPath = findPath(value)
    QObject::connect(m_pathsList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= static_cast<int>(m_paths.size()))
            selectPath(nullptr);
        else
            selectPath(&m_paths[static_cast<size_t>(row)]);
    });
    // :372-378——Create 钮 = Enter 同链
    QObject::connect(m_createButton, &QPushButton::clicked, this, [this]() {
        if (appendNewPathToPathList())
            writeExternalCameraPaths(m_vp->GetView()->GetIModel()->GetKey(),
                                     cameraPathsToJsonString(m_paths));
    });
    // :384-388——Delete 钮：删 + 存盘
    QObject::connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
        if (deletePath())
            writeExternalCameraPaths(m_vp->GetView()->GetIModel()->GetKey(),
                                     cameraPathsToJsonString(m_paths));
    });
    // :394-419——Record/Play/Stop 钮：play(true)/play(false)/stop()
    QObject::connect(m_recordButton, &QPushButton::clicked, this, [this]() { play(true); });
    QObject::connect(m_playButton, &QPushButton::clicked, this, [this]() { play(false); });
    QObject::connect(m_stopButton, &QPushButton::clicked, this, [this]() { stop(); });

    // 泵承载（:543-556 的 rAF 链 → 16ms 单发——E2=M-U E5 先例）
    m_animTimer = new QTimer(this);
    m_animTimer->setSingleShot(true);
    m_animTimer->setInterval(16);
    QObject::connect(m_animTimer, &QTimer::timeout, this, [this]() {
        animateFrame(static_cast<double>(m_elapsedTimer.elapsed()));
    });

    // :303-310——onViewChanged：iModel 变更才重 populate（参考 this._imodel !==
    // this._viewport.iModel 门；TimelinePanel 同族订阅面）
    m_eventScope.add(m_vp->onViewChanged.AddListener([this](dqApp::Viewport*) {
        dqApp::IModelConnection* imodel =
            (m_vp->GetView() != nullptr) ? m_vp->GetView()->GetIModel() : nullptr;
        if (imodel != m_imodel) {
            m_imodel = imodel;
            populate();
        }
    }));
    m_imodel = (m_vp->GetView() != nullptr) ? m_vp->GetView()->GetIModel() : nullptr;  // :279

    onStateChanged();  // :450
}

CameraPathsPanel::~CameraPathsPanel() = default;

const CameraPath* CameraPathsPanel::selectedPath() const noexcept
{
    // :266-268
    if (!m_selectedIndex.has_value() || *m_selectedIndex >= m_paths.size())
        return nullptr;
    return &m_paths[*m_selectedIndex];
}

void CameraPathsPanel::selectPath(const CameraPath* path)
{
    // :270-273——setter 触发 _onStateChanged
    if (path == nullptr) {
        m_selectedIndex = std::nullopt;
    } else {
        for (size_t i = 0; i < m_paths.size(); ++i) {
            if (&m_paths[i] == path) {
                m_selectedIndex = i;
                break;
            }
        }
    }
    onStateChanged();
}

void CameraPathsPanel::populate()
{
    // :312-319——isOpen 门（参考 !this._viewport.iModel.isOpen 早退）+
    // loadPathsFromExternalFile + populateFromPathList
    if (m_vp == nullptr || m_vp->GetView() == nullptr)
        return;
    m_paths = parseCameraPathsString(
        readExternalCameraPaths(m_vp->GetView()->GetIModel()->GetKey()));  // :490-499
    m_selectedIndex = std::nullopt;
    populateFromPathList();
}

void CameraPathsPanel::populateFromPathList()
{
    // :321-453（Qt 持久件刷新形——EQUIVALENCE 见头注）
    m_nameEdit->setText(QString::fromStdString(m_newPathName));  // :339 value 保持

    m_pathsList->blockSignals(true);
    m_pathsList->clear();
    for (const auto& path : m_paths) {
        auto* item = new QListWidgetItem(QString::fromStdString(path.name));
        // :358-361——空路径灰字 + 提示
        if (path.duration == 0.0 || path.keyframes.empty()) {
            item->setForeground(QColor(Qt::gray));
            item->setToolTip(QStringLiteral("This path is empty. Please use the record button to record it."));
        }
        m_pathsList->addItem(item);
    }
    // 选中项回映（参考 selectedPath 对象在 populate 后仍指 _paths 内对象）
    if (m_selectedIndex.has_value() && *m_selectedIndex < m_paths.size())
        m_pathsList->setCurrentRow(static_cast<int>(*m_selectedIndex));
    m_pathsList->blockSignals(false);
    m_pathsList->setVisible(!m_paths.empty());  // :346

    onStateChanged();  // :450
}

bool CameraPathsPanel::deletePath()
{
    // :461-474
    if (!m_selectedIndex.has_value())
        return false;
    const size_t index = *m_selectedIndex;
    if (index < m_paths.size()) {
        m_paths.erase(m_paths.begin() + static_cast<ptrdiff_t>(index));
        m_selectedIndex = std::nullopt;  // :468 selectedPath=undefined
        populateFromPathList();          // :469
        return true;
    }
    return false;
}

bool CameraPathsPanel::appendNewPathToPathList()
{
    // :476-488
    const bool pathExist = findPath(m_newPathName) != nullptr;
    if (!m_newPathName.empty() && !pathExist) {
        m_paths.push_back(CameraPath::CreateEmpty());
        m_selectedIndex = m_paths.size() - 1;           // :480
        m_paths.back().name = m_newPathName;            // :481
        m_newPathName.clear();                          // :482
        populateFromPathList();                         // :483
        return true;
    }
    return false;
}

void CameraPathsPanel::onStateChanged()
{
    // :434-448
    if (!m_selectedIndex.has_value()) {
        m_playButton->setEnabled(false);
        m_recordButton->setEnabled(false);
        m_stopButton->setEnabled(false);
        m_deleteButton->setEnabled(false);
        m_pathsList->setEnabled(true);                  // :437
    } else {
        const CameraPath& selected = m_paths[*m_selectedIndex];
        const bool canRecord = selected.duration == 0.0 || selected.keyframes.empty();  // :439
        m_playButton->setEnabled(!m_isPlaying && !canRecord);   // :440
        m_recordButton->setEnabled(!m_isPlaying && canRecord);  // :441
        m_stopButton->setEnabled(m_isPlaying);                  // :442
        m_pathsList->setEnabled(!m_isPlaying);                  // :443
        m_deleteButton->setEnabled(true);                       // :444
    }
    updateCreateButtonState();  // :447
}

void CameraPathsPanel::updateCreateButtonState()
{
    // :421-427
    const bool pathExist = findPath(m_newPathName) != nullptr;
    const bool isPathNameValid = !m_newPathName.empty() && !pathExist;
    m_nameEdit->setStyleSheet(isPathNameValid ? QString() : QStringLiteral("color: red"));
    m_createButton->setEnabled(isPathNameValid);
}

const CameraPath* CameraPathsPanel::findPath(const std::string& name) const
{
    // :510-514
    for (const auto& path : m_paths)
        if (path.name == name)
            return &path;
    return nullptr;
}

CameraPath* CameraPathsPanel::findPath(const std::string& name)
{
    for (auto& path : m_paths)
        if (path.name == name)
            return &path;
    return nullptr;
}

bool CameraPathsPanel::eventFilter(QObject* watched, QEvent* event)
{
    // :350-353——列表 keyup Delete → deletePath（**不存盘**，参考怪癖）
    if (watched == m_pathsList && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Delete) {
            deletePath();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

// --- V-d 录制/播放泵（:516-684） ---

void CameraPathsPanel::prepareView(bool record)
{
    // :516-529——selectedPath 缺席 throw（:517-518）由调用面 play 的门保证；
    // C++ 禁异常 → 早退（EQUIVALENCE）
    if (selectedPath() == nullptr)
        return;
    m_vp->setAnimator(nullptr);          // :520
    if (!record)
        m_vp->TurnCameraOn();            // :525（防正交无视史 bug——非透视首帧关相机）
    m_vp->synchWithView();               // :528
}

void CameraPathsPanel::play(bool record)
{
    // :531-561
    if (m_isPlaying)
        stop();                          // :532-533

    m_previousAnimationFrame = std::nullopt;  // :535
    m_currentAnimationTime = 0.0;             // :536

    if (selectedPath() == nullptr)
        return;                          // :538-539

    findPath(selectedPath()->name)->SortKeyframes();  // :541（对象同一性——selectPath 指 m_paths 内）

    prepareView(record);                 // :555
    m_elapsedTimer.restart();            // rAF timestamp 源（E2）
    queueAnimationFrame();               // :556

    m_isPlaying = true;                  // :558
    m_isRecording = record;              // :559
    onStateChanged();                    // :560
}

void CameraPathsPanel::stop()
{
    // :563-589
    m_animTimer->stop();                 // :565-567 cancelAnimationFrame

    // :570-581——录制收尾（isPlaying && isRecording 门）
    if (m_isPlaying && m_isRecording) {
        CameraPath* sel = (selectedPath() != nullptr)
            ? findPath(selectedPath()->name) : nullptr;
        if (sel == nullptr)
            return;                      // :571-572 throw → 早退（EQUIVALENCE；契约守卫）
        sel->duration = m_currentAnimationTime;                    // :574
        sel->Simplifies(0.005, dqGeom::Angle::FromDegrees(1.0), 0.01);  // :577
        writeExternalCameraPaths(m_vp->GetView()->GetIModel()->GetKey(),
                                 cameraPathsToJsonString(m_paths));     // :579
        populateFromPathList();                                       // :580
    }

    // :583-587
    m_isPlaying = false;
    m_isRecording = false;
    m_currentAnimationTime = 0.0;
    m_previousAnimationFrame = std::nullopt;
    onStateChanged();                    // :588
}

void CameraPathsPanel::animateFrame(double timestamp)
{
    // :543-553——rAF 回调体
    const double elapsed = m_previousAnimationFrame.has_value()
        ? timestamp - *m_previousAnimationFrame : 0.0;   // :544（首帧=0 怪癖）
    m_currentAnimationTime += elapsed;                   // :545

    computeAnimationFrame();                             // :547

    m_previousAnimationFrame = timestamp;                // :549
    if (m_isPlaying)                                     // :551-552
        queueAnimationFrame();
}

void CameraPathsPanel::queueAnimationFrame()
{
    // requestAnimationFrame（:552/:556）→ QTimer 单发重启动
    m_animTimer->start();
}

void CameraPathsPanel::computeAnimationFrame()
{
    // :591-606
    if (selectedPath() == nullptr) {
        endAnimation();                  // :593-594
        return;
    }
    if (m_isRecording) {
        recordAnimationFrame();          // :598
    } else {
        playAnimationFrame();            // :600
        if (m_currentAnimationTime > selectedPath()->duration)
            endAnimation();              // :602-603
    }
}

void CameraPathsPanel::playAnimationFrame()
{
    // :608-615
    CameraPath* sel = (selectedPath() != nullptr)
        ? findPath(selectedPath()->name) : nullptr;
    if (sel == nullptr)
        return;                          // :609-610 throw → 早退（EQUIVALENCE）
    m_vp->setAnimator(nullptr);          // :612
    const auto keyframe = sel->GetKeyframeAtTime(m_currentAnimationTime);  // :613
    if (keyframe.has_value())
        setCameraPosition(*keyframe);    // :614
}

void CameraPathsPanel::recordAnimationFrame()
{
    // :617-642
    auto* view = (m_vp->GetView() != nullptr) ? m_vp->GetView()->AsViewState3d() : nullptr;
    if (view == nullptr || !view->supportsCamera())
        return;  // :622-623 throw（"Invalid view for camera path"）→ 早退（EQUIVALENCE；
                 // AsViewState3d nullptr ≡ !is3d()）

    // :625——TODO: Handle global coordinates（参考未处理——原样不落）
    const dqGeom::Point3d location = m_vp->NpcToWorld(dqGeom::Point3d::From(0.5, 0.5, 1.0));  // :626
    const dqGeom::Point3d target = m_vp->NpcToWorld(dqGeom::Point3d::From(0.5, 0.5, 0.0));    // :627

    // :630-631——同点帧无效，跳过
    if (target.Distance(location) <= std::numeric_limits<double>::epsilon())
        return;

    const dqGeom::Point3d topScreen = m_vp->NpcToWorld(dqGeom::Point3d::From(0.5, 1.0, 0.0));  // :633
    const auto up = location.UnitVectorTo(topScreen);   // :634
    if (!up.has_value())
        return;                                          // :635-636

    // :638-639——isCameraOn 分流：lensAngle（透视）| extents（正交）
    std::optional<dqGeom::Angle> lensAngle;
    std::optional<dqGeom::Vector3d> extents;
    if (view->IsCameraOn())
        lensAngle = dqGeom::Angle::FromRadians(view->GetLensAngle());
    else
        extents = view->GetExtents();

    CameraPath* sel = findPath(selectedPath()->name);
    if (sel == nullptr)
        return;
    Keyframe kf;
    kf.time = m_currentAnimationTime;
    kf.location = location;
    kf.target = target;
    kf.up = *up;
    kf.lensAngle = lensAngle;
    kf.extents = extents;
    sel->keyframes.push_back(kf);        // :641
}

void CameraPathsPanel::endAnimation()
{
    stop();  // :644-646
}

void CameraPathsPanel::setCameraPosition(const Keyframe& keyframe)
{
    // :648-684
    auto* view = (m_vp->GetView() != nullptr) ? m_vp->GetView()->AsViewState3d() : nullptr;
    if (view == nullptr || !view->supportsCamera())
        return;  // :650-651 throw → 早退（EQUIVALENCE）

    if (keyframe.lensAngle.has_value()) {
        // :654-666——透视
        double lensAngle = dqCommon::Camera::validateLensAngle(
            keyframe.lensAngle->Radians());  // :655-656（clone+validate——值传递等价）
        dqApp::LookAtArgs args;
        args.eyePoint = keyframe.location;
        args.targetPoint = keyframe.target;
        args.upVector = keyframe.up;
        args.lensAngleRadians = lensAngle;
        view->lookAt(args);
    } else {
        // :668-680——正交
        if (!keyframe.extents.has_value())
            return;  // :669-670 throw → 早退（EQUIVALENCE）
        dqApp::LookAtArgs args;
        args.eyePoint = keyframe.location;
        args.viewDirection = dqGeom::Vector3d::FromStartEnd(keyframe.location, keyframe.target);
        args.upVector = keyframe.up;
        // LookAtOrthoArgs.newExtents?: XAndY（ViewState.ts:163）——Vector3d 结构型
        // 别兼容 {x,y}，z 弃（参考同弃）
        args.newExtents = dqGeom::Point2d{keyframe.extents->x, keyframe.extents->y};
        view->lookAt(args);
    }

    dqApp::ViewChangeOptions opts;       // :683
    opts.noSaveInUndo = true;
    m_vp->synchWithView(opts);
}

}  // namespace Gui
