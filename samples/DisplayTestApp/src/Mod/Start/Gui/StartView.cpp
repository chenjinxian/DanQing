// SPDX-License-Identifier: LGPL-2.1-or-later
/****************************************************************************
 *                                                                          *
 *   Copyright (c) 2024 The FreeCAD Project Association AISBL               *
 *                                                                          *
 *   This file is part of FreeCAD.                                          *
 *                                                                          *
 *   FreeCAD is free software: you can redistribute it and/or modify it     *
 *   under the terms of the GNU Lesser General Public License as            *
 *   published by the Free Software Foundation, either version 2.1 of the   *
 *   License, or (at your option) any later version.                        *
 *                                                                          *
 *   FreeCAD is distributed in the hope that it will be useful, but         *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU Lesser General Public       *
 *   License along with FreeCAD. If not, see                                *
 *   <https://www.gnu.org/licenses/>.                                       *
 *                                                                          *
 ***************************************************************************/


// Ported from: FreeCAD src/Mod/Start/Gui/StartView.cpp
// Simplified for DisplayTestApp UI shell — examples/custom folder removed


#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QListView>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QWidget>
#include <QStackedWidget>
#include <QShowEvent>

#include "StartView.h"
#include "FileCardDelegate.h"
#include "FileCardView.h"
#include "FirstStartWidget.h"
#include "FlowLayout.h"
#include "NewFileButton.h"
#include <App/Application.h>

using namespace StartGui;

TYPESYSTEM_SOURCE_ABSTRACT(StartGui::StartView, Gui::MDIView)  // NOLINT


StartView::StartView(QWidget* parent)
    : Gui::MDIView(nullptr, parent)
    , _contents(new QStackedWidget(parent))
    , _newFileLabel {nullptr}
    , _recentFilesLabel {nullptr}
    , _showOnStartupCheckBox {nullptr}
{
    setObjectName(QLatin1String("StartView"));
    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Mod/Start"
    );
    auto cardSpacing = hGrp->GetInt("FileCardSpacing", 15);  // NOLINT

    // First start page
    auto firstStartScrollArea = new QScrollArea();
    auto firstStartScrollWidget = new QWidget(firstStartScrollArea);
    firstStartScrollArea->setWidget(firstStartScrollWidget);
    firstStartScrollArea->setWidgetResizable(true);

    auto firstStartRegion = new QHBoxLayout(firstStartScrollWidget);
    firstStartRegion->setAlignment(Qt::AlignCenter);
    auto firstStartWidget = new FirstStartWidget(this);
    connect(firstStartWidget, &FirstStartWidget::dismissed, this, &StartView::firstStartWidgetDismissed);
    firstStartRegion->addWidget(firstStartWidget);
    _contents->addWidget(firstStartScrollArea);

    // Documents page
    auto documentsWidget = new QWidget();
    _contents->addWidget(documentsWidget);
    auto documentsMainLayout = new QVBoxLayout();
    documentsWidget->setLayout(documentsMainLayout);
    auto documentsScrollArea = new QScrollArea();
    documentsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
    documentsMainLayout->addWidget(documentsScrollArea);
    auto documentsScrollWidget = new QWidget(documentsScrollArea);
    documentsScrollArea->setWidget(documentsScrollWidget);
    documentsScrollArea->setWidgetResizable(true);
    auto documentsContentLayout = new QVBoxLayout(documentsScrollWidget);
    documentsContentLayout->setSizeConstraint(QLayout::SizeConstraint::SetMinAndMaxSize);

    _newFileLabel = new QLabel();
    documentsContentLayout->addWidget(_newFileLabel);

    auto createNewRow = new QWidget;
    auto flowLayout = new FlowLayout;

    // Reset margins of layout to provide consistent spacing
    flowLayout->setContentsMargins({});

    // This allows new file widgets to be targeted via QSS
    createNewRow->setObjectName(QStringLiteral("CreateNewRow"));
    createNewRow->setLayout(flowLayout);

    documentsContentLayout->addWidget(createNewRow);
    configureNewFileButtons(flowLayout);

    _recentFilesLabel = new QLabel();
    documentsContentLayout->addWidget(_recentFilesLabel);
    auto recentFilesListWidget = new FileCardView(_contents);
    connect(recentFilesListWidget, &QListView::clicked, this, &StartView::fileCardSelected);
    documentsContentLayout->addWidget(recentFilesListWidget);

    documentsContentLayout->setSpacing(static_cast<int>(cardSpacing));
    documentsContentLayout->addStretch();


    // Documents page footer
    auto footerLayout = new QHBoxLayout();
    documentsMainLayout->addLayout(footerLayout);

    _openFirstStart = new QPushButton();
    _openFirstStart->setIcon(QIcon(QLatin1String(":/icons/preferences-general.svg")));
    connect(_openFirstStart, &QPushButton::clicked, this, &StartView::openFirstStartClicked);

    _showOnStartupCheckBox = new QCheckBox();
    bool showOnStartup = hGrp->GetBool("ShowOnStartup", true);
    _showOnStartupCheckBox->setCheckState(
        showOnStartup ? Qt::CheckState::Unchecked : Qt::CheckState::Checked
    );
    connect(_showOnStartupCheckBox, &QCheckBox::toggled, this, &StartView::showOnStartupChanged);

    footerLayout->addWidget(_openFirstStart);
    footerLayout->addStretch();
    footerLayout->addWidget(_showOnStartupCheckBox);

    setCentralWidget(_contents);

    // Ported from: FreeCAD src/Mod/Start/Gui/StartView.cpp:184 (FirstStart2024 default true)
    // Default to showing the FirstStart wizard on the very first launch; the
    // wizard's "Done" button sets FirstStart2024=false (StartView.cpp:344).
    auto firstStart = hGrp->GetBool("FirstStart2024", true);  // FreeCAD default true
    _contents->setCurrentWidget(firstStart ? firstStartScrollArea : documentsWidget);
    configureRecentFilesListWidget(recentFilesListWidget, _recentFilesLabel);

    isInitialized = true;

    retranslateUi();
}

