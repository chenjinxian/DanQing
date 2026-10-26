// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — text decoration tool (DTA frontend host face)
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/TextDecoration.ts
//              + core/backend TextBlockLayout 的注入缝消费形。
//
// 字形度量缝（ComputeRangesForTextLayout）的宿主实现 = QRawFont/QFontMetricsF
//（参考缺省 = IModelJsNative DgnDb.computeRangesForText——imodel-native 字形
// 引擎；EQUIVALENCE：度量源为宿主系统字体 vs 内嵌 DgnFont——逐字宽差在字体
// 差异域内，布局逻辑面经 TextBlockLayoutTest 直移锁独立验证）。
// 字形显示 = QPainterPath 轮廓 → addLineString（参考 DgnFont 填充多边形；
// 轮廓形视觉等价——填充面 TODO 随 dqRender TextString 消费落地）。
#pragma once

#include "dqApp/Decorator.h"
#include "dqApp/ToolAdmin.h"
#include "dqApp/Viewport.h"

#include <dqCommon/GeometryStreamBuilder.h>

#include <dqCommon/TextAnnotation.h>
#include <dqCommon/TextAnnotationGeometry.h>
#include <dqCommon/TextBlockLayout.h>

#include <QFont>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class QRawFont;

namespace dta {

// QRawFont 度量的 ComputeRangesForTextLayout 实现（每字体名缓存 QRawFont）。
// layout = [0, 0, advance*widthFactor, textHeight]；justification = 扣尾随空白
// advance（参考 computeRangesForText 语义——AnnotationTestUtils 桩的字符数
// 度量为测试形，此处为宿主真形）。
dqCommon::TextLayoutRanges computeRangesForTextLayoutQt(
    dqCommon::ComputeRangesForTextLayoutArgs const& args);

// The text editor decorator (DTA TextEditor 的 DanQing 宿主形——TextBlock 编辑
// 状态 + update() 出图形 + decorate 挂 Scene）。样式持久化（insertstyle 等键）
// 属➖9 编辑写入族结案——applystyle 本地承载（FindTextStyle 返回 defaults）。
class TextAnnotationHost final : public dqApp::IDecorator {
public:
    // --- TextEditor 状态面（:47-70 同名） ---
    dqCommon::TextAnnotationProps::Anchor anchor{
        dqCommon::AnchorHorizontal::Left, dqCommon::AnchorVertical::Top};
    dqGeom::Point3d origin;
    double rotation = 0.0;  // degrees（yaw）
    double offsetX = 0.0, offsetY = 0.0;
    std::vector<dqCommon::TextAnnotationLeader> leaders;
    bool debugAnchorPointAndRange = false;
    dqCommon::TextStyleSettingsProps runStyle;   // 下一 run 的样式（font.name 默认 Arial）
    std::string baselineShift = dqCommon::BaselineShift::None;
    std::unique_ptr<dqCommon::TextBlock> textBlock;

    TextAnnotationHost();

    // Ported from: TextEditor.init/clear (:137-155)
    void init(dqApp::Viewport& vp);
    void clear();

    // --- 编辑命令（:157-258 同名面） ---
    void appendText(std::string const& content);
    void appendFraction(std::string const& numerator, std::string const& denominator);
    void appendTab(std::optional<double> spaces);
    void appendBreak();
    void appendParagraph();
    void setDocumentWidth(double width);
    void setIndentation(double indentation);
    void setMargins(dqCommon::TextBlockMargins const& margins);
    void setLeaderProps();

    // 注解 props（:86-100 annotationProps）。
    dqCommon::TextAnnotationProps annotationProps() const;

    // Ported from: TextEditor.update (:263-292) — layout + geometry + graphic。
    bool update(dqApp::Viewport& vp);

    bool isInitialized() const noexcept { return m_vp != nullptr; }

    // IDecorator（:295-299 useCachedDecorations/decorate；hit/tooltip 面 = 参考
    // 无拾取装饰——AcsTriadDecorator 同形 false/空）。
    void Decorate(dqApp::DecorateContext& context) override;
    bool UseCachedDecorations() const override { return true; }  // :295 `get useCachedDecorations(): true`
    bool TestDecorationHit(uint32_t) const override { return false; }
    QString GetDecorationToolTip(uint32_t) const override { return {}; }

private:
    dqApp::Viewport* m_vp = nullptr;
    dqCommon::GeometryStreamBuilder m_stream;  // update() 产物（装饰图形源）
    // 非持有（UseCachedDecorations=true：DecorationsCache 持图形并在条目删除时
    // delete——参考 GraphicOwner 的 TS GC 所有权面；update/clear 经
    // InvalidateCachedDecorations 释放旧图形后再换/清指针）。
    dqRender::RenderGraphic* m_graphic = nullptr;
    std::unique_ptr<dqCommon::TextStyleSettingsProps> m_applyStyleOverrides;
};

// Keyin 命令面（"dta text <cmd> ..."——TextDecorationTool :321-627 的 DanQing
// 面；insert/insertstyle/updatestyle/deletestyle = ➖9 编辑写入族结案，缺席登记）。
bool runTextDecorationCommand(dqApp::Viewport& vp, TextAnnotationHost& editor,
                              std::vector<std::string> const& inArgs,
                              std::string* outError = nullptr);

// 参考 module 级单例 `const editor = new TextEditor()`（:320——TS 模块态的
// C++ 函数局部静态承载；Tool 实例每 keyin 新建，编辑态必须跨 keyin 存活）。
TextAnnotationHost& globalTextEditor();

// The keyin tool（:321-334——toolId AddTextDecoration / minArgs 1 / maxArgs
// unbounded；parseAndRun → runTextDecorationCommand）。
class TextDecorationTool final : public dqApp::InteractiveTool
{
public:
    const char* getToolId() const override { return "AddTextDecoration"; }
    int minArgs() const override { return 1; }
    int maxArgs() const override { return -1; }  // unbounded
    std::string englishKeyin() const override { return "dta text"; }

    bool parseAndRun(std::vector<std::string> const& args) override;
};

}  // namespace dta
