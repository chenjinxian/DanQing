// SPDX-License-Identifier: LGPL-2.1-or-later
/****************************************************************************
 *                                                                          *
 *   Copyright (c) 2025 Alfredo Monclus <alfredomonclus@gmail.com>          *
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

#include <QLabel>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QHash>
#include <QIcon>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QString>

#include "NewFileButton.h"

namespace StartGui
{

namespace {
// 从 qrc 加载图标字体并返回其内部 family 名（进程每字体一次；失败返回空串）。
// 不能用文件名当 family——DTA 图标字体的内部名是 "icomoon"（CSS 里的
// "Display-Test-App-Icons" 只是 @font-face 别名，Qt 按内部名匹配）。
// Ported from: itwinjs-core test-apps/display-test-app/public/Fonts/（MIT）。
QString loadIconFontFamily(const QString& resourcePath)
{
    static QHash<QString, QString> cache;
    auto it = cache.constFind(resourcePath);
    if (it != cache.constEnd())
        return it.value();
    QString family;
    int const id = QFontDatabase::addApplicationFont(resourcePath);
    if (id >= 0) {
        auto const families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty())
            family = families.first();
    }
    cache.insert(resourcePath, family);
    return family;
}
}  // namespace

NewFileButton::NewFileButton(const NewButton& newButton)
    : mainLayout(new QHBoxLayout(this))
    , textLayout(new QVBoxLayout())
    , headingLabel(new QLabel())
    , descriptionLabel(new QLabel())
    , iconLabel(new QLabel(this))
    , iconFontFamily(newButton.iconFontResource.isEmpty()
                         ? QString()
                         : loadIconFontFamily(newButton.iconFontResource))
    , iconGlyph(newButton.iconGlyph)
{
    setObjectName(QStringLiteral("newFileButton"));
    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/Mod/Start"
    );

    constexpr int defaultWidth = 180;  // #newFileButton width in QSS
    labelWidth = int(hGrp->GetInt("FileCardLabelWith", defaultWidth));

    constexpr int defaultSize = 48;
    iconSize = int(hGrp->GetInt("NewFileIconSize", defaultSize));

    if (!iconFontFamily.isEmpty()) {
        updateIconPixmap();
    } else {
        QIcon baseIcon(newButton.iconPath);
        iconLabel->setPixmap(baseIcon.pixmap(iconSize, iconSize));
    }

    textLayout->addWidget(headingLabel);
    textLayout->addWidget(descriptionLabel);
    textLayout->setSpacing(0);
    textLayout->setContentsMargins(0, 0, 0, 0);

    headingLabel->setText(newButton.heading);
    QFont font = headingLabel->font();
    font.setWeight(QFont::Bold);
    headingLabel->setFont(font);

    descriptionLabel->setText(newButton.description);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setFixedWidth(labelWidth);
    descriptionLabel->setAlignment(Qt::AlignTop);

    mainLayout->setAlignment(Qt::AlignVCenter);
    mainLayout->addWidget(iconLabel);
    mainLayout->addLayout(textLayout);
    mainLayout->addStretch();
    QFontMetrics qfm(font);
    int margin = qfm.height() / 2;
    mainLayout->setSpacing(margin);
    mainLayout->setContentsMargins(margin, margin, 2 * margin, margin);
    setLayout(mainLayout);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

// 字形模式：把 DTA 图标字体的 codepoint 绘制为图标位图。固定色（不随系统
// 调色板——Windows 深色应用模式下 palette WindowText 为白，浅色卡片上不可读；
// DTA 侧即默认黑文本，index.css .simpleicon 无 color 声明）。
void NewFileButton::updateIconPixmap()
{
    QColor const normal(0x1a, 0x1a, 0x1a);
    QColor const disabled(0x9e, 0x9e, 0x9e);
    QPixmap pm(iconSize, iconSize);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f(iconFontFamily);
    f.setPixelSize(int(iconSize * 0.75));  // 字形四周留白，视觉尺寸对齐 SVG 图标
    p.setFont(f);
    p.setPen(isEnabled() ? normal : disabled);
    p.drawText(QRect(0, 0, iconSize, iconSize), Qt::AlignCenter, QString(iconGlyph));
    iconLabel->setPixmap(pm);
}

void NewFileButton::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::EnabledChange && !iconFontFamily.isEmpty())
        updateIconPixmap();
    QPushButton::changeEvent(event);
}

QSize NewFileButton::minimumSizeHint() const
{
    int minWidth = labelWidth + iconSize + mainLayout->contentsMargins().left()
        + mainLayout->contentsMargins().right() + mainLayout->spacing();

    int textHeight = headingLabel->sizeHint().height() + descriptionLabel->sizeHint().height();

    int minHeight = std::max(iconSize, textHeight) + mainLayout->contentsMargins().top()
        + mainLayout->contentsMargins().bottom();

    return {minWidth, minHeight};
}

}  // namespace StartGui
