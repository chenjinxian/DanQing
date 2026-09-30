// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Models/瓦树停靠面板（M-L(2) 裁决档改造；M-L(3)
// Models 选择器演示版——per-model 可见性 + 单步隔离）。
//
// 原 Model 停靠面板 = FreeCAD ComboView 的 1:1 移植（TreePanel + PropertyView），
// 无文档后端：树永不填充/属性恒空（分析报告 §3.1）。按协调者裁决改造为
// "Models/瓦树"实用面板：列出当前活动视口已打开 iModel 的 models（连接数据面
// DumpIModelConnection::getModels——imodel.json models[]）与 tile trees
// （DumpOpenResult::treeLoadLog——modelSelector 逐 model 装载序），数据源与
// 打开链同源（dta::findOpenedDump 注册表）。
//
// M-L(3) 接线级 #3（分析报告 TOP 差距 #3 的演示版）：per-model 显示复选 +
// 共享动作（Show All/Hide All/Invert）+ ModelPicker 的 6 个单步隔离键。
// Ported from: itwinjs-core display-test-app IdPicker.ts — ModelPicker
// (:321-459：复选 + ⏪◀️➕⛶▶️⏩ 单步隔离 + fit-on-step) + IdPicker 共享动作
// (:40-53：Show All/Hide All/Invert……)。参考的逐模型显示开关 =
// vp.addViewedModels/changeModelDisplay；本仓的对应物 = provider 通道（树全部
// 经 DumpOpenTreeProvider 进场——逐模型开关 = provider 过滤 + InvalidateScene，
// 见 DumpOpenHelper.h 注释）。**EQUIVALENCE 发散登记（M-L(3) 终审
// Important-1）**：可见性位在本面板/provider 上，不在 ViewState 的
// modelSelector——换视图（ChangeView/视图装载/视口同步）不携带隐藏位
// （参考随 modelSelector 迁移）；发散细节与转正 TODO 见 DumpOpenHelper.h
// DumpOpenTreeProvider 注释。参考共享动作中的 "Isolate/Hide Selected"（按选择
// 集元素查所属 model——IdPicker.ts:190-213 ECSQL 查询）与 "Hilite Enabled"
// （model hilite）依赖本仓没有的数据面/高亮通道，不接线（登记）。
//
// Authored: no reference equivalent exists for the widget shell —— DTA 的
// ModelPicker 依赖 RPC models 面；本面板是零网络回放数据面的宿主侧陈列。
#pragma once

#include <QWidget>

#include <QTimer>

class QComboBox;
class QTreeWidget;

namespace Gui {

class TileTreePanel : public QWidget
{
    Q_OBJECT
public:
    explicit TileTreePanel(QWidget* parent = nullptr);

public Q_SLOTS:
    // 重建清单（活动视口的 models + tile trees + 可见性态）。内容指纹不变时
    // 跳过——避免轮询期重建丢失选中/展开状态。
    void refresh();

private:
    // ModelPicker 单步隔离（IdPicker.ts:420-428 stepToIndex）：全部隐藏后只
    // 启用第 index 个 model；fitOnStep → 取景该 model 的世界域。
    void stepToIndex(int index);
    // ModelPicker ⛶ 键（IdPicker.ts:383-392）——隔离后是否 fit。
    void setFitOnStep(bool on);
    // IdPicker 共享动作（:40-53）的接线子集。
    void showAll();
    void hideAll();
    void invert();

    QTimer m_refreshTimer;  // 1s 轮询（tab 切换/打开完成均无需事件接线）
    QString m_content;      // 上次内容指纹
    QComboBox* m_actions = nullptr;
    QTreeWidget* m_tree = nullptr;
    bool m_fitOnStep = true;      // ModelPicker._fitOnStep（默认 true）
    int m_stepIndex = -1;         // ModelPicker._stepIndex
    bool m_updating = false;      // 重建期 itemChanged 守卫
};

// 面板装配（main.cpp 调用——与 DtaToolBarSet 同位）：DockWindowManager 登记
// （沿用原 ComboView 的注册键）+ 左区停靠（addDockWindow 内部经 getMainWindow()
// 取宿主——与 FreeCAD DockWindowManager 同语义）。
void setupModelsPanel();

}  // namespace Gui
