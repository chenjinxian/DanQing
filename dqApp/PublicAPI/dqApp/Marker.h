// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Marker / Cluster / MarkerSet (screen-space decorations that
// follow world locations, with view-overlap clustering)
//
// Ported from: itwinjs-core core/frontend/src/Marker.ts (494 行全量——Marker
//              :71-318 / Cluster :325-350 / MarkerSet :357-494 / getMinScaleViewW
//              自由函数 :46-63)。
//
// 画布承载裁决（M-X §0.2 裁决 A——XE1）：Marker 即画布装饰（参考
// `Marker implements CanvasDecoration`，Marker.ts:71）；DOM div 仅是 itwinjs
// 的画布宿主。DanQing 的 CanvasDecoration = dqRender 的 std::function 结构体
// （dqRender/CanvasDecoration.h——W4 面：position/drawDecoration/pick 三字段），
// Marker 经 addMarker 产出该结构条目（draw/pick 具名函子捕获 this），绘入
// GLCanvasContext 2D overlay 层（DecorateContext::AddCanvasDecoration 通路——
// WindowAreaTool/MeasureLabel 先例）。
//
// EQUIVALENCE 登记（§11.10；计划文档 §2 的 XE 族）：
//   XE1  画布承载：DOM div → canvas decoration 层（见上）。发散=DOM 事件模型
//        差异；验证=MarkerTest 绘制序/pick/聚类锁 + 真窗像素锁（主会话收口）。
//   XE6  htmlElement（Marker.ts:121）与 positionHtml（:289-297）不移植——DOM
//        覆盖面在 DanQing 无对应层；title 的 HTMLElement 形态（:110）同归——
//        title 走 XE3 纯文本。验证=tooltip 文本锁。
//   XE7  CanvasDecoration 结构体无 handler 字段（onMouseEnter/Leave/Move/
//        onMouseButton/onWheel/decorationCursor——CanvasDecoration.ts:38-61；
//        W4 TODO 的落地点在 dqRender 头，本里程碑 B 道边界外）→ handler 族移植
//        在 Marker 本体，路由恢复经具名函子 MarkerDrawFunctor/MarkerPickFunctor
//        + std::function::target<>（Marker::fromDecoration）。decorationCursor
//        可选成员不移植（Marker 从不设置——悬停光标恒 "pointer"，参考同效）。
//        验证=ToolAdmin 路由锁（enter/leave/吞事件/光标）。
//   XE8  CanvasContext 无 text/scale/shadow 面（GLCanvasContext 子集）：
//        drawDecoration 的 label 文本腿（:216-222）不栅格化（MeasureLabel 同款
//        登记——dqApp/src/MeasureTool.cpp:108-113）；drawHilited 的
//        shadowBlur/shadowColor/ctx.scale（:188-190）与 drawDecoration 的
//        scaleFactor ctx.scale（:199-200）无对应面——m_isHilited/m_scaleFactor
//        状态与 setPosition 的 rect.scaleAboutCenter（拾取域缩放）按参考移植，
//        绘制期缩放/发光为视觉缺口。发散=标签文字不可见、悬停无发光、透视下
//        marker 绘制尺寸不随距离变（拾取域正确缩放）；验证=drawDecoration 序锁
//        + setPosition 缩放锁。
//   XE10 MarkerImage（Marker.ts:26——HTMLImageElement|HTMLCanvasElement|...）=
//        rhi::TextureHandle + 自然尺寸（纹理经 Viewport::createTexture 上传；
//        canvas drawImage 通路已被 Sprites 验证——dqApp/src/Sprites.cpp:85）。
//   XE11 title HTML 标记→纯文本（`<br>`→换行，XE3——demo 侧赋值点承载转换，
//        Marker 透传）；tooltipOptions 字段移植（NotificationManager.ts:115-118
//        的 duration/placement），宿主 QToolTip 不消费（登记）。
//   XE14 Marker 析构 → ToolAdmin::clearCanvasDecorationIfMarker + Viewport::
//        RemoveCanvasDecorationsForMarker 生命周期缝（JS GC vs C++ 显式所有权
//        ——参考悬停指针随 GC 安全；TD-22 同族）。修复轮 1 扩维：视口清扫 =
//        活列表 + DecorationsCache 的 Canvas 缓存条目双扫（cacheable
//        decorator 存活期 erase marker → 缓存命中重放复活死指针的 UAF 路径
//        封堵；同点同纹）。验证=析构清悬停锁 + 缓存清扫锁。
#pragma once

