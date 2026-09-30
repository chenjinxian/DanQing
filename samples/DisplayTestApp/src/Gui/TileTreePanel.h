// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Models/瓦树停靠面板（M-L(2) 裁决档改造）。
//
// 原 Model 停靠面板 = FreeCAD ComboView 的 1:1 移植（TreePanel + PropertyView），
// 无文档后端：树永不填充/属性恒空（分析报告 §3.1）。按协调者裁决改造为
// "Models/瓦树"实用面板：列出当前活动视口已打开 iModel 的 models（连接数据面
// DumpIModelConnection::getModels——imodel.json models[]）与 tile trees
// （DumpOpenResult::treeLoadLog——modelSelector 逐 model 装载序），数据源与
// 打开链同源（dta::findOpenedDump 注册表——main.cpp 迁入 DumpOpenHelper）。
//
// Authored: no reference equivalent exists —— DTA 的 ModelPicker（IdPicker.ts）
// 依赖 RPC models 面；本面板是零网络回放数据面的宿主侧陈列。单树可见性切换
// 需引擎 TileTreeReference 显示位（现无）——登记 M-L(3)（Models/Categories
// 选择器，分析报告 TOP 差距 #3）。
#pragma once

#include <QTreeWidget>
#include <QTimer>

namespace Gui {

class TileTreePanel : public QTreeWidget
{
    Q_OBJECT
public:
    explicit TileTreePanel(QWidget* parent = nullptr);

public Q_SLOTS:
    // 重建清单（活动视口的 models + tile trees）。内容指纹不变时跳过——
    // 避免轮询期重建丢失选中/展开状态。
    void refresh();

private:
    QTimer m_refreshTimer;  // 1s 轮询（tab 切换/打开完成均无需事件接线）
    QString m_content;      // 上次内容指纹
};

// 面板装配（main.cpp 调用——与 DtaToolBarSet 同位）：DockWindowManager 登记
// （沿用原 ComboView 的注册键）+ 左区停靠（addDockWindow 内部经 getMainWindow()
// 取宿主——与 FreeCAD DockWindowManager 同语义）。
void setupModelsPanel();

}  // namespace Gui
