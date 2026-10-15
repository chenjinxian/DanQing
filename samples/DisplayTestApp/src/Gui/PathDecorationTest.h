// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Path decoration test/demo（小路径装饰 + canvas 文本）
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/PathDecorationTest.ts
//              (PathCanvasDecoration :10-18 + PathDecorationTest :22-72 +
//              PathDecorationTestTool :74-82 + _getPath :84-109)。
//
// EQUIVALENCE（§11.10）：
//   - E4 拾取 id：参考 `context.viewport.iModel.transientIds.getNext()`
//     （PathDecorationTest.ts:39）——DanQing IModelConnection 无 transientIds
//     面；本地静态计数器承载（ViewClipDecoration.cpp:150 s_nextTransientId
//     先例同型）。发散 = 计数器按装饰族各自维护（ViewClip 与本件同基址
//     0xFF000000 起——跨族 id 可相撞；参考 transientIds 为 iModel 全局单调）。
//     当前无实害：拾取 id 不进 pick 缓冲（见下条），仅 TestDecorationHit
//     对账面消费。验证法 = PathDecorationTestLock.DecorationHitMatchesPickId。
//   - E4b 拾取 id 进 builder：参考 createGraphicBuilder(type, undefined, id)
//     第三参 = GraphicBuilderOptions.pickable{id}（ViewContext.ts:205-207）——
//     DanQing GraphicBuilderOptions 无 pickable 载体（未移植——GraphicBuilder.ts
//     createGraphicBuilder id 形参面，TODO）；装饰图形不带 id 入 pick 缓冲，
//     TestDecorationHit 仅经直接 id 对账可达（语义保留、拾取缓冲接线缺席）。
//     验证法 = PathDecorationTestLock.DecorationHitMatchesPickId（对账面）。
//   - E7 canvas 文本：参考 PathCanvasDecoration.drawDecoration 以 14px
//     sans-serif 白字居中 fillText("Path Decoration Test Tool Active",256,256)
//     （:11-17）——DanQing CanvasContext 无 text 族面（fillText/font/textAlign
//     缺席）；MeasureLabel 先例（dqApp/src/MeasureTool.cpp:105-133 M-O(3) P2
//     登记同型）以文本 bbox 视觉占位承载：白色填充框居中 (256,256)，宽 =
//     文本长 × 14 × 0.6、高 = 14。发散 = 文本栅格化缺席（框占位）；验证法 =
//     PathDecorationTestLock.CanvasDecorationDrawsPlaceholderAt256（mock
//     CanvasContext 调用序）+ PathDecorationPixel 真窗锁（主会话收口跑）。
#pragma once

#include <dqApp/Decorator.h>
#include <dqApp/ToolAdmin.h>

#include <dqGeom/Path.h>
#include <dqGeom/Range3d.h>

#include <dqRender/CanvasDecoration.h>

#include <cstdint>
#include <optional>

namespace Gui {

// ---------------------------------------------------------------------------
// PathDecorationTest — 小路径装饰器（参考 :22-72）。
// 静态 decorator 指针语义 1:1（:23——"static variable so we can tell if the
// test is active"）；C++ 显式所有权：Start new / Stop delete（ViewManager
// AddDecorator/DropDecorator 非拥有——CesiumDecorator.cpp:37-49 先例）。
// ---------------------------------------------------------------------------
class PathDecorationTest final : public dqApp::IDecorator {
public:
    // Ported from: PathDecorationTest.decorator (:23)。
    static PathDecorationTest* s_decorator;
    // Ported from: PathDecorationTest.canvasDecoration (:24)——PathCanvasDecoration
    // (:10-18) 的 DanQing 结构体承载（drawDecoration lambda，E7 见文件头）。
    static dqRender::CanvasDecoration s_canvasDecoration;

    // Ported from: constructor(extents) (:29-31)。
    explicit PathDecorationTest(dqGeom::Range3d const& extents);

    // Ported from: useCachedDecorations = true (:34)。
    bool UseCachedDecorations() const override { return true; }

    // Ported from: decorate (:37-44)。
    void Decorate(dqApp::DecorateContext& context) override;

    // Ported from: testDecorationHit (:47——id === _pickId；undefined 守卫 =
    // optional 缺席恒假)。
    bool TestDecorationHit(uint32_t featureId) const override;

    // 参考无 tooltip 面（PathDecorationTest 未实现 getDecorationToolTip）；
    // IDecorator 纯虚要求 → 空串（CesiumDecorator.h:49-52 先例）。
    QString GetDecorationToolTip(uint32_t /*featureId*/) const override { return {}; }

    // Ported from: toggle (:66-71)。
    static void Toggle(dqGeom::Range3d const& extents);

    // --- 锁面（测试可达；参考私有成员的显式读口） ---
    dqGeom::PathPtr const& path() const noexcept { return m_path; }
    std::optional<uint32_t> const& pickId() const noexcept { return m_pickId; }
    // Test seam：参考 _pickId 私有且仅 decorate 内惰性赋值（:38-39）——锁
    // 直接驱动同一赋值行（不动引用语义）。
    void AssignPickIdForTest() { ensurePickId(); }

private:
    // Ported from: start (:53-56)。
    static void Start(dqGeom::Range3d const& extents);
    // Ported from: stop (:59-63)。
    static void Stop();

    // :38-39 的惰性赋值行（Decorate 与测试缝共用）。
    void ensurePickId();

    dqGeom::PathPtr m_path;                  // ← _path (:26)
    std::optional<uint32_t> m_pickId;        // ← _pickId?: string (:27)
};

// ---------------------------------------------------------------------------
// PathDecorationTestTool — keyin 工具（参考 :74-82；minArgs/maxArgs 未覆写
// = 参考 Tool 默认 0/0）。
// ---------------------------------------------------------------------------
class PathDecorationTestTool final : public dqApp::InteractiveTool {
public:
    // Ported from: PathDecorationTestTool.toolId (:75)。
    const char* getToolId() const override { return "TogglePathDecoration"; }
    // keyin = SVTTools.json tools.TogglePathDecoration.keyin（:67-68）。
    std::string englishKeyin() const override { return "dta path decoration"; }

    // Ported from: PathDecorationTestTool.run (:76-81——selectedView 缺席也
    // 恒 true)。
    bool run() override;
};

// Ported from: _getPath (:84-109)——TS 模块私有函数（_ 前缀）的测试可达承载
// （Gui 命名空间自由函数；Range1d(0,0.2) × 1000 迭代线段串+弧交替）。
dqGeom::PathPtr getPath(dqGeom::Range3d const& extents);

}  // namespace Gui