#include "DecorateContext.h"  // Marker::addMarker/addDecoration + MarkerSet::addDecoration
#include "Export.h"
#include "Viewport.h"         // setPosition 的 worldToView4d/viewRect/isCameraOn
#include "ViewingSpace.h"     // ViewRect（Marker.rect——Marker.ts:88）

#include <dqCommon/ColorDef.h>
#include <dqCommon/Hilite.h>

#include <dqGeom/Matrix4d.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>  // Range1d（setScaleFactor——Marker.ts:155）

#include <dqRender/CanvasDecoration.h>
#include <dqRender/rhi/Handle.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dqApp {

class Marker;
template<typename T> class MarkerSet;
struct BeButtonEvent;

// ---------------------------------------------------------------------------
// MarkerImage — 参考 MarkerImage 联合类型（Marker.ts:26）的 DanQing 承载
// （XE10：GPU 纹理句柄 + 自然尺寸；HTMLImageElement.naturalWidth/Height 对应物）。
// ---------------------------------------------------------------------------
struct MarkerImage {
    dqRender::rhi::TextureHandle texture;   // 无效句柄 = 未上传（绘制腿安全跳过）
    uint32_t width = 0;                     // 自然宽（px）
    uint32_t height = 0;                    // 自然高（px）
};

// ---------------------------------------------------------------------------
// ToolTipOptions — Ported from: itwinjs-core NotificationManager.ts:115-118
// （duration?: BeDuration[秒] / placement?: string）。XE11：字段移植；宿主
// QToolTip 不消费（M-O(1) tooltip 链无 duration/placement 面）。
// ---------------------------------------------------------------------------
struct ToolTipOptions {
    std::optional<double> durationSeconds;  // BeDuration（秒）承载
    std::string placement;
};

// ---------------------------------------------------------------------------
// MarkerTextAlign / MarkerTextBaseline — Ported from: Marker.ts:38/:44。
// 字段面移植（label 文本腿 XE8 登记未栅格化——样式字段随数据面落地）。
// ---------------------------------------------------------------------------
enum class MarkerTextAlign : uint8_t { Left, Right, Center, Start, End };
enum class MarkerTextBaseline : uint8_t { Top, Hanging, Middle, Alphabetic, Ideographic, Bottom };

// ---------------------------------------------------------------------------
// MarkerDrawFunctor / MarkerPickFunctor — CanvasDecoration 结构体（dqRender，
// W4 面仅 position/drawDecoration/pick）与 Marker 身份的恢复缝（XE7）。
// Marker::addMarker 把自身包进 CanvasDecoration 条目时以这两个具名函子填充；
// ToolAdmin 的悬停/点击路由经 std::function::target<> 恢复 Marker*。
// ---------------------------------------------------------------------------
struct MarkerDrawFunctor {
    Marker* marker = nullptr;
    DQ_APP_EXPORT void operator()(dqRender::CanvasContext& ctx) const;
};
struct MarkerPickFunctor {
    Marker* marker = nullptr;
    DQ_APP_EXPORT bool operator()(dqGeom::Point2d pt) const;
};

