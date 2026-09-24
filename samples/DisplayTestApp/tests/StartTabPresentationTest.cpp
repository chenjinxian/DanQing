// StartTabPresentationTest.cpp — MDI tab strip 的屏幕上屏像素回归。
//
// Authored: no reference test exists in FreeCAD/Qt for on-screen presentation of
// the MDI tab strip. Regression for the 2026-09-20 Start-tab saga:
//
//   Qt 6.11.1 + Windows 11 + 200% DPI：把 showMaximized() 作为窗口的【首个 show】
//   会让 mdiArea 子树在屏幕上以 2× 逻辑坐标呈现——tab 条/状态栏整带不上屏、
//   Start 页内容放大溢出（30 行纯 Qt 复现：build/mdi-repro；QWidget::grab() 渲染
//   始终正确——只有屏幕呈现损坏）。先普通 show()、下一事件轮次再最大化则完全正常
//   （最大化转换本身无害；还原亦可自愈）。
//
//   本测试锁定修复：loadWindowSettings() 必须先普通 show、再延迟最大化。
//   判据（§11.11 位置断言）：tab 条区域左段（Start/3D View 两个 tab 文字处）
//   必须出现暗色像素；损坏态整带为背景色、零暗像素。
//
//   注意：grab()/离屏渲染对坏态也输出正确内容——唯一有效判据是屏幕合成结果
//   （QScreen::grabWindow）。需要真实桌面会话（与 View3DResizeTest 的屏幕
//   采样同一约束，TD-12 已登记该类测试的无人值守风险）。
#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QImage>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QScreen>
#include <QTabBar>
#include <QWindow>

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

#include "QtTestFixtures.h"
#include "Gui/MainWindow.h"
#include "Gui/MDIView.h"

#include <App/Application.h>

namespace {

void spin(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

}  // namespace

// Authored: no reference test exists（窗口系统呈现行为，参考无对应测试——见 §5g）。
TEST(StartTabPresentation, TabStripRendersWhenShownMaximized)
{
    ensureAppReady();

    // 注入"上次会话是最大化"（loadWindowSettings 读取的键），复现真实启动路径。
    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/MainWindow");
    hGrp->SetBool("Maximized", true);

    Gui::MainWindow mw;
    MainWindowGuard guard(&mw);
    // 置顶（非前台）即可被 grabWindow(0) 采到本窗内容——ctest 后台拉起进程拿不到
    // OS 前台，置顶是确定性的替代（§12.9 前台守卫的测试形态）。
    mw.setWindowFlag(Qt::WindowStaysOnTopHint, true);

    // 两个普通 MDI view 即可触发（纯 Qt 复现用的是 QLabel——无需 StartView 全链）。
    auto* v1 = new Gui::MDIView(nullptr, &mw);
    v1->setWindowTitle(QStringLiteral("Start"));
    mw.addWindow(v1);
    auto* v2 = new Gui::MDIView(nullptr, &mw);
    v2->setWindowTitle(QStringLiteral("3D View"));
    mw.addWindow(v2);

    mw.loadWindowSettings();  // 最大化首 show 路径（修复前=触发器）
    mw.raise();
    mw.activateWindow();
#ifdef _WIN32
    SetForegroundWindow(reinterpret_cast<HWND>(mw.winId()));
#endif
    spin(1000);  // 延迟最大化 + 最大化动画 + 数帧呈现

    ASSERT_TRUE(mw.isMaximized()) << "deferred maximize did not land";

#ifdef _WIN32
    // 前台状态仅作诊断（置顶保证采样有效，不以前台为判据）。
    bool const foreground = GetForegroundWindow() == reinterpret_cast<HWND>(mw.winId());
    fprintf(stderr, "[StartTabPres] foreground=%d active=%d\n",
            static_cast<int>(foreground), static_cast<int>(mw.isActiveWindow()));
#endif

    auto* mdi = mw.mdiArea();
    ASSERT_NE(mdi, nullptr);
    auto* tab = mdi->findChild<QTabBar*>();
    ASSERT_NE(tab, nullptr);
    ASSERT_EQ(tab->count(), 2);

    // 屏幕抓取（物理像素）；tab 条全局逻辑矩形 × dpr = 物理矩形。
    auto const dpr = mw.windowHandle() ? mw.windowHandle()->devicePixelRatio() : 1.0;
    QPoint const tl = tab->mapToGlobal(QPoint(0, 0));
    QRect const stripLog(tl, tab->size());
    QRect const stripPhys(qRound(stripLog.x() * dpr), qRound(stripLog.y() * dpr),
                          qRound(stripLog.width() * dpr), qRound(stripLog.height() * dpr));

    QImage const screen
        = mw.screen()->grabWindow(0).toImage().convertToFormat(QImage::Format_RGB32);
    ASSERT_TRUE(stripPhys.left() >= 0 && stripPhys.right() < screen.width());
    ASSERT_TRUE(stripPhys.top() >= 0 && stripPhys.bottom() < screen.height());

    // 取证（默认零开销）：DANQING_TABTEST_DUMP=1 → 保存 tab 条裁剪图 + 计数。
    if (std::getenv("DANQING_TABTEST_DUMP")) {
        screen.copy(stripPhys).save(QStringLiteral("D:/Github/DanQing/build/starttab-strip.png"));
    }

    // 位置断言：tab 条左段（前两个 tab 的文字区，约逻辑 300px 内）暗色像素计数。
    int const rightBound = stripPhys.left() + qRound(300.0 * dpr);
    int darkCount = 0;
    for (int y = stripPhys.top(); y <= stripPhys.bottom(); ++y) {
        for (int x = stripPhys.left(); x < qMin(rightBound, stripPhys.right()); ++x) {
            QRgb const p = screen.pixel(x, y);
            if (qRed(p) + qGreen(p) + qBlue(p) < 300)
                ++darkCount;
        }
    }
    // 坏态：整带背景色，暗像素≈0；好态：两个 tab 的文字 + 边框给出数百暗像素。
    fprintf(stderr, "[StartTabPres] darkCount=%d strip=(%d,%d %dx%d) dpr=%g\n",
            darkCount, stripPhys.x(), stripPhys.y(), stripPhys.width(), stripPhys.height(), dpr);
    EXPECT_GT(darkCount, 20)
        << "tab strip is blank on screen (shown-maximized presentation bug)";
}
