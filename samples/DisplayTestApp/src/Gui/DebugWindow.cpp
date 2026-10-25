// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Debug info window implementation
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/DebugWindow.ts
//              + frontend/Window.ts (floating-window shell)
//              + core/frontend-devtools/src/widgets/DiagnosticsPanel.ts
#include "DebugWindow.h"
#include "DevToolsWidgets.h"

#include <dqApp/Application.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewManager.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSizeGrip>
#include <QStyle>
#include <QVBoxLayout>

namespace Gui {

// 每列保底宽（HOST UI DEVIATION——参考单列 480px 的双列化，2026-10-25 用户指令）。
constexpr int kDebugPanelColumnWidth = 400;

// ---------------------------------------------------------------------------
// Construction — DebugWindow.ts:17-37 + DiagnosticsPanel.ts:53-103
// ---------------------------------------------------------------------------
DebugWindow::DebugWindow(dqApp::Viewport* vp, QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint)
    , m_vp(vp)
{
    // ← DebugWindow.ts:21 super(Surface.instance, { top: 0, left: 0 }) — the
    // window starts at the surface's top-left corner.
    setObjectName(QStringLiteral("debugPanel"));
    // 面板整体浅色不透明背景 + 全子控件透明/深色文字——参考 debugPanel 是浅色浮窗
    // （浏览器 DTA 面板为深色文字/浅色底）。仅给外层 #debugPanel 上色不够：
    // freecad.qss 只覆盖 QMainWindow/QDockWidget 等类型的背景，QScrollArea/通用
    // QWidget/QLabel 回落到 Fusion 默认暗色调色板（Win11 暗色模式），内容区整片
    // 黑底灰字不可读（2026-09-21 用户报告）。标题栏自身的 #floating-window-header
    // 规则优先级更高（id 选择器），不受子控件透明规则影响。
    setStyleSheet(QStringLiteral(
        "QWidget#debugPanel { background: #f5f5f5; border: 1px solid gray; }"
        "QWidget#debugPanel QWidget { background: transparent; }"
        "QWidget#debugPanel QLabel { color: #1a1a1a; background: transparent; font: 8pt 'Consolas'; }"));

    buildUi();

    // ← DebugWindow.ts:30-31: title `[ ${viewport.viewportId} ] Diagnostics`;
    // windowId `debugPanel-${viewport.viewportId}`.
    int const viewportId = m_vp ? m_vp->GetViewportId() : 0;
    m_titleLabel->setText(QStringLiteral("[ %1 ] Diagnostics").arg(viewportId));

    // ← DebugWindow.ts:32 isPinned = true（Window.markAsPinned — pinned visual
    // state; DanQing 的无边框工具窗随选中视口，无 dock/resize 语义可置）。

    // ← DebugWindow.ts:34-36: container display flex/none on selected-viewport
    // change — the window is visible only while its viewport is selected.
    m_selectedViewportDisconnect =
        dqApp::Application::Get().GetViewManager().OnSelectedViewportChanged.AddListener(
            [this](dqApp::SelectedViewportChangedArgs const& args) {
                if (m_isOpen)
                    setVisible(args.current == m_vp);
            });
}

DebugWindow::~DebugWindow()
{
    // ← DebugWindow.ts:39-43 [Symbol.dispose] — dispose panel + hide + remove
    // the listener.
    if (m_selectedViewportDisconnect)
        m_selectedViewportDisconnect();
}

// ← DebugWindow.ts:46 windowId.
QString DebugWindow::windowId() const
{
    int const viewportId = m_vp ? m_vp->GetViewportId() : 0;
    return QStringLiteral("debugPanel-%1").arg(viewportId);
}

// ---------------------------------------------------------------------------
// UI — the window shell (Window.ts) + the 7-widget panel (DiagnosticsPanel.ts)
// ---------------------------------------------------------------------------
void DebugWindow::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(2, 2, 2, 2);
    outer->setSpacing(0);

