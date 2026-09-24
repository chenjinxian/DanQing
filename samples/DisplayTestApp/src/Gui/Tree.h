// Ported from: FreeCAD src/Gui/Tree.h
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

#pragma once

#include <QStyledItemDelegate>
#include <QTreeWidget>

class QLineEdit;

namespace Gui
{

class TreeWidget;
class TreePanel;

// Ported from: FreeCAD src/Gui/Tree.h:351-387 + Tree.cpp:391-567
class TreeWidgetItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit TreeWidgetItemDelegate(QObject* parent = nullptr);

    QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
    ) const override;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
    ) const override;
};

// Ported from: FreeCAD src/Gui/Tree.h:60-341 + Tree.cpp:570-749
class TreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit TreeWidget(QWidget* parent = nullptr);
    ~TreeWidget() override = default;

    // Column constants — Ported from: FreeCAD src/Gui/Tree.h:101-102
    static constexpr int DocumentType = QTreeWidgetItem::UserType + 1;
    static constexpr int ObjectType = QTreeWidgetItem::UserType + 2;

    /// Returns the tree widget instance (singleton pattern).
    static TreeWidget* instance();

protected:
    void contextMenuEvent(QContextMenuEvent* e) override;

Q_SIGNALS:
    void emitSearchObjects();

private:
    // Ported from: FreeCAD src/Gui/Tree.cpp:592-662 — action creation
    void setupContextMenu();

    // Ported from: FreeCAD src/Gui/Tree.cpp:3882-3938 — action text setup
    void setupText();

    // Context menu actions — Ported from: FreeCAD src/Gui/Tree.h:271-284
    QAction* m_showHiddenAction = nullptr;
    QAction* m_toggleVisibilityInTreeAction = nullptr;
    QAction* m_createGroupAction = nullptr;
    QAction* m_relabelObjectAction = nullptr;
    QAction* m_selectDependentsAction = nullptr;
    QAction* m_skipRecomputeAction = nullptr;
    QAction* m_allowPartialRecomputeAction = nullptr;
    QAction* m_markRecomputeAction = nullptr;
    QAction* m_recomputeObjectAction = nullptr;
    QAction* m_closeDocAction = nullptr;
    QAction* m_reloadDocAction = nullptr;
    QAction* m_searchObjectsAction = nullptr;
    QAction* m_openFileLocationAction = nullptr;

    QTreeWidgetItem* m_contextItem = nullptr;

    static TreeWidget* s_instance;
};

// Ported from: FreeCAD src/Gui/Tree.h:580-599 + Tree.cpp:4213-4297
class TreePanel : public QWidget
{
    Q_OBJECT
public:
    explicit TreePanel(QWidget* parent = nullptr);
    ~TreePanel() override = default;

    TreeWidget* tree() const { return m_tree; }
    QLineEdit* searchBox() const { return m_searchBox; }

private Q_SLOTS:
    void accept();
    void itemSearch(const QString& text);

public Q_SLOTS:
    // Deviation: made public for testability (FreeCAD has these as private)
    void showEditor();
    void hideEditor();

private:
    bool eventFilter(QObject* obj, QEvent* ev) override;

    TreeWidget* m_tree;
    QLineEdit* m_searchBox;
};

}  // namespace Gui