void StartView::configureNewFileButtons(QLayout* layout) const
{
    auto newEmptyFile = (NewFileButton*)(new NewFileButton(
        {tr("Empty File"),
         tr("Creates a new empty FreeCAD file"),
         QLatin1String(":/icons/document-new.svg")}
    ));
    auto openFile = (NewFileButton*)(new NewFileButton(
        {tr("Open File"),
         tr("Opens an existing CAD file or 3D model"),
         QLatin1String(":/icons/document-open.svg")}
    ));
    auto partDesign = (NewFileButton*)(new NewFileButton(
        {tr("Parametric Body"),
         tr("Creates a body with the Part Design workbench"),
         QLatin1String(":/icons/PartDesignWorkbench.svg")}
    ));
    auto assembly = (NewFileButton*)(new NewFileButton(
        {tr("Assembly"),
         tr("Creates an assembly project"),
         QLatin1String(":/icons/AssemblyWorkbench.svg")}
    ));
    auto draft = (NewFileButton*)(new NewFileButton(
        {tr("2D Draft"), tr("Creates a 2D Draft document"), QLatin1String(":/icons/DraftWorkbench.svg")}
    ));
    auto arch = (NewFileButton*)(new NewFileButton(
        {tr("BIM/Architecture"),
         tr("Creates an architectural project"),
         QLatin1String(":/icons/BIMWorkbench.svg")}
    ));

    // --- DTA Surface 页 5 项（用户指令 2026-09-11：自工具栏移至 Start 页，直接使用
    //     DTA 图标字体字形）---
    // Ported from: itwinjs-core display-test-app Surface.ts:126-176（createToolBar
    //              的 5 个按钮：图标 codepoint + tooltip + 点击行为）。
    const QString dtaIcons = QStringLiteral(":/fonts/Display-Test-App-Icons.ttf");  // ToolBar.ts:18 字体
    auto openIModelDisk = (NewFileButton*)(new NewFileButton(
        {tr("Open iModel from disk"),
         tr("Opens an iModel snapshot file from disk"),
         {}, dtaIcons, QChar(0xe9cc)}  // Surface.ts:127 "briefcases"
    ));
    auto openBlank = (NewFileButton*)(new NewFileButton(
        {tr("Open Blank Connection"),
         tr("Opens a blank 3D viewport with no backend data"),
         {}, dtaIcons, QChar(0xe9d8)}  // Surface.ts:135 "property-data"
    ));
    auto analysisStyle = (NewFileButton*)(new NewFileButton(
        {tr("Analysis Style Example"),
         tr("Opens a blank connection with an analysis style example"),
         {}, dtaIcons, QChar(0xea32)}  // Surface.ts:143 "play"
    ));
    auto decorationGeometry = (NewFileButton*)(new NewFileButton(
        {tr("Decoration Geometry Example"),
         tr("Opens a blank connection with decoration geometry"),
         {}, dtaIcons, QChar(0xe9d8)}  // Surface.ts:156
    ));
    auto cesiumRenderer = (NewFileButton*)(new NewFileButton(
        {tr("Cesium Renderer Example"),
         tr("Opens a blank connection for the Cesium renderer example"),
         {}, dtaIcons, QChar(0xe9f4)}  // Surface.ts:168
    ));

    // Open Blank Connection 是真功能（Surface.ts:137-139 → openBlankConnection()；
    // DanQing 对应 requestBlankConnection → Gui::Application::newDocument）。
    connect(openBlank, &QPushButton::clicked, this, &StartView::requestBlankConnection);
    // "Decoration Geometry Example"（Surface.ts:155-165——应用顶层入口；点击新建
    // blank connection 并装装饰）。参考的 Surface 顶层工具栏在 DanQing 无对应物
    // （工具栏区在 Start 页卡片之下——Start 页即入口）；信号路由到 main.cpp 的
    // newDocument + openDecorationGeometryExample。
    connect(decorationGeometry, &QPushButton::clicked, this,
            &StartView::requestDecorationGeometryExample);
    // 其余 3 项未实现（文件打开需仓外数据层；Cesium/Analysis 示例 setup 代码未移植）——
    // 置灰 + tooltip 注明，与此前工具栏惯例一致。
    const QString notImpl = QStringLiteral(" (not yet implemented)");
    for (NewFileButton* b : {openIModelDisk, analysisStyle, cesiumRenderer}) {
        b->setEnabled(false);
        b->setToolTip(b->toolTip() + notImpl);
    }

    layout->addWidget(openIModelDisk);
    layout->addWidget(openBlank);
    layout->addWidget(analysisStyle);
    layout->addWidget(decorationGeometry);
    layout->addWidget(cesiumRenderer);

    // TODO: Ensure all of the required WBs are actually available
    layout->addWidget(partDesign);
    layout->addWidget(assembly);
    layout->addWidget(draft);
    layout->addWidget(arch);
    layout->addWidget(newEmptyFile);
    layout->addWidget(openFile);

    connect(newEmptyFile, &QPushButton::clicked, this, &StartView::newEmptyFile);
    connect(openFile, &QPushButton::clicked, this, &StartView::openExistingFile);
    connect(partDesign, &QPushButton::clicked, this, &StartView::newPartDesignFile);
    connect(assembly, &QPushButton::clicked, this, &StartView::newAssemblyFile);
    connect(draft, &QPushButton::clicked, this, &StartView::newDraftFile);
    connect(arch, &QPushButton::clicked, this, &StartView::newArchFile);
}

