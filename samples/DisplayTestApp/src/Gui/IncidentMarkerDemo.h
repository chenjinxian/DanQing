// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — IncidentMarkerDemo（Marker/Cluster/MarkerSet 的
// 演示：500 个随机"事故"标记 + 屏幕重叠聚类 + 悬停 tooltip + 点击消息框）
//
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/
//              IncidentMarkerDemo.ts（262 行全量——IncidentMarker :16-77 /
//              IncidentClusterMarker :80-137 / IncidentMarkerSet :140-144 /
//              IncidentMarkerDemo :150-251 / IncidentMarkerDemoTool :253-262）。
//
// EQUIVALENCE 登记（§11.10；XE 族之外的本文件面）：
//   XE2  imageElementFromUrl（IncidentMarkerDemo.ts:161-170 的 loadOne）→
//        QImageReader 同步本地读（§8.2 零网络——DTA 的 public/ 静态伺服 =
//        DanQing 的 assets/ 目录装载）。装载失败记 stderr 并跳过该图标
//        （loadOne 的 catch → Logger.logError 语义）。
//   XE9  异步装载形（:175-199 的 Promise.all + :202 的浮动 promise +
//        :219-225 的 _loading.then 完成回调）→ 图像同步解码 + QTimer::
//        singleShot(0) 延迟建标记（参考的"装载完成前 decorate 走 loading 门"
//        窗口语义保留：装载完成回调里建 500 marker + invalidateCachedDecorations
//        [:222]）。timer 的 Qt 上下文对象 = m_lifetimeToken（demo 析构即取消，
//        防悬垂回调——M-V V-e 面板教训同族）。
//   XE15 纹理上传延后至装载完成后的首个 Decorate（GL 上下文在帧内才保活——
//        TD-14 GltfDecoration 先例）；IncidentMarker 携 m_iconIndex 簿记
//        （参考在 ctor 收现成图像——DanQing 解码[CPU 同步]/上传[GPU 帧内]
//        分离的承载）。发散=装载完成帧的图像在下一帧才上屏（不可观测——
//        loading 门挡着首帧）；验证=演示锁（toggle/装载/聚类）。
//   XE16 随机源：Math.random（:191-196）→ std::mt19937（默认
//        random_device 播种 = 参考的非确定性；测试经 ctor 注入固定种子）。
//        位置公式（extents.low + rand*xLength 等）逐行 1:1。
//   XE17 参考怪癖 1:1 保留（:194）：`(i % len) + 1` 当 i%6==5 时下标 6
//        越界 = undefined → marker 不建（500 迭代 → 417 marker[83 跳过]，
//        warning 图标[0] 从不在事故上用——只服务组 marker）。C++ 越界读是
//        UB——以边界检查复现 undefined 语义（idx < size 门）。
#pragma once

#include <dqApp/Marker.h>
#include <dqApp/ToolAdmin.h>   // InteractiveTool（IncidentMarkerDemoTool 基类）+ BeButtonEvent

#include <dqGeom/Range3d.h>

#include <QImage>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

class QObject;

namespace Gui {

// ---------------------------------------------------------------------------
// IncidentMarker — Ported from: IncidentMarkerDemo.ts:16-77。
// 单个事故标记：严重度插值色 + 图标 + id 标签 + 悬停 tooltip + 点击消息框 +
// 世界装饰圆（200mm 半径，随缩放变化）。
// ---------------------------------------------------------------------------
class IncidentMarker : public dqApp::Marker {
public:
    // :28-31——绿(0) → 琥珀(15) → 红(30) 插值。
    static dqCommon::ColorDef makeColor(int severity);

    // :44-53。iconIndex = 图标槽索引（XE15 簿记——延后上传时的归属）；
    // icon 可缺席（装载失败——参考 `undefined !== img` 门外的缺失形）。
    IncidentMarker(dqGeom::Point3d const& location, int severity, int id,
                   dqApp::MarkerImage const* icon, int iconIndex);
    ~IncidentMarker() override = default;

