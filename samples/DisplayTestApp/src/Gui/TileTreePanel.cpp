// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Models/瓦树停靠面板实现（M-L(2) 裁决档改造 + M-L(3)
// Models 选择器演示版），设计说明见 TileTreePanel.h。
#include "TileTreePanel.h"

#include <QCoreApplication>
#include <QDockWidget>
#include <QComboBox>
#include <QHBoxLayout>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "../DumpOpenHelper.h"  // 打开产物注册表（src/ 根——app 目标含 src/Gui 与仓库根两条 include 路径）
#include "MainWindow.h"
#include "View3DInventor.h"
#include "DockWindowManager.h"

#include <dqApp/Application.h>
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>

namespace Gui {

namespace {
// 活动视口的打开产物（可空）。
dta::DumpOpenResult* openedDump()
{
    auto* mw = MainWindow::getInstance();
    auto* view3d = mw ? qobject_cast<View3DInventor*>(mw->activeWindow()) : nullptr;
    return view3d ? dta::findOpenedDump(view3d) : nullptr;
}

// 活动视口（隔离开关后的场景失效 + step-fit 的取景主体）。
dqApp::Viewport* activeViewport()
{
    auto* mw = MainWindow::getInstance();
    auto* view3d = mw ? qobject_cast<View3DInventor*>(mw->activeWindow()) : nullptr;
    return view3d ? view3d->getUeViewport() : nullptr;
}

// QTreeWidget UserRole 数据约定：组行 = -1；Models 分组的 model 条目 = provider
// 条目索引。
constexpr int kGroupRole = -1;
}  // namespace

TileTreePanel::TileTreePanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ModelsTileTreePanel"));
    setWindowTitle(QCoreApplication::translate("MainWindow", "Models"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    // IdPicker 共享动作下拉（IdPicker.ts:40-53）——接线子集：Show All / Hide All
    // / Invert（"Isolate/Hide Selected" 的选择集→model 查询与 "Hilite Enabled"
    // 依赖本仓没有的数据面/高亮通道——头注登记）。
    m_actions = new QComboBox(this);
    m_actions->setObjectName(QStringLiteral("DTA.Models.Actions"));
    m_actions->addItem(QString());
    m_actions->addItem(QStringLiteral("Show All"));
    m_actions->addItem(QStringLiteral("Hide All"));
    m_actions->addItem(QStringLiteral("Invert"));
    connect(m_actions, &QComboBox::activated, this, [this](int index) {
        switch (index) {
            case 1: showAll(); break;
            case 2: hideAll(); break;
            case 3: invert(); break;
            default: break;
        }
        m_actions->setCurrentIndex(0);  // 动作语义，非状态选择（参考 combo 同款）
        refresh();
    });
    layout->addWidget(m_actions);

    // ModelPicker 单步隔离 6 键（IdPicker.ts:354-397——⏪◀️➕⛶▶️⏩）。
    auto* steps = new QHBoxLayout;
    steps->setSpacing(2);
    struct Step { const char* label; const char* tip; };
    static const Step kSteps[] = {
        { "⏪", "Isolate first" },
        { "◀️", "Isolate previous" },
        { "➕", "Set first enabled as step index" },
        { "⛶", "Fit after isolate" },
        { "▶️", "Isolate next" },
        { "⏩", "Isolate last" },
    };
    for (int i = 0; i < 6; ++i) {
        auto* b = new QToolButton(this);
        b->setObjectName(QStringLiteral("DTA.Models.Step%1").arg(i));
        b->setText(QString::fromUtf8(kSteps[i].label));
        b->setToolTip(QString::fromUtf8(kSteps[i].tip));
        b->setCheckable(3 == i);  // ⛶ 是 fit-on-step 开关（参考 inset/outset 表态）
        b->setChecked(3 == i);    // 参考 fit 初始态 inset = true
        connect(b, &QToolButton::clicked, this, [this, i](bool checked) {
            switch (i) {
                case 0: stepToIndex(0); break;
                case 1: stepToIndex(m_stepIndex - 1); break;
                case 2: {
                    // "Set first enabled as step index"（IdPicker.ts:368-379——
                    // 命中即 stepToIndex(i)：toggleAll(false)+enableById+fit 副作用
                    // 2026-10-07 审计 S-1：原先只记 m_stepIndex 不隔离）。
                    if (auto* opened = openedDump()) {
                        auto const& entries = opened->provider->entries();
                        for (std::size_t k = 0; k < entries.size(); ++k)
                            if (entries[k].visible) {
                                stepToIndex(static_cast<int>(k));
                                break;
                            }
                    }
                    break;
                }
                case 3: setFitOnStep(checked); break;
                case 4: stepToIndex(m_stepIndex + 1); break;
                case 5:
                    if (auto* opened = openedDump())
                        stepToIndex(static_cast<int>(opened->provider->entries().size()) - 1);
                    break;
                default: break;
            }
            refresh();
        });
        steps->addWidget(b);
    }
    layout->addLayout(steps);

    // Models + Tile Trees 两分组。
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("DTA.Models.Tree"));
    m_tree->setColumnCount(1);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    layout->addWidget(m_tree);

    // 复选 → provider 开关 + 场景失效（changeDisplay 语义——见 DumpOpenHelper.h）。
    // 重建期（setCheckState）触发的 itemChanged 由 m_updating 守卫吞掉。
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int) {
        if (m_updating || item == nullptr)
            return;
        bool const isModelEntry = item->parent() != nullptr
            && item->data(0, Qt::UserRole).isValid()
            && item->data(0, Qt::UserRole).toInt() >= 0;
        if (!isModelEntry)
            return;
        if (auto* opened = openedDump()) {
            opened->provider->setModelVisible(
                static_cast<std::size_t>(item->data(0, Qt::UserRole).toInt()),
                item->checkState(0) == Qt::Checked);
            if (auto* vp = activeViewport())
                vp->InvalidateScene();
        }
    });

    connect(&m_refreshTimer, &QTimer::timeout, this, &TileTreePanel::refresh);
    m_refreshTimer.start(1000);
    refresh();
}