// ---------------------------------------------------------------------------
// Marker — A Marker is a CanvasDecoration whose position follows a fixed
// location in world space. Ported from: Marker.ts:71-318.
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT Marker {
public:
    // Ported from: Marker.ts:76-112（public 字段族）。
    bool visible = true;                             // :76
    dqGeom::Point3d worldLocation;                   // :82
    dqGeom::Point2d size;                            // :84
    // :86——当前视口像素位置（setPosition 更新）。
    dqGeom::Point3d position;
    // :88——当前视口像素矩形（setPosition 更新）。参考 readonly = 同一 ViewRect
    // 对象原地改写（init/setFrom）；C++ 值成员 + 同语义。
    ViewRect rect;
    std::optional<MarkerImage> image;                // :90
    std::optional<dqGeom::Point2d> imageOffset;      // :92
    std::optional<dqGeom::Point2d> imageSize;        // :94
    std::optional<std::string> label;                // :96
    std::optional<dqGeom::Point2d> labelOffset;      // :98
    std::optional<double> labelMaxWidth;             // :100
    std::optional<dqCommon::ColorDef> labelColor;    // :102（MarkerFillStyle→ColorDef）
    std::optional<MarkerTextAlign> labelAlign;       // :104
    std::optional<MarkerTextBaseline> labelBaseline; // :106
    std::optional<std::string> labelFont;            // :108
    std::optional<std::string> title;                // :110（XE3 纯文本）
    std::optional<ToolTipOptions> tooltipOptions;    // :112（XE11——宿主不消费）
    // :121 htmlElement——不移植（XE6）。

    // :127 drawFunc——可选绘制钩子（drawDecoration 在 image/label 之前调用）。
    std::function<void(dqRender::CanvasContext&)> drawFunc;

    // Constructor（Marker.ts:164-167）。
    Marker(dqGeom::Point3d const& worldLocation, dqGeom::Point2d const& size);
    virtual ~Marker();   // XE14——析构经 ToolAdmin 清悬停指针。

    // Return true to display image, if present（Marker.ts:124）。
    virtual bool wantImage() const { return true; }

    // --- 鼠标反馈族（Marker.ts:130-145；路由 = ToolAdmin.pickCanvasDecoration） ---
    virtual void onMouseEnter(BeButtonEvent& ev);    // :130-134
    virtual void onMouseLeave();                     // :137
    virtual void onMouseMove(BeButtonEvent& ev);     // :140-143
    // :145——参考为可选成员（undefined = dispatch 不调用）；C++ 默认实现返回
    // false（"未处理"），与参考的 undefined 检查在 dispatch 处观测等价。
    virtual bool onMouseButton(BeButtonEvent const& ev);

    // Determine whether the point is within this Marker（:148）。
    bool pick(dqGeom::Point2d pt) const;

    // Establish a range of scale factors based on distance from the camera
    //（:155-158——range.low=视锥远端缩放，high=近端；仅相机视图生效）。
    void setScaleFactor(dqGeom::Range1d const& range);

    // makeFrom（:172-180）——同位置/尺寸新建 T（rect/position/scaleFactor 随拷）。
    // §3.4：参考 `new (this as any)(...)` 的动态构造 → C++ 模板 + unique_ptr
    // 所有权移交。
    template<typename T, typename... Args>
    static std::unique_ptr<T> makeFrom(Marker const& other, Args&&... args)
    {
        auto out = std::make_unique<T>(other.worldLocation, other.size,
                                       std::forward<Args>(args)...);
        out->rect.setFrom(other.rect);
        out->position = other.position;
        if (other.m_scaleFactor)
            out->m_scaleFactor = other.m_scaleFactor;
        out->m_scaleFactorRange = other.m_scaleFactorRange;
        return out;
    }

    // Set the image（:229-241——Promise 形参面归入 XE2/XE9：DanQing 装载面为
    // 同步本地文件 + QTimer 延迟完成[演示侧]，Marker 本体只收已解析的图像）。
    void setImage(MarkerImage image);

    // Set the image from a URL（:244——imageElementFromUrl 的 DanQing 承载 =
    // QImageReader 本地文件 + Viewport::createTexture 上传；装载失败仅记日志，
    // 参考同形[loadOne catch→Logger.logError]）。
    void setImageUrl(Viewport& vp, std::string const& url);

    // Set the position (in pixels) for this Marker in the supplied Viewport,
    // based on its worldLocation（:250-283）。返回 false = 不可见/视锥外，
    // 不入绘制。
    // §3.4：参考的 markerSet?: MarkerSet<Marker> 可选形参经默认模板形参承载
    // （MarkerSet<T>——T 派生自 Marker——间无值转换）。
    bool setPosition(Viewport& vp) { return setPositionT<Marker>(vp, nullptr); }
    template<typename T>
    bool setPosition(Viewport& vp, MarkerSet<T>* markerSet)
    {
        return setPositionT(vp, markerSet);
    }

    // Add this Marker to the supplied DecorateContext（:300-307——
    // addCanvasDecoration；htmlElement 分支 XE6 不移植）。
    virtual void addMarker(DecorateContext& context);

    // Set the position and add this Marker to the context if visible（:314-317）。
    void addDecoration(DecorateContext& context);

    // CanvasDecoration 条目 → Marker 身份恢复（XE7 缝；非 Marker 产出[无 pick
    // 或 pick 非 MarkerPickFunctor]返回 nullptr）。
    static Marker* fromDecoration(dqRender::CanvasDecoration const& decoration);

    // Called during frame rendering to display this Marker（:195-223——
    // drawFunc→image→label 序；label 腿 XE8 登记）。参考 public
    //（CanvasDecoration 接口成员——绘制由 2D overlay 层发起；DanQing 由
    // MarkerDrawFunctor 发起）。
    void drawDecoration(dqRender::CanvasContext& ctx);

protected:
    // :72-80 保护成员族（参考 protected；派生 marker 可读——IncidentMarkerDemo.ts
    // :25 的注释行即 `wantImage() { return this._isHilited; }` 的子类消费形）。
    std::optional<dqGeom::Point2d> m_scaleFactor;        // :72
    std::optional<dqGeom::Range1d> m_scaleFactorRange;   // :73
    bool m_isHilited = false;                            // :78
    std::optional<dqCommon::ColorDef> m_hiliteColor;     // :80

    // drawHilited（:187-192）——XE8：CanvasContext 无 shadowBlur/shadowColor/
    // scale 面，视觉效（发光+放大 25%）登记为缺口；恒返回 false（继续正常绘制）
    // 的参考语义保留。返回 true = 不再绘制本 marker。
    virtual bool drawHilited(dqRender::CanvasContext& ctx);

private:
    // setPosition 的模板共享体（:250-283 逐行——实现于本文件尾部，待
    // MarkerSet 完整类型）。
    template<typename T>
    bool setPositionT(Viewport& vp, MarkerSet<T>* markerSet);

    // XE14 生命周期簿记：最近一次 addMarker 的目标视口（~Marker 清扫该视口
    // 当前装饰列表 + 装饰缓存里的本 marker 条目——Viewport::
    // RemoveCanvasDecorationsForMarker）。边界如实登记：同一 marker 若被
    // addMarker 进多个视口的装饰列表，仅清扫最近一处——MarkerSet 语义上
    // marker 从属单一视口（MarkerSet.addDecoration 的视口门 Marker.ts:
    // 429-430 保证集合内 marker 只装饰其绑定视口），跨视口复用裸 Marker 为
    // 边界态（参考无此场景——Marker 经 MarkerSet 与视口 1:1）。
    Viewport* m_lastDecorateViewport = nullptr;

    friend struct MarkerDrawFunctor;
};

