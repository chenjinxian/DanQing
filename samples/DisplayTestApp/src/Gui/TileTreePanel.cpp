// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Models/瓦树停靠面板实现（M-L(2) 裁决档改造，
// 数据面与设计说明见 TileTreePanel.h）。
#include "TileTreePanel.h"

#include "../DumpOpenHelper.h"  // 打开产物注册表（src/ 根——app 目标含 src/Gui 与仓库根两条 include 路径）
#include "MainWindow.h"
#include "View3DInventor.h"
#include "DockWindowManager.h"

#include <QCoreApplication>
#include <QDockWidget>
#include <QMainWindow>

namespace Gui {

TileTreePanel::TileTreePanel(QWidget* parent)
    : QTreeWidget(parent)
{
    setColumnCount(1);
    setHeaderHidden(true);
    setRootIsDecorated(true);
    setAlternatingRowColors(false);

    connect(&m_refreshTimer, &QTimer::timeout, this, &TileTreePanel::refresh);
    m_refreshTimer.start(1000);
    refresh();
}

void TileTreePanel::refresh()
{
    // 活动视口 → 打开产物注册表（main.cpp 打开链登记；未打开 → 占位项）。
    QStringList lines;
    auto* mw = MainWindow::getInstance();
    auto* view3d = mw ? qobject_cast<View3DInventor*>(mw->activeWindow()) : nullptr;
    dta::DumpOpenResult* opened = view3d ? dta::findOpenedDump(view3d) : nullptr;

    if (opened == nullptr) {
        lines << QStringLiteral("(no iModel open — use the Start page)");
    }
    else {
        // Models（连接数据面——imodel.json models[]）
        if (auto* connection = opened->connection.Get()) {
            lines << QStringLiteral("Models (%1)").arg(quint64(connection->getModels().size()));
            for (auto const& model : connection->getModels()) {
                lines << QStringLiteral("  %1 [%2]")
                             .arg(QString::fromStdString(model.name),
                                  QString::fromStdString(model.classFullName));
            }
        }
        // Tile Trees（modelSelector 逐 model 装载序——treeLoadLog）
        lines << QStringLiteral("Tile Trees (%1)").arg(quint64(opened->treeLoadLog.size()));
        for (auto const& treeId : opened->treeLoadLog)
            lines << QStringLiteral("  %1").arg(QString::fromStdString(treeId));
    }

    // 内容指纹不变 → 跳过重建（保住选中/展开状态）。
    QString next = lines.join(QLatin1Char('\n'));
    if (next == m_content)
        return;
    m_content = next;

    clear();
    for (QString const& line : lines) {
        auto* item = new QTreeWidgetItem(static_cast<QTreeWidget*>(nullptr), QStringList(line));
        bool const isGroup = !line.startsWith(QLatin1Char(' '));
        item->setDisabled(!isGroup || line.startsWith(QStringLiteral("(")));
        // 组行加粗（区分 models/trees 分组与其条目）。
        if (isGroup && !line.startsWith(QStringLiteral("("))) {
            QFont bold = item->font(0);
            bold.setBold(true);
            item->setFont(0, bold);
        }
        addTopLevelItem(item);
    }
}

// 面板装配（FreeCAD ComboView 的 dock 位——左区；注册键沿用 "Std_ComboView"）。
void setupModelsPanel()
{
    auto* panel = new TileTreePanel;
    panel->setObjectName(QStringLiteral("ModelsTileTreePanel"));
    panel->setWindowTitle(QCoreApplication::translate("MainWindow", "Models"));

    auto* dockMgr = DockWindowManager::instance();
    dockMgr->registerDockWindow("Std_ComboView", panel);
    if (auto* dock = dockMgr->addDockWindow("Models", panel, Qt::LeftDockWidgetArea))
        dock->show();
}

}  // namespace Gui
