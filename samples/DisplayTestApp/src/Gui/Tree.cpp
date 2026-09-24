// Ported from: FreeCAD src/Gui/Tree.cpp
/***************************************************************************
 *   Copyright (c) 2004 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "Tree.h"

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>

namespace Gui
{

// ============================================================================
// TreeWidgetItemDelegate
// Ported from: FreeCAD src/Gui/Tree.cpp:391-567
// ============================================================================

// Ported from: FreeCAD src/Gui/Tree.cpp:391-397
TreeWidgetItemDelegate::TreeWidgetItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

// Ported from: FreeCAD src/Gui/Tree.cpp:426-458
void TreeWidgetItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // Ported from: FreeCAD src/Gui/Tree.cpp:457 — delegate to style
    QTreeView* tree = qobject_cast<QTreeView*>(parent());
    if (tree) {
        tree->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, tree);
    }
}

// Ported from: FreeCAD src/Gui/Tree.cpp:460-503
void TreeWidgetItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    QStyledItemDelegate::initStyleOption(option, index);

    // Ported from: FreeCAD src/Gui/Tree.cpp:480 — elide middle for long text
    option->textElideMode = Qt::ElideMiddle;
}

// Ported from: FreeCAD src/Gui/Tree.cpp:532-559
QWidget* TreeWidgetItemDelegate::createEditor(
    QWidget* parent,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
) const
{
    Q_UNUSED(option);

    // Only allow editing on column 0 (Label)
    if (index.column() != 0) {
        return nullptr;
    }

    // Shell stub: use default QLineEdit editor for inline rename (F2)
    auto* editor = new QLineEdit(parent);
    editor->setFrame(true);
    return editor;
}

// Ported from: FreeCAD src/Gui/Tree.cpp:561-567
QSize TreeWidgetItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    // Add 4px vertical padding for comfortable row spacing
    size.setHeight(size.height() + 4);
    return size;
}

// ============================================================================
// TreeWidget
// Ported from: FreeCAD src/Gui/Tree.cpp:570-749
// ============================================================================

TreeWidget* TreeWidget::s_instance = nullptr;

// Ported from: FreeCAD src/Gui/Tree.cpp:570-749
TreeWidget::TreeWidget(QWidget* parent)
    : QTreeWidget(parent)
{
    s_instance = this;

    // Ported from: FreeCAD src/Gui/Tree.cpp:586-589 — core setup
    setDragEnabled(true);
    setAcceptDrops(true);
    setColumnCount(3);
    setItemDelegate(new TreeWidgetItemDelegate(this));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Ported from: FreeCAD src/Gui/Tree.cpp:694 — stretch last section
    header()->setStretchLastSection(true);

    // Ported from: FreeCAD src/Gui/Tree.cpp:710 — ExtendedSelection
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Ported from: FreeCAD src/Gui/Tree.cpp:712 — mouse tracking
    setMouseTracking(true);

    // Ported from: FreeCAD src/Gui/Tree.cpp:588 — 3 columns: Label, Name, Type
    // Columns 1 (Name) and 2 (Type) are hidden by default
    setColumnHidden(1, true);
    setColumnHidden(2, true);

    // Ported from: FreeCAD src/Gui/Tree.cpp:656-662 — actions + setupText
    setupContextMenu();
    setupText();
}

// Ported from: FreeCAD src/Gui/Tree.cpp:3882-3938
void TreeWidget::setupText()
{
    headerItem()->setText(0, tr("Labels & Attributes"));
    headerItem()->setText(1, tr("Description"));
    headerItem()->setText(2, tr("Internal name"));

    m_showHiddenAction->setText(tr("Show Items Hidden in Tree View"));
    m_showHiddenAction->setStatusTip(tr("Shows items that are marked as 'hidden' in the tree view"));

    m_toggleVisibilityInTreeAction->setText(tr("Toggle Visibility in Tree View"));
    m_toggleVisibilityInTreeAction->setStatusTip(
        tr("Toggles the visibility of selected items in the tree view")
    );

    m_createGroupAction->setText(tr("Create Group"));
    m_createGroupAction->setStatusTip(tr("Creates a group"));

    m_relabelObjectAction->setText(tr("Rename"));
    m_relabelObjectAction->setStatusTip(tr("Renames object"));

    m_selectDependentsAction->setText(tr("Add Dependent Objects to Selection"));
    m_selectDependentsAction->setStatusTip(tr("Adds all dependent objects to the selection"));

    m_closeDocAction->setText(tr("Close Document"));
    m_closeDocAction->setStatusTip(tr("Closes the document"));

#ifdef Q_OS_MAC
    m_openFileLocationAction->setText(tr("Reveal in Finder"));
    m_openFileLocationAction->setStatusTip(tr("Reveals the current file location in Finder"));
#else
    m_openFileLocationAction->setText(tr("Open File Location"));
    m_openFileLocationAction->setStatusTip(tr("Opens the current file location"));
#endif

    m_reloadDocAction->setText(tr("Reload Document"));
    m_reloadDocAction->setStatusTip(tr("Reloads a partially loaded document"));

    m_skipRecomputeAction->setText(tr("Skip Recomputes"));
    m_skipRecomputeAction->setStatusTip(tr("Enables or disables the recomputations of document"));

    m_allowPartialRecomputeAction->setText(tr("Allow Partial Recomputes"));
    m_allowPartialRecomputeAction->setStatusTip(
        tr("Enables or disables the recomputating editing object when 'skip recomputation' is enabled")
    );

    m_markRecomputeAction->setText(tr("Mark to Recompute"));
    m_markRecomputeAction->setStatusTip(tr("Marks this object to be recomputed"));

    m_recomputeObjectAction->setText(tr("Recompute Object"));
    m_recomputeObjectAction->setStatusTip(tr("Recomputes the selected object"));

    m_searchObjectsAction->setText(tr("Search Objects"));
    m_searchObjectsAction->setStatusTip(tr("Searches for objects in the tree"));
}

// Ported from: FreeCAD src/Gui/Tree.cpp:592-662
void TreeWidget::setupContextMenu()
{
    // Ported from: FreeCAD src/Gui/Tree.cpp:592-594 — showHiddenAction
    m_showHiddenAction = new QAction(this);
    m_showHiddenAction->setCheckable(true);

    // Ported from: FreeCAD src/Gui/Tree.cpp:596-602 — toggleVisibilityInTreeAction
    m_toggleVisibilityInTreeAction = new QAction(this);

    // Ported from: FreeCAD src/Gui/Tree.cpp:604-605 — createGroupAction
    m_createGroupAction = new QAction(this);

    // Ported from: FreeCAD src/Gui/Tree.cpp:607-614 — relabelObjectAction + F2 shortcut
    m_relabelObjectAction = new QAction(this);
#ifndef Q_OS_MAC
    m_relabelObjectAction->setShortcut(Qt::Key_F2);
#else
    m_relabelObjectAction->setShortcut(QKeySequence(Qt::Key_Return));
#endif
    m_relabelObjectAction->setShortcutVisibleInContextMenu(true);

    // Ported from: FreeCAD src/Gui/Tree.cpp:619-620 — selectDependentsAction
    m_selectDependentsAction = new QAction(this);

    // Ported from: FreeCAD src/Gui/Tree.cpp:622-623 — closeDocAction
    m_closeDocAction = new QAction(this);

    // Ported from: FreeCAD src/Gui/Tree.cpp:625-626 — reloadDocAction
    m_reloadDocAction = new QAction(this);

    // Ported from: FreeCAD src/Gui/Tree.cpp:628-630 — skipRecomputeAction (checkable)
    m_skipRecomputeAction = new QAction(this);
    m_skipRecomputeAction->setCheckable(true);

    // Ported from: FreeCAD src/Gui/Tree.cpp:635-642 — allowPartialRecomputeAction (checkable)
    m_allowPartialRecomputeAction = new QAction(this);
    m_allowPartialRecomputeAction->setCheckable(true);

    // Ported from: FreeCAD src/Gui/Tree.cpp:644-645 — markRecomputeAction
    m_markRecomputeAction = new QAction(this);

    // Ported from: FreeCAD src/Gui/Tree.cpp:647-648 — recomputeObjectAction
    m_recomputeObjectAction = new QAction(this);

    // Ported from: FreeCAD src/Gui/Tree.cpp:656-659 — searchObjectsAction
    m_searchObjectsAction = new QAction(this);
    connect(m_searchObjectsAction, &QAction::triggered, this, &TreeWidget::emitSearchObjects);

    // Ported from: FreeCAD src/Gui/Tree.cpp:661-662 — openFileLocationAction
    m_openFileLocationAction = new QAction(this);
}

// Ported from: FreeCAD src/Gui/Tree.cpp:1182-1369
void TreeWidget::contextMenuEvent(QContextMenuEvent* e)
{
    QMenu contextMenu;

    m_contextItem = itemAt(e->pos());

    if (m_contextItem && m_contextItem->type() == DocumentType) {
        // Ported from: FreeCAD src/Gui/Tree.cpp:1203-1246 — document item context menu
        contextMenu.addAction(m_showHiddenAction);
        contextMenu.addAction(m_openFileLocationAction);
        contextMenu.addAction(m_searchObjectsAction);
        contextMenu.addAction(m_closeDocAction);
        contextMenu.addAction(m_reloadDocAction);
        contextMenu.addAction(m_selectDependentsAction);
        contextMenu.addAction(m_skipRecomputeAction);
        contextMenu.addAction(m_allowPartialRecomputeAction);
        contextMenu.addAction(m_markRecomputeAction);
        contextMenu.addAction(m_createGroupAction);
        contextMenu.addSeparator();
    }
    else if (m_contextItem && m_contextItem->type() == ObjectType) {
        // Ported from: FreeCAD src/Gui/Tree.cpp:1247-1303 — object item context menu
        contextMenu.addAction(m_showHiddenAction);
        contextMenu.addAction(m_toggleVisibilityInTreeAction);
        contextMenu.addAction(m_createGroupAction);
        contextMenu.addAction(m_selectDependentsAction);
        contextMenu.addSeparator();
        contextMenu.addAction(m_markRecomputeAction);
        contextMenu.addAction(m_recomputeObjectAction);
        contextMenu.addSeparator();
        contextMenu.addAction(m_relabelObjectAction);
    }
    else {
        // No item under cursor — show general menu
        contextMenu.addAction(m_searchObjectsAction);
    }

    contextMenu.exec(e->globalPos());
}

// Ported from: FreeCAD src/Gui/Tree.cpp:99 — instance accessor
TreeWidget* TreeWidget::instance()
{
    return s_instance;
}

// ============================================================================
// TreePanel
// Ported from: FreeCAD src/Gui/Tree.cpp:4213-4297
// ============================================================================

// Ported from: FreeCAD src/Gui/Tree.cpp:4213-4237
TreePanel::TreePanel(QWidget* parent)
    : QWidget(parent)
{
    m_tree = new TreeWidget(this);

    auto* pLayout = new QVBoxLayout(this);
    pLayout->setSpacing(0);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->addWidget(m_tree);

    // Ported from: FreeCAD src/Gui/Tree.cpp:4226 — connect emitSearchObjects → showEditor
    connect(m_tree, &TreeWidget::emitSearchObjects, this, &TreePanel::showEditor);

    // Ported from: FreeCAD src/Gui/Tree.cpp:4228-4236 — search box (simplified: plain QLineEdit)
    m_searchBox = new QLineEdit(this);
    pLayout->addWidget(m_searchBox);
    m_searchBox->hide();
    m_searchBox->installEventFilter(this);
    m_searchBox->setPlaceholderText(tr("Search"));
    connect(m_searchBox, &QLineEdit::returnPressed, this, &TreePanel::accept);
    connect(m_searchBox, &QLineEdit::textChanged, this, &TreePanel::itemSearch);
}

// Ported from: FreeCAD src/Gui/Tree.cpp:4241-4247
void TreePanel::accept()
{
    QString text = m_searchBox->text();
    hideEditor();
    m_tree->setFocus();
    // Shell stub: filtering not wired to backend
    Q_UNUSED(text);
}

// Ported from: FreeCAD src/Gui/Tree.cpp:4249-4273
bool TreePanel::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj != m_searchBox) {
        return false;
    }

    if (ev->type() == QEvent::KeyPress) {
        int key = static_cast<QKeyEvent*>(ev)->key();
        switch (key) {
        case Qt::Key_Escape:
            hideEditor();
            m_tree->setFocus();
            return true;
        default:
            break;
        }
    }

    return false;
}

// Ported from: FreeCAD src/Gui/Tree.cpp:4275-4280
void TreePanel::showEditor()
{
    m_searchBox->show();
    m_searchBox->setFocus();
}

// Ported from: FreeCAD src/Gui/Tree.cpp:4282-4292
void TreePanel::hideEditor()
{
    m_searchBox->clear();
    m_searchBox->hide();
    auto sels = m_tree->selectedItems();
    if (!sels.empty()) {
        m_tree->scrollToItem(sels.front());
    }
}

// Ported from: FreeCAD src/Gui/Tree.cpp:4294-4297
void TreePanel::itemSearch(const QString& text)
{
    Q_UNUSED(text);
    // Shell stub: itemSearch filtering not wired to backend
}

}  // namespace Gui