// ---------------------------------------------------------------------------
// Cluster — a cluster of one or more Markers that overlap one another in the
// view（Marker.ts:325-350）。markers 为非拥有引用（所有权在 MarkerSet 的
// markers 集合）；clusterMarker 由 MarkerSet::getClusterMarker 惰性创建、
// 随 Cluster 所有权（参考 GC → unique_ptr，§3.4）。
// ---------------------------------------------------------------------------
template<typename T>
class Cluster {
public:
    // :329-332（assert(markers.length > 0)——C++ assert 承载；§9 禁异常不禁
    // 断言）。
    explicit Cluster(std::vector<T*> markers) : markers(std::move(markers))
    {
        assert(!this->markers.empty());
    }

    std::vector<T*> markers;                  // :326
    std::unique_ptr<Marker> clusterMarker;    // :327

    // :334-336——组屏幕位置 = 首个成员的位置（参考返回同一对象——const& 保活
    // 引用语义，Marker::position 随 setPosition 原地更新）。
    dqGeom::Point3d const& position() const { return markers.front()->position; }

    // :342-349——组世界位置 = 成员 worldLocation 平均。
    dqGeom::Point3d getClusterLocation() const
    {
        dqGeom::Point3d location = dqGeom::Point3d::From(0.0, 0.0, 0.0);
        if (!markers.empty()) {
            for (T const* marker : markers) {
                location.x += marker->worldLocation.x;
                location.y += marker->worldLocation.y;
                location.z += marker->worldLocation.z;
            }
            location.Scale(1.0 / static_cast<double>(markers.size()));
        }
        return location;
    }
};

