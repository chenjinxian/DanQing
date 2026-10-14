// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Camera Paths 下拉面板实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/CameraPaths.ts
#include "CameraPathsPanel.h"

#include <dqApp/IModelConnection.h>
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>

#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

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
    // :290-296——toolMenu 容器（300px）
    m_widget = new QWidget(parentWidget);
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
    // Record/Play/Stop 钮 handler = V-d 泵（:394-419）

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

}  // namespace Gui
