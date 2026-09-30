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
// M-H(4)（2026-09-28 用户指令"DanQing 只实现前端图形渲染，不要 FreeCAD 的
// 任何功能"）：Start 页 = 模型打开入口（点击经 requestOpenDumpModel 进
// DumpOpenHelper 打开链直接渲染）+ DTA 对齐保留项（Blank Connection /
// Decoration Geometry Example）。FreeCAD-only 件（New/Open File 卡片、
// recent-files 卡片面、FirstStart 向导、ShowOnStartup、postStart）全清。
// M-K(2)：Models 分组扩至五入口（两旧 + House_Model / Baytown / 编辑大桥
// 测试——M-K(1) 三数据面包）。


#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QWidget>

#include "StartView.h"
#include "FlowLayout.h"
#include "NewFileButton.h"
#include <App/Application.h>

using namespace StartGui;

TYPESYSTEM_SOURCE_ABSTRACT(StartGui::StartView, Gui::MDIView)  // NOLINT


StartView::StartView(QWidget* parent)
    : Gui::MDIView(nullptr, parent)
{
    setObjectName(QLatin1String("StartView"));
    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Mod/Start"
    );
    auto cardSpacing = hGrp->GetInt("FileCardSpacing", 15);  // NOLINT

    // Single page: two model entries + DTA-aligned examples (scrollable column).
    auto scrollArea = new QScrollArea();
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
    auto scrollWidget = new QWidget(scrollArea);
    scrollArea->setWidget(scrollWidget);
    scrollArea->setWidgetResizable(true);
    auto contentLayout = new QVBoxLayout(scrollWidget);
    contentLayout->setSizeConstraint(QLayout::SizeConstraint::SetMinAndMaxSize);

    _modelsLabel = new QLabel();
    contentLayout->addWidget(_modelsLabel);
    auto modelsRow = new QWidget;
    auto modelsFlow = new FlowLayout;
    modelsFlow->setContentsMargins({});
    modelsRow->setObjectName(QStringLiteral("ModelsRow"));
    modelsRow->setLayout(modelsFlow);
    contentLayout->addWidget(modelsRow);
    configureModelButtons(modelsFlow);

    _examplesLabel = new QLabel();
    contentLayout->addWidget(_examplesLabel);
    auto examplesRow = new QWidget;
    auto examplesFlow = new FlowLayout;
    examplesFlow->setContentsMargins({});
    examplesRow->setObjectName(QStringLiteral("ExamplesRow"));
    examplesRow->setLayout(examplesFlow);
    contentLayout->addWidget(examplesRow);
    configureExampleButtons(examplesFlow);

    // M-L(3)：第三分组 "ReadMe"（分析报告 §4.2 入口形态——卡片 → 滚动只读页）。
    _readmeLabel = new QLabel();
    contentLayout->addWidget(_readmeLabel);
    auto readmeRow = new QWidget;
    auto readmeFlow = new FlowLayout;
    readmeFlow->setContentsMargins({});
    readmeRow->setObjectName(QStringLiteral("ReadMeRow"));
    readmeRow->setLayout(readmeFlow);
    contentLayout->addWidget(readmeRow);
    configureReadMeButtons(readmeFlow);

    contentLayout->setSpacing(static_cast<int>(cardSpacing));
    contentLayout->addStretch();

    setCentralWidget(scrollArea);

    isInitialized = true;

    retranslateUi();
}

void StartView::configureModelButtons(QLayout* layout)
{
    // M-H(4)（2026-09-28 用户指令）：两模型打开入口——点击经
    // requestOpenDumpModel(modelId) 进 main.cpp 槽 → DumpOpenHelper 打开链
    //（M-H Task 3）→ saved 视图直接渲染。图标复用 DTA 图标字体的
    // "briefcases" 字形（Surface.ts:127 "Open iModel from disk" 原字形——
    // 该置灰占位入口由本两卡片实现）。
    const QString dtaIcons = QStringLiteral(":/fonts/Display-Test-App-Icons.ttf");  // ToolBar.ts:18 字体
    auto joesHouse = new NewFileButton(
        {tr("Joe's House"),
         tr("Opens the saved JoesHouse iModel (10 tile trees) and renders its default view"),
         {}, dtaIcons, QChar(0xe9cc)}  // Surface.ts:127 "briefcases"
    );
    auto instances60 = new NewFileButton(
        {tr("60 Instances (Properties)"),
         tr("Opens the saved Properties_60InstancesWithUrl2 iModel and renders its default view"),
         {}, dtaIcons, QChar(0xe9cc)}
    );
    // M-K(2)：三模型入口（House_Model / Baytown / 编辑大桥测试——M-K(1) 采集
    // 入库的三个数据面包；bridge-edit 的 dump 目录名按 §11.11 用 ASCII，
    // 卡片标注中文名映射）。House_Model 的默认视图带透视相机（cameraOn=true
    // ——M-H 打开链首个透视用例）；bridge-edit 的默认视图指向空域（坑 24
    // ——打开链取景到世界域几何 contentRange）。
    auto houseModel = new NewFileButton(
        {tr("House_Model"),
         tr("Opens the saved House_Model iModel (106k tiles, perspective default view) "
            "and renders its default view"),
         {}, dtaIcons, QChar(0xe9cc)}
    );
    auto baytown = new NewFileButton(
        {tr("Baytown"),
         tr("Opens the saved Baytown iModel (OpenPlant process plant) and renders "
            "its default view"),
         {}, dtaIcons, QChar(0xe9cc)}
    );
    auto bridgeEdit = new NewFileButton(
        {tr("Bridge Edit (编辑大桥测试)"),
         tr("Opens the saved 编辑大桥测试 iModel (1.5 km bridge) and frames its "
            "geometry volume"),
         {}, dtaIcons, QChar(0xe9cc)}
    );
    connect(joesHouse, &QPushButton::clicked, this, [this]() {
        Q_EMIT requestOpenDumpModel(QStringLiteral("joeshouse"));
    });
    connect(instances60, &QPushButton::clicked, this, [this]() {
        Q_EMIT requestOpenDumpModel(QStringLiteral("instances60"));
    });
    connect(houseModel, &QPushButton::clicked, this, [this]() {
        Q_EMIT requestOpenDumpModel(QStringLiteral("housemodel"));
    });
    connect(baytown, &QPushButton::clicked, this, [this]() {
        Q_EMIT requestOpenDumpModel(QStringLiteral("baytown"));
    });
    connect(bridgeEdit, &QPushButton::clicked, this, [this]() {
        Q_EMIT requestOpenDumpModel(QStringLiteral("bridge-edit"));
    });

    layout->addWidget(joesHouse);
    layout->addWidget(instances60);
    layout->addWidget(houseModel);
    layout->addWidget(baytown);
    layout->addWidget(bridgeEdit);
}

