// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Sectioning 弹出面板
// Ported from: itwinjs-core test-apps/display-test-app SectionTools.ts
//              SectionsPanel (:25-103)
//
// M-P P-G。EQUIVALENCE / 裁决：
//  - Geometry 项（ViewClipByElementGeometryTool.toolId）未移植——该工具为
//    display-test-app 专有扩展（非 core/frontend 面，SectionTools.ts:16 引）；
//    下拉不列该项。
//  - "Add Panel"/"Negate Plane" 两钮（:70-90，TwoPanelDivider + ModelClipTool
//    .applyModelClipping）依赖 ModelClipGroups（core-common 的 per-model clip
//    组）——DanQing 未移植该面；两钮不落（➖ 登记）。
//  - 工具运行：参考 IModelApp.tools.run(toolName, provider)（provider 经
//    tools.run 的 args→ctor 传入）；DanQing registry 工厂无参——面板按
//    toolName 直构（provider ctor 形参）+ ToolAdmin::SetActiveTool（Select
//    动作 DtaToolBars.cpp:370-380 先例）。
#pragma once

#include <QWidget>

class QComboBox;

namespace dqApp {
class Viewport;
}

namespace Gui {

class SectionsPanel final : public QWidget {
    Q_OBJECT
public:
    SectionsPanel(dqApp::Viewport* vp, QWidget* parent);

private:
    dqApp::Viewport* m_vp;
    QComboBox* m_typeCombo = nullptr;
    QString m_toolName = QStringLiteral("ViewClip.ByPlane");  // :27 _toolName
};

}  // namespace Gui
