// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — 第二 iModel 瓦叠加（TiledGraphics proof-of-concept）
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/TiledGraphics.ts
//              （Reference :13-39 / Provider :41-79 /
//               computeTransformFromSecondaryIModel :81-93 /
//               toggleExternalTiledGraphicsProvider :95-121 /
//               ToggleSecondaryIModelTool :123-135）
//
// EQUIVALENCE（§11.10）：
//   - 取数面：参考源 = BriefcaseConnection.openFile（:113）后两连接各自后端
//     取数；发散 = DanQing §8.2 单 TileAdmin fetcher——第二包瓦键经
//     DumpTileFetcher::addFallbackRoot 追加根进入同一查找域（DumpTileFetcher.h
//     同源登记）；验证法 = TiledGraphicsTest 多根命中锁（两包键各自 hitRoot）。
//   - per-ref symbology overrides（:30 getSymbologyOverrides →
//     `new FeatureSymbology.Overrides(view)` :57——参考用它保证第二 iModel 的
//     全类别显示）；发散 = DanQing TileTreeReference 无 per-ref overrides 面
//     （TileDrawArgs 不携 overrides——批次 overrides 是 per-viewport 面
//     [M-O(2) I10]），而 DanQing 缺省**无 category 隐藏面**（不可见集恒空——
//     M-N(1) 面），第二连接的特征以自然 symbology 显示 = 参考 ovrs 的效果面
//     等价；验证法 = I11 像素锁（第二包内容上屏）。
//   - ecef 对齐（:85-92）：两包 ecefLocation 均有效才走 ECEF 复合；rpc-dumps
//     各包数据面缺席（DumpIModelConnection.hasEcefLocation 恒 false）——恒等
//     回退分支即数据面实态（EcefLocation 类型随 geolocated 数据面落地）。
//   - getTransformFromIModel（:36-38）：DanQing TileTreeReference 无该面
//     （toViewport 恒等变换下无消费者）——登记。
//   - 文件选择（:107 selectFileName）：无参 keyin 走 QFileDialog；keyin 可携
//     包路径参（测试/确定性驱动面——参考无参）。
#pragma once

#include "../DumpOpenHelper.h"  // dta::DumpOpenPackage/打开产物（src/ 相对）

#include <dqApp/ToolAdmin.h>
#include <dqApp/tile/TiledGraphicsProvider.h>
#include <dqApp/tile/TileTreeReference.h>

#include <dqGeom/Transform.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dqApp {
class IModelConnection;
class Viewport;
}
namespace dqRender {
class ImdlTileTree;
}

namespace Gui {

// Ported from: computeTransformFromSecondaryIModel（TiledGraphics.ts:81-93——
// secondary.dbToEcef ∘ tileTreeToWorld ∘ primary.ecef⁻¹；任一 ecef 无效 →
// tileTreeToWorld 原样[恒等回退]）。
dqGeom::Transform computeTransformFromSecondaryIModel(
    dqApp::IModelConnection const& primary,
    dqApp::IModelConnection const& secondary,
    dqGeom::Transform const& tileTreeToWorld);

// Ported from: TiledGraphics.ts Reference（:13-39——内层 ref 的委托包装 +
// computeTransform 走 Provider[缓存] + per-ref ovrs 面[见文件头 EQUIVALENCE]）。
class TiledGraphicsReference final : public dqApp::TileTreeReference {
public:
    TiledGraphicsReference(dqApp::TileTreeReference& inner,
                           dqGeom::Transform const& transform)
        : m_inner(inner)
        , m_transform(transform)
    {}

    dqApp::TileTreeOwner& getTreeOwner() override { return m_inner.getTreeOwner(); }
    void resetTreeOwner() override { m_inner.resetTreeOwner(); }
    // :32-34 computeTransform → Provider.computeTransform（缓存于构造）。
    dqGeom::Transform computeTransform(dqRender::TileTree& tree) override
    {
        (void)tree;
        return m_transform;
    }

private:
    dqApp::TileTreeReference& m_inner;  // 非拥有（attachment 持有）
    dqGeom::Transform m_transform;
};

// 叠加产物生命周期（ownership：trees/refs/provider 堆稳定——DumpOpenResult
// 同款纪律，DumpOpenHelper.h 注释）。
struct TiledGraphicsAttachment {
    dqBase::RefPtr<dqApp::DumpIModelConnection> connection;
    std::optional<dqApp::DumpTileTreeProps> props;
    std::vector<std::unique_ptr<dqRender::ImdlTileTree>> trees;
    std::vector<std::unique_ptr<dqApp::TileTreeReference>> innerRefs;  // Simple 引用
    std::vector<std::unique_ptr<TiledGraphicsReference>> refs;
    std::unique_ptr<dqApp::TiledGraphicsProvider> provider;
    std::vector<std::string> treeLoadLog;
};

// Ported from: TiledGraphics.ts Provider（:41-79——forEachTileTreeRef 逐 ref；
// create = 打开第二连接 + 建 view 的树 refs[DanQing：宿主打开链 ④ 同款
// per-model 树装配]）。失败（imodel.json/manifest/树 props 不可达）→ nullptr。
std::unique_ptr<TiledGraphicsAttachment> createSecondaryIModelAttachment(
    dta::DumpOpenPackage const& pkg, dqApp::Viewport* vp);

// Ported from: toggleExternalTiledGraphicsProvider（:95-121——per-viewport
// 注册表；toggle：在 → drop+关连接；不在 → 建并 add）。pkg 非空 = 直接指定
// 包（测试面）；空 = QFileDialog 选 imodel.json（:107 selectFileName 等价）。
// 返回 toggle 后状态（true = 已挂载）。
bool toggleExternalTiledGraphicsProvider(dqApp::Viewport* vp,
                                         dta::DumpOpenPackage const* pkg);

// 测试/断言面：per-viewport 当前挂载（无 → nullptr）。
TiledGraphicsAttachment* findTiledGraphicsAttachment(dqApp::Viewport* vp);

// Ported from: ToggleSecondaryIModelTool（:123-135——toolId
// "ToggleSecondaryIModel"；run = selectedView → toggle）。keyin
// "dta tiled graphics"（可选参 = 包根路径——文件选择面的确定性等价）。
class ToggleSecondaryIModelTool final : public dqApp::InteractiveTool {
public:
    const char* getToolId() const override { return "ToggleSecondaryIModel"; }
    int minArgs() const override { return 0; }
    int maxArgs() const override { return 1; }
    std::string englishKeyin() const override { return "dta tiled graphics"; }
    bool run() override;
    bool parseAndRun(std::vector<std::string> const& args) override;

    std::string m_pkgRoot;  // ← 可选参（imodel.json 所在目录）
};

}  // namespace Gui
