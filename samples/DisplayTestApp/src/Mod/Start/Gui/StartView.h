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

#pragma once

// Ported from: FreeCAD src/Mod/Start/Gui/StartView.h
// M-H(4)（2026-09-28 用户指令）：FreeCAD-only 功能全清——New/Open File 卡片、
// recent-files 卡片面（FileCardView/FileCardDelegate/RecentFilesModel/
// DisplayedFilesModel）、FirstStart 向导页（FirstStartWidget/ThemeSelector/
// GeneralSettings）、ShowOnStartup/footer、postStart 死代码一并移除；保留面 =
// DTA 对齐入口（Blank Connection / Decoration Geometry Example）+ 新增两模型
// 打开入口（requestOpenDumpModel——DumpOpenHelper 打开链，M-H Task 3）。
#include <Base/Type.h>
#include "MDIView.h"

class QEvent;
class QLabel;

namespace StartGui
{

class StartGuiExport StartView: public Gui::MDIView
{
    Q_OBJECT

    TYPESYSTEM_HEADER_WITH_OVERRIDE();  // NOLINT

public:
    StartView(QWidget* parent);

    const char* getName() const override
    {
        return "StartView";
    }

    bool onHasMsg(const char* pMsg) const override;

    // DanQing integration signals (replace FreeCAD command calls)
Q_SIGNALS:
    void requestBlankConnection();
    void requestDecorationGeometryExample();      // Surface.ts:155-165 entry
    void requestCesiumExample();                  // Surface.ts:167-177 entry（b87a96a960 删 appToolBar 时
                                                  // 误失的 UI 入口恢复——M-O(4) P8 陈列馆当时仅剩 keyin）
    void requestOpenDumpModel(QString modelId);   // M-H(4)："joeshouse"/"instances60"；M-K(2)：+"housemodel"/"baytown"/"bridge-edit"

protected:
    void changeEvent(QEvent* e) override;

    void configureModelButtons(QLayout* layout);
    void configureExampleButtons(QLayout* layout);

private:
    void retranslateUi();

    QLabel* _modelsLabel = nullptr;
    QLabel* _examplesLabel = nullptr;

    bool isInitialized = false;

};  // namespace StartGui

}  // namespace StartGui
