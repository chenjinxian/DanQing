// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — AnalysisStyleExample（分析样式演示：位移/scalar 通道）
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/AnalysisStyleExample.ts
//
// 面：populateAnalysisStyles（:26-64）+ createCantilever（:66-77）+ createFlatMesh
// WithWaves（:85-177）+ AnalysisDecorator（:187-240）+ openAnalysisStyleExample
// （:242-299 的等价入口——主会话挂 appToolBar）。
//
// EQUIVALENCE 登记（§11.10）：
// - E2：mesh/style 双 DOM select → Qt QComboBox（控件形态差异；交互语义 1:1——
//   切换 dispose 重建 + 样式置位）。
// - E4：transientIds.getNext 拾取 id → DanQing GraphicBuilderOptions 无 pickable
//   接线（AcsTriadDecorator 同族登记在案）——本件不注册 pick id（testDecorationHit
//   恒 false），视觉等价；转正随 AccuDraw/ElementLocate 的 transientIds 面。
// - Cantilever 资产：samples/DisplayTestApp/assets/cantilever.imjs（E5——自
//   Cantilever.ts 提取的 JSON 本体；参考经 import + JSON.parse，DanQing 经
//   本地文件读取 + imodeljson::parseJsonDocument）。
#pragma once

#include <dqGeom/IndexedPolyface.h>

#include <memory>
#include <string>
#include <vector>

class QWidget;

namespace dqApp { class Viewport; }
namespace dqApp { class IDecorator; }
namespace dqCommon { class AnalysisStyle; }

namespace dta {

// AnalysisMeshType（:16）+ AnalysisMesh（:18-22）。
struct AnalysisMesh {
    std::string type;  // "Cantilever" | "Flat with waves"
    dqBase::RefPtr<dqGeom::IndexedPolyface> polyface;
    // 样式表：名 → AnalysisStyle（"None" → nullopt 首位——:31）。
    std::vector<std::pair<std::string, std::optional<dqCommon::AnalysisStyle>>> styles;
};

// Ported from: AnalysisStyleExample.ts populateAnalysisStyles（:26-64——逐 scalar
// 通道产 style；"Height" 结尾者 SteppedWithDelimiter+Custom seaMountain 五色键 :24；
// 配套 Vector 通道在场 → displacement 并入名）。
void populateAnalysisStyles(AnalysisMesh& mesh, double displacementScale);

// Ported from: AnalysisStyleExample.ts createCantilever（:66-77——IModelJson.Reader
// .parse + tryTransformInPlace(scale 30)）。资产经 DANQING_DTA_ASSETS_DIR 定位
// （E5）；装载失败 → nullptr。
dqBase::RefPtr<dqGeom::IndexedPolyface> createCantilever();

// Ported from: AnalysisStyleExample.ts createFlatMeshWithWaves（:85-177——100×100
// 网格 + 六通道[Static×3/Animated Radial×3/Linear×3]；auxData indices=pointIndex）。
dqBase::RefPtr<dqGeom::IndexedPolyface> createFlatMeshWithWaves();

// Ported from: AnalysisStyleExample.ts openAnalysisStyleExample（:242-299）的等价入口。
// 双 QComboBox 以 toolbarParent 为父（E2——参考 appendChild 语义）；Iso 旋转 +
// zoomToVolume(projectExtents) + SolidFill + 双色天空盒。
void openAnalysisStyleExample(dqApp::Viewport& viewport, QWidget* toolbarParent);

// AnalysisDecorator 的测试缝（AnalysisStyleExample.ts:187-240 的类——
// W-a6 E2E 经本缝注册装饰器而不开 UI 入口）。调用方持有（析构即
// DropDecorator + dispose）。
dqApp::IDecorator* createAnalysisStyleDecorator(dqApp::Viewport& viewport,
                                                AnalysisMesh const& mesh);

}  // namespace dta
