// DtaToolBars — DTA 工具栏区（M-R：单一主工具栏 + app 工具栏换位 + 下拉交互合同）
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/ToolBar.ts（容器）
//              + Viewer.ts:236-447（viewer 工具栏 26 项单行）
//              + Surface.ts:103-119（焦点换位：viewer 聚焦 → viewer 工具栏；
//                无聚焦 → app 工具栏）+ Surface.ts:122-178（app 工具栏 5 项）。
//
// M-R 结构裁决（用户 2026-10-06）：留 Start 页（dump 数据面入口——DTA 的 app 级
// 打开入口双承载）、留菜单栏、留 MDI 多视口。
#pragma once

#include <QComboBox>
#include <QObject>
#include <QPointer>
#include <dqBase/DqEvent.h>

#include <QVector>

class QMainWindow;
class QToolBar;
class QWidget;

namespace Gui {

// ViewPickerComboBox — DTA ViewPicker 的 Qt 对应控件：HTML <select>
//（ViewPicker.ts:176-190）→ QComboBox（FreeCAD WorkbenchSelector.cpp 同为
// 工具栏内 QComboBox：activated 信号 + showPopup 重载）。列表每次弹出前重建；
// blank 下合成条目 "Spatial View"；选中 → getView→clone→ChangeView。
class ViewPickerComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit ViewPickerComboBox(QWidget* parent);

    // ← ViewPicker.populate（ViewPicker.ts:187-203）：清空后按 ViewList 重建。
    void repopulate();

protected:
    void showPopup() override;
};

// DtaToolBarSet — DTA 工具栏区。两条工具栏共享主窗顶部区域（DTA topdiv 的
// appendChild 换位语义）：
//  - appToolBar（Surface.createToolBar :122-178）：Open from disk / Open Blank /
//    Analysis Style Example（置灰）/ Decoration Geometry Example / Cesium
//    Renderer Example——无聚焦视口时显示。
//  - mainToolBar（Viewer.ts:238-446 的 26 项严格序）：聚焦视口时显示。
// 交互合同（ToolBar.ts:122-197）：下拉单开互斥（open 先 close 全部）、
// 视口切换关全部打开下拉 + only3d 项显隐（is3d）。
// FreeCAD 壳不变（菜单栏/MDI/Start 页不动）；本类只依赖 QMainWindow::addToolBar，
// 可在无 Gui::MainWindow 的测试目标里实例化。
class DtaToolBarSet : public QObject {
    Q_OBJECT
public:
    explicit DtaToolBarSet(QMainWindow* mainWindow);

    QToolBar* appToolBar() const { return m_appToolBar; }
    QToolBar* mainToolBar() const { return m_mainToolBar; }

    // ── 下拉交互合同（测试 seam；ToolBar.ts:163-196）──
    // ToolBar.close（:163-172）：关闭全部打开的下拉面板。
    void closeOpenDropDowns();
    // 当前打开的下拉面板数（测试观测）。
    int openDropDownCount() const { return m_openPanels.size(); }
    // 打开一个下拉面板（ToolBar.open :168-180 的单开互斥语义——先 close 全部）。
    void openDropDown(QPointer<QWidget> panel);

private:
    void buildAppToolBar(QMainWindow* mw);      // Surface.ts:122-178
    void buildMainToolBar(QMainWindow* mw);     // Viewer.ts:238-446（26 项）
    // 视口聚焦换位（Surface.ts:103-119 appendChild 换位 + 既有
    // setViewToolbarsEnabled 的置灰语义合并——DTA 是换位非置灰）。
    void swapToolBarsForViewport(void* activeViewport);
    // only3d 项显隐（ToolBar.onViewChanged :192-193——is3d ? block : none）。
    void updateOnly3dVisibility();

    // 下拉登记（openDropDown 经此保证互斥）；面板 destroyed 自动摘除。
    void registerDropDownPanel(QWidget* panel);

    QToolBar* m_appToolBar = nullptr;
    QToolBar* m_mainToolBar = nullptr;
    QVector<QPointer<QWidget>> m_openPanels;
    QVector<QAction*> m_only3dActions;   // Viewer.ts only3d 项（Models/StdRot/
                                         // Walk/Classification）
    dqBase::DqEventScope m_scope;
};

}  // namespace Gui