// ---------------------------------------------------------------------------
// MarkerSet — a set of Markers that cluster when they overlap in screen space
//（Marker.ts:357-494——聚类算法纯几何零 DOM）。
// ---------------------------------------------------------------------------
template<typename T>
class MarkerSet {
public:
    // TS ObservableSet<T>（:366——onAdded/onDeleted/onCleared → markDirty，
    // :380-383）的 C++ 承载（§3.4）：插入序保有（TS Set 迭代序 = 插入序——
    // 聚类结果依赖它）+ unique_ptr 所有权 + 变更即 markDirty。
    class Markers {
    public:
        // Set.add——追加并标脏。返回裸指针（集合持有所有权）。
        T* add(std::unique_ptr<T> marker)
        {
            T* raw = marker.get();
            m_set.m_markers.push_back(std::move(marker));
            m_set.markDirty();
            return raw;
        }
        // Set.delete——按指针移除并标脏；返回是否命中。
        bool erase(T* marker)
        {
            auto& v = m_set.m_markers;
            for (auto it = v.begin(); it != v.end(); ++it) {
                if (it->get() == marker) {
                    v.erase(it);
                    m_set.markDirty();
                    return true;
                }
            }
            return false;
        }
        // Set.clear。
        void clear()
        {
            if (m_set.m_markers.empty())
                return;
            m_set.m_markers.clear();
            m_set.markDirty();
        }
        size_t size() const { return m_set.m_markers.size(); }
        bool empty() const { return m_set.m_markers.empty(); }
        // 插入序遍历（TS Set 的 forEach 语义——Marker.ts:102/444 的迭代序）。
        template<typename F>
        void forEach(F&& fn) { for (auto& m : m_set.m_markers) fn(m.get()); }
        template<typename F>
        void forEach(F&& fn) const { for (auto const& m : m_set.m_markers) fn(m.get()); }

    private:
        friend class MarkerSet;
        explicit Markers(MarkerSet& set) : m_set(set) {}
        MarkerSet& m_set;
    };

    // :369——最小成组数；<= 该数的组逐成员绘制。默认 1（恒成组）。
    int minimumClusterSize = 1;

    // Construct a new MarkerSet for a specific Viewport（:378-384——
    // undefined → IModelApp.viewManager.selectedView）。
    explicit MarkerSet(Viewport* viewport = nullptr);
    virtual ~MarkerSet() = default;

    // The Viewport of this MarkerSet（:387）。
    Viewport* viewport() const { return m_viewport; }

    // Change the Viewport for this MarkerSet（:393-396）。
    void changeViewport(Viewport* vp)
    {
        m_viewport = vp;
        markDirty();
    }

    // :371——markers 集合访问面（见 Markers 类注释）。
    Markers& markers() { return m_markersFacade; }
    Markers const& markers() const { return m_markersFacade; }

    // Indicate that this MarkerSet is dirty（:405-407——worldToViewMap 归零即
    // 失效标记；下一 addDecoration 重建 entries）。
    void markDirty() { m_worldToViewMap.SetZero(); }

    // :417-421——minScaleViewW 惰性缓存（frustum 变化时随 entries 一并失效）。
    double getMinScaleViewW(Viewport& vp);

