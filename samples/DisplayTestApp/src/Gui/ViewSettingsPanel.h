// ViewSettingsPanel — 视图设置弹出面板（View Settings 工具栏按钮的下拉内容）
// Ported from: itwinjs-core display-test-app ViewAttributes.ts（ViewAttributes 面板）
#pragma once

#include <QFrame>

#include <functional>

#include <dqCommon/HiddenLine.h>   // HiddenLineSettingsProps（P6 overrideEdgeSettings）
#include <dqCommon/SkyBox.h>       // SkyBoxProps（P7 updateSkyEnvironment）
#include <dqCommon/ViewFlags.h>   // ViewFlagsProperties（applyFlags 签名；Step 4 备注批准）

class QCheckBox;
class QComboBox;
class QColor;
class QPushButton;
class QSlider;
class QSpinBox;
class QWidget;

namespace Gui {

class ThematicDisplayEditor;  // M-S S-g（同目录 Gui/——编辑器拆分文件）

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

    // Ported from: ViewAttributes.ts:829-833 overrideEdgeSettings（M-O(4) P6
    // ——hiddenLineSettings = current.override(props) + sync）。独立可测槽。
    void overrideEdgeSettings(dqCommon::HiddenLineSettingsProps const& props);

    // Ported from: ViewAttributes.ts:865-869 Smooth Polyface Edges 复选
    //（tileAdmin.edgeOptions.smooth + invalidateScene + sync）。独立可测槽。
    void setSmoothPolyfaceEdges(bool enabled);

    // Ported from: EnvironmentEditor.ts:258-271 updateEnvironment（M-O(4) P7
    // ——渐变字段段合并 {...current, ...newEnv} → setEnvironment + sync）。
    // 独立可测槽（QColorDialog 模态面直驱锁写通道）。
    void updateSkyEnvironment(dqCommon::SkyBoxProps const& newEnv);
    // Ported from: EnvironmentEditor.ts:296-304 resetEnvironmentEditor
    //（Environment.defaults().withDisplay({sky:true}) + sync + UI 刷新）。
    void resetEnvironment();
    // Ported from: EnvironmentEditor.ts:306-316 addEnvAttribute 的 withDisplay
    // 半边（sky/ground 显隐位）。
    void setEnvironmentDisplay(bool sky, bool enabled);

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
    // Edge Display 分区（M-O(4) P6——ViewAttributes.ts:835-1008）。
    QSlider* m_transThreshold = nullptr;
    QCheckBox* m_smoothEdges = nullptr;
    QCheckBox* m_visColorCb = nullptr;
    QPushButton* m_visColorButton = nullptr;
    QCheckBox* m_visWidthCb = nullptr;
    QSpinBox* m_visWidth = nullptr;
    QComboBox* m_visPattern = nullptr;
    QCheckBox* m_hidWidthCb = nullptr;
    QSpinBox* m_hidWidth = nullptr;
    QComboBox* m_hidPattern = nullptr;
    // Thematic Display 编辑区（M-S S-g——ThematicDisplay.ts:37-777；DTA 内序
    // 最末）。syncFromViewport 时经它回读（updateThematicDisplayUI）。
    ThematicDisplayEditor* m_thematicEditor = nullptr;
public:
    // 测试/宿主访问面（面板内控件直读防呆——锁内用）。
    ThematicDisplayEditor* thematicEditor() const noexcept { return m_thematicEditor; }
};
}  // namespace Gui
