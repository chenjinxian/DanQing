// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Sectioning 弹出面板实现
// Ported from: itwinjs-core test-apps/display-test-app SectionTools.ts
//              SectionsPanel ctor (:28-103)
#include "SectionsPanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <dqApp/Application.h>
#include <dqApp/ClipViewTool.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/Viewport.h>

namespace Gui {
namespace {

// tools.run(toolName, provider) 的 DanQing 承载（SectionTools.ts:56 的
// IModelApp.tools.run）：按 toolName 直构（provider ctor 形参——参考
// tools.run args→ctor 语义；registry 工厂无参）。返回 nullptr = 未知名。
dqApp::InteractiveTool* createSectionTool(QString const& toolName,
                                          dqApp::ViewClipEventHandler* handler)
{
    if (toolName == QStringLiteral("ViewClip.ByPlane"))
        return new dqApp::ViewClipByPlaneTool(handler);
    if (toolName == QStringLiteral("ViewClip.ByRange"))
        return new dqApp::ViewClipByRangeTool(handler);
    if (toolName == QStringLiteral("ViewClip.ByElement"))
        return new dqApp::ViewClipByElementTool(handler);
    if (toolName == QStringLiteral("ViewClip.ByShape"))
        return new dqApp::ViewClipByShapeTool(handler);
    if (toolName == QStringLiteral("ViewClip.Clear"))
        return new dqApp::ViewClipClearTool(handler);
    return nullptr;
}

}  // namespace

SectionsPanel::SectionsPanel(dqApp::Viewport* vp, QWidget* parent)
    : QWidget(parent)
    , m_vp(vp)
{
    setWindowTitle(QStringLiteral("Sectioning"));
    auto* root = new QVBoxLayout(this);

    // Clip type 下拉（:33-47——Geometry 项未移植见文件头）。
    {
        auto* line = new QHBoxLayout();
        line->addWidget(new QLabel(QStringLiteral("Clip type: "), this));
        m_typeCombo = new QComboBox(this);
        m_typeCombo->addItem(QStringLiteral("Plane"), QStringLiteral("ViewClip.ByPlane"));
        m_typeCombo->addItem(QStringLiteral("Range"), QStringLiteral("ViewClip.ByRange"));
        m_typeCombo->addItem(QStringLiteral("Element"), QStringLiteral("ViewClip.ByElement"));
        m_typeCombo->addItem(QStringLiteral("Shape"), QStringLiteral("ViewClip.ByShape"));
        m_typeCombo->setCurrentIndex(0);
        connect(m_typeCombo, &QComboBox::currentIndexChanged, this,
                [this](int index) { m_toolName = m_typeCombo->itemData(index).toString(); });
        line->addWidget(m_typeCombo);
        root->addLayout(line);
    }

    // Define / Edit / Clear 三钮（:49-68）。
    {
        auto* div = new QHBoxLayout();
        div->setAlignment(Qt::AlignCenter);

        auto* define = new QPushButton(QStringLiteral("Define"), this);
        define->setToolTip(QStringLiteral("Define clip"));
        connect(define, &QPushButton::clicked, this, [this] {
            auto& ta = dqApp::Application::Get().GetToolAdmin();
            if (auto* tool = createSectionTool(m_toolName, &dqApp::ViewClipDecorationProvider::create())) {
                static_cast<dqApp::PrimitiveTool*>(tool)->targetView = m_vp;  // 参考 ToolAdmin 安装路径赋值
                ta.SetActiveTool(tool);
            }
        });
        div->addWidget(define);

        auto* edit = new QPushButton(QStringLiteral("Edit"), this);
        edit->setToolTip(QStringLiteral("Show clip edit handles"));
        connect(edit, &QPushButton::clicked, this, [this] {
            dqApp::ViewClipDecorationProvider::create().toggleDecoration(*m_vp);
        });
        div->addWidget(edit);

        auto* clear = new QPushButton(QStringLiteral("Clear"), this);
        clear->setToolTip(QStringLiteral("Clear clips"));
        connect(clear, &QPushButton::clicked, this, [this] {
            auto& ta = dqApp::Application::Get().GetToolAdmin();
            if (auto* tool = createSectionTool(QStringLiteral("ViewClip.Clear"),
                                               &dqApp::ViewClipDecorationProvider::create())) {
                static_cast<dqApp::PrimitiveTool*>(tool)->targetView = m_vp;  // 安装即清（:462-466）
                ta.SetActiveTool(tool);
            }
        });
        div->addWidget(clear);

        root->addLayout(div);
    }
}

}  // namespace Gui