    // :34-41——Data 键按下开消息框（severity = N）；恒 true（吞事件）。
    bool onMouseButton(dqApp::BeButtonEvent const& ev) override;

    // :64-76——super.addMarker + WorldDecoration 200mm 圆（视口旋转面内，
    // 填充 = 严重度色 withTransparency(200) + 同色描边）。
    void addMarker(dqApp::DecorateContext& context) override;

    int severity = 0;   // :44 参数属性（public severity）
    int id = 0;         // :44 参数属性（public id）
    int iconIndex = -1; // XE15 簿记（参考无此字段——延后上传的图标归属）

private:
    dqCommon::ColorDef m_color;   // :22 _color = makeColor(severity)
};

// ---------------------------------------------------------------------------
// IncidentClusterMarker — Ported from: IncidentMarkerDemo.ts:80-137。
// 聚类占位 marker：白圆 + 组严重度色描边 + 计数标签 + top-10 严重度 tooltip。
// ---------------------------------------------------------------------------
class IncidentClusterMarker : public dqApp::Marker {
public:
    // :96-136。image 可缺席（warning 图标装载失败——:134-135 的 if 门）。
    IncidentClusterMarker(dqGeom::Point3d const& location, dqGeom::Point2d const& size,
                          dqApp::Cluster<IncidentMarker>& cluster,
                          dqApp::MarkerImage const* image);

private:
    dqCommon::ColorDef m_clusterColor;   // :81 _clusterColor（XE 等价：参考存
                                         // hex 字符串经 toHexString；DanQing 直存
                                         // ColorDef——同一颜色值，省 CSS 桥）。
};

// ---------------------------------------------------------------------------
// IncidentMarkerSet — Ported from: IncidentMarkerDemo.ts:140-144。
// ---------------------------------------------------------------------------
class IncidentMarkerSet : public dqApp::MarkerSet<IncidentMarker> {
public:
    using dqApp::MarkerSet<IncidentMarker>::MarkerSet;

protected:
    // :141-143——组占位 = IncidentClusterMarker（warning 图标 + 组位置）。
    dqApp::Marker* getClusterMarker(dqApp::Cluster<IncidentMarker>& cluster) override;
};

// ---------------------------------------------------------------------------
// IncidentMarkerDemo — Ported from: IncidentMarkerDemo.ts:150-251。
// 装饰器生命周期：toggle 开（建 + addDecorator）/关（dropDecorator + 销毁）。
// ---------------------------------------------------------------------------
class IncidentMarkerDemo final : public dqApp::IDecorator {
public:
    // :155——_numMarkers = 500。
    static constexpr int kNumMarkers = 500;

    // :150-203 构造（ctor 即启装载——:202 的浮动 promise = XE9 的
    // singleShot(0) 延迟完成）。rand01 = XE16 随机源注入（nullptr →
    // random_device 播种的 mt19937，Math.random 语义）。
    IncidentMarkerDemo(dqApp::Viewport& vp, dqGeom::Range3d const& extents,
                       std::function<double()> rand01 = nullptr);
    ~IncidentMarkerDemo() override;

    // :206——useCachedDecorations = true。
    bool UseCachedDecorations() const override { return true; }
    // :209-226——isSpatialView 门 + loading 门 + incidents.addDecoration。
    void Decorate(dqApp::DecorateContext& context) override;
    bool TestDecorationHit(uint32_t) const override { return false; }
    QString GetDecorationToolTip(uint32_t) const override { return {}; }

    // :228-250 静态开关面。
    static void start(dqApp::Viewport& vp, dqGeom::Range3d const& extents);
    static void stop();
    static void toggle(dqApp::Viewport& vp, dqGeom::Range3d const& extents);
    // :156——decorator 静态（C++ 所有权 = unique_ptr 持有 + 裸指针访问面）。
    static IncidentMarkerDemo* decorator() { return s_decorator.get(); }