void TileTreePanel::refresh()
{
    // 内容指纹不变 → 跳过重建（保住选中/展开/勾选交互；M-L(2) 同款）。
    QString next = QStringLiteral("(none)");
    if (auto* opened = openedDump()) {
        QStringList lines;
        for (auto const& entry : opened->provider->entries())
            lines << QStringLiteral("%1=%2").arg(QString::fromStdString(entry.modelId),
                                                 entry.visible ? QStringLiteral("1")
                                                               : QStringLiteral("0"));
        for (auto const& treeId : opened->treeLoadLog)
            lines << QString::fromStdString(treeId);
        next = lines.join(QLatin1Char(';'));
    }
    if (next == m_content)
        return;
    m_content = next;

    m_updating = true;
    m_tree->clear();

    auto* opened = openedDump();
    if (opened == nullptr || opened->provider == nullptr) {
        auto* item = new QTreeWidgetItem(
            m_tree, QStringList(QStringLiteral("(no iModel open — use the Start page)")));
        item->setDisabled(true);
        item->setData(0, Qt::UserRole, kGroupRole);
        m_updating = false;
        return;
    }

    auto* connection = opened->connection.Get();

    // Models 分组（ModelPicker._populate 的复选形态——IdPicker.ts:398-405；
    // 参考按 model 名排序——imodel.json models[] 的 provider 条目序即装载序，
    // 名字顺序登记为 provider 条目序）。
    auto const& entries = opened->provider->entries();
    auto* modelsItem = new QTreeWidgetItem(
        m_tree, QStringList(QStringLiteral("Models (%1)").arg(quint64(entries.size()))));
    QFont bold = modelsItem->font(0);
    bold.setBold(true);
    modelsItem->setFont(0, bold);
    modelsItem->setData(0, Qt::UserRole, kGroupRole);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        QString name = QString::fromStdString(entries[i].modelId);
        if (connection) {
            for (auto const& model : connection->getModels()) {
                if (model.id.ToString() == entries[i].modelId) {
                    name = QString::fromStdString(model.name);
                    break;
                }
            }
        }
        auto* item = new QTreeWidgetItem(modelsItem, QStringList(name));
        item->setData(0, Qt::UserRole, static_cast<int>(i));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, entries[i].visible ? Qt::Checked : Qt::Unchecked);
    }
    modelsItem->setExpanded(true);

    // Tile Trees 分组（modelSelector 逐 model 装载序——treeLoadLog）。
    auto* treesItem = new QTreeWidgetItem(
        m_tree, QStringList(QStringLiteral("Tile Trees (%1)").arg(
                    quint64(opened->treeLoadLog.size()))));
    treesItem->setFont(0, bold);
    treesItem->setData(0, Qt::UserRole, kGroupRole);
    for (auto const& treeId : opened->treeLoadLog)
        new QTreeWidgetItem(treesItem, QStringList(QString::fromStdString(treeId)));
    treesItem->setExpanded(true);

    m_updating = false;
}

