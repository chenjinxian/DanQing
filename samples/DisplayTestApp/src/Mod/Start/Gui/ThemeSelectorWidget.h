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

// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.h

#pragma once

#include <FCGlobal.h>
#include <QWidget>
#include <array>

class QBoxLayout;
class QLabel;
class QToolButton;

namespace StartGui
{

// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.h:36-41
enum class Theme
{
    Classic,
    Light,
    Dark
};

/// A widget to allow selection of the UI theme (color scheme).
// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.h:44-64
class StartGuiExport ThemeSelectorWidget: public QWidget
{
    Q_OBJECT
public:
    explicit ThemeSelectorWidget(QWidget* parent = nullptr);
    bool eventFilter(QObject* object, QEvent* event) override;

protected:
    // Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:212-238 (themeChanged)
    void themeChanged(Theme newTheme);

private:
    void retranslateUi();
    void setupUi();
    // Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:109-160 (setupButtons)
    void setupButtons(QBoxLayout* layout);
    // Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:176-193 (onLinkActivated)
    void onLinkActivated(const QString& link);
    // Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:195-210 (preselectThemeFromSystemSettings)
    void preselectThemeFromSystemSettings();

    QLabel* m_titleLabel;
    QLabel* m_descriptionLabel;
    std::array<QToolButton*, 3> m_buttons;
};

}  // namespace StartGui
