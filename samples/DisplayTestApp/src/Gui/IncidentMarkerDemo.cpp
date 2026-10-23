// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — IncidentMarkerDemo 实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/
//              IncidentMarkerDemo.ts（262 行）。EQUIVALENCE 登记见头文件。
#include "IncidentMarkerDemo.h"

#include <dqApp/Application.h>
#include <dqApp/NotificationManager.h>
#include <dqApp/ToolAdmin.h>     // BeButtonEvent + Tool 基类
#include <dqApp/ViewManager.h>

#include <dqApp/IModelConnection.h>   // GetProjectExtents（工具 run 面）

#include <dqGeom/Arc3d.h>        // IncidentMarker 世界装饰圆（:67）

#include <dqRender/GraphicBuilder.h>   // GraphicBuilderOptions/GraphicType（:66）
#include <dqRender/rhi/Driver.h>        // ~IncidentMarkerDemo 纹理回收面（XE15 配对）

#include <QCoreApplication>
#include <QImageReader>
#include <QTimer>

#include <cmath>
#include <cstdio>
#include <cstdlib>  // P-4：getenv（DANQING_DTA_ASSETS_DIR 运行时覆写）
#include <random>

namespace Gui {

namespace {

// 六图标装载清单（:176-183 的 loads 数组——Warning_sign 必须首位，见
// "get warningSign()" 注释）。
constexpr char const* kIconFiles[6] = {
    "Warning_sign.svg", "Hazard_biological.svg", "Hazard_electric.svg",
    "Hazard_flammable.svg", "Hazard_toxic.svg", "Hazard_tripping.svg",
};

constexpr double k2Pi = 2.0 * 3.14159265358979323846;

}  // namespace

// ---------------------------------------------------------------------------
// IncidentMarker（:16-77）
// ---------------------------------------------------------------------------

// Ported from: IncidentMarker.makeColor（:28-31——severity <= 16 绿→琥珀，
// 否则琥珀→红）。静态 _amber（:20）= ColorDef.create(ColorByName.amber)。
dqCommon::ColorDef IncidentMarker::makeColor(int severity)
{
    // :20 _amber = ColorDef.create(ColorByName.amber)（DanQing create =
    // fromTbgr——ColorByName 双端 0xBBGGRR 同形）。
    static dqCommon::ColorDef const amber = dqCommon::ColorDef::create(dqCommon::ColorByName::amber);
    if (severity <= 16)
        return dqCommon::ColorDef::green.lerp(amber, (severity - 1) / 15.0);
    return amber.lerp(dqCommon::ColorDef::red, (severity - 16) / 14.0);
}

// Ported from: IncidentMarker 构造器（:44-53）。
IncidentMarker::IncidentMarker(dqGeom::Point3d const& location, int severity_, int id_,
                               dqApp::MarkerImage const* icon, int iconIndex_)
    : dqApp::Marker(location, dqGeom::Point2d::From(30, 30))   // :17 _size
    , severity(severity_)
    , id(id_)
    , iconIndex(iconIndex_)
    , m_color(makeColor(severity_))         // :46
{
    if (icon != nullptr)
        setImage(*icon);                    // :47 setImage(icon)
    imageOffset = dqGeom::Point2d::From(0, 30);   // :48——旗杆底对事故点
    imageSize = dqGeom::Point2d::From(40, 40);    // :49
    // :50——title（XE3：<br> → 换行的纯文本形）。
    title = "Severity: " + std::to_string(severity) + "\nId: " + std::to_string(id);
    setScaleFactor(dqGeom::Range1d(0.2, 1.4));    // :51
    label = std::to_string(id);                   // :52
}

// Ported from: IncidentMarker.onMouseButton（:34-41）。
bool IncidentMarker::onMouseButton(dqApp::BeButtonEvent const& ev)
{
    if (ev.button == dqApp::BeButton::Data && ev.isDown) {
        // :37——openMessageBox(LargeOk, "severity = N", Information)。
        dqApp::Application::Get().GetNotificationManager().OpenMessageBox(
            dqApp::MessageBoxType::LargeOk,
            "severity = " + std::to_string(severity),
            dqApp::MessageBoxIconType::Information);
    }
    return true;
}

// Ported from: IncidentMarker.addMarker（:64-76——super.addMarker +
// WorldDecoration 圆）。
void IncidentMarker::addMarker(dqApp::DecorateContext& context)
{
    dqApp::Marker::addMarker(context);   // :65 super.addMarker(context)

    dqApp::Viewport& vp = context.GetViewport();
    dqRender::GraphicBuilderOptions opts;
    opts.type = dqRender::GraphicType::WorldDecoration;   // :66
    auto builder = vp.createGraphicBuilder(opts);
    if (!builder)
        return;   // headless 面（无 GL 管道）——参考 builder 恒在；登记缺口
                  //（真窗消费面不受影响，主会话 E2E 覆盖）。

    // :67——Arc3d.createScaledXYColumns(worldLocation, vp.rotation.transpose(),
    // .2, .2, sweep360) 的等价组装（CesiumDecorator 先例——FromVectors 向量形
    // 即该工厂展开的 vector0/vector90 承载；Matrix3d 列 = 转置后列）。
    dqGeom::Matrix3d const rotT = vp.getRotation().Transpose();
    dqGeom::Vector3d v0 = rotT.ColumnX();
    dqGeom::Vector3d v90 = rotT.ColumnY();
    v0.Scale(0.2);
    v90.Scale(0.2);
    auto const ellipse = dqGeom::Arc3d::FromVectors(worldLocation, v0, v90,
                                                    dqGeom::AngleSweep::FullCircle());
    // :70——白线色 + 严重度填充色、线宽 1。
    builder->setSymbology(dqCommon::ColorDef::white, m_color, 1);
    // :72——先描边（不填充）。
    builder->addArc(*ellipse, false, false);
    // :73-74——透明填充（withTransparency 200）后填充圆。
    builder->setBlankingFill(m_color.withTransparency(200));
    builder->addArc(*ellipse, true, true);
    // :75——addDecorationFromBuilder(builder)。
    if (dqRender::RenderGraphic* graphic = builder->finish())
        context.AddDecoration(dqRender::GraphicType::WorldDecoration, graphic);
}

// ---------------------------------------------------------------------------
// IncidentClusterMarker（:80-137）
// ---------------------------------------------------------------------------

// Ported from: IncidentClusterMarker 构造器（:96-136）。
IncidentClusterMarker::IncidentClusterMarker(dqGeom::Point3d const& location,
                                             dqGeom::Point2d const& size_,
                                             dqApp::Cluster<IncidentMarker>& cluster,
                                             dqApp::MarkerImage const* image)
    : dqApp::Marker(location, size_)
    , m_clusterColor(dqCommon::ColorDef::from(0, 0, 0))   // :133 前占位（ColorDef
                                                          // 无默认构造）
{
    // :99-112——top 10 严重度（降序排序 + 截断；参考的 sorted.length 截断形
    // 1:1）。
    std::vector<IncidentMarker*> sorted;
    constexpr size_t maxLen = 10;
    for (IncidentMarker* marker : cluster.markers) {
        if (sorted.size() < maxLen || marker->severity > sorted.back()->severity) {
            auto const it = std::find_if(sorted.begin(), sorted.end(),
                                         [marker](IncidentMarker const* v) {
                                             return v->severity < marker->severity;
                                         });
            if (it == sorted.end())
                sorted.push_back(marker);
            else
                sorted.insert(it, marker);
            if (sorted.size() > maxLen)
                sorted.resize(maxLen);
        }
    }

    imageOffset = dqGeom::Point2d::From(0, 28);   // :114
    imageSize = dqGeom::Point2d::From(30, 30);    // :115
    label = std::to_string(cluster.markers.size());          // :116（toLocaleString
    // 的等价——千分位本地化在 500 域内不可达，整串等价）
    labelColor = dqCommon::ColorDef::from(0, 0, 0);          // :117 "black"
    labelFont = std::string("bold 14px sans-serif");         // :118
    setScaleFactor(dqGeom::Range1d(0.7, 1.2));               // :119

    // :121-128——top-10 严重度 tooltip（XE3：<br> → 换行纯文本）。
    std::string tip;
    for (IncidentMarker const* marker : sorted) {
        if (!tip.empty())
            tip += "\n";
        tip += "Severity: " + std::to_string(marker->severity)
             + " Id: " + std::to_string(marker->id);
    }
    if (cluster.markers.size() > maxLen)
        tip += "\n...";
    title = tip;

    // :133——组色 = 最高严重度（sorted[0]）的插值色（XE 等价：参考存 hex
    // 字符串；DanQing 直存 ColorDef——drawFunc 的 strokeStyle 消费面同值）。
    m_clusterColor = IncidentMarker::makeColor(sorted[0]->severity);

    // :85-93——drawFunc：白圆 + 组色 5px 描边（r=13）。
    dqCommon::ColorDef const clusterColor = m_clusterColor;
    drawFunc = [clusterColor](dqRender::CanvasContext& ctx) {
        ctx.beginPath();
        ctx.setStrokeStyle(clusterColor);
        ctx.setFillStyle(dqCommon::ColorDef::white);
        ctx.setLineWidth(5);
        ctx.arc(0, 0, 13, 0, k2Pi);
        ctx.fill();
        ctx.stroke();
    };

    // :134-135——image 在场才设（warning 图标装载失败的缺席形）。
    if (image != nullptr)
        setImage(*image);
}

// ---------------------------------------------------------------------------
// IncidentMarkerSet（:140-144）
// ---------------------------------------------------------------------------

// Ported from: IncidentMarkerSet.getClusterMarker（:141-143）。
dqApp::Marker* IncidentMarkerSet::getClusterMarker(dqApp::Cluster<IncidentMarker>& cluster)
{
    // 参考 `IncidentMarkerDemo.decorator!.warningSign` 的非空断言（!）——demo
    // 恒经 start() 建立时静态在册；DanQing 无异常（§9），缺席（直构驱动面）
    // 退化为图标缺席（:134-135 的 if 门同形）。
    std::optional<dqApp::MarkerImage> warning;
    if (auto* demo = IncidentMarkerDemo::decorator())
        warning = demo->warningSign();
    return new IncidentClusterMarker(cluster.getClusterLocation(), cluster.markers[0]->size,
                                     cluster,
                                     warning.has_value() ? &*warning : nullptr);
}

// ---------------------------------------------------------------------------
// IncidentMarkerDemo（:150-251）
// ---------------------------------------------------------------------------

std::unique_ptr<IncidentMarkerDemo> IncidentMarkerDemo::s_decorator;   // :156

// Ported from: IncidentMarkerDemo 构造器（:201-203——ctor 即启 loadAll）。
IncidentMarkerDemo::IncidentMarkerDemo(dqApp::Viewport& vp, dqGeom::Range3d const& extents,
                                       std::function<double()> rand01)
    : m_incidents(&vp)   // :154——MarkerSet 缺省视口（参考缺省 =
                         // selectedView；start 的调用点携选中视口——见 :229-235）
    , m_extents(extents)
{
    m_vp = &vp;
    m_lifetimeToken = std::make_unique<QObject>();

    // XE16——随机源：注入优先；缺省 = random_device 播种的 mt19937（[0,1)
    // 均匀，Math.random 语义；成员值持有——§9 禁 shared_ptr）。
    if (rand01) {
        m_rand01 = std::move(rand01);
    } else {
        m_engine.seed(std::random_device{}());
        m_rand01 = [this]() {
            return std::generate_canonical<double, 53>(m_engine);
        };
    }

    // :176-183——六图标同步解码（XE2：本地文件 = QImageReader；失败记日志
    // 并缺席）。
    for (size_t i = 0; i < m_images.size(); ++i)
        loadOne(i, kIconFiles[i]);

    // XE9——:184-199 的装载完成点（建标记 + _loading 清除）延迟到下一事件
    // 循环圈（参考的 Promise.all().then 形）。timer 上下文 = m_lifetimeToken
    //（demo 析构 → Qt 自动取消未触发回调）。
    QTimer::singleShot(0, m_lifetimeToken.get(), [this]() { onLoadComplete(); });
}

IncidentMarkerDemo::~IncidentMarkerDemo()
{
    // 纹理回收（XE15 上传面的配对——GltfDecoration 析构先例：driver 存活门）。
    if (m_vp != nullptr) {
        if (auto* driver = m_vp->getDriver()) {
            for (auto& icon : m_images) {
                if (icon.texture != dqRender::rhi::TextureHandle{})
                    driver->destroyTexture(icon.texture);
            }
        }
    }
}

// Ported from: loadOne（:161-170——失败记日志继续[catch 分支]）。
void IncidentMarkerDemo::loadOne(size_t index, char const* src)
{
    // P-4：运行时 env 覆写（二进制包 run.cmd 注入；编译期宏为默认）。
    char const* dtaDir = DANQING_DTA_ASSETS_DIR;
    if (char const* env = std::getenv("DANQING_DTA_ASSETS_DIR"))
        dtaDir = env;
    std::string const path = std::string(dtaDir) + "/" + src;
    QImageReader reader(QString::fromStdString(path));
    QImage const img = reader.read();
    if (img.isNull()) {
        // :165-168——Could not load image（Logger.logError + console.log 的
        // DanQing 等价 = stderr）。
        fprintf(stderr, "[INCIDENT] Could not load image %s\n", path.c_str());
        return;
    }
    auto& icon = m_images[index];
    // 预乘形（GLCanvasContext 的 (GL_ONE, ONE_MINUS_SRC_ALPHA) 混合
    // = premultiplied 源契约——SVG 抗锯齿边缘正确性）。
    icon.image = img.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    icon.width = static_cast<uint32_t>(img.width());
    icon.height = static_cast<uint32_t>(img.height());
    icon.decoded = true;
}

// Ported from: loadAll 的建标记段（:188-198——500 随机 incidents）。
void IncidentMarkerDemo::buildMarkers(dqGeom::Range3d const& extents)
{
    size_t const len = m_images.size();
    for (int i = 0; i < kNumMarkers; ++i) {
        // :191-193——位置 = extents.low + rand × 轴长（逐轴）。
        dqGeom::Point3d const pos = dqGeom::Point3d::From(
            extents.low.x + m_rand01() * (extents.high.x - extents.low.x),
            extents.low.y + m_rand01() * (extents.high.y - extents.low.y),
            extents.low.z + m_rand01() * (extents.high.z - extents.low.z));
        // :194——(i % len) + 1（XE17 参考怪癖：i%6==5 → 下标 6 越界 =
        // undefined → 不建。边界检查复现 undefined 语义）。
        size_t const iconIndex = static_cast<size_t>(i) % len + 1;
        if (iconIndex >= len)
            continue;
        if (!m_images[iconIndex].decoded)
            continue;   // :195——undefined !== img 门（装载失败跳过）
        int const severity = 1 + static_cast<int>(std::lround(m_rand01() * 29.0));  // :196
        auto marker = std::make_unique<IncidentMarker>(pos, severity, i,
                                                       nullptr, static_cast<int>(iconIndex));
        m_incidents.markers().add(std::move(marker));   // :196 markers.add
    }
}

// XE15——纹理上传面（装载完成后的首个 Decorate 调用点；GL 上下文帧内保活）。
void IncidentMarkerDemo::ensureTexturesUploaded(dqApp::Viewport& vp)
{
    bool anyUploaded = false;
    for (size_t i = 0; i < m_images.size(); ++i) {
        auto& icon = m_images[i];
        if (!icon.decoded || icon.texture != dqRender::rhi::TextureHandle{})
            continue;
        // QImage 行对齐补齐——按行拷贝到紧凑 RGBA 缓冲。
        std::vector<uint8_t> bytes;
        bytes.reserve(static_cast<size_t>(icon.width) * icon.height * 4u);
        for (uint32_t row = 0; row < icon.height; ++row) {
            auto const* scanline = icon.image.constScanLine(static_cast<int>(row));
            bytes.insert(bytes.end(), scanline,
                         scanline + static_cast<size_t>(icon.width) * 4u);
        }
        auto buffer = dqCommon::ImageBuffer::create(
            std::move(bytes), dqCommon::ImageBufferFormat::Rgba,
            static_cast<int>(icon.width));
        if (!buffer.has_value())
            continue;
        dqRender::CreateTextureArgs args{};
        args.imageBuffer = std::move(*buffer);
        icon.texture = vp.createTexture(args);
        anyUploaded = true;
    }
    if (!anyUploaded)
        return;
    // 上传完成 → 推入各 marker（XE15 的 ctor 缺席补位——按 iconIndex 归属）。
    m_incidents.markers().forEach([this](IncidentMarker* marker) {
        if (marker->iconIndex < 0)
            return;
        auto const& icon = m_images[static_cast<size_t>(marker->iconIndex)];
        if (icon.decoded)
            marker->setImage({icon.texture, icon.width, icon.height});
    });
}

// XE9——装载完成点（:198-199 的 _loading=undefined + :219-225 的
// invalidateCachedDecorations 回调体）。
void IncidentMarkerDemo::onLoadComplete()
{
    // :184-197——建标记（ctor 携取景域——参考 loadAll(extents) 同形）。
    buildMarkers(m_extents);
    m_loading = false;   // :198
    // :221-224——loading 门期间的首次 decorate 登记过 awaiting 视口。
    if (m_awaiting && m_awaitingVp != nullptr) {
        m_awaitingVp->InvalidateCachedDecorations(this);   // :222
        m_awaiting = false;                                // :223
    }
    m_awaitingVp = nullptr;
}

// Ported from: IncidentMarkerDemo.decorate（:209-226）。
void IncidentMarkerDemo::Decorate(dqApp::DecorateContext& context)
{
    // :210-211——空间视图门。
    dqApp::Viewport& vp = context.GetViewport();
    if (nullptr == vp.GetView() || !vp.GetView()->isSpatialView())
        return;

    if (!m_loading) {
        // XE15——装载已完成：首帧上传纹理（无 GL 管道 = headless → 跳过）。
        ensureTexturesUploaded(vp);
        m_incidents.addDecoration(context);   // :213-214
        return;
    }

    // :219-225——装载中：登记完成回调（awaiting 门只挂一次）。
    if (!m_awaiting) {
        m_awaiting = true;
        m_awaitingVp = &vp;
    }
}

// Ported from: IncidentMarkerDemo.start（:229-235）。
void IncidentMarkerDemo::start(dqApp::Viewport& vp, dqGeom::Range3d const& extents)
{
    s_decorator = std::make_unique<IncidentMarkerDemo>(vp, extents);
    dqApp::Application::Get().GetViewManager().AddDecorator(s_decorator.get());

    // :234——视口换图即停（含视图关闭路径）。OnChangeView → DqEvent AddOnce。
    if (dqApp::Viewport* setVp = s_decorator->m_incidents.viewport())
        setVp->OnChangeView.AddOnce([](dqApp::ViewState*) { stop(); });
}

// Ported from: IncidentMarkerDemo.stop（:238-242）。
void IncidentMarkerDemo::stop()
{
    if (s_decorator)
        dqApp::Application::Get().GetViewManager().DropDecorator(s_decorator.get());
    s_decorator.reset();
}

// Ported from: IncidentMarkerDemo.toggle（:245-250）。
void IncidentMarkerDemo::toggle(dqApp::Viewport& vp, dqGeom::Range3d const& extents)
{
    if (!s_decorator)
        start(vp, extents);
    else
        stop();
}

// :158——warningSign 访问面（_images[0]）。
std::optional<dqApp::MarkerImage> IncidentMarkerDemo::warningSign() const
{
    auto const& icon = m_images[0];
    if (!icon.decoded)
        return std::nullopt;
    // XE15——纹理可能尚未上传（装载后首帧前）；句柄无效时 drawImage 安全跳过
    //（GLCanvasContext::drawImage 的无效句柄早退）。
    return dqApp::MarkerImage{icon.texture, icon.width, icon.height};
}

size_t IncidentMarkerDemo::loadedImageCount() const
{
    size_t n = 0;
    for (auto const& icon : m_images)
        if (icon.decoded)
            ++n;
    return n;
}

// ---------------------------------------------------------------------------
// IncidentMarkerDemoTool（:253-262）
// ---------------------------------------------------------------------------

// Ported from: IncidentMarkerDemoTool.run（:255-261）。
bool IncidentMarkerDemoTool::run()
{
    dqApp::Viewport* vp =
        dqApp::Application::Get().GetViewManager().GetSelectedViewport();
    if (vp != nullptr && vp->GetView() != nullptr && vp->GetView()->isSpatialView()
        && vp->GetIModel() != nullptr)   // :256-258
        IncidentMarkerDemo::toggle(*vp, vp->GetIModel()->GetProjectExtents());   // :258
    return true;
}

}  // namespace Gui
