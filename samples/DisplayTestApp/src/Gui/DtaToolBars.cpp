// Ported from: itwinjs-core display-test-app Viewer.ts:238-446（主工具栏 26 项单行——M-R）
//              + Surface.ts:103-119（焦点换位）/122-178（app 工具栏 5 项）
//              + ToolBar.ts:122-197（下拉交互合同：单开互斥/onViewChanged/only3d）
#include "DtaToolBars.h"

#include <QAction>
#include <QComboBox>
#include <QCursor>
#include <QDockWidget>
#include <QFileDialog>
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
#include "FeatureOverridesPanel.h"   // M-O(2) I10 Overrides 弹出面板
#include "SectionsPanel.h"           // M-P P-G Sectioning 弹出面板
#include "SavedViewsPanel.h"         // M-O(2) I9 Saved Views 弹出面板
#include "MainWindow.h"               // M-O(1) R2 addPanelToggle（dock 面板开关）
#include "View3DInventor.h"
#include "ViewSettingsPanel.h"   // Task 4: View Settings 弹出面板
#include "../DumpOpenHelper.h"   // M-R：Open iModel from disk（dump 打开链）

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

// M-O(1) R2：面板开关按钮（Models/Categories 工具栏位）——点击 toggle 对应
// dock 面板（addDockWindow(name) 的 QDockWidget 标题 = name）。
QAction* addPanelToggle(QToolBar* tb, const QString& text, ushort dtaGlyph,
                        const QString& dockName)
{
    QAction* a = tb->addAction(text);
    if (dtaGlyph)
        setGlyphIcon(a, dtaGlyph);
    a->setObjectName(QStringLiteral("DTA.PanelToggle.") + text);
    a->setCheckable(true);
    QObject::connect(a, &QAction::triggered, tb, [a, dockName](bool on) {
        auto* mw = MainWindow::getInstance();
        if (!mw)
            return;
        // 按标题找 dock（addDockWindow 以 name 为标题创建）。
        for (auto* dock : mw->findChildren<QDockWidget*>()) {
            if (dock->windowTitle() == dockName) {
                dock->setVisible(on);
                return;
            }
        }
        (void)a;
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

// ---------------------------------------------------------------------------
// DtaToolBarSet（M-R：app 工具栏 + 主工具栏 26 项 + 换位 + 交互合同）
// ---------------------------------------------------------------------------

DtaToolBarSet::DtaToolBarSet(QMainWindow* mw)
    : QObject(mw)
{
    buildAppToolBar(mw);
    buildMainToolBar(mw);

    // 焦点换位（Surface.ts:103-119：viewer 聚焦 → topdiv 换 viewer 工具栏；
    // 无聚焦 → app 工具栏）+ 交互合同（ToolBar.onViewChanged :186-196：
    // 关全部下拉 + only3d 显隐）。
    swapToolBarsForViewport(dqApp::Application::Get().GetViewManager().GetActiveViewport());
    m_scope.add(dqApp::Application::Get().GetViewManager().OnSelectedViewportChanged.AddListener(
        [this](dqApp::SelectedViewportChangedArgs const& args) {
            swapToolBarsForViewport(args.current);
        }));
}

// ToolBar.close（ToolBar.ts:163-172）——关闭全部打开的下拉面板。
void DtaToolBarSet::closeOpenDropDowns()
{
    for (auto& panel : m_openPanels) {
        if (panel)
            panel->close();
    }
    m_openPanels.clear();
}

// ToolBar.open（:168-180）——单开互斥：先 close 全部再开。
void DtaToolBarSet::openDropDown(QPointer<QWidget> panel)
{
    closeOpenDropDowns();
    if (!panel)
        return;
    m_openPanels.push_back(panel);
    // 面板销毁自动摘除（WA_DeleteOnClose 面板）。
    QPointer<QWidget> const stable = panel;
    QObject::connect(panel, &QObject::destroyed, this, [this, stable]() {
        for (int i = 0; i < m_openPanels.size(); ++i) {
            if (m_openPanels[i] == stable || m_openPanels[i].isNull()) {
                m_openPanels.removeAt(i);
                return;
            }
        }
    });
    panel->show();
}

void DtaToolBarSet::registerDropDownPanel(QWidget* panel)
{
    // 登记面（无操作——openDropDown 经按钮 handler 调用）。
    (void)panel;
}

// onViewChanged（ToolBar.ts:186-196）——only3d 项显隐（is3d ? block : none）。
void DtaToolBarSet::updateOnly3dVisibility()
{
    auto* vp = dqApp::Application::Get().GetViewManager().GetActiveViewport();
    bool const is3d = vp && vp->GetView() && vp->GetView()->AsViewState3d() != nullptr;
    for (QAction* a : m_only3dActions)
        a->setVisible(is3d);
}

// 焦点换位 + onViewChanged 合同（Surface.ts:103-119 + ToolBar.ts:186-196）。
void DtaToolBarSet::swapToolBarsForViewport(void* activeViewport)
{
    bool const hasViewport = activeViewport != nullptr;
    // DTA appendChild 换位 = Qt 显隐换位（同一 topdiv 槽位）。
    if (m_appToolBar)
        m_appToolBar->setVisible(!hasViewport);
    if (m_mainToolBar)
        m_mainToolBar->setVisible(hasViewport);
    // onViewChanged：关全部打开下拉 + only3d 显隐。
    closeOpenDropDowns();
    updateOnly3dVisibility();
}

// ---------------------------------------------------------------------------
// app 工具栏（Surface.createToolBar :122-178——无聚焦视口时显示）
// ---------------------------------------------------------------------------

void DtaToolBarSet::buildAppToolBar(QMainWindow* mw)
{
    m_appToolBar = makeBar(mw, QStringLiteral("DTA App"));
    m_appToolBar->setObjectName(QStringLiteral("DTA.App"));

    // Surface.ts:126-131 — Open iModel from disk：QFileDialog 选 dump 包根
    // 目录（imodel.json 所在目录）→ openDumpIModel 既有链。
    QAction* openDisk = m_appToolBar->addAction(QStringLiteral("Open iModel"));
    openDisk->setToolTip(QStringLiteral("Open iModel from disk"));
    setGlyphIcon(openDisk, 0xe9cc);
    QObject::connect(openDisk, &QAction::triggered, m_appToolBar, [] {
        QString const dir = QFileDialog::getExistingDirectory(
            nullptr, QStringLiteral("Select dump package root (contains imodel.json)"),
            QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (dir.isEmpty())
            return;
        dta::DumpOpenPackage pkg;
        pkg.imodelRoot = dir.toStdString();
        pkg.tileRoots = {dir.toStdString()};
        auto* mdView = Gui::Application::Instance()->newDocument();
        auto* view3d = qobject_cast<Gui::View3DInventor*>(mdView);
        if (view3d == nullptr)
            return;
        auto opened = dta::openDumpIModel(*view3d, pkg);
        if (!opened.has_value() && MainWindow::getInstance())
            MainWindow::getInstance()->showStatus(
                1, QStringLiteral("Open failed: dump package unreadable (%1)").arg(dir));
    });

    // Surface.ts:133-139 — Open Blank Connection（新 blank 文档视图）。
    QAction* openBlank = m_appToolBar->addAction(QStringLiteral("Blank"));
    openBlank->setToolTip(QStringLiteral("Open Blank Connection"));
    setGlyphIcon(openBlank, 0xe9d8);
    QObject::connect(openBlank, &QAction::triggered, m_appToolBar, [] {
        Gui::Application::Instance()->newDocument();
    });

    // Surface.ts:141-152 — Analysis Style Example（引擎未移植——置灰）。
    addDisabled(m_appToolBar, QStringLiteral("Analysis Style Example"), 0xea32);

    // Surface.ts:154-165 — Decoration Geometry Example。
    QAction* deco = m_appToolBar->addAction(QStringLiteral("Deco"));
    deco->setToolTip(QStringLiteral("Decoration Geometry Example"));
    setGlyphIcon(deco, 0xe9d8);
    QObject::connect(deco, &QAction::triggered, m_appToolBar, [] {
        auto* view3d = qobject_cast<Gui::View3DInventor*>(
            Gui::Application::Instance()->activeView());
        if (!view3d)
            view3d = qobject_cast<Gui::View3DInventor*>(
                Gui::Application::Instance()->newDocument());
        if (view3d)
            Gui::openDecorationGeometryExample(*view3d);
    });

    // Surface.ts:167-178 — Cesium Renderer Example（keyin dta cesium example
    // 既有——M-O(4) P8 陈列馆）。
    QAction* cesium = m_appToolBar->addAction(QStringLiteral("Cesium"));
    cesium->setToolTip(QStringLiteral("Cesium Renderer Example"));
    setGlyphIcon(cesium, 0xe9f4);
    QObject::connect(cesium, &QAction::triggered, m_appToolBar, [] {
        auto& ta = dqApp::Application::Get().GetToolAdmin();
        if (auto* tool = ta.GetRegistry().Create("CesiumExampleTool"))
            ta.SetActiveTool(tool);
    });
}

// ---------------------------------------------------------------------------
// 主工具栏（Viewer.ts:238-446 的 26 项严格序）
// ---------------------------------------------------------------------------

void DtaToolBarSet::buildMainToolBar(QMainWindow* mw)
{
    m_mainToolBar = makeBar(mw, QStringLiteral("DTA Main"));
    m_mainToolBar->setObjectName(QStringLiteral("DTA.Main"));
    QToolBar* tb = m_mainToolBar;

    // ── 1. Debug info（Viewer.ts:238-242 —— toggleDebugWindow）──
    QAction* debugInfo = tb->addAction(QStringLiteral("Debug"));
    debugInfo->setObjectName(QStringLiteral("DTA.Main.DebugInfo"));
    debugInfo->setToolTip(QStringLiteral("Debug info"));
    setGlyphIcon(debugInfo, 0xe90c);
    {
        static QPointer<Gui::DebugWindow> s_debugWindow;
        QObject::connect(debugInfo, &QAction::triggered, tb, [] {
            if (s_debugWindow) {
                s_debugWindow->close();
                return;
            }
            auto* vp = activeViewport();
            if (!vp)
                return;
            s_debugWindow = new Gui::DebugWindow(vp, MainWindow::getInstance());
            s_debugWindow->setAttribute(Qt::WA_DeleteOnClose);
            s_debugWindow->show();
        });
    }

    // ── 2. Open iModel from disk（:244-249——与 app 工具栏同链）──
    {
        QAction* a = tb->addAction(QStringLiteral("Open iModel"));
        a->setToolTip(QStringLiteral("Open iModel from disk"));
        setGlyphIcon(a, 0xe9cc);
        QObject::connect(a, &QAction::triggered, tb, [this]() {
            if (m_appToolBar && !m_appToolBar->actions().isEmpty())
                m_appToolBar->actions().constFirst()->trigger();
        });
    }

    // ── 3. Open iModel from hub（:252-268——➖ 零网络置灰）──
    addDisabled(tb, QStringLiteral("Open Hub"), 0xe9e0);

    // ── 4. ViewPicker（:270-272——<select> 控件）──
    {
        auto* picker = new ViewPickerComboBox(tb);
        QObject::connect(picker, qOverload<int>(&QComboBox::activated),
                         tb, [picker](int index) {
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
        QAction* pickerAction = tb->addWidget(picker);
        pickerAction->setObjectName(QStringLiteral("DTA.Views.ViewPicker"));
    }

    // ── 5. Models（:274-283——only3d 下拉）──
    {
        QAction* a = addPanelToggle(tb, QStringLiteral("Models"), 0xe90b,
                                    QStringLiteral("Models"));
        m_only3dActions.push_back(a);
    }

    // ── 6. Categories（:286-295——下拉）──
    addPanelToggle(tb, QStringLiteral("Categories"), 0xe901,
                   QStringLiteral("Categories"));

    // ── 7. External saved views（:296-305——下拉）──
    {
        auto* btn = new QToolButton(tb);
        btn->setObjectName(QStringLiteral("DTA.SavedViews.Button"));
        btn->setText(QStringLiteral("Saved Views"));
        btn->setIcon(dtaGlyphIcon(0xe90d));
        btn->setToolTip(QStringLiteral("External saved views"));
        btn->setPopupMode(QToolButton::InstantPopup);
        DtaToolBarSet* self = this;
        QObject::connect(btn, &QToolButton::clicked, btn, [btn, self]() {
            auto* vp = activeViewport();
            if (!vp)
                return;
            auto* panel = new Gui::SavedViewPicker(vp, btn);
            panel->setAttribute(Qt::WA_DeleteOnClose);
            panel->move(btn->mapToGlobal(QPoint(0, btn->height())));
            self->openDropDown(panel);
        });
        QAction* sva = tb->addWidget(btn);
        sva->setText(QStringLiteral("Saved Views"));
        sva->setIcon(dtaGlyphIcon(0xe90d));
    }

    // ── 8. Saved camera paths（:306-315——大件未移植置灰）──
    addEmptyDropDown(tb, QStringLiteral("Camera Paths"), 0xe932);

    // ── 9. Element selection（:316-320——zoom.svg 图按钮）──
    {
        QAction* sel = tb->addAction(QStringLiteral("Select"));
        sel->setToolTip(QStringLiteral("Element selection"));
        setSvgIcon(sel, ":/icons/dta/zoom.svg");
        QObject::connect(sel, &QAction::triggered, tb, [] {
            auto& ta = dqApp::Application::Get().GetToolAdmin();
            if (auto* tool = ta.GetRegistry().Create("Select"))
                ta.SetActiveTool(tool);
        });
    }

    // ── 10. Measure distance（:321-325——M-R 激活：Measure.Distance 引擎已
    //     移植 [M-O(3) P2]——此前置灰）──
    {
        QAction* measure = tb->addAction(QStringLiteral("Measure"));
        measure->setToolTip(QStringLiteral("Measure distance"));
        setGlyphIcon(measure, 0xeb08);
        QObject::connect(measure, &QAction::triggered, tb, [] {
            auto& ta = dqApp::Application::Get().GetToolAdmin();
            if (auto* tool = ta.GetRegistry().Create("Measure.Distance"))
                ta.SetActiveTool(tool);
        });
    }

    // ── 11. View settings（:326-334——下拉）──
    {
        auto* btn = new QToolButton(tb);
        btn->setText(QStringLiteral("View Settings"));
        btn->setIcon(dtaGlyphIcon(0xe90e));
        btn->setToolTip(QStringLiteral("View settings"));
        btn->setPopupMode(QToolButton::InstantPopup);
        auto* panel = new ViewSettingsPanel(btn);
        DtaToolBarSet* self = this;
        QObject::connect(btn, &QToolButton::clicked, btn, [btn, panel, self]() {
            panel->syncFromViewport();
            panel->move(btn->mapToGlobal(QPoint(0, btn->height())));
            self->openDropDown(panel);
        });
        QAction* wa = tb->addWidget(btn);
        wa->setText(QStringLiteral("View Settings"));
        wa->setIcon(dtaGlyphIcon(0xe90e));
    }

    // ── 12. Fit view（:335-339）──
    {
        QAction* fit = tb->addAction(QStringLiteral("Fit"));
        fit->setToolTip(QStringLiteral("Fit view"));
        setSvgIcon(fit, ":/icons/dta/fit-to-view.svg");
        QObject::connect(fit, &QAction::triggered, tb, [] {
            if (auto* vp = activeViewport())
                runViewTool(new dqApp::FitViewTool(vp, /*oneShot=*/true));
        });
    }

    // ── 13. Window area（:340-344）──
    {
        QAction* windowArea = tb->addAction(QStringLiteral("Window Area"));
        windowArea->setToolTip(QStringLiteral("Window area"));
        setSvgIcon(windowArea, ":/icons/dta/window-area.svg");
        QObject::connect(windowArea, &QAction::triggered, tb, [] {
            if (auto* vp = activeViewport())
                runViewTool(new dqApp::WindowAreaTool(vp));
        });
    }

    // ── 14. Rotate（:345-349）──
    {
        QAction* rotate = tb->addAction(QStringLiteral("Rotate"));
        rotate->setToolTip(QStringLiteral("Rotate"));
        setSvgIcon(rotate, ":/icons/dta/rotate-left.svg");
        QObject::connect(rotate, &QAction::triggered, tb, [] {
            if (auto* vp = activeViewport())
                runViewTool(new dqApp::RotateViewTool(vp, /*oneShot=*/false));
        });
    }

    // ── 15. Standard rotations（:350-355——only3d 下拉）──
    {
        auto* svBtn = new QToolButton(tb);
        svBtn->setIcon(dtaGlyphIcon(0xe909));
        svBtn->setToolTip(QStringLiteral("Standard rotations"));
        auto* svMenu = new QMenu(svBtn);
        svMenu->setObjectName(QStringLiteral("DTA.StdRot.Menu"));
        auto* svPanelAction = new QWidgetAction(svMenu);
        svPanelAction->setDefaultWidget(new StandardRotationsPanel(svMenu));
        svMenu->addAction(svPanelAction);
        DtaToolBarSet* self = this;
        QObject::connect(svBtn, &QToolButton::clicked, svBtn, [svBtn, svMenu, self]() {
            self->closeOpenDropDowns();
            svMenu->popup(svBtn->mapToGlobal(QPoint(0, svBtn->height())));
        });
        QAction* svAction = tb->addWidget(svBtn);
        svAction->setObjectName(QStringLiteral("DTA.ViewTools.StandardRotations"));
        m_only3dActions.push_back(svAction);
    }

    // ── 16. Walk（:356-362——only3d；M-R 激活：View.LookAndMove 引擎已移植
    //     [M-O(3) P1]——此前置灰）──
    {
        QAction* walk = tb->addAction(QStringLiteral("Walk"));
        walk->setToolTip(QStringLiteral("Walk"));
        setSvgIcon(walk, ":/icons/dta/walk.svg");
        QObject::connect(walk, &QAction::triggered, tb, [] {
            if (auto* vp = activeViewport())
                runViewTool(new dqApp::LookAndMoveTool(vp));
        });
        m_only3dActions.push_back(walk);
    }

    // ── 17/18. View undo / redo（:363-372）──
    {
        QAction* undo = tb->addAction(QStringLiteral("Undo"));
        undo->setToolTip(QStringLiteral("View undo"));
        setGlyphIcon(undo, 0xe982);
        QObject::connect(undo, &QAction::triggered, tb, [] {
            if (auto* vp = activeViewport())
                runViewTool(new dqApp::ViewUndoTool(vp));
        });
        QAction* redo = tb->addAction(QStringLiteral("Redo"));
        redo->setToolTip(QStringLiteral("View redo"));
        setGlyphIcon(redo, 0xe983);
        QObject::connect(redo, &QAction::triggered, tb, [] {
            if (auto* vp = activeViewport())
                runViewTool(new dqApp::ViewRedoTool(vp));
        });
    }

    // ── 19. Animation（:373-377——大件未移植置灰）──
    addEmptyDropDown(tb, QStringLiteral("Animation"), 0xe931);

    // ── 20. Sectioning tools（:378-383——M-P P-G live）──
    {
        auto* btn = new QToolButton(tb);
        btn->setText(QStringLiteral("Sectioning"));
        btn->setIcon(dtaGlyphIcon(0xe916));
        btn->setToolTip(QStringLiteral("Sectioning tools"));
        btn->setPopupMode(QToolButton::InstantPopup);
        DtaToolBarSet* self = this;
        QObject::connect(btn, &QToolButton::clicked, btn, [btn, self]() {
            auto* vp = activeViewport();
            if (!vp)
                return;
            auto* panel = new Gui::SectionsPanel(vp, btn);
            panel->setAttribute(Qt::WA_DeleteOnClose);
            panel->move(btn->mapToGlobal(QPoint(0, btn->height())));
            self->openDropDown(panel);
        });
        QAction* sa = tb->addWidget(btn);
        sa->setText(QStringLiteral("Sectioning"));
        sa->setIcon(dtaGlyphIcon(0xe916));
    }

    // ── 21. Spatial Classification（:384-393——only3d 大件未移植置灰）──
    {
        QAction* a = addDisabled(tb, QStringLiteral("Classification"), 0xe9d8);
        m_only3dActions.push_back(a);
    }

    // ── 22. Override feature symbology（:394-399——M-O(2) I10 live）──
    {
        auto* btn = new QToolButton(tb);
        btn->setText(QStringLiteral("Overrides"));
        btn->setIcon(dtaGlyphIcon(0xe90a));
        btn->setToolTip(QStringLiteral("Override feature symbology"));
        btn->setPopupMode(QToolButton::InstantPopup);
        DtaToolBarSet* self = this;
        QObject::connect(btn, &QToolButton::clicked, btn, [btn, self]() {
            auto* vp = activeViewport();
            if (!vp)
                return;
            auto* panel = new Gui::FeatureOverridesPanel(vp, btn);
            panel->setAttribute(Qt::WA_DeleteOnClose);
            panel->move(btn->mapToGlobal(QPoint(0, btn->height())));
            self->openDropDown(panel);
        });
        QAction* oa = tb->addWidget(btn);
        oa->setText(QStringLiteral("Overrides"));
        oa->setIcon(dtaGlyphIcon(0xe90a));
    }

    // ── 23/24/25. Point cloud / Contours / Format Set（:400-421——置灰）──
    addDisabled(tb, QStringLiteral("Point Cloud"), 0xe923);
    addDisabled(tb, QStringLiteral("Contours"), 0xe94b);
    addDisabled(tb, QStringLiteral("Format Set"), 0xe9cc);

    // ── 26. Google Maps（:424-434——config googleMapsUi 门；DanQing 无该
    //     config=关态 → 不出现（DTA 关态同形）──
}

}  // namespace Gui
