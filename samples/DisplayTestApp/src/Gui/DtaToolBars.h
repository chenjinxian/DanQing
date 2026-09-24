// DtaToolBars — display-test-app 功能分类的 5 条可停靠工具栏
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/ToolBar.ts（工具栏容器）
//              + Viewer.ts:236-447（视口工具栏）。
// 2026-09-11：Surface.ts:122-180 的 app 级 5 项（Open iModel/Blank Connection/三个
// 示例）按 DTA 语义移至 Start 页卡片（它们是打开前入口，进入 view 后不再出现），
// 不再占用常驻工具栏。
#pragma once

#include <QComboBox>
#include <QObject>
#include <dqBase/DqEvent.h>

class QMainWindow;
class QToolBar;

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

// DtaToolBarSet — 按 DTA 功能分类重建 DisplayTestApp 工具栏区（Views/Selection/
// View Settings/View Tools/Analysis）。FreeCAD 壳不变（菜单栏/MDI 不动）；本类只
// 依赖 QMainWindow::addToolBar，可在无 Gui::MainWindow 的测试目标里实例化。
class DtaToolBarSet : public QObject {
    Q_OBJECT
public:
    explicit DtaToolBarSet(QMainWindow* mainWindow);

    QToolBar* viewsToolBar() const { return m_viewsToolBar; }
    QToolBar* selectionToolBar() const { return m_selectionToolBar; }
    QToolBar* viewSettingsToolBar() const { return m_viewSettingsToolBar; }
    QToolBar* viewToolsToolBar() const { return m_viewToolsToolBar; }
    QToolBar* analysisToolBar() const { return m_analysisToolBar; }

private:
    void buildViewsToolBar(QMainWindow* mw);     // Viewer.ts:236-313（Task 2 实装）
    void buildSelectionToolBar(QMainWindow* mw); // Viewer.ts:315-325（Task 2 实装）
    void buildViewSettingsToolBar(QMainWindow* mw); // Task 4
    void buildViewToolsToolBar(QMainWindow* mw); // Task 3
    void buildAnalysisToolBar(QMainWindow* mw);  // Task 3
    void setViewToolbarsEnabled(bool enabled);   // Task 5 接线

    QToolBar* m_viewsToolBar = nullptr;
    QToolBar* m_selectionToolBar = nullptr;
    QToolBar* m_viewSettingsToolBar = nullptr;
    QToolBar* m_viewToolsToolBar = nullptr;
    QToolBar* m_analysisToolBar = nullptr;
    // "Decoration Geometry Example" 独立工具栏（Surface.ts:155-165 应用顶层入口
    // 语义）——不随视口可用性置灰，不入 setViewToolbarsEnabled 列表。
    QToolBar* m_decoToolBar = nullptr;
    dqBase::DqEventScope m_scope;
};

}  // namespace Gui
