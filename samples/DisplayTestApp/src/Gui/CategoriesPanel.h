// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Categories 停靠面板（M-N(1) Categories 选择器
// 演示版——per-category 可见性）。
//
// 数据源 = 活动 ViewState 的 CategorySelector（imodel.json
// categorySelectorProps.categories 经打开链应用——与 DTA Categories 面板的
// 列举源同面）。复选切换 → Viewport::SetInvisibleSubCategories 引擎通道。
//
// **EQUIVALENCE 发散登记（§11.10）**：参考链 = view.categorySelector +
// iModel.subcategories RPC 表扩展出 _visibleSubCategories
// （FeatureSymbology.ts:134-143——category → 全部 subCategory Id）。本仓
// dump 回放面无该 RPC 表，映射取 **imodel-native 默认子类规则**
// （DgnCategory.cpp:171-175："default sub-category id is always catId + 1"
// ——category C ↔ subCategory {C+1}）。发散：非默认子类不随 category 隐藏
// （baytown 实测存在 cat+2 形态——0x40000000e71 的第二子类 e73）；
// housemodel/joeshouse/instances60 全表为默认子类（categories 0x71..0xa5 奇
// ↔ 特征表 subCats 0x72..0xa4 偶 = catId+1 全覆盖实证）。验证法：特征表
// [CAT] 探针（DANQING_CAT_TRACE）对照 categorySelector；转正路径 = 采集
// iModel.subcategories RPC 数据面后换真表。
//
// 参考 UI 锚：DTA Categories 面板（ViewSettingsPanel 族——列举视图
// categories 复选切换）。共享动作 Show All/Hide All（IdPicker.ts:40-53 同款
// 接线子集，TileTreePanel 先例）。
//
// Authored: no reference widget shell exists offline（DTA 面板依赖 RPC
// subCategory 面；本面板为零网络回放数据面的宿主侧陈列——ModelsPanel
// 演示版同先例）。
#pragma once

#include <QWidget>

#include <QTimer>

#include <set>

class QTreeWidget;
class QTimer;

namespace Gui {

class CategoriesPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CategoriesPanel(QWidget* parent = nullptr);

public Q_SLOTS:
    // 重建清单（活动视口的 categories + 勾选态）。内容指纹不变时跳过。
    void refresh();

private:
    // 勾选态 → 引擎通道（勾选 = 可见；未勾 category C → 隐藏 subCategory C+1）。
    void applyVisibility();
    // IdPicker 共享动作子集（TileTreePanel 同款）。
    void showAll();
    void hideAll();

    QTimer m_refreshTimer;  // 1s 轮询（与 TileTreePanel 同配方）
    QString m_content;      // 上次内容指纹
    QTreeWidget* m_tree = nullptr;
    bool m_updating = false;  // 重建期 itemChanged 守卫
};

// 面板装配（main.cpp 调用）：Categories 页签加入 Models 停靠区。
void setupCategoriesPanel();

}  // namespace Gui
