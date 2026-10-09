// ThematicDisplayEditor — Thematic Display 编辑区（View Settings 面板末节，
// DTA 内序最末——ViewAttributes.ts:327 addThematicDisplay）。
// Ported from: itwinjs-core display-test-app ThematicDisplay.ts:37-777
// （ThematicDisplayEditor 全量——13 控件组 + 写回环 updateThematicDisplay
//  :758-763[toJSON→修改→fromJSON→setter→sync] + UI 回读 updateThematicDisplayUI
//  :688-752 + Reset :767-772）。
#pragma once

#include <QWidget>

#include <functional>
#include <vector>

#include <dqCommon/ThematicDisplay.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>  // Range1d 寓居此头（dqGeom 无 Range1d.h）

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSlider;
class QSpinBox;

namespace Gui {

// Thematic Display 编辑区。控件 1:1 对齐参考；Qt 承载映射：ComboBox→QComboBox
//（value 存 itemData）、LabeledNumericInput→QLabel+QDoubleSpinBox（Step Count=
// QSpinBox）、Slider→QSlider+读数 QLabel、CheckBox→QCheckBox、Button→
// QPushButton。写通道全部经 updateThematicDisplay 单环（参考同形）→
// DisplayStyle::setThematic 门面（S-f：equals 短路+OnThematicChanged）+
// vp->synchWithView()（参考 sync() :773-776）。
class ThematicDisplayEditor : public QWidget {
    Q_OBJECT
public:
    explicit ThematicDisplayEditor(QWidget* parent = nullptr);

    // UI 回读（参考 updateThematicDisplayUI :688-752——自活动视口读设置刷新
    // 全部控件；range null 时按 projectExtents 默认域显示[:695-698]）。
    void updateThematicDisplayUI();
    // 开关态回读（参考 _update 闭包 :658-662——checkbox=vf.thematicDisplay +
    // showHideControls）。宿主面板 syncFromViewport 时调用。
    void syncEnabledState(bool enabled);

    // ── 写通道（测试直驱面——参考 handler 体逐条）──────────────────
    // 开关（:180-208——首启副作用：defaultSettings 域/截断/4 传感器 +
    // vf.thematicDisplay + 显隐 + sync）。
    void enableThematicDisplay(bool enabled);
    // Display Mode（:230-246——Slope→range [0,90]；离 Slope→默认域回填）。
    void setDisplayMode(int mode);
    void setGradientMode(int mode);          // :248-256
    void setStepCount(int count);            // :260-274
    void setColorScheme(int scheme);         // :304-323（含 Custom 三伪项键值表）
    void setMultiplyAlpha(bool on);          // :325-332（transparencyMode 两态）
    void setRangeHigh(double v);             // :337-351
    void setRangeLow(double v);              // :353-367
    void setAxisComponent(int axis, double v);   // :369-420（XYZ 三输入同体）
    void setColorMix(double v);              // :432-441
    void setSunDirComponent(int axis, double v); // :445-501（XYZ 三输入同体）
    void setDistanceCutoff(double v);        // :503-515
    // 传感器编辑（:517-614——选中项的 X/Y/Z/Value）。
    void selectSensor(int index);
    void setSensorComponent(int axis, double v);
    void setSensorValue(double v);
    void addSensor();                        // :619-632
    void deleteSensor();                     // :634-649（>1 门）
    void createSensorGrid();                 // :651-665（32×32 网格 :38-65）
    void resetThematicDisplay();             // :767-772

    // 测试读面。
    int sensorCount() const;
    dqCommon::ThematicDisplaySensor sensorAt(int index) const;
    int selectedSensor() const;

private:
    // 写回环（:758-763 1:1——mod 改 props → fromJSON → setThematic → sync）。
    void updateThematicDisplay(std::function<void(dqCommon::ThematicDisplayProps&)> mod);
    // 传感器条目重建（:92-97 _resetSensorEntries——"Sensor N" 命名 :99-102）。
    void resetSensorEntries(int count);
    // :67-83 _pushNewSensor（无参臂=中点 0.5；带参臂直推——默认域边注：参考
    // 在 `_pushNewSensor` 内顺带 updateDefaultRange）。
    void pushNewSensor(std::vector<dqCommon::ThematicDisplaySensorProps>& sensors,
                       dqCommon::ThematicDisplaySensorProps const* sensorProps);
    // :38-65 _createSensorGrid（17 值循环原样）。
    void createSensorGridImpl(std::vector<dqCommon::ThematicDisplaySensorProps>& sensors);

    QCheckBox* m_checkbox = nullptr;
    QWidget* m_controls = nullptr;      // thematicControlsDiv 承载
    QComboBox* m_displayMode = nullptr;
    QComboBox* m_gradientMode = nullptr;
    QSpinBox* m_stepCount = nullptr;
    QComboBox* m_colorScheme = nullptr;
    QCheckBox* m_alpha = nullptr;
    QDoubleSpinBox* m_rangeHigh = nullptr;
    QDoubleSpinBox* m_rangeLow = nullptr;
    QDoubleSpinBox* m_axisX = nullptr;
    QDoubleSpinBox* m_axisY = nullptr;
    QDoubleSpinBox* m_axisZ = nullptr;
    QSlider* m_colorMix = nullptr;
    QLabel* m_colorMixReadout = nullptr;
    QDoubleSpinBox* m_sunDirX = nullptr;
    QDoubleSpinBox* m_sunDirY = nullptr;
    QDoubleSpinBox* m_sunDirZ = nullptr;
    QDoubleSpinBox* m_distanceCutoff = nullptr;
    QComboBox* m_sensor = nullptr;
    QDoubleSpinBox* m_sensorX = nullptr;
    QDoubleSpinBox* m_sensorY = nullptr;
    QDoubleSpinBox* m_sensorZ = nullptr;
    QDoubleSpinBox* m_sensorValue = nullptr;
};

}  // namespace Gui
