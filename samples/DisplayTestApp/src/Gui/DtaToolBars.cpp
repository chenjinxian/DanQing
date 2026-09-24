// Ported from: itwinjs-core display-test-app Viewer.ts:236-436（Views/Selection/View Settings/View Tools/Analysis 工具栏内容/顺序）
//              + Surface.ts:97-119（enablement 语义）,122-180（app 工具栏内容/顺序）
#include "DtaToolBars.h"

#include <QAction>
#include <QComboBox>
#include <QCursor>
#include <QFontDatabase>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QGridLayout>
#include <QSize>
#include <QToolBar>
#include <QToolButton>
#include <QWidgetAction>
#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/Viewport.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ViewTool.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewState.h>

#include "Application.h"   // Gui::Application::Instance()->newDocument()
#include "DebugWindow.h"              // Viewer.ts:238-242 Debug info panel
#include "DecorationGeometryExample.h"   // Surface.ts:155-165 entry
#include "View3DInventor.h"
#include "ViewSettingsPanel.h"   // Task 4: View Settings 弹出面板

namespace Gui {
namespace {

// --- DTA 图标 ------------------------------------------------------------
// 两类图标（Viewer.ts）：iconUnicode（Display-Test-App-Icons 字体字形）与
// createImageButton 的 SVG（.static-assets/）。SVG 经 qrc :/icons/dta/ 使用，
// 字形经下方 dtaGlyphIcon 渲染。
// 注意：该 TTF 内部 family 名是 "icomoon"——DTA CSS 的 "Display-Test-App-Icons"
// 只是 @font-face 别名，Qt 必须加载后解析真实 family（同 NewFileButton.cpp）。

// DTA 图标字体的内部 family 名（进程一次；加载失败返回空串）。
QString dtaIconFontFamily()
{
    static const QString family = [] {
        int const id = QFontDatabase::addApplicationFont(
            QStringLiteral(":/fonts/Display-Test-App-Icons.ttf"));
        return id >= 0 ? QFontDatabase::applicationFontFamilies(id).value(0)
                       : QString();
    }();
    return family;
}

// 字形 → QIcon。固定色渲染（不随系统调色板）：DTA 按钮是浅色瓷砖 #f0f0f0 +
// 默认黑文本（index.css .simpleicon 无 color 声明）→ DanQing 浅色 chrome 对应
// 固定近黑；构造时读 palette 在 Windows 深色应用模式下会烘出白图标（浅底不可读）。
// QPainter 位图不自动派生禁用态，显式渲染 Normal + Disabled 两态。
QIcon dtaGlyphIcon(ushort codepoint)
{
    constexpr int px = 32;   // 工具栏 iconSize 之上的高清渲染，QIcon 自行缩放
    QColor const normal(0x1a, 0x1a, 0x1a);
    QColor const disabled(0x9e, 0x9e, 0x9e);
    QIcon icon;
    for (QIcon::Mode mode : { QIcon::Normal, QIcon::Disabled }) {
        QPixmap pm(px, px);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QFont f(dtaIconFontFamily());
        f.setPixelSize(int(px * 0.8));
        p.setFont(f);
        p.setPen(mode == QIcon::Normal ? normal : disabled);
        p.drawText(QRect(0, 0, px, px), Qt::AlignCenter, QChar(codepoint));
        p.end();
        icon.addPixmap(pm, mode);
    }
    return icon;
}

void setGlyphIcon(QAction* a, ushort codepoint) { a->setIcon(dtaGlyphIcon(codepoint)); }
void setSvgIcon(QAction* a, const char* rcPath) { a->setIcon(QIcon(QLatin1String(rcPath))); }

// 置灰工具：未实现项统一 enabled=false + tooltip 注明；dtaGlyph≠0 时设 DTA 字形图标。
QAction* addDisabled(QToolBar* tb, const QString& text, ushort dtaGlyph = 0)
{
    QAction* a = tb->addAction(text);
    a->setEnabled(false);
    a->setToolTip(text + " (not yet implemented)");
    if (dtaGlyph)
        setGlyphIcon(a, dtaGlyph);
    return a;
}

QToolBar* makeBar(QMainWindow* mw, const QString& title)
{
    QToolBar* tb = mw->addToolBar(title);
    tb->setObjectName(QStringLiteral("DTA.") + title);
    tb->setWindowTitle(title);
    // DTA 工具栏形态：图标 + tooltip（无图标的动作——如 Views 的 ViewPicker——
    // Qt 在 IconOnly 下自动回退显示文本，对应 DTA 的 <select> 文本形态）。
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    // 图标尺寸 24px（DTA 按钮瓷砖 35px 含 3px padding，图标区约 24px）。
    tb->setIconSize(QSize(24, 24));
    // DTA 按钮瓷砖样式（index.css .simpleicon：#f0f0f0 底 + 1px 灰边框）。
    // 下拉按钮不设 menu（DTA 的 DropDown 无箭头，点击即开面板）——无重叠源。
    tb->setStyleSheet(QStringLiteral(
        "QToolButton { background: #f0f0f0; border: 1px solid gray;"
        " border-radius: 2px; margin: 1px; padding: 2px; }"
        "QToolButton:disabled { background: #f7f7f7; border-color: #c0c0c0; }"));
    return tb;
}

// 活动视口（可空）。← IModelApp.viewManager.selectedView
dqApp::Viewport* activeViewport()
{
    return dqApp::Application::Get().GetViewManager().GetActiveViewport();
}

// 空下拉：可点开、显示空列表（DTA blank 语义——Viewer.ts:274-313 的 picker 在
// blank 下 populate 为空）；dtaGlyph≠0 时设 DTA 字形图标。
// 不设 action 菜单（DTA 的 DropDown 无箭头，ToolBar.ts:99-121 点击即 toggle）——
// 点击经 triggered 手动 popup。
QAction* addEmptyDropDown(QToolBar* tb, const QString& text, ushort dtaGlyph = 0)
{
    QAction* a = tb->addAction(text);
    if (dtaGlyph)
        setGlyphIcon(a, dtaGlyph);
    QMenu* m = new QMenu(tb);
    m->setObjectName(QStringLiteral("DTA.DropDown.") + text);
    QAction* empty = m->addAction(QStringLiteral("(empty)"));
    empty->setEnabled(false);
    QObject::connect(a, &QAction::triggered, tb, [m] {
        m->popup(QCursor::pos());
    });
    return a;
}

// 视图工具安装（现有 main.cpp 模式）：adopt 接管堆所有权（TD-11——ToolAdmin
// 在替换/退出时删除），run() 失败（未安装）则 disown 后释放。
void runViewTool(dqApp::ViewTool* tool)
{
    if (!tool)
        return;
    auto& ta = dqApp::Application::Get().GetToolAdmin();
    ta.adoptViewTool(tool);
    if (!tool->run()) {
        ta.disownViewTool(tool);
        delete tool;
    }
}
}  // namespace

// ViewPickerComboBox — 声明见 DtaToolBars.h（DTA 的 <select> 对应控件）。
ViewPickerComboBox::ViewPickerComboBox(QWidget* parent) : QComboBox(parent)
{
    setObjectName(QStringLiteral("DTA.Views.ViewPicker"));
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
    setMinimumContentsLength(12);
    setToolTip(QStringLiteral("Views"));
}

void ViewPickerComboBox::repopulate()
{
    blockSignals(true);
    clear();
    auto* vp = activeViewport();
    if (vp && vp->GetIModel()) {
        auto list = dqApp::ViewList::create(vp->GetIModel());
        for (int i = 0; i < list.length(); ++i) {
            if (auto const* spec = list.get(i))
                addItem(QString::fromStdString(spec->name));
        }
    }
    blockSignals(false);
}

void ViewPickerComboBox::showPopup()
{
    repopulate();   // 每次弹出重建（原 aboutToShow 菜单语义；FreeCAD showPopup 重载同位）
    QComboBox::showPopup();
}

// StandardRotationsPanel — DTA StandardRotations 的弹出面板：div.toolMenu 内
// 两行四个纯字形按钮（StandardRotations.ts:29-47）。Qt 映射 = QMenu 内单个
// QWidgetAction 包装的网格面板（FreeCAD 工具栏小面板的惯用法——菜单承载控件
// 而非菜单项）；点击后关面板（参考 Viewer.ts:273 点击即 toolBar.close()）。
class StandardRotationsPanel : public QWidget {
public:
    explicit StandardRotationsPanel(QWidget* parent) : QWidget(parent)
    {
        struct Dir { const char* name; dqApp::StandardViewId id; ushort glyph; };
        static const Dir dirs[] = {
            { "Top", dqApp::StandardViewId::Top, 0xe916 },       { "Bottom", dqApp::StandardViewId::Bottom, 0xe910 },
            { "Left", dqApp::StandardViewId::Left, 0xe914 },     { "Right", dqApp::StandardViewId::Right, 0xe915 },
            { "Front", dqApp::StandardViewId::Front, 0xe911 },   { "Back", dqApp::StandardViewId::Back, 0xe90f },
            { "Iso", dqApp::StandardViewId::Iso, 0xe912 },       { "RightIso", dqApp::StandardViewId::RightIso, 0xe913 },
        };
        auto* grid = new QGridLayout(this);
        grid->setSpacing(2);
        grid->setContentsMargins(4, 4, 4, 4);
        for (int i = 0; i < 8; ++i) {
            auto* b = new QToolButton(this);
            b->setObjectName(QString::fromLatin1("DTA.StdRot.%1").arg(i));
            b->setIcon(dtaGlyphIcon(dirs[i].glyph));
            b->setToolTip(QString::fromLatin1(dirs[i].name));
            b->setAutoRaise(true);
            const auto id = dirs[i].id;
            QObject::connect(b, &QToolButton::clicked, this, [this, id] {
                if (auto* vp = activeViewport())
                    runViewTool(new dqApp::StandardViewTool(vp, id));
                // 点击后关闭弹出面板（Viewer.ts:273 this.toolBar.close()）。
                for (QWidget* w = this; w != nullptr; w = w->parentWidget())
                    if (auto* m = qobject_cast<QMenu*>(w)) {
                        m->close();
                        break;
                    }
            });
            grid->addWidget(b, i / 4, i % 4);   // 2 行 × 4 列
        }
    }
};

DtaToolBarSet::DtaToolBarSet(QMainWindow* mw)
    : QObject(mw)
{
    buildViewsToolBar(mw);
    buildSelectionToolBar(mw);
    buildViewSettingsToolBar(mw);
    buildViewToolsToolBar(mw);
    buildAnalysisToolBar(mw);

    // DTA 语义（Surface.ts:97-119）：无视口时视口工具不可用。桌面形态：工具栏
    // 常显、整体置灰。初始按当前活动视口；选中变化事件驱动切换。
    // （2026-09-11：Surface.ts:122-180 的 app 级入口移至 Start 页卡片，File
    // 工具栏随之移除——DTA 中这些入口只在打开前出现。）
    setViewToolbarsEnabled(dqApp::Application::Get().GetViewManager().GetActiveViewport() != nullptr);
    m_scope.add(dqApp::Application::Get().GetViewManager().OnSelectedViewportChanged.AddListener(
        [this](dqApp::SelectedViewportChangedArgs const& args) {
            setViewToolbarsEnabled(args.current != nullptr);
        }));
}

// Ported from: itwinjs-core Viewer.ts:236-313 — Debug info / Open iModel from disk /
// Open from hub / ViewPicker / Models / Categories / Saved views / Camera paths。
// 图标：Viewer.ts:239 / 245 / 253 / 275 / 286 / 296 / 306 的 iconUnicode 字形；
//       ViewPicker（ViewPicker.ts:176-190）是 <select> 文本控件，无图标——
//       IconOnly 下 Qt 回退显示其文本。
void DtaToolBarSet::buildViewsToolBar(QMainWindow* mw)
{
    m_viewsToolBar = makeBar(mw, QStringLiteral("Views"));

    // Viewer.ts:238-242 — "Debug info": first click lazily creates the
    // DebugWindow for the active viewport, then toggles it (toggleDebugWindow
    // :630-635). The window floats over the view (debugPanel).
    QAction* debugInfo = m_viewsToolBar->addAction(QStringLiteral("Debug"));
    debugInfo->setToolTip(QStringLiteral("Debug info"));
    setGlyphIcon(debugInfo, 0xe90c);   // Viewer.ts:239 iconUnicode
    {
        // Per-view window cache (Viewer.ts:180 _debugWindow member equivalent),
        // keyed by the toolbar's lifetime.
        static QPointer<Gui::DebugWindow> s_debugWindow;
        QObject::connect(debugInfo, &QAction::triggered, m_viewsToolBar, [mw] {
            if (s_debugWindow) {
                s_debugWindow->toggle();
                return;
            }
            auto* view3d = qobject_cast<Gui::View3DInventor*>(Gui::Application::Instance()->activeView());
            if (!view3d || !view3d->getUeViewport())
                return;
            s_debugWindow = new Gui::DebugWindow(view3d->getUeViewport(), view3d);
            s_debugWindow->setGeometry(8, 8, 330, 200);
            s_debugWindow->showPanel();
        });
    }
    addDisabled(m_viewsToolBar, QStringLiteral("Open iModel"), 0xe9cc);
    addDisabled(m_viewsToolBar, QStringLiteral("Open Hub"), 0xe9e0);

    // ViewPicker（Viewer.ts:270-272 + ViewPicker.ts:172-203）：<select> 控件——
    // QComboBox（FreeCAD WorkbenchSelector 模式）。选中 → getView(id)→clone→
    // ChangeView（Viewer.ts:492-497 changeView）。
    auto* picker = new ViewPickerComboBox(m_viewsToolBar);
    QObject::connect(picker, qOverload<int>(&QComboBox::activated),
                     m_viewsToolBar, [picker](int index) {
        auto* vp = activeViewport();
        if (!vp || !vp->GetIModel() || index < 0)
            return;
        auto list = dqApp::ViewList::create(vp->GetIModel());
        if (auto const* spec = list.get(index)) {
            dqBase::DqId const id = spec->id;
            auto view = dqApp::ViewList::create(vp->GetIModel()).getView(id, vp->GetIModel());
            if (view)
                vp->ChangeView(view);
        }
    });
    QAction* pickerAction = m_viewsToolBar->addWidget(picker);
    pickerAction->setObjectName(QStringLiteral("DTA.Views.ViewPicker"));

    addEmptyDropDown(m_viewsToolBar, QStringLiteral("Models"), 0xe90b);
    addEmptyDropDown(m_viewsToolBar, QStringLiteral("Categories"), 0xe901);
    addEmptyDropDown(m_viewsToolBar, QStringLiteral("Saved Views"), 0xe90d);
    addEmptyDropDown(m_viewsToolBar, QStringLiteral("Camera Paths"), 0xe932);
}

// Ported from: itwinjs-core Viewer.ts:315-325 — Element selection / Measure distance。
void DtaToolBarSet::buildSelectionToolBar(QMainWindow* mw)
{
    m_selectionToolBar = makeBar(mw, QStringLiteral("Selection"));

    // Viewer.ts:317 — IModelApp.tools.run("SVTSelect")（DTA 的默认工具=SVTSelect，
    // SelectionTool 子类）。DanQing：注册表创建 "Select" 并置为活动工具。
    QAction* sel = m_selectionToolBar->addAction(QStringLiteral("Select"));
    sel->setToolTip(QStringLiteral("Element selection"));
    setSvgIcon(sel, ":/icons/dta/zoom.svg");   // Viewer.ts:317 zoom.svg
    QObject::connect(sel, &QAction::triggered, m_selectionToolBar, [] {
        auto& ta = dqApp::Application::Get().GetToolAdmin();
        if (auto* tool = ta.GetRegistry().Create("Select"))
            ta.SetActiveTool(tool);
    });

    addDisabled(m_selectionToolBar, QStringLiteral("Measure"), 0xeb08);
}

// Ported from: itwinjs-core Viewer.ts:327-335 — View settings 下拉（ViewAttributesPanel）。
void DtaToolBarSet::buildViewSettingsToolBar(QMainWindow* mw)
{
    m_viewSettingsToolBar = makeBar(mw, QStringLiteral("View Settings"));
    auto* btn = new QToolButton(m_viewSettingsToolBar);
    btn->setText(QStringLiteral("View Settings"));
    btn->setIcon(dtaGlyphIcon(0xe90e));   // Viewer.ts:328 字形
    btn->setPopupMode(QToolButton::InstantPopup);
    auto* panel = new ViewSettingsPanel(btn);
    QObject::connect(btn, &QToolButton::clicked, btn, [btn, panel] {
        panel->syncFromViewport();   // 弹出前回读当前视口状态
        panel->show();
        // 定位到按钮下方
        panel->move(btn->mapToGlobal(QPoint(0, btn->height())));
    });
    // addWidget 包装 action 也携带图标/文本（工具栏 actions() 断言 + 菜单化时的呈现）。
    QAction* wa = m_viewSettingsToolBar->addWidget(btn);
    wa->setText(QStringLiteral("View Settings"));
    wa->setIcon(dtaGlyphIcon(0xe90e));
}

// Ported from: itwinjs-core Viewer.ts:337-368 — Fit / Window area / Rotate /
// Standard rotations / Walk。
void DtaToolBarSet::buildViewToolsToolBar(QMainWindow* mw)
{
    m_viewToolsToolBar = makeBar(mw, QStringLiteral("View Tools"));

    // "Decoration Geometry Example"（Surface.ts:155-165——应用顶层工具栏按钮，
    // 点击新建 blank connection 装示例）：独立工具栏——不随 Views/View Tools
    // 的视口可用性置灰（setViewToolbarsEnabled 不碰它）。
    {
        m_decoToolBar = makeBar(mw, QStringLiteral("Deco Example"));
        QAction* decoGeo = m_decoToolBar->addAction(QStringLiteral("Deco"));
        decoGeo->setToolTip(QStringLiteral("Decoration Geometry Example"));
        setSvgIcon(decoGeo, ":/icons/dta/zoom.svg");
        QObject::connect(decoGeo, &QAction::triggered, m_decoToolBar, [] {
            auto* view3d = qobject_cast<Gui::View3DInventor*>(Gui::Application::Instance()->activeView());
            if (!view3d)
                view3d = qobject_cast<Gui::View3DInventor*>(Gui::Application::Instance()->newDocument());
            if (view3d)
                Gui::openDecorationGeometryExample(*view3d);
        });
    }

    QAction* fit = m_viewToolsToolBar->addAction(QStringLiteral("Fit"));
    fit->setToolTip(QStringLiteral("Fit view"));
    setSvgIcon(fit, ":/icons/dta/fit-to-view.svg");   // Viewer.ts:340
    QObject::connect(fit, &QAction::triggered, m_viewToolsToolBar, [] {
        if (auto* vp = activeViewport())
            runViewTool(new dqApp::FitViewTool(vp, /*oneShot=*/true));
    });

    // Viewer.ts:343-347 — Window area → IModelApp.tools.run("View.WindowArea", viewport)。
    // DanQing 等价：直接构造 WindowAreaTool 并 run()（runViewTool 既有模式，见 Fit）。
    QAction* windowArea = m_viewToolsToolBar->addAction(QStringLiteral("Window Area"));
    windowArea->setToolTip(QStringLiteral("Window area"));   // Viewer.ts:346
    setSvgIcon(windowArea, ":/icons/dta/window-area.svg");   // Viewer.ts:345
    QObject::connect(windowArea, &QAction::triggered, m_viewToolsToolBar, [] {
        if (auto* vp = activeViewport())
            runViewTool(new dqApp::WindowAreaTool(vp));
    });

    QAction* rotate = m_viewToolsToolBar->addAction(QStringLiteral("Rotate"));
    rotate->setToolTip(QStringLiteral("Rotate"));
    setSvgIcon(rotate, ":/icons/dta/rotate-left.svg");   // Viewer.ts:351
    QObject::connect(rotate, &QAction::triggered, m_viewToolsToolBar, [] {
        if (auto* vp = activeViewport())
            runViewTool(new dqApp::RotateViewTool(vp, /*oneShot=*/false));
    });

    // StandardRotations.ts:20-51 — 陀螺仪按钮点击弹出 2×4 字形按钮面板。
    // Qt 映射：QToolButton（无菜单——DTA DropDown 无箭头，ToolBar.ts:107-109
    // 点击即 toggle）+ QMenu 内 QWidgetAction 包装 StandardRotationsPanel
    //（FreeCAD 工具栏小面板惯用法），点击手动 popup 到按钮下方。
    auto* svBtn = new QToolButton(m_viewToolsToolBar);
    svBtn->setIcon(dtaGlyphIcon(0xe909));   // Viewer.ts:356 "gyroscope"
    svBtn->setToolTip(QStringLiteral("Standard rotations"));
    auto* svMenu = new QMenu(svBtn);
    svMenu->setObjectName(QStringLiteral("DTA.StdRot.Menu"));
    auto* svPanelAction = new QWidgetAction(svMenu);
    svPanelAction->setDefaultWidget(new StandardRotationsPanel(svMenu));
    svMenu->addAction(svPanelAction);
    QObject::connect(svBtn, &QToolButton::clicked, svBtn, [svBtn, svMenu] {
        svMenu->popup(svBtn->mapToGlobal(QPoint(0, svBtn->height())));
    });
    QAction* svAction = m_viewToolsToolBar->addWidget(svBtn);
    svAction->setObjectName(QStringLiteral("DTA.ViewTools.StandardRotations"));

    {
        QAction* walk = addDisabled(m_viewToolsToolBar, QStringLiteral("Walk"));
        setSvgIcon(walk, ":/icons/dta/walk.svg");   // Viewer.ts:364
    }
}

// Ported from: itwinjs-core Viewer.ts:370-436 — Undo/Redo + 分析置灰组。
void DtaToolBarSet::buildAnalysisToolBar(QMainWindow* mw)
{
    m_analysisToolBar = makeBar(mw, QStringLiteral("Analysis"));

    // Viewer.ts:370-380 — View undo / redo（View.Undo/View.Redo 工具，one-shot；
    // ViewTool.ts:4111-4134）。
    QAction* undo = m_analysisToolBar->addAction(QStringLiteral("Undo"));
    undo->setToolTip(QStringLiteral("View undo"));
    setGlyphIcon(undo, 0xe982);   // Viewer.ts:372 "undo"
    QObject::connect(undo, &QAction::triggered, m_analysisToolBar, [] {
        if (auto* vp = activeViewport())
            runViewTool(new dqApp::ViewUndoTool(vp));
    });
    QAction* redo = m_analysisToolBar->addAction(QStringLiteral("Redo"));
    redo->setToolTip(QStringLiteral("View redo"));
    setGlyphIcon(redo, 0xe983);   // Viewer.ts:378 "redo"
    QObject::connect(redo, &QAction::triggered, m_analysisToolBar, [] {
        if (auto* vp = activeViewport())
            runViewTool(new dqApp::ViewRedoTool(vp));
    });

    m_analysisToolBar->addSeparator();
    addDisabled(m_analysisToolBar, QStringLiteral("Animation"), 0xe931);
    addDisabled(m_analysisToolBar, QStringLiteral("Sectioning"), 0xe916);
    addDisabled(m_analysisToolBar, QStringLiteral("Classification"), 0xe9d8);
    addDisabled(m_analysisToolBar, QStringLiteral("Overrides"), 0xe90a);
    addDisabled(m_analysisToolBar, QStringLiteral("Point Cloud"), 0xe923);
    addDisabled(m_analysisToolBar, QStringLiteral("Contours"), 0xe94b);
    addDisabled(m_analysisToolBar, QStringLiteral("Format Set"), 0xe9cc);
}

void DtaToolBarSet::setViewToolbarsEnabled(bool enabled)
{
    // 全部 5 条工具栏随视口可用性整体置灰（File 已移至 Start 页）。
    for (QToolBar* tb : { m_viewsToolBar, m_selectionToolBar, m_viewSettingsToolBar,
                          m_viewToolsToolBar, m_analysisToolBar })
        if (tb) tb->setEnabled(enabled);

}

}  // namespace Gui
