// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Contour display 下拉面板
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/Contours.ts
//              ContoursSettings（:27-433）+ ContoursPanel（:435-454
//              ToolBarDropDown 包装——_open/_close 生命周期由工具栏按钮
//              handler 承载，SectionsPanel 同族）。
//
// 工具栏 item 24 Contours 下拉（Viewer.ts:411-427）。EQUIVALENCE（E4：
// DOM→Qt 控件，§11.10 登记制）：
//  - ColorInput（frontend-devtools ColorInput.ts:45-84——<input type="color">，
//    值恒 "#rrggbb" 文本）→ QLineEdit 十六进制文本框（值串契约保持：面板写
//    toHexString、读侧 ColorDef.tryComputeTbgrFromString——解析链 1:1；
//    取色弹窗系浏览器 chrome，QColorDialog 反离参考数据流且不可无头驱动）。
//  - LabeledNumericInput（NumericInput.ts:79-110——<input type=number> 值串
//    + parseAsFloat）→ QLineEdit 数值文本框（loadContourDef 写
//    QString::number、getTerrainProps 经 tryParseFloat 读——空/垃圾文本 →
//    nullopt 的参考怪癖路径保持；min 属性仅约束 stepper，直输不钳）。
//  - Slider（Slider.ts:37-，min 1.0/max 8.5/step 0.5/readout right）→
//    QSlider ×2 整数域（2..17）+ 右侧 readout QLabel；HTML range 赋值落
//    step 网格（浏览器钳制）→ qRound 钳位等价。
//  - 程序化 select.value 赋值不触发 onchange（addContourDef :297 /
//    deleteContourDef :336）→ QSignalBlocker 包裹 setCurrentIndex
//    （CameraPathsPanel populateFromPathList 同族）。
//  - ctor 尾 `checkbox.checked = ...`（:212 直赋不触发 handler）→
//    QSignalBlocker 包裹 setChecked。
//  - 参考 `_currentTerrainProps`（:34/:53-54 初始化后全程无读——死状态）
//    不移植；`assert(view.is3d())`（C++ 禁断言异常路径）→ GetView()==nullptr
//    早退（CameraPathsPanel :420-423 同族 EQUIVALENCE）。
//  - 按钮行 DOM 序在组编辑器**之前**（:203 appendChild(buttonDiv) 先于
//    :205 appendChild(_curGroupDiv)——参考实际视觉序）——Qt 布局同序。
#pragma once

#include <dqCommon/ContourDisplay.h>

#include <QWidget>

#include <optional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;

namespace dqApp {
class Viewport;
}

namespace Gui {

// ContoursPanel — ContoursSettings 的 Qt 承载（Contours.ts:27-433）。
// 写回链经 A 道 dqApp DisplayStyle 门面 setContours（X-a1——equals 短路 →
// 先 raise OnContoursChanged → 赋值，DisplayStyleSettings.ts:1231-1239）。
class ContoursPanel final : public QWidget {
    Q_OBJECT
public:
    // ContoursPanel ctor（:440-445 open() 即建 ContoursSettings）。
    ContoursPanel(dqApp::Viewport* vp, QWidget* parent);

    // --- 测试缝（CameraPathsPanel/TimelinePanel 同族直驱面——参考 private） ---
    void loadContourDef(int index);   // :253-279
    void addContourDef();             // :281-300
    void applyContourDef();           // :302-317
    void deleteContourDef();          // :319-340
    int currentContourIndex() const noexcept { return m_currentContourIndex; }  // :35

private:
    dqCommon::ContourDisplayProps getContourDisplayProps() const;      // :219-222
    dqCommon::ContourGroupProps getTerrainProps() const;               // :229-251
    static std::optional<double> tryParseFloat(QString const& value);  // :224-227
    void toggleCurrentGroup(bool enable);                              // :48-50
    void sync();                                                       // :430-432

    dqApp::Viewport* m_vp;
    QWidget* m_curGroupDiv = nullptr;        // :31 _curGroupDiv
    QLineEdit* m_subCatTextBox = nullptr;    // :32 _subCatTextBox
    QLineEdit* m_nameTextBox = nullptr;      // :33 _nameTextBox
    // :34 _currentTerrainProps——死状态不移植（头注 EQUIVALENCE）
    int m_currentContourIndex = 0;           // :35 _currentContourIndex
    QLineEdit* m_minorInterval = nullptr;    // :36 _minorInterval
    QLineEdit* m_majorIntervalCount = nullptr;  // :37 _majorIntervalCount
    QComboBox* m_minorLineStyle = nullptr;   // :38 _minorLineStyle
    QComboBox* m_majorLineStyle = nullptr;   // :39 _majorLineStyle
    QLineEdit* m_minorColor = nullptr;       // :40 _minorColor
    QLineEdit* m_majorColor = nullptr;       // :41 _majorColor
    QSlider* m_minorWidth = nullptr;         // :42 _minorWidth
    QSlider* m_majorWidth = nullptr;         // :43 _majorWidth
    QLabel* m_majorWidthReadoutPlaceholder = nullptr;  // Slider readout（"right"）
    QLabel* m_minorWidthReadoutPlaceholder = nullptr;  // Slider readout（"right"）
    QCheckBox* m_dispElemCkbx = nullptr;     // :44 _dispElemCkbx
    QCheckBox* m_checkbox = nullptr;         // :45 _checkbox
    QComboBox* m_combobox = nullptr;         // :46 _combobox
};

}  // namespace Gui