void StartView::configureFileCardWidget(QListView* fileCardWidget)
{
    auto delegate = new FileCardDelegate(fileCardWidget);
    fileCardWidget->setItemDelegate(delegate);

    fileCardWidget->setMinimumWidth(fileCardWidget->parentWidget()->width());
    //    fileCardWidget->setGridSize(
    //        fileCardWidget->itemDelegate()->sizeHint(QStyleOptionViewItem(),
    //                                                 fileCardWidget->model()->index(0, 0)));
}


void StartView::configureRecentFilesListWidget(QListView* recentFilesListWidget, QLabel* recentFilesLabel)
{
    _recentFilesModel.loadRecentFiles();
    recentFilesListWidget->setModel(&_recentFilesModel);
    configureFileCardWidget(recentFilesListWidget);

    auto recentFilesGroup = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/RecentFiles"
    );
    auto numRecentFiles {recentFilesGroup->GetInt("RecentFiles", 0)};
    if (numRecentFiles == 0) {
        recentFilesListWidget->hide();
        recentFilesLabel->hide();
    }
    else {
        recentFilesListWidget->show();
        recentFilesLabel->show();
    }
}


void StartView::newEmptyFile()
{
    // DanQing: emit signal instead of FreeCAD command
    Q_EMIT requestNewFile();
}

void StartView::newPartDesignFile()
{
    // DanQing: emit signal instead of FreeCAD command
    Q_EMIT requestNewFile();
}

void StartView::openExistingFile()
{
    // DanQing: emit signal instead of FreeCAD command
    Q_EMIT requestOpenFile();
}

void StartView::newAssemblyFile()
{
    // DanQing: emit signal instead of FreeCAD command
    Q_EMIT requestNewFile();
}