    // WindowHeader — Window.ts:126-168: title span + close div (+ resize
    // corner, hidden for non-resizable windows — DebugWindow.isResizable ==
    // false :45). Left-drag on the header moves the window (DragState).
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName(QStringLiteral("floating-window-header"));
    m_titleBar->setStyleSheet(QStringLiteral(
        "QWidget#floating-window-header { background: #e0e0e0; border: 1px solid gray; }"));
    m_titleBar->setFixedHeight(22);
    m_titleBar->setCursor(Qt::SizeAllCursor);
    auto* headerLayout = new QHBoxLayout(m_titleBar);
    headerLayout->setContentsMargins(6, 0, 4, 0);
    headerLayout->setSpacing(4);
    m_titleLabel = new QLabel(m_titleBar);
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch(1);
    // ← Window.ts:141-146 resize corner（isResizable==true 时右下/标题栏缩放柄）。
    // DebugWindow.ts:45 isResizable=false——2026-10-25 用户指令（"面板太小/滚动
    // 麻烦"）转可缩放：标题栏右侧 QSizeGrip 承载（frameless 窗的 resize corner
    // 等价物）。拖拽移动（DragState）在标题栏空白区，与柄互不干扰（柄自消费
    // 鼠标事件）。
    m_resizeGrip = new QSizeGrip(m_titleBar);
    m_resizeGrip->setFixedSize(16, 16);
    m_resizeGrip->setToolTip(QStringLiteral("Resize"));
    headerLayout->addWidget(m_resizeGrip);
    m_closeButton = new QPushButton(QStringLiteral("x"), m_titleBar);
    m_closeButton->setFixedSize(16, 16);
    m_closeButton->setToolTip(QStringLiteral("Close"));
    // ← :141-142 _closeElement.onclick = surface.close(window).
    connect(m_closeButton, &QPushButton::clicked, this, [this]() { hidePanel(); });
    headerLayout->addWidget(m_closeButton);
    outer->addWidget(m_titleBar);

    // Content — DiagnosticsPanel.ts:53-103: each widget in order, <hr>
    // separators between (the QFrame HLine rows below). DTA excludes keyin.
    // HOST UI DEVIATION（2026-10-25 用户指令"面板太小/信息太少/滚动麻烦"）：
    // 参考 .debugPanel { width: 480px } 是单列（浏览器浮层，滚动自然）；
    // DanQing 改双列（左=统计族 FPS/TileStats/TileMem，右=诊断族
    // RenderCmds/Memory/Gpu/ToolSettings）+ 最小宽 2×400 + 打开即高窗
    // （85% 宿主高，见 showPanel）+ 标题栏 QSizeGrip 缩放。7 组件集合与
    // 行为不变（逐个仍是移植的 frontend-devtools 组件；行为面锁在
    // DebugWindowDiagTest——findChild 按 objectName/text 寻址，布局无关）。
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    // 垂直滚动条常驻（宽度恒定）：跟踪开启时内容变高，v-scrollbar 若按需出现
    // 会瞬时吃掉 ~14px 视口宽度 → 内容溢出 → h-scrollbar 出现并被 QScrollArea
    // 自动横滚（"滚动条一下在动、窗口显示跳动"的完整链）。常驻后宽度从打开起
    // 就稳定，跟踪开启只长纵向、永不出横向滚动。
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    // ← .debugPanel { width: 480px } (public/index.css:341-355) 的双列化：
    // 每列保底 400（统计表两个 50% 列各 ~200px，"Decoding mean time (ms): 0"
    // 等长行完整落列），setMinimumWidth 替代原 setFixedWidth——用户可经
    // QSizeGrip 放大，不可缩到两列以下。宽度从打开起稳定，永不出横向滚动。
    int const sbExtent = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    scroll->setMinimumWidth(2 * kDebugPanelColumnWidth + 8 + sbExtent + 2 * scroll->frameWidth());
    auto* content = new QWidget(scroll);
    m_content = content;
    auto* columns = new QHBoxLayout(content);
    columns->setContentsMargins(3, 3, 3, 3);  // .debugPanel padding: 3px
    columns->setSpacing(4);

    auto* colStat = new QVBoxLayout;
    colStat->setSpacing(4);
    auto* colDiag = new QVBoxLayout;
    colDiag->setSpacing(4);
    auto addSeparator = [content](QVBoxLayout* col) {  // DiagnosticsPanel.ts:120-122
        auto* sep = new QFrame(content);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(QStringLiteral("color: #c0c0c0;"));
        col->addWidget(sep);
    };

    // 左列 = 统计族（:59-62 FPS → :76-79 TileStats → :81-84 TileMem）。
    m_fpsTracker = new FpsTracker(m_vp, content);
    colStat->addWidget(m_fpsTracker);
    addSeparator(colStat);

    // :64-74 KeyinField — EXCLUDED by DTA (DebugWindow.ts:20).

    m_statsTracker = new TileStatisticsTracker(m_vp, content);
    colStat->addWidget(m_statsTracker);
    addSeparator(colStat);

