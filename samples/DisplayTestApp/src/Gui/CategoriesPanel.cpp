// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Categories 停靠面板实现（M-N(1)），设计说明见
// CategoriesPanel.h。
#include "CategoriesPanel.h"

#include <QCoreApplication>
#include <QDockWidget>
#include <QComboBox>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "../DumpOpenHelper.h"
#include "DockWindowManager.h"
#include "MainWindow.h"
#include "TileTreePanel.h"
#include "View3DInventor.h"

#include <dqApp/Application.h>
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>

namespace Gui {

namespace {

dta::DumpOpenResult* openedDump()
{
    auto* mw = MainWindow::getInstance();
    auto* view3d = mw ? qobject_cast<View3DInventor*>(mw->activeWindow()) : nullptr;
    return view3d ? dta::findOpenedDump(view3d) : nullptr;
}

dqApp::Viewport* activeViewport()
{
    auto* mw = MainWindow::getInstance();
    auto* view3d = mw ? qobject_cast<View3DInventor*>(mw->activeWindow()) : nullptr;
    return view3d ? view3d->getUeViewport() : nullptr;
}

}  // namespace

CategoriesPanel::CategoriesPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("CategoriesPanel"));
    setWindowTitle(QCoreApplication::translate("MainWindow", "Categories"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    // 共享动作下拉（IdPicker.ts:40-53 接线子集——TileTreePanel 同款）。
    auto* actions = new QComboBox(this);
    actions->setObjectName(QStringLiteral("DTA.Categories.Actions"));
    actions->addItem(QString());
    actions->addItem(QStringLiteral("Show All"));
    actions->addItem(QStringLiteral("Hide All"));
    connect(actions, &QComboBox::activated, this,
            [this, actions](int index) {
                if (1 == index)
                    showAll();
                else if (2 == index)
                    hideAll();
                actions->setCurrentIndex(0);
                refresh();
            });
    layout->addWidget(actions);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("DTA.Categories.Tree"));
    m_tree->setHeaderLabel(
        QCoreApplication::translate("MainWindow", "Category"));
    m_tree->setRootIsDecorated(false);
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem*, int) {
        if (!m_updating)
            applyVisibility();
    });
    layout->addWidget(m_tree);

    connect(&m_refreshTimer, &QTimer::timeout, this, &CategoriesPanel::refresh);
    m_refreshTimer.start(1000);
    refresh();
}

void CategoriesPanel::refresh()
{
    auto* opened = openedDump();
    auto* vp = activeViewport();
    if (!opened || !vp || !opened->viewState) {
        // 无活动 dump 视口：空清单（指纹区分空/非空避免闪重建）。
        if (m_content != QStringLiteral("<empty>")) {
            m_updating = true;
            m_tree->clear();
            m_updating = false;
            m_content = QStringLiteral("<empty>");
        }
        return;
    }

    // 数据源：ViewState 的 CategorySelector（打开链应用的 saved
    // categorySelectorProps——与 DTA 面板列举源同面）。
    auto const& categories = opened->viewState->GetCategorySelector().getCategories();
    QString fingerprint;
    for (auto const& cat : categories)
        fingerprint += QStringLiteral("%1;").arg(cat.ToString().c_str());
    // 勾选态变化也要刷新指纹（勾选态由本面板持有——视口
    // GetInvisibleSubCategories 是唯一持久真源，指纹并入）。
    auto const& invisible = vp->GetInvisibleSubCategories();
    for (uint64_t id : invisible)
        fingerprint += QStringLiteral("i%1;").arg(id);
    if (fingerprint == m_content)
        return;
    m_content = fingerprint;

    m_updating = true;
    m_tree->clear();
    for (auto const& cat : categories) {
        // subCategory = category + 1（imodel-native 默认子类规则——头注
        // EQUIVALENCE 发散登记）。
        uint64_t const subCat = cat.GetValue() + 1u;
        bool const checked = invisible.find(subCat) == invisible.end();
        auto* item = new QTreeWidgetItem(m_tree);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
        item->setText(0, QStringLiteral("0x%1")
                                 .arg(cat.GetValue(), 0, 16));
        item->setData(0, Qt::UserRole,
                      QVariant(static_cast<qulonglong>(cat.GetValue())));
    }
    m_updating = false;
}

void CategoriesPanel::applyVisibility()
{
    auto* vp = activeViewport();
    if (!vp)
        return;

    // 遍历树勾选态：未勾 category C → 隐藏 subCategory C+1。
    std::set<uint64_t> invisible;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto* item = m_tree->topLevelItem(i);
        if (item->checkState(0) != Qt::Checked) {
            uint64_t const cat =
                item->data(0, Qt::UserRole).toULongLong();
            invisible.insert(cat + 1u);
        }
    }
    vp->SetInvisibleSubCategories(invisible);
    refresh();
}

void CategoriesPanel::showAll()
{
    m_updating = true;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        m_tree->topLevelItem(i)->setCheckState(0, Qt::Checked);
    m_updating = false;
    applyVisibility();
}

void CategoriesPanel::hideAll()
{
    m_updating = true;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        m_tree->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
    m_updating = false;
    applyVisibility();
}

void setupCategoriesPanel()
{
    auto* panel = new CategoriesPanel;
    // Models 停靠区追加 tab（Qt::RightDockWidgetArea 内 tab 化——与
    // DockWindowManager 的区域语义一致）。
    auto* dockMgr = DockWindowManager::instance();
    dockMgr->registerDockWindow("Std_CategoriesView", panel);
    if (auto* dock = dockMgr->addDockWindow("Categories", panel,
                                             Qt::LeftDockWidgetArea))
        dock->show();
}

}  // namespace Gui