void TileTreePanel::setFitOnStep(bool on)
{
    // Ported from: IdPicker.ts:383-392 — ⛶ toggles _fitOnStep.
    m_fitOnStep = on;
}

void TileTreePanel::stepToIndex(int index)
{
    // Ported from: ModelPicker.stepToIndex (IdPicker.ts:420-428) — hide all,
    // enable the indexed model, fit when _fitOnStep.
    auto* opened = openedDump();
    if (opened == nullptr || opened->provider == nullptr)
        return;
    auto const& entries = opened->provider->entries();
    if (index < 0 || index >= static_cast<int>(entries.size()))
        return;

    m_stepIndex = index;
    opened->provider->setAllVisible(false);
    opened->provider->setModelVisible(static_cast<std::size_t>(index), true);

    // Ported from: IdPicker.ts:426 — ViewManip.fitView(vp, true)：computeFitRange
    // （已装载树的并集——隔离后仅剩该 model 的树，含 ensureMinLengths 语义）
    // + lookAtVolume + synchWithView。2026-10-07 审计 S-7：原先直取该树
    // rootTile.range×location 的 worldRange（与 computeFitRange 的最小域/
    // 并集面存在差异——FitViewTool::doFit 同款通道对齐）。
    if (auto* vp = activeViewport()) {
        vp->InvalidateScene();
        auto* view3d = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr;
        if (auto* spatial = view3d ? view3d->AsSpatialViewState() : nullptr) {
            dqGeom::Range3d const range = spatial->ComputeFitRange();
            if (!range.isNull()) {
                double const aspect = vp->viewRect().aspect();
                spatial->LookAtVolume(range, &aspect, nullptr);
            }
        }
        vp->synchWithView();
        vp->RequestRedraw();
    }
}

void TileTreePanel::showAll()
{
    // Ported from: IdPicker.show "All" → toggleIds(all, true)。
    if (auto* opened = openedDump()) {
        opened->provider->setAllVisible(true);
        if (auto* vp = activeViewport())
            vp->InvalidateScene();
    }
}

void TileTreePanel::hideAll()
{
    // Ported from: IdPicker.show "None" → toggleAll(false)。
    if (auto* opened = openedDump()) {
        opened->provider->setAllVisible(false);
        if (auto* vp = activeViewport())
            vp->InvalidateScene();
    }
}

void TileTreePanel::invert()
{
    // Ported from: IdPicker.invertAll (IdPicker.ts:27-36)。
    if (auto* opened = openedDump()) {
        for (auto& entry : opened->provider->entries())
            entry.visible = !entry.visible;
        if (auto* vp = activeViewport())
            vp->InvalidateScene();
    }
}

// 面板装配（FreeCAD ComboView 的 dock 位——左区；注册键沿用 "Std_ComboView"）。
void setupModelsPanel()
{
    auto* panel = new TileTreePanel;
    auto* dockMgr = DockWindowManager::instance();
    dockMgr->registerDockWindow("Std_ComboView", panel);
    if (auto* dock = dockMgr->addDockWindow("Models", panel, Qt::LeftDockWidgetArea))
        dock->show();
}

}  // namespace Gui