void StartView::configureExampleButtons(QLayout* layout)
{
    // --- DTA Surface 页保留 2 项（2026-09-11 自工具栏移至 Start 页；M-H(4) 裁决
    //     保留——Blank Connection 与 Decoration Geometry Example 是 DtaTest 与
    //     真实 app 配方依赖的 DTA 对齐入口）---
    // Ported from: itwinjs-core display-test-app Surface.ts:126-176（createToolBar
    //              的按钮：图标 codepoint + tooltip + 点击行为）。
    const QString dtaIcons = QStringLiteral(":/fonts/Display-Test-App-Icons.ttf");  // ToolBar.ts:18 字体
    auto openBlank = new NewFileButton(
        {tr("Open Blank Connection"),
         tr("Opens a blank 3D viewport with no backend data"),
         {}, dtaIcons, QChar(0xe9d8)}  // Surface.ts:135 "property-data"
    );
    auto decorationGeometry = new NewFileButton(
        {tr("Decoration Geometry Example"),
         tr("Opens a blank connection with decoration geometry"),
         {}, dtaIcons, QChar(0xe9d8)}  // Surface.ts:156
    );

    // Open Blank Connection 是真功能（Surface.ts:137-139 → openBlankConnection()；
    // DanQing 对应 requestBlankConnection → Gui::Application::newDocument）。
    connect(openBlank, &QPushButton::clicked, this, &StartView::requestBlankConnection);
    // "Decoration Geometry Example"（Surface.ts:155-165——应用顶层入口；点击新建
    // blank connection 并装装饰）。参考的 Surface 顶层工具栏在 DanQing 无对应物
    // （工具栏区在 Start 页卡片之下——Start 页即入口）；信号路由到 main.cpp 的
    // newDocument + openDecorationGeometryExample。
    connect(decorationGeometry, &QPushButton::clicked, this,
            &StartView::requestDecorationGeometryExample);

    layout->addWidget(openBlank);
    layout->addWidget(decorationGeometry);
}

void StartView::configureReadMeButtons(QLayout* layout)
{
    // M-L(3) Task B：ReadMe 展示页入口卡（分析报告 §4.2——"Start 页第三分组
    // ReadMe，复用 NewFileButton 卡片形态，点击开一个滚动只读页 MDI——与
    // StartView 同构"）。内容口径见 ReadMeView.h（已锁能力 × 判据测试名；
    // 不放截图位）。
    const QString dtaIcons = QStringLiteral(":/fonts/Display-Test-App-Icons.ttf");
    auto readme = new NewFileButton(
        {tr("ReadMe"),
         tr("What this app demonstrates — 12 locked capabilities with their "
            "evidence tests"),
         {}, dtaIcons, QChar(0xe90c)}  // Viewer.ts:239 "info" 字形（Debug info 同源）
    );
    connect(readme, &QPushButton::clicked, this, &StartView::requestReadMe);
    layout->addWidget(readme);
}

bool StartView::onHasMsg(const char* pMsg) const
{
    if (strcmp("AllowsOverlayOnHover", pMsg) == 0) {
        return false;
    }

    return MDIView::onHasMsg(pMsg);
}

void StartView::changeEvent(QEvent* event)
{
    if (!isInitialized) {
        return;
    }

    if (event->type() == QEvent::LanguageChange) {
        this->retranslateUi();
    }

    Gui::MDIView::changeEvent(event);
}

void StartView::retranslateUi()
{
    QString title = QCoreApplication::translate("Workbench", "Start");
    setWindowTitle(title);

    const QLatin1String h1Start("<h1>");
    const QLatin1String h1End("</h1>");

    _modelsLabel->setText(h1Start + tr("Models") + h1End);
    _examplesLabel->setText(h1Start + tr("Examples") + h1End);
    _readmeLabel->setText(h1Start + tr("ReadMe") + h1End);
}