    // This method should be called from Decorator.decorate（:427-493——
    // 屏幕 XY 距离聚类 + 绘制）。
    void addDecoration(DecorateContext& context);

protected:
    // :361 _entries——Marker 或 Cluster 的条目数组（仅 frustum 变化时重建）。
    struct Entry {
        T* marker = nullptr;                       // 非拥有（MarkerSet 持有）
        std::unique_ptr<Cluster<T>> cluster;       // 拥有（参考 GC）
    };
    // 测试/派生面：重建后条目检视（聚类锁的观测点）。
    size_t entriesCountForTest() const { return m_entries.size(); }

    // :414——由派生类为组创建占位 marker（每次调用新建）。
    virtual Marker* getClusterMarker(Cluster<T>& cluster) = 0;

    // :373——聚类半径（px）；<=0 时按首个可见 marker 的 imageSize/size×1.5 计算。
    double m_clusterRadius = 0.0;

    std::vector<Entry> m_entries;                  // :361
    dqGeom::Matrix4d m_worldToViewMap;             // :363（CreateZero——恒脏初态）
    std::optional<double> m_minScaleViewW;         // :365

private:
    Viewport* m_viewport = nullptr;                // :358
    std::vector<std::unique_ptr<T>> m_markers;     // :366 的 C++ 所有权承载
    Markers m_markersFacade{*this};
};

}  // namespace dqApp

// ---------------------------------------------------------------------------
// 模板实现段——MarkerSet<T> 与 Marker::setPositionT 的方法体（参考单文件承载；
// 置尾为 C++ include 次序[DecorateContext/Viewport/MarkerSet 完整类型]）。
// ---------------------------------------------------------------------------
#include "Application.h"  // MarkerSet 缺省视口（:379——selectedView 等价物）

