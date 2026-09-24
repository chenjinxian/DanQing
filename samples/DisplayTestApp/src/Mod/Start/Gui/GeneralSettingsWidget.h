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

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.h

#pragma once

#include <FCGlobal.h>
#include <QWidget>
#include <QString>

class QLabel;
class QComboBox;

namespace StartGui
{

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.h:35-67
class StartGuiExport GeneralSettingsWidget: public QWidget
{
    Q_OBJECT
public:
    explicit GeneralSettingsWidget(QWidget* parent = nullptr);
    ~GeneralSettingsWidget() override;  // 卸载 qApp 事件过滤器（栈实例测试暴露的悬空 filter SEH）

    bool eventFilter(QObject* object, QEvent* event) override;

private:
    void retranslateUi();

    void setupUi();
    void createHorizontalUi();

    QString createLabelText(const QString& translatedText) const;
    QComboBox* createLanguageComboBox();
    QComboBox* createUnitSystemComboBox();
    QComboBox* createNavigationStyleComboBox();

    void onLanguageChanged(int index);
    void onUnitSystemChanged(int index);
    void onNavigationStyleChanged(int index);

    // Non-owning pointers to things that need to be re-translated when the language changes
    // (Qt parent-child ownership applies: each widget is parented to `this`).
    QLabel* m_languageLabel;
    QLabel* m_unitSystemLabel;
    QLabel* m_navigationStyleLabel;
    QComboBox* m_languageComboBox;
    QComboBox* m_unitSystemComboBox;
    QComboBox* m_navigationStyleComboBox;
};

}  // namespace StartGui