void StartView::newDraftFile()
{
    // DanQing: emit signal instead of FreeCAD command
    Q_EMIT requestNewFile();
}

void StartView::newArchFile()
{
    // DanQing: emit signal instead of FreeCAD command
    Q_EMIT requestNewFile();
}

bool StartView::onHasMsg(const char* pMsg) const
{
    if (strcmp("AllowsOverlayOnHover", pMsg) == 0) {
        return false;
    }

    return MDIView::onHasMsg(pMsg);
}

void StartView::postStart(PostStartBehavior behavior)
{
    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Mod/Start"
    );

    if (behavior == PostStartBehavior::switchWorkbench) {
        auto wb = hGrp->GetASCII("AutoloadModule", "");
        if (wb == "$LastModule") {
            wb = App::GetApplication()
                     .GetParameterGroupByPath("User parameter:BaseApp/Preferences/General")
                     ->GetASCII("LastModule", "");
        }
        if (!wb.empty()) {
            // Stub: activateWorkbench not available in UI shell
        }
    }
    if (hGrp->GetBool("closeStart", false)) {
        for (QWidget* w = this; w != nullptr; w = w->parentWidget()) {
            if (auto mdiSub = qobject_cast<QMdiSubWindow*>(w)) {
                mdiSub->close();
                return;
            }
        }
    }
}


void StartView::fileCardSelected(const QModelIndex& index)
{
    // DanQing: emit signal instead of FreeCAD ModuleIO call
    Q_UNUSED(index);
    Q_EMIT requestOpenFile();
}

void StartView::showOnStartupChanged(bool checked)
{
    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Mod/Start"
    );
    hGrp->SetBool(
        "ShowOnStartup",
        !checked
    );  // The sense of this option has been reversed: the checkbox actually says
        // "*Don't* show on startup" now, but the option is preserved in its
        // original sense, so is stored inverted.
}

void StartView::openFirstStartClicked()
{
    _contents->setCurrentIndex(0);
}

void StartView::firstStartWidgetDismissed()
{
    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Mod/Start"
    );
    hGrp->SetBool("FirstStart2024", false);
    _contents->setCurrentIndex(1);
}

void StartView::changeEvent(QEvent* event)
{
    if (!isInitialized) {
        return;
    }

    _openFirstStart->setEnabled(true);
    // Stub: In real FreeCAD, this checks if a 3D view is in editing mode.
    // Since we don't have full view support, always enable the button.

    if (event->type() == QEvent::LanguageChange) {
        this->retranslateUi();
    }

    Gui::MDIView::changeEvent(event);
}

void StartView::showEvent(QShowEvent* event)
{
    // Find parent QMdiArea to connect subWindowActivated signal
    for (QWidget* w = parentWidget(); w != nullptr; w = w->parentWidget()) {
        if (auto mdiArea = qobject_cast<QMdiArea*>(w)) {
            connect(
                mdiArea,
                &QMdiArea::subWindowActivated,
                this,
                &StartView::onMdiSubWindowActivated,
                Qt::UniqueConnection
            );
            break;
        }
    }
    Gui::MDIView::showEvent(event);
}

void StartView::onMdiSubWindowActivated(QMdiSubWindow* subWindow)
{
    // check if start view is activated subwindow if yes, then enable updates
    // so we can once again receive paint events
    bool isOurWindow = subWindow && subWindow->isAncestorOf(this);
    setListViewUpdatesEnabled(isOurWindow);
}

void StartView::setListViewUpdatesEnabled(bool enabled)
{
    // disable updates on all QListView widgets when inactive to prevent unnecessary paint events
    QList<QListView*> listViews = findChildren<QListView*>();
    for (QListView* listView : listViews) {
        listView->setUpdatesEnabled(enabled);
        if (listView->viewport()) {
            listView->viewport()->setUpdatesEnabled(enabled);
        }
    }
}

void StartView::recentFileAdded(const QString& filename)
{
    _recentFilesModel.recentFileAdded(filename);
}

void StartView::retranslateUi()
{
    QString title = QCoreApplication::translate("Workbench", "Start");
    setWindowTitle(title);

    const QLatin1String h1Start("<h1>");
    const QLatin1String h1End("</h1>");

    _newFileLabel->setText(h1Start + tr("New File") + h1End);
    _recentFilesLabel->setText(h1Start + tr("Recent Files") + h1End);

    _openFirstStart->setText(tr("Open First Start Setup"));
    _showOnStartupCheckBox->setText(tr("Do not show this Start page again (start with blank screen)"));
}