    // :158——warningSign = _images[0]（缺席 = 装载失败）。XE15 快照语义——
    // 纹理在装载后首帧上传，调用点（getClusterMarker）在建组时刻读现值。
    std::optional<dqApp::MarkerImage> warningSign() const;

    // 测试观察面（Authored——参考经私有字段直读）：装载完成态 + 标记数 +
    // 图标自然尺寸。随机源确定性由 ctor 注入承载（XE16）。
    bool isLoading() const { return m_loading; }
    IncidentMarkerSet const& incidents() const { return m_incidents; }
    size_t loadedImageCount() const;
    // 槽位自然尺寸（Authored——资产装载锁的观测面；未解码 = (0,0)）。
    std::pair<uint32_t, uint32_t> imageDimsForTest(size_t index) const
    {
        if (index >= m_images.size() || !m_images[index].decoded)
            return {0, 0};
        return {m_images[index].width, m_images[index].height};
    }

private:
    // 一槽图标：解码图像（CPU）+ 自然尺寸 + 帧内上传的纹理（XE15）。
    struct IconImage {
        QImage image;                          // 已解码（QImageReader）
        uint32_t width = 0;
        uint32_t height = 0;
        dqRender::rhi::TextureHandle texture;  // 无效 = 未上传
        bool decoded = false;
    };

    // :161-170——loadOne（QImageReader 同步读；失败记日志 + 缺席）。
    void loadOne(size_t index, char const* src);
    // :175-199——loadAll（六图标 + 建标记——建标记段在 XE9 延迟完成点）。
    void buildMarkers(dqGeom::Range3d const& extents);
    // XE15——首个 Decorate 帧上传全部已解码图标并把 MarkerImage 推入 marker。
    void ensureTexturesUploaded(dqApp::Viewport& vp);
    // XE9 完成回调内容（:198-199 的 _loading=undefined + :221-224 的
    // invalidateCachedDecorations/_awaiting 复位）。
    void onLoadComplete();

    IncidentMarkerSet m_incidents;            // :154
    std::array<IconImage, 6> m_images;        // :153 _images（六槽定序——
                                              // :176-183 的 loads 顺序）
    bool m_loading = true;                    // :152 _loading 在场 = 装载中
    bool m_awaiting = false;                  // :151
    dqApp::Viewport* m_awaitingVp = nullptr;  // :221 的 context.viewport 捕获
    dqGeom::Range3d m_extents;                // :201 ctor 形参（建标记域）
    std::function<double()> m_rand01;         // XE16
    std::mt19937 m_engine;                    // XE16 缺省引擎（成员——禁
                                              // shared_ptr[§9]，值持有）
    std::unique_ptr<QObject> m_lifetimeToken; // XE9 timer 上下文（析构即取消）
    dqApp::Viewport* m_vp = nullptr;          // 纹路上传视口（XE15——start 时
                                              // = m_incidents.viewport()）

    static std::unique_ptr<IncidentMarkerDemo> s_decorator;   // :156
};

// ---------------------------------------------------------------------------
// IncidentMarkerDemoTool — Ported from: IncidentMarkerDemo.ts:253-262。
// toolId "ToggleIncidentMarkers"（keyin "dta incident markers"——
// SVTTools.json:64-65）。run：选中视口 + isSpatialView 门 → toggle。
// §3.4 基类适配（W-b1 PathDecorationTestTool 先例）：参考 extends Tool；
// DanQing 注册面 ToolType=InteractiveTool*（ToolAdmin.h 注册表形）→
// InteractiveTool 承载。
// ---------------------------------------------------------------------------
class IncidentMarkerDemoTool : public dqApp::InteractiveTool {
public:
    const char* getToolId() const override { return "ToggleIncidentMarkers"; }   // :254
    std::string englishKeyin() const override { return "dta incident markers"; }
    bool run() override;   // :255-261
};

}  // namespace Gui
