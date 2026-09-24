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

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QWidget>

#include <algorithm>

#include "GeneralSettingsWidget.h"

#include <App/Application.h>
#include <Base/UnitsApi.h>
#include <Gui/Language/Translator.h>
#include <Gui/Navigation/NavigationStyle.h>

using namespace StartGui;

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:45-57
GeneralSettingsWidget::GeneralSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , m_languageLabel {nullptr}
    , m_unitSystemLabel {nullptr}
    , m_navigationStyleLabel {nullptr}
    , m_languageComboBox {nullptr}
    , m_unitSystemComboBox {nullptr}
    , m_navigationStyleComboBox {nullptr}
{
    setObjectName(QLatin1String("GeneralSettingsWidget"));
    setupUi();
    qApp->installEventFilter(this);
}

GeneralSettingsWidget::~GeneralSettingsWidget()
{
    // 构造时装到 qApp 的过滤器必须在析构时卸载：栈实例（测试）析构后
    // 悬空 this 残留在 qApp filter 链上，后续任何事件分发即 SEH/UB
    if (qApp)
        qApp->removeEventFilter(this);
}

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:59-73
void GeneralSettingsWidget::setupUi()
{
    if (layout()) {
        qDeleteAll(findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
        delete layout();
    }
    m_languageLabel = new QLabel;
    m_navigationStyleLabel = new QLabel;
    m_unitSystemLabel = new QLabel;
    createLanguageComboBox();
    createUnitSystemComboBox();
    createNavigationStyleComboBox();
    createHorizontalUi();
    retranslateUi();
}

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:75-87
void GeneralSettingsWidget::createHorizontalUi()
{
    auto mainLayout = new QHBoxLayout(this);
    const int extraSpace {36};
    mainLayout->addWidget(m_languageLabel);
    mainLayout->addWidget(m_languageComboBox);
    mainLayout->addSpacing(extraSpace);
    mainLayout->addWidget(m_unitSystemLabel);
    mainLayout->addWidget(m_unitSystemComboBox);
    mainLayout->addSpacing(extraSpace);
    mainLayout->addWidget(m_navigationStyleLabel);
    mainLayout->addWidget(m_navigationStyleComboBox);
}

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:90-95
QString GeneralSettingsWidget::createLabelText(const QString& translatedText) const
{
    static const auto h2Start = QLatin1String("<h2>");
    static const auto h2End = QLatin1String("</h2>");
    return h2Start + translatedText + h2End;
}

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:97-143
QComboBox* GeneralSettingsWidget::createLanguageComboBox()
{
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/General"
    );
    auto langToStr = Gui::Translator::instance()->activeLanguage();
    QByteArray language = hGrp->GetASCII("Language", langToStr.c_str()).c_str();
    auto comboBox = new QComboBox;
    comboBox->setObjectName(QLatin1String("languageComboBox"));
    comboBox->addItem(QStringLiteral("English"), QByteArray("English"));
    Gui::TStringMap list = Gui::Translator::instance()->supportedLocales();
    int index {1};
    for (auto it = list.begin(); it != list.end(); ++it, ++index) {
        QByteArray lang = it->first.c_str();
        QString langname = QString::fromLatin1(lang.constData());

        if (it->second == "sr-CS") {
            // Qt does not treat sr-CS (Serbian, Latin) as a Latin-script variant by default: this
            // forces it to do so.
            it->second = "sr_Latn";
        }

        QLocale locale(QString::fromLatin1(it->second.c_str()));
        QString native = locale.nativeLanguageName();
        if (!native.isEmpty()) {
            if (native[0].isLetter()) {
                native[0] = native[0].toUpper();
            }
            langname = native;
        }

        comboBox->addItem(langname, lang);
        if (language == lang) {
            comboBox->setCurrentIndex(index);
        }
    }
    if (QAbstractItemModel* model = comboBox->model()) {
        model->sort(0);
    }
    m_languageComboBox = comboBox;
    connect(
        m_languageComboBox,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &GeneralSettingsWidget::onLanguageChanged
    );
    return comboBox;
}

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:145-157
QComboBox* GeneralSettingsWidget::createUnitSystemComboBox()
{
    // Contents are created in retranslateUi()
    auto comboBox = new QComboBox;
    comboBox->setObjectName(QLatin1String("unitSystemComboBox"));
    m_unitSystemComboBox = comboBox;
    connect(
        m_unitSystemComboBox,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &GeneralSettingsWidget::onUnitSystemChanged
    );
    return comboBox;
}

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:159-171
QComboBox* GeneralSettingsWidget::createNavigationStyleComboBox()
{
    // Contents are created in retranslateUi()
    auto comboBox = new QComboBox;
    comboBox->setObjectName(QLatin1String("navigationStyleComboBox"));
    m_navigationStyleComboBox = comboBox;
    connect(
        m_navigationStyleComboBox,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &GeneralSettingsWidget::onNavigationStyleChanged
    );
    return comboBox;
}

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:173-186
void GeneralSettingsWidget::onLanguageChanged(int index)
{
    if (index < 0) {
        return;  // happens when clearing the combo box in retranslateUi()
    }
    Gui::Translator::instance()->activateLanguage(
        m_languageComboBox->itemData(index).toByteArray().data()
    );
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/General"
    );
    auto langToStr = Gui::Translator::instance()->activeLanguage();
    hGrp->SetASCII("Language", langToStr.c_str());
}

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:188-198
void GeneralSettingsWidget::onUnitSystemChanged(int index)
{
    if (index < 0) {
        return;  // happens when clearing the combo box in retranslateUi()
    }
    // TODO: deferred — Base::UnitsApi::setSchema(index) when the stub supports it
    // (mirrors MainWindow_p.h:127). The DTA shell persists the choice; behavior
    // switch is deferred to the real UnitsApi integration.
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Units"
    );
    hGrp->SetInt("UserSchema", index);
}

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:200-210
void GeneralSettingsWidget::onNavigationStyleChanged(int index)
{
    if (index < 0) {
        return;  // happens when clearing the combo box in retranslateUi()
    }
    auto navStyleName = m_navigationStyleComboBox->itemData(index).toByteArray();
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/View"
    );
    hGrp->SetASCII("NavigationStyle", navStyleName.constData());
    // TODO: deferred — implement the actual navigation-style behavior switch via
    // itwinjs-core camera controller integration; see design 2026-07-16 §5.
    // The DTA shell persists the preference; runtime behavior is applied when
    // the 3D viewport wires the navigation style to the camera controller.
}

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:212-218
bool GeneralSettingsWidget::eventFilter(QObject* object, QEvent* event)
{
    if (object == this && event->type() == QEvent::LanguageChange) {
        this->retranslateUi();
    }
    return QWidget::eventFilter(object, event);
}

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:220-261
void GeneralSettingsWidget::retranslateUi()
{
    m_languageLabel->setText(createLabelText(tr("Language")));
    m_unitSystemLabel->setText(createLabelText(tr("Unit System")));

    m_unitSystemComboBox->clear();

    const ParameterGrp::handle hGrpUnits = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Units"
    );
    auto userSchema = hGrpUnits->GetInt("UserSchema", 0);

    auto addItem = [&, index {0}](const std::string& item) mutable {
        m_unitSystemComboBox->addItem(QString::fromStdString(item), index++);
    };
    auto descriptions = Base::UnitsApi::getDescriptions();
    std::for_each(descriptions.begin(), descriptions.end(), addItem);

    m_unitSystemComboBox->setCurrentIndex(userSchema);

    m_navigationStyleLabel->setText(createLabelText(tr("Navigation Style")));
    m_navigationStyleComboBox->clear();
    ParameterGrp::handle hGrpNav = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/View"
    );
    auto navStyleName = hGrpNav->GetASCII(
        "NavigationStyle",
        Gui::UserNavigationStyle::defaultStyleName()  // Stub: FreeCAD uses Gui::CADNavigationStyle::getClassTypeId().getName()
    );
    std::map<std::string, std::string> styles = Gui::UserNavigationStyle::getUserFriendlyNames();
    for (const auto& style : styles) {
#pragma warning(suppress : 4458) // 局部 data 为 Qt 惯用名，遮蔽同名成员仅 MSVC /W4 提示
        QByteArray data(style.first.c_str());
        QString name = QApplication::translate(
            style.first.c_str(),
            style.second.c_str()
        );
        m_navigationStyleComboBox->addItem(name, data);
        if (navStyleName == style.first) {
            m_navigationStyleComboBox->setCurrentIndex(m_navigationStyleComboBox->count() - 1);
        }
    }
}