    m_tileMemoryBreakdown = new TileMemoryBreakdown(content);
    colStat->addWidget(m_tileMemoryBreakdown);
    colStat->addStretch(1);

    // 列间分隔（单列时代是行间 HLine；双列下为竖线）。
    auto* vline = new QFrame(content);
    vline->setFrameShape(QFrame::VLine);
    vline->setStyleSheet(QStringLiteral("color: #c0c0c0;"));
    columns->addLayout(colStat, 1);
    columns->addWidget(vline);

    // 右列 = 诊断族（:86-89 RenderCmds → :91-94 Memory → :96-99 Gpu →
    // :101-102 ToolSettings，末位无分隔——同参考）。
    m_renderCommands = new RenderCommandBreakdown(content);
    colDiag->addWidget(m_renderCommands);
    addSeparator(colDiag);

    m_memoryTracker = new MemoryTracker(m_vp, content);
    colDiag->addWidget(m_memoryTracker);
    addSeparator(colDiag);

    m_gpuProfiler = new GpuProfiler(content);
    colDiag->addWidget(m_gpuProfiler);
    addSeparator(colDiag);

    m_toolSettingsTracker = new ToolSettingsTracker(m_vp, content);
    colDiag->addWidget(m_toolSettingsTracker);
    colDiag->addStretch(1);
    columns->addLayout(colDiag, 1);

    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
}

// ---------------------------------------------------------------------------
// Drag — Window.ts:10-45 DragState (left-drag on the header moves the window,
// clamped so it stays inside the parent surface).
// ---------------------------------------------------------------------------
void DebugWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_titleBar && m_titleBar->underMouse()) {
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QWidget::mousePressEvent(event);
}

void DebugWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton && !m_dragOffset.isNull() && parentWidget()) {
        QPoint target = event->globalPosition().toPoint() - m_dragOffset;
        // DragState :18-31 — clamp to the surface (parent) bounds.
        QRect const surface = parentWidget()->rect();
        QPoint const origin = parentWidget()->mapToGlobal(QPoint(0, 0));
        int const maxX = origin.x() + surface.width() - width();
        int const maxY = origin.y() + surface.height() - height();
        target.setX(std::clamp(target.x(), origin.x(), std::max(origin.x(), maxX)));
        target.setY(std::clamp(target.y(), origin.y(), std::max(origin.y(), maxY)));
        move(target);
    }
    QWidget::mouseMoveEvent(event);
}

// ---------------------------------------------------------------------------
// Open/close — DebugWindow.ts:48-75
// ---------------------------------------------------------------------------
void DebugWindow::toggle()
{
    // ← DebugWindow.ts:48-53.
    if (m_isOpen)
        hidePanel();
    else
        showPanel();
}

void DebugWindow::showPanel()
{
    // ← DebugWindow.ts:55-64: surface.addWindow + append container + isOpen;
    // then resizeContent(panel.clientWidth + 2, panel.clientHeight) —
    // Window.ts:305-320 makes the contentDiv exactly the panel's box (+4/+4
    // border kludge). The width comes from the fixed 480px scroll area (the
    // .debugPanel CSS width); the height is the panel's NATURAL height at
    // open (trackers collapsed) — expansion afterwards scrolls inside the
    // scroll area, the window does not resize (reference: resizeContent is
    // called only in show()).
    bool const vpSelected =
        dqApp::Application::Get().GetViewManager().GetSelectedViewport() == m_vp;
    m_isOpen = true;
    m_content->ensurePolished();
    auto const margins = layout()->contentsMargins();
    int const w = 2 * kDebugPanelColumnWidth + 8
                + style()->pixelMetric(QStyle::PM_ScrollBarExtent)
                + margins.left() + margins.right();
    int h = m_titleBar->height() + m_content->sizeHint().height()
          + margins.top() + margins.bottom();
    if (parentWidget()) {
        // HOST UI DEVIATION（2026-10-25 用户指令）：参考打开高度=内容天然高
        // （跟踪折叠态，之后展开全靠滚动）；DanQing 打开即 85% 宿主高
        // （双列内容一眼可见，滚动只剩兜底），不低于 560（折叠态也有用）。
        int const cap = static_cast<int>(parentWidget()->height() * 0.85);
        h = std::min(h, cap);
        h = std::max(h, std::min(560, parentWidget()->height()));
    }
    resize(w, h);
    setVisible(vpSelected);
    raise();
}

void DebugWindow::hidePanel()
{
    // ← DebugWindow.ts:66-71 + onClosed (:73-75 isOpen = false).
    m_isOpen = false;
    hide();
}

}  // namespace Gui
