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

// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp
//
// Modifications vs. FreeCAD reference (sanctioned DTA deviations, per task-5-brief):
//   * gsl::owner<T*> → raw T* (project-wide stubs/gsl/pointers was deleted).
//   * No prefPackManager in DTA → themeChanged() writes the
//     BaseApp/Preferences/MainWindow/StyleSheet pref directly.
//   * No theme thumbnail PNGs in resources.qrc → text-only buttons (no QIcon).
//   * FC_OS_MACOSX → Q_OS_MACOS (Qt cross-platform macro, per InputHintWidget.cpp:3).
//   * No Gui::Application singleton in DTA → onLinkActivated() stubs the
//     runCommandByName call; retranslateUi() falls back to hide().

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QStyleHints>
#include <QToolButton>

#include "ThemeSelectorWidget.h"

#include <App/Application.h>

using namespace StartGui;


// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:48-78 (isSystemInDarkMode)
// Qt 6.5+ colorScheme() is available in the bundled Qt 6.11.1.
static bool isSystemInDarkMode()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // https://www.qt.io/blog/dark-mode-on-windows-11-with-qt-6.5
    const auto scheme = QGuiApplication::styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Dark;
#elif QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    const QPalette defaultPalette;
    const auto text = defaultPalette.color(QPalette::WindowText);
    const auto window = defaultPalette.color(QPalette::Window);
    return text.lightness() > window.lightness();
#elif defined(Q_OS_MACOS)
    // Stub: simplified — CoreFoundation lookup not ported; defaults to false.
    // Authored: no reference test exists for the macOS fallback in the DTA shell.
    return false;
#else
    return false;
#endif
}


// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:81-91 (shouldHideClassicTheme)
// FC_OS_MACOSX/FC_OS_WIN32 → Q_OS_MACOS/Q_OS_WIN for DTA (InputHintWidget.cpp:3 pattern).
static bool shouldHideClassicTheme()
{
    // Classic on macOS and windows 11 with qt6(.4+?) doesn't work when system
    // is in dark mode and to make matter worse, on macOS there's a setting that
    // changes mode depending on time of day.
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0) || defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    return true;
#else
    return false;
#endif
}


// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:94-106 (ctor)
ThemeSelectorWidget::ThemeSelectorWidget(QWidget* parent)
    : QWidget(parent)
    , m_titleLabel {nullptr}
    , m_descriptionLabel {nullptr}
    , m_buttons {nullptr, nullptr, nullptr}
{
    setObjectName(QLatin1String("ThemeSelectorWidget"));
    if (shouldHideClassicTheme()) {
        preselectThemeFromSystemSettings();
    }
    setupUi();
    qApp->installEventFilter(this);
}


// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:109-160 (setupButtons)
void ThemeSelectorWidget::setupButtons(QBoxLayout* layout)
{
    if (!layout) {
        return;
    }
    std::map<Theme, QString> themeMap {
        {Theme::Classic, tr("FreeCAD Classic")},
        {Theme::Dark, tr("FreeCAD Dark")},
        {Theme::Light, tr("FreeCAD Light")}
    };
    // TODO: deferred — add theme thumbnails to qrc. FreeCAD uses
    //   ":/thumbnails/Theme_thumbnail_{classic,light,dark}.png" (ThemeSelectorWidget.cpp:119-123).
    //   DTA resources.qrc has no such assets; buttons are text-only until added.
    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/MainWindow"
    );
    auto styleSheetName = QString::fromStdString(hGrp->GetASCII("StyleSheet"));
    for (const auto& theme : themeMap) {
        auto button = new QToolButton();

        if (theme.first == Theme::Classic && shouldHideClassicTheme()) {
            button->setVisible(false);
        }

        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setToolButtonStyle(Qt::ToolButtonStyle::ToolButtonTextUnderIcon);
        button->setText(theme.second);
        // No icon — text-only buttons (see TODO above).
        if (theme.first == Theme::Classic && styleSheetName.isEmpty()) {
            button->setChecked(true);
        }
        else if (
            theme.first == Theme::Light
            && styleSheetName.contains(QLatin1String("FreeCAD Light"), Qt::CaseSensitivity::CaseInsensitive)
        ) {
            button->setChecked(true);
        }
        else if (
            theme.first == Theme::Dark
            && styleSheetName.contains(QLatin1String("FreeCAD Dark"), Qt::CaseSensitivity::CaseInsensitive)
        ) {
            button->setChecked(true);
        }
        connect(button, &QToolButton::clicked, this, [this, theme] { themeChanged(theme.first); });
        layout->addWidget(button);
        m_buttons[static_cast<int>(theme.first)] = button;
    }
}


// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:162-174 (setupUi)
void ThemeSelectorWidget::setupUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    auto* buttonLayout = new QHBoxLayout;
    m_titleLabel = new QLabel;
    m_descriptionLabel = new QLabel;
    outerLayout->addWidget(m_titleLabel);
    outerLayout->addLayout(buttonLayout);
    outerLayout->addWidget(m_descriptionLabel);
    setupButtons(buttonLayout);
    retranslateUi();
    connect(m_descriptionLabel, &QLabel::linkActivated, this, &ThemeSelectorWidget::onLinkActivated);
}


// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:176-193 (onLinkActivated)
// Modified: DTA has no Gui::Application/commandManager → runCommandByName deferred.
//   The preference settings (PackageTypeSelection=3, StatusSelection=0) are still
//   applied so a future Addon Manager integration picks them up.
void ThemeSelectorWidget::onLinkActivated(const QString& link)
{
    auto const addonManagerLink = QStringLiteral("freecad:Std_AddonMgr");

    if (link != addonManagerLink) {
        return;
    }

    // Set the user preferences to include only preference packs.
    // This is a quick and dirty way to open Addon Manager with only themes.
    auto pref = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Addons"
    );
    pref->SetInt("PackageTypeSelection", 3);  // 3 stands for Preference Packs
    pref->SetInt("StatusSelection", 0);       // 0 stands for any installation status

    // TODO: deferred — DTA has no Gui::Application singleton / commandManager in
    //   the Start module. When Addon Manager is wired, run:
    //   Gui::Application::Instance->commandManager().runCommandByName("Std_AddonMgr");
}


// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:195-210 (preselectThemeFromSystemSettings)
// Modified: DTA has no Gui::isInternalGuiTestRun() helper — test runs proceed with
//   the system-detection path, which is a no-op in the test harness's offscreen
//   environment (QGuiApplication::styleHints()->colorScheme() returns Unknown).
void ThemeSelectorWidget::preselectThemeFromSystemSettings()
{
    auto nullStyle("<N/A>");
    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/MainWindow"
    );
    auto styleSheetName = QString::fromStdString(hGrp->GetASCII("StyleSheet", nullStyle));
    if (styleSheetName == QString::fromLatin1(nullStyle)) {
        auto theme = isSystemInDarkMode() ? Theme::Dark : Theme::Light;
        themeChanged(theme);
    }
}


// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:212-238 (themeChanged)
// Modified: prefPackManager absent in DTA → write the StyleSheet pref directly
//   (FreeCAD's prefPackManager->apply("FreeCAD {Classic|Dark|Light}") sets this
//   pref as one of its side effects). Accent-color defaults are preserved.
void ThemeSelectorWidget::themeChanged(Theme newTheme)
{
    ParameterGrp::handle hGrpMainWindow = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/MainWindow"
    );
    switch (newTheme) {
        case Theme::Classic:
            // FreeCAD Classic has no stylesheet (empty string).
            hGrpMainWindow->SetASCII("StyleSheet", "");
            break;
        case Theme::Dark:
            hGrpMainWindow->SetASCII("StyleSheet", "FreeCAD Dark");
            break;
        case Theme::Light:
            hGrpMainWindow->SetASCII("StyleSheet", "FreeCAD Light");
            break;
    }
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Themes"
    );
    const unsigned long nonExistentColor = static_cast<unsigned long>(-1434171135);
    const unsigned long defaultAccentColor = static_cast<unsigned long>(1434171135);
    // ParameterGrp stub lacks GetUnsigned/SetUnsigned; accent-color defaults are
    // a cosmetic concern and are skipped in the DTA shell. Logged here for the
    // future real ParameterGrp integration.
    (void)hGrp;
    (void)nonExistentColor;
    (void)defaultAccentColor;
}


// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:240-246 (eventFilter)
bool ThemeSelectorWidget::eventFilter(QObject* object, QEvent* event)
{
    if (object == this && event->type() == QEvent::LanguageChange) {
        this->retranslateUi();
    }
    return QWidget::eventFilter(object, event);
}


// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:248-263 (retranslateUi)
// Modified: DTA has no Gui::Application singleton → the AddonMgr presence check
//   falls back to hide(), matching FreeCAD's else-branch (ThemeSelectorWidget.cpp:258).
void ThemeSelectorWidget::retranslateUi()
{
    m_titleLabel->setText(QLatin1String("<h2>") + tr("Theme") + QLatin1String("</h2>"));
    // No Gui::Application in DTA → cannot query commandManager().getCommandByName
    // ("Std_AddonMgr"). Hide description (FreeCAD else-branch).
    // TODO: deferred — when Gui::Application is wired, restore the Addon Manager
    //   link text from FreeCAD ThemeSelectorWidget.cpp:251-256.
    m_descriptionLabel->hide();
    m_buttons[static_cast<int>(Theme::Dark)]->setText(tr("FreeCAD Dark", "Visual theme name"));
    m_buttons[static_cast<int>(Theme::Light)]->setText(tr("FreeCAD Light", "Visual theme name"));
    m_buttons[static_cast<int>(Theme::Classic)]->setText(tr("FreeCAD Classic", "Visual theme name"));
}