namespace dqApp {

// getMinScaleViewW 的自由函数形（Marker.ts:46-63）——声明于 Marker.cpp
// （Marker::setPositionT 与 MarkerSet::getMinScaleViewW 共用）。
DQ_APP_EXPORT double getMinScaleViewW(Viewport& vp);

// Ported from: Marker.setPosition（Marker.ts:250-283）。
template<typename T>
bool Marker::setPositionT(Viewport& vp, MarkerSet<T>* markerSet)
{
    if (!visible)                          // :251-252
        return false;

    // :254——worldToView4d（Viewport.ts:2118 的 Viewport 面）。
    dqGeom::Point4d const pt4 = vp.worldToView4d(worldLocation);
    if (pt4.w > 1.0 || pt4.w < 1.0e-6)     // :255——视锥外或贴眼
        return false;

    // :258——realPoint（w 已守卫非零）。
    position = dqGeom::Point3d::From(pt4.x / pt4.w, pt4.y / pt4.w, pt4.z / pt4.w);
    if (!vp.viewRect().containsPoint(position))   // :259——视口矩形外
        return false;

    // :262-265
    double const sizeX = size.x / 2.0;
    double const sizeY = size.y / 2.0;
    rect.init(position.x - sizeX, position.y - sizeY, position.x + sizeX, position.y + sizeY);

    // :267-280——scaleFactor 计算（相机视图才缩放）。
    if (m_scaleFactor && m_scaleFactorRange) {
        double scale = 1.0;
        if (vp.isCameraOn()) {
            dqGeom::Range1d const& range = *m_scaleFactorRange;
            double const minScaleViewW =
                (nullptr != markerSet) ? markerSet->getMinScaleViewW(vp)
                                       : getMinScaleViewW(vp);   // :272
            if (minScaleViewW > 0.0)
                scale = dqGeom::clamp(range.high - (pt4.w / minScaleViewW) * range.Length(), 0.4, 2.0);
            else
                scale = dqGeom::clamp(range.low + ((1.0 - pt4.w) * range.Length()), 0.4, 2.0);
            rect.scaleAboutCenter(scale, scale);   // :277
        }
        m_scaleFactor = dqGeom::Point2d::From(scale, scale);   // :279
    }
    return true;
}

// Ported from: MarkerSet 构造器（Marker.ts:378-384——undefined → selectedView）。
template<typename T>
MarkerSet<T>::MarkerSet(Viewport* viewport)
    : m_viewport(nullptr != viewport ? viewport : Application::Get().GetViewManager().GetSelectedViewport())
{
}

// Ported from: MarkerSet.getMinScaleViewW（Marker.ts:417-421）。
template<typename T>
double MarkerSet<T>::getMinScaleViewW(Viewport& vp)
{
    if (!m_minScaleViewW.has_value())
        m_minScaleViewW = getMinScaleViewW(vp);
    return *m_minScaleViewW;
}

// Ported from: MarkerSet.addDecoration（Marker.ts:427-493）。
template<typename T>
void MarkerSet<T>::addDecoration(DecorateContext& context)
{
    Viewport& vp = context.GetViewport();
    if (&vp != m_viewport)                 // :429-430
        return;

    // :434-439——frustum 未变不重建 entries（性能 + 组 marker 悬停身份存续）。
    // 参考判据 = worldToViewMap.isAlmostEqual（Matrix4d.ts:251-252——maxDiff ≤
    // Geometry.smallMetricDistance=1e-6；DanQing IsAlmostEqual 缺省 1e-10，
    // 显式传参考容差）。
    if (m_worldToViewMap.MaxDiff(vp.worldToViewMap().Transform0()) > 1.0e-6) {
        m_worldToViewMap.SetFrom(vp.worldToViewMap().Transform0());
        m_minScaleViewW = std::nullopt;
        m_entries.clear();                 // :439——start over

        double distSquared = m_clusterRadius * m_clusterRadius;   // :441

        // :444——遍历 markers（TS Set 迭代序 = 插入序 = m_markers 向量序）。
        for (auto const& owned : m_markers) {
            T* marker = owned.get();
            // :446——不可见/视锥外跳过。
            if (!marker->setPosition(vp, this))
                continue;

            // :449-453——半径缺省：首个可见 marker 的 imageSize/size × 1.5。
            if (distSquared <= 0.0) {
                dqGeom::Point2d const markerSize =
                    marker->imageSize.has_value() ? *marker->imageSize : marker->size;
                double const dist = std::max(markerSize.x, markerSize.y) * 1.5;
                distSquared = dist * dist;
            }

            bool added = false;
            for (size_t i = 0; i < m_entries.size(); ++i) {   // :456
                Entry& entry = m_entries[i];
                dqGeom::Point3d const& entryPos =
                    (nullptr != entry.cluster) ? entry.cluster->position()
                                               : entry.marker->position;   // :458 的 entry.position
                if (marker->position.DistanceSquaredXY(entryPos) <= distSquared) {
                    added = true;
                    if (nullptr != entry.cluster) {           // :460——已成组 → 追加
                        entry.cluster->markers.push_back(marker);
                    } else {                                  // :463——新建组（两成员）
                        T* first = entry.marker;
                        entry.marker = nullptr;
                        entry.cluster = std::make_unique<Cluster<T>>(
                            std::vector<T*>{first, marker});
                    }
                    break;
                }
            }
            if (!added) {                                     // :469——无重叠 → 单条
                Entry entry;
                entry.marker = marker;
                m_entries.push_back(std::move(entry));
            }
        }
    }

    // :474-492——逐条目入绘制。
    for (auto& entry : m_entries) {
        if (nullptr != entry.cluster) {
            Cluster<T>& cluster = *entry.cluster;
            if (static_cast<int>(cluster.markers.size()) <= minimumClusterSize) {
                // :476-477——不足最小成组数 → 逐成员绘制。
                for (T* marker : cluster.markers)
                    marker->addMarker(context);
            } else {
                // :480-486——组 marker 惰性创建（getClusterMarker 返回新对象——
                // 参考 GC 语义，unique_ptr 承接所有权）。
                if (!cluster.clusterMarker) {
                    std::unique_ptr<Marker> clusterMarker(getClusterMarker(cluster));
                    // :483-484——getClusterMarker 可不设位置 → 补 setPosition。
                    if (clusterMarker->rect.isNull())
                        clusterMarker->setPosition(vp, this);
                    cluster.clusterMarker = std::move(clusterMarker);
                }
                cluster.clusterMarker->addMarker(context);
            }
        } else {
            entry.marker->addMarker(context);                 // :490
        }
    }
}

}  // namespace dqApp
