// ViewSettingsPanel — 视图设置弹出面板（View Settings 工具栏按钮的下拉内容）
// Ported from: itwinjs-core display-test-app ViewAttributes.ts（ViewAttributes 面板）
#pragma once

#include <QFrame>

#include <functional>

#include <dqCommon/ViewFlags.h>   // ViewFlagsProperties（applyFlags 签名；Step 4 备注批准）

class QCheckBox;
class QComboBox;
class QColor;
class QPushButton;
class QWidget;

namespace Gui {

// 弹出面板：View Flags 复选组（12 + Camera + Monochrome）+ Render Mode 下拉 +
// Monochrome Color/Scaled 子项（M-O(1) I1）+ 置灰分区标注。flags 读写经活动视口
// 的 DisplayStyle（setViewFlags + SetupFromView）。
class ViewSettingsPanel : public QFrame {
    Q_OBJECT
public:
    explicit ViewSettingsPanel(QWidget* parent = nullptr);
    void syncFromViewport();   // 弹出时从活动视口回读当前值

    // 应用单色颜色（ViewAttributes.ts:393-396 handler——Color input 选色后
    // `settings.monochromeColor = ColorDef.create(color); this.sync()`）。
    // 独立为可测槽：QColorDialog 是模态 UI，测试直接调用本函数锁写通道。
    void applyMonochromeColor(QColor const& color);

private:
    // 任务书 Step 4 批准偏差：捕获 lambda 无法转 void(*)(...) 函数指针，
    // 采用备选方案 std::function（行为不变）。
    void applyFlags(std::function<void(dqCommon::ViewFlagsProperties&)> mod);
    QComboBox* m_renderMode = nullptr;
    // Monochrome 子项（ViewAttributes.ts:386-419 addMonochrome——Color 输入 +
    // "Scaled" 复选，monochrome viewFlag 位开时才显示）。
    QWidget* m_monochromeRow = nullptr;
    QPushButton* m_monochromeColorButton = nullptr;
    QCheckBox* m_scaledCheckbox = nullptr;
};
}  // namespace Gui
