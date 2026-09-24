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

#pragma once

#include <QLabel>
#include <QString>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <App/Application.h>

namespace StartGui
{

struct NewButton
{
    QString heading;
    QString description;
    QString iconPath;
    // DanQing 扩展（2026-09-11）：图标字体字形模式。iconFontResource 非空时忽略
    // iconPath，从该 qrc 路径加载字体并以字形渲染图标（禁用态随 palette 重着色）。
    // 注意：字体内部 family 名可能与文件名不同（DTA 图标字体内部名为 "icomoon"
    // ——其 CSS 的 "Display-Test-App-Icons" 只是 @font-face 别名），故这里传资源
    // 路径，实际 family 由加载后解析。
    // Ported from: itwinjs-core display-test-app ToolBar.ts:18（字体渲染图标）+
    //              Surface.ts 各按钮的 iconUnicode。
    QString iconFontResource;
    QChar iconGlyph = QChar(0);
};

class NewFileButton: public QPushButton
{
public:
    explicit NewFileButton(const NewButton& newButton);

private:
    int iconSize;
    int labelWidth;
    QHBoxLayout* mainLayout;
    QVBoxLayout* textLayout;
    QLabel* headingLabel;
    QLabel* descriptionLabel;
    // 字形模式状态（iconFontResource 为空时未使用）
    QLabel* iconLabel;
    QString iconFontFamily;   // 加载后解析出的内部 family 名
    QChar iconGlyph;
    void updateIconPixmap();

protected:
    QSize minimumSizeHint() const override;
    void changeEvent(QEvent* event) override;  // EnabledChange 时按 palette 重绘字形
};

}  // namespace StartGui
