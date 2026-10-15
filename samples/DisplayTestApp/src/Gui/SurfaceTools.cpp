// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Surface 窗口 keyin 工具族实现（MDI 承载）
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/Surface.ts
//              + Window.ts（逐段行号见 SurfaceTools.h 头注与各函数注）。
#include "SurfaceTools.h"

#include <dqApp/Application.h>
#include <dqApp/IModelConnection.h>  // GetKey（ReopenIModelTool——E-Y7）
#include <dqApp/ParseArgs.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>

#include "../DumpOpenHelper.h"  // dta::modelIdForIModelKey/dumpPackageForModel/openDumpIModel/注册表（Y-b2）
#include "MainWindow.h"
#include "View3DInventor.h"

#include <QMdiArea>
#include <QPointer>
#include <QWidget>

#include <cstdlib>
#include <unordered_map>
#include <vector>

namespace Gui {

namespace {

// 测试缝（Surface::setMdiAreaForTest）。
QMdiArea* s_mdiAreaForTest = nullptr;

// E-Y6 激活序追踪器状态：逐 mdiArea 的焦点序**窗内容件**表（QPointer 防悬垂）。
// 追踪 widget 而非子窗壳（二跑取证实锤：QMdiSubWindow 的 WA_DeleteOnClose 与
// 内容件析构不同步——子窗可从 area 摘除而壳对象残存，壳的 widget() 悬垂；
// QPointer<内容件> 在内容件析构时精确置空）。
std::unordered_map<QMdiArea*, std::vector<QPointer<QObject>>>& activationOrders()
{
    static std::unordered_map<QMdiArea*, std::vector<QPointer<QObject>>> s_orders;
    return s_orders;
}

// 追踪器懒装（本层首次触及该 area 时接线 subWindowActivated——参考
// Surface.focus 的 _windows 提前语义 :331-332）。
std::vector<QPointer<QObject>>& activationOrderFor(QMdiArea* area)
{
    auto& orders = activationOrders();
    auto it = orders.find(area);
    if (it == orders.end()) {
        it = orders.emplace(area, std::vector<QPointer<QObject>>{}).first;
        // 注意：subWindowActivated 的参数子窗其 parentWidget() 是 area 的
        // viewport 而非 area 自身（首跑红旗实锤）——lambda 直捕 area（连接
        // context=area，area 析构后不再发射，无悬垂）。
        QObject::connect(area, &QMdiArea::subWindowActivated, area,
                         [area](QMdiSubWindow* sub) {
                             if (sub == nullptr || sub->widget() == nullptr)
                                 return;
                             QPointer<QObject> const tracked(sub->widget());
                             auto& vec = activationOrders()[area];
                             for (auto vit = vec.begin(); vit != vec.end(); ++vit)
                                 if (vit->data() == tracked.data()) {
                                     vec.erase(vit);
                                     break;
                                 }
                             vec.insert(vec.begin(), tracked);
                         });
    }
    return it->second;
}

// dock 态动态属性键（E-Y1——WindowHeader._dockState 的 Qt 承载）。
char const* const kDockStateProp = "dtaDockState";
char const* const kDockSavedGeomProp = "dtaDockSavedGeom";

// Ported from: Window.ts:61 ResizeState ctor 的 minSize=50 缺省。
constexpr double kMinWindowContentSize = 50.0;

}  // namespace

// ---------------------------------------------------------------------------
// Surface 静态层
// ---------------------------------------------------------------------------

void Surface::setMdiAreaForTest(QMdiArea* area)
{
    s_mdiAreaForTest = area;
}

void Surface::resetForTest()
{
    s_mdiAreaForTest = nullptr;
    activationOrders().clear();
}

QMdiArea* Surface::mdiArea()
{
    if (s_mdiAreaForTest != nullptr)
        return s_mdiAreaForTest;
    auto* mw = MainWindow::getInstance();
    return mw != nullptr ? mw->mdiArea() : nullptr;
}

// Ported from: Surface.findWindowById (Surface.ts:399-401——
// `_windows.find(x => x.windowId === id)`；windowId 双形态 = 视口
// viewportId 串[Viewer.ts:609]｜NamedWindow 注册 id[E-Y1]）。
QMdiSubWindow* Surface::findWindowById(QString const& id)
{
    QMdiArea* area = mdiArea();
    if (area == nullptr)
        return nullptr;
    for (auto* sub : area->subWindowList()) {
        if (sub == nullptr || sub->widget() == nullptr)
            continue;
        if (auto* view = qobject_cast<Gui::View3DInventor*>(sub->widget())) {
            if (auto* vp = view->getUeViewport()) {
                if (id == QString::number(vp->GetViewportId()))
                    return sub;
            }
        } else if (sub->widget()->objectName() == id) {
            return sub;
        }
    }
    return nullptr;
}

// Ported from: Surface.focusedWindow (Surface.ts:403-405——_windows[0] 焦点序
// 首）。QMdiArea activeSubWindow = 当前焦点窗（E-Y1）。
QMdiSubWindow* Surface::focusedWindow()
{
    QMdiArea* area = mdiArea();
    return area != nullptr ? area->activeSubWindow() : nullptr;
}

// Ported from: Surface.firstViewer (Surface.ts:216-221——焦点序首个 Viewer)。
Gui::View3DInventor* Surface::firstViewer()
{
    QMdiArea* area = mdiArea();
    if (area == nullptr)
        return nullptr;
    // 激活序优先（追踪器懒装；QPointer<内容件>——死引用自动置空）。
    for (auto const& ptr : activationOrderFor(area))
        if (!ptr.isNull())
            if (auto* view = qobject_cast<Gui::View3DInventor*>(ptr.data()))
                return view;
    // 未追踪 viewer 创建序补位（E-Y6 发散注）。
    for (auto* sub : area->subWindowList())
        if (sub != nullptr)
            if (auto* view = qobject_cast<Gui::View3DInventor*>(sub->widget()))
                return view;
    return nullptr;
}

// Ported from: Surface.createNamedWindow (Surface.ts:293-297) + Window ctor
// 默认几何（Window.ts:358-372——top/left=0、width/height=面 1/3）+
// NamedWindow ctor（:434-441——title 缺席=id）+ addWindow 的
// ensureInSurface+focus（Surface.ts:310-315）。
QMdiSubWindow* Surface::createNamedWindow(NamedWindowProps const& props)
{
    QMdiArea* area = mdiArea();
    if (area == nullptr)
        return nullptr;
    auto* content = new QWidget();  // contentDiv 等价物（无视口命名窗占位）
    content->setObjectName(QString::fromStdString(props.id));
    auto* sub = new QMdiSubWindow(area->viewport());
    sub->setAttribute(Qt::WA_DeleteOnClose);
    sub->setWidget(content);
    QRect const vp = area->viewport()->rect();
    int const w = props.width.value_or(vp.width() / 3);    // Window.ts:362
    int const h = props.height.value_or(vp.height() / 3);  // :363
    int const l = props.left.value_or(0);                  // :360
    int const t = props.top.value_or(0);                   // :361
    sub->setGeometry(l, t, w, h);
    // NamedWindow ctor（Window.ts:437-439——title 缺席 → windowId）。
    sub->setWindowTitle(QString::fromStdString(props.title.value_or(props.id)));
    area->addSubWindow(sub);
    ensureInSurface(sub);  // addWindow 内的 ensureInSurface（Surface.ts:312）
    sub->show();
    focus(sub);            // addWindow→focus（Surface.ts:315）
    return sub;
}

// Ported from: Surface.focus (Surface.ts:322-338——参考含 keyinField.loseFocus
// :337 副作用；DanQing keyin 聚焦归 KeyinField 自身焦点事件链，不在本层）。
void Surface::focus(QMdiSubWindow* window)
{
    QMdiArea* area = mdiArea();
    if (area != nullptr && window != nullptr)
        area->setActiveSubWindow(window);
}

// Ported from: Surface.close (Surface.ts:407-410——isCloseable 恒 true 坍塌）。
void Surface::close(QMdiSubWindow* window)
{
    if (window != nullptr)
        window->close();
}

// Ported from: Surface.closeAllViewers (Surface.ts:412-415——filter Viewer
// 逐 forceClose）。Viewer ≡ View3DInventor 子窗（Start 页等其它子窗不关——
// "Start 页回落"语义）。
void Surface::closeAllViewers()
{
    QMdiArea* area = mdiArea();
    if (area == nullptr)
        return;
    for (auto* sub : area->subWindowList())
        if (sub != nullptr && qobject_cast<Gui::View3DInventor*>(sub->widget()) != nullptr)
            sub->close();
}

// Ported from: WindowHeader.dock (Window.ts:185-198——非 dock 态才存档当前
// 几何；置 dock 态后 applyDock）。
void Surface::dock(QMdiSubWindow* window, Dock dockState)
{
    if (window == nullptr)
        return;
    if (!isDocked(window))
        window->setProperty(kDockSavedGeomProp, window->geometry());
    window->setProperty(kDockStateProp, static_cast<int>(dockState));
    applyDock(window);
}

// Ported from: WindowHeader.undock (Window.ts:200-213——恢复存档 + 清态 +
// focus；未 dock = no-op）。
void Surface::undock(QMdiSubWindow* window)
{
    if (window == nullptr || !isDocked(window))
        return;
    QRect const saved = window->property(kDockSavedGeomProp).toRect();
    window->setProperty(kDockStateProp, QVariant());  // 清态（属性失效）
    window->setProperty(kDockSavedGeomProp, QVariant());
    window->setGeometry(saved);
    focus(window);  // :212
}

// Ported from: WindowHeader.isDocked (Window.ts:246——_dockState !==
// undefined）。Full=0 是合法 dock 态 → 以属性**存在性**判定。
bool Surface::isDocked(QMdiSubWindow* window)
{
    return window != nullptr && window->property(kDockStateProp).isValid();
}

// Ported from: WindowHeader.applyDock (Window.ts:216-245——半边/四分/全幅
// 几何公式逐行；isResizable 门 :218 坍塌[Window.isResizable 恒 true :421]）。
void Surface::applyDock(QMdiSubWindow* window)
{
    QMdiArea* area = mdiArea();
    if (window == nullptr || area == nullptr || !isDocked(window))
        return;
    QRect const vp = area->viewport()->rect();
    int const sw = vp.width();   // :221 surf.element.clientWidth
    int const sh = vp.height();  // :222 clientHeight
    int const hw = sw / 2;       // :223 Math.floor(sw/2)——正数同
    int const hh = sh / 2;       // :224
    int l = 0, w = sw, t = 0, h = sh;  // :226-229
    int const dockBits = window->property(kDockStateProp).toInt();
    if (static_cast<int>(Dock::Full) != dockBits) {                    // :232
        if (dockBits & static_cast<int>(Dock::Top))                    // :233
            h = hh;
        else if (dockBits & static_cast<int>(Dock::Bottom))            // :235
            t = h = hh;                                                // :236（右结合：h=hh 后 t=h）
        if (dockBits & static_cast<int>(Dock::Left))                   // :238
            w = hw;
        else if (dockBits & static_cast<int>(Dock::Right))             // :240
            l = w = hw;                                                // :241（右结合：w=hw 后 l=w）
    }
    window->setGeometry(l, t, w, h);  // :243-247 四 style 赋值
    focus(window);                    // :244 this.window.focus()
}

// Ported from: WindowHeader.ensureInSurface (Window.ts:247-281——出界回钳）。
void Surface::ensureInSurface(QMdiSubWindow* window)
{
    QMdiArea* area = mdiArea();
    if (window == nullptr || area == nullptr)
        return;
    QRect const vp = area->viewport()->rect();
    int const surfaceTop = vp.top();        // :251 clientTop
    int const surfaceBottom = vp.height();  // :252 clientHeight
    int const surfaceLeft = vp.left();      // :253 clientLeft
    int const surfaceRight = surfaceLeft + vp.width();  // :254
    QRect geom = window->geometry();
    int windowTop = geom.y();   // :257 parseInt(style.top)
    int windowLeft = geom.x();  // :258
    int const windowHeight = geom.height();
    int const windowWidth = geom.width();
    if (windowTop + windowHeight >= surfaceBottom)  // :262-263
        windowTop = surfaceBottom - windowHeight;
    if (windowTop < surfaceTop)                     // :264-265
        windowTop = surfaceTop;
    geom.moveTop(windowTop);                        // :266
    if (windowHeight > surfaceBottom)               // :267-268
        geom.setHeight(surfaceBottom);
    if (windowLeft + windowWidth >= surfaceRight)   // :271-272
        windowLeft = surfaceRight - windowWidth;
    if (windowLeft < surfaceLeft)                   // :273-274
        windowLeft = surfaceLeft;
    geom.moveLeft(windowLeft);                      // :275
    if (windowWidth > surfaceRight)                 // :276-277
        geom.setWidth(surfaceRight);
    window->setGeometry(geom);
}

// Ported from: WindowHeader.resizeContent (Window.ts:318-335——w/h 各 +4
// 边框 kludge :319-320、min 50 钳 :321-322、清 dock 态 :323、按内容 delta
// 改 container :324-332）。NaN 侧跳过（E-Y1——参考 "NaNpx" 浏览器忽略同效）。
void Surface::resizeContent(QMdiSubWindow* window, double w, double h)
{
    if (window == nullptr)
        return;
    w += 4.0;  // :319
    h += 4.0;  // :320
    if (!std::isnan(w))
        w = std::max(w, kMinWindowContentSize);  // :321（NaN 在参考经
    if (!std::isnan(h))                          //  Math.max 落 NaN→浏览器 no-op——
        h = std::max(h, kMinWindowContentSize);  //  本承载 NaN 侧整体跳过，同效）

    window->setProperty(kDockStateProp, QVariant());  // :323 清 dock 态
    window->setProperty(kDockSavedGeomProp, QVariant());

    QWidget* content = window->widget();
    if (content == nullptr)
        return;
    QSize const subSize = window->size();
    QSize const contentSize = content->size();
    int const dw = std::isnan(w) ? 0 : static_cast<int>(w) - contentSize.width();   // :327-329
    int const dh = std::isnan(h) ? 0 : static_cast<int>(h) - contentSize.height();  // :330
    window->resize(subSize.width() + dw, subSize.height() + dh);  // :331-332
}

// ---------------------------------------------------------------------------
// CreateWindowTool（Surface.ts:451-487）
// ---------------------------------------------------------------------------

// Ported from: run (:457-460)。
bool CreateWindowTool::run(NamedWindowProps const& props)
{
    Surface::createNamedWindow(props);  // :458 DisplayTestApp.surface.createNamedWindow
    return true;                        // :459
}

// Ported from: parseAndRun (:462-486)。
bool CreateWindowTool::parseAndRun(std::vector<std::string> const& args)
{
    std::optional<std::string> name;  // :463 `let name: string | undefined`
    WindowProps props;                // :464

    auto const parsed = dqApp::parseArgs(args);  // :466
    if (auto id = parsed.get("id"))              // :467-469
        name = *id;
    if (auto title = parsed.get("title"))        // :471-473
        props.title = *title;

    // :475-480——top/left/width/height 四整数键循环。
    for (char const* key : {"top", "left", "width", "height"}) {
        auto const value = parsed.getInteger(key);
        if (!value.has_value())
            continue;
        if (std::strcmp(key, "top") == 0)
            props.top = *value;
        else if (std::strcmp(key, "left") == 0)
            props.left = *value;
        else if (std::strcmp(key, "width") == 0)
            props.width = *value;
        else
            props.height = *value;
    }

    if (name.has_value()) {  // :482-484——id 缺席整窗不建（恒 true 返回）。
        NamedWindowProps namedProps;
        namedProps.id = *name;
        static_cast<WindowProps&>(namedProps) = props;
        static_cast<void>(run(namedProps));      // :483 `await this.run(namedProps)`
    }
    return true;             // :486
}

// ---------------------------------------------------------------------------
// WindowIdTool 基类（Surface.ts:489-506）
// ---------------------------------------------------------------------------

// Ported from: run (:495-501——id 在场查表、缺席取焦点窗；窗在才 execute）。
bool WindowIdTool::run(std::optional<std::string> const& windowId)
{
    QMdiSubWindow* window = windowId.has_value()
        ? Surface::findWindowById(QString::fromStdString(*windowId))
        : Surface::focusedWindow();
    if (window != nullptr)
        execute(window);
    return true;
}

// Ported from: parseAndRun (:503-505——args[0] 直传，缺席=undefined）。
bool WindowIdTool::parseAndRun(std::vector<std::string> const& args)
{
    return run(args.empty() ? std::nullopt
                            : std::optional<std::string>(args.front()));
}

// FocusWindowTool.execute（:510-512——window.focus()）。
void FocusWindowTool::execute(QMdiSubWindow* window)
{
    Surface::focus(window);
}

// MaximizeWindowTool.execute（:518-520——已 dock 不重复 dock[存档不被覆盖]）。
void MaximizeWindowTool::execute(QMdiSubWindow* window)
{
    if (!Surface::isDocked(window))
        Surface::dock(window, Dock::Full);
}

// RestoreWindowTool.execute（:526-527）。
void RestoreWindowTool::execute(QMdiSubWindow* window)
{
    Surface::undock(window);
}

// CloseWindowTool.execute（:533-534）。
void CloseWindowTool::execute(QMdiSubWindow* window)
{
    Surface::close(window);
}

// ---------------------------------------------------------------------------
// ResizeWindowTool（Surface.ts:536-559）
// ---------------------------------------------------------------------------

// Ported from: run (:541-547)。
bool ResizeWindowTool::run(double width, double height,
                           std::optional<std::string> const& id)
{
    QMdiSubWindow* window = id.has_value()
        ? Surface::findWindowById(QString::fromStdString(*id))
        : Surface::focusedWindow();
    if (window != nullptr)
        Surface::resizeContent(window, width, height);
    return true;
}

// Ported from: parseAndRun (:549-558)。E-Y3 strtol=parseInt 前导整数语义；
// E-Y4 **OR 怪癖原样**（单侧重法即 run，NaN 侧经 resizeContent 落 no-op）。
bool ResizeWindowTool::parseAndRun(std::vector<std::string> const& args)
{
    auto const parseIntArg = [&args](size_t index) -> double {
        if (index >= args.size())
            return std::numeric_limits<double>::quiet_NaN();
        char* end = nullptr;
        long const num = std::strtol(args[index].c_str(), &end, 10);
        if (end == args[index].c_str())
            return std::numeric_limits<double>::quiet_NaN();  // 无数字=NaN
        return static_cast<double>(num);
    };
    double const w = parseIntArg(0);  // :550 parseInt(args[0], 10)
    double const h = parseIntArg(1);  // :551 parseInt(args[1], 10)
    if (!std::isnan(w) || !std::isnan(h))  // :552——OR 怪癖（非 &&）
        static_cast<void>(run(w, h, args.size() > 2
                                        ? std::optional<std::string>(args[2])
                                        : std::nullopt));  // :553
    return true;  // :555
}

// ---------------------------------------------------------------------------
// DockWindowTool（Surface.ts:560-598）
// ---------------------------------------------------------------------------

// Ported from: run (:565-571)。
bool DockWindowTool::run(Dock dockState, std::optional<std::string> const& windowId)
{
    QMdiSubWindow* window = windowId.has_value()
        ? Surface::findWindowById(QString::fromStdString(*windowId))
        : Surface::focusedWindow();
    if (window != nullptr)
        Surface::dock(window, dockState);
    return true;
}

// Ported from: parseAndRun (:573-597——l/r/t/b 位累积；未知字母 default
// 早退 true[E-Y5]；dock=0 不 run）。
bool DockWindowTool::parseAndRun(std::vector<std::string> const& args)
{
    int dockBits = 0;  // :574 `let dock = 0`
    std::string const arg0 = args.empty() ? std::string() : args.front();
    for (char raw : arg0) {                 // :575 `for (const c of args[0].toLowerCase())`
        char const c = static_cast<char>(std::tolower(static_cast<unsigned char>(raw)));
        switch (c) {
            case 'l': dockBits |= static_cast<int>(Dock::Left); break;    // :577-579
            case 'r': dockBits |= static_cast<int>(Dock::Right); break;   // :580-582
            case 't': dockBits |= static_cast<int>(Dock::Top); break;     // :583-585
            case 'b': dockBits |= static_cast<int>(Dock::Bottom); break;  // :586-588
            default:
                return true;  // :589-590——未知字母早退（已累积位不落窗）
        }
    }
    if (0 != dockBits)  // :592
        static_cast<void>(run(static_cast<Dock>(dockBits),
                              args.size() > 1 ? std::optional<std::string>(args[1])
                                              : std::nullopt));  // :593
    return true;  // :595
}

// ---------------------------------------------------------------------------
// CloseIModelTool（Surface.ts:645-652）
// ---------------------------------------------------------------------------

// Ported from: run (:648-651——closeAllViewers 恒 true）。
bool CloseIModelTool::run()
{
    Surface::closeAllViewers();  // :649
    return true;                 // :650
}

// ---------------------------------------------------------------------------
// ReopenIModelTool（Surface.ts:654-665）
// ---------------------------------------------------------------------------

// Ported from: run (:657-664——`const viewer = Surface.instance.firstViewer;
// if (undefined !== viewer) await viewer.openFile(viewer.viewport.iModel.key);
// return true`）。E-Y7：openFile(key) 的 DanQing 承载 = key→modelId→包映射
// → 同一视图 openDumpIModel 重开（参考 resetIModel 的同 viewer 换绑语义，
// Viewer.ts:545-552/571-580）。
bool ReopenIModelTool::run()
{
    auto* viewer = Surface::firstViewer();  // :659
    if (viewer == nullptr)
        return true;                        // :660-661 否臂（无 viewer 不动）

    auto* vp = viewer->getUeViewport();
    std::string const key = (vp != nullptr && vp->GetIModel() != nullptr)
                                ? vp->GetIModel()->GetKey()
                                : std::string();  // :660 viewport.iModel.key

    bool opened = false;
    if (auto const modelId = dta::modelIdForIModelKey(key)) {
        if (auto pkg = dta::dumpPackageForModel(*modelId)) {
            // 旧 provider 先摘（Viewport.dropTiledGraphicsProvider
            // :1738-1740）再卸注册——次序防悬垂（旧 DumpOpenResult 删除即
            // provider 死；参考 resetIModel 同序：旧 iModel 先关）。
            if (auto* old = dta::findOpenedDump(viewer))
                vp->DropTiledGraphicsProvider(old->provider.get());
            dta::forgetOpenedDump(viewer);
            // registerOpenedDump 为 emplace 不覆盖（DumpOpenHelper.cpp:276）
            // ——先卸后装是同视图重开的必要序。
            auto result = dta::openDumpIModel(*viewer, *pkg);
            if (result.has_value()) {
                dta::registerOpenedDump(
                    viewer,
                    std::make_unique<dta::DumpOpenResult>(std::move(*result)));
                opened = true;
            }
        }
    }
    if (!opened) {
        // 参考 openIModel 失败 catch → alert("Error - could not open file.")
        //（Viewer.ts:577-582）的等价消息面（E-Y7/E-Y8——未映射 key/blank
        // 空键/打开失败同路；MainWindow 缺席[测试态]则静默）。
        if (auto* mw = MainWindow::getInstance())
            mw->showStatus(MainWindow::Err,
                           QObject::tr("Error - could not open file."));
    }
    return true;  // :663
}

}  // namespace Gui
