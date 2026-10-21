// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Marker / Cluster / MarkerSet implementation
// Ported from: itwinjs-core core/frontend/src/Marker.ts
//              (getMinScaleViewW :46-63 + Marker :71-318 + Cluster :325-350 +
//              MarkerSet :357-494)。模板实现段在 Marker.h 尾部。
#include "dqApp/Marker.h"

#include "dqApp/Application.h"
#include "dqApp/IModelConnection.h"   // GetProjectExtents（getMinScaleViewW）
#include "dqApp/ToolAdmin.h"   // BeButtonEvent + clearCanvasDecorationIfMarker（XE14）

#include <dqRender/CreateTextureArgs.h>

#include <QImage>
#include <QImageReader>
#include <QString>

#include <cstdio>

namespace dqApp {

// Ported from: Marker.ts:46-63（getMinScaleViewW——视锥后端 w 值：投影域 8 角
// 沿视线[-Z]的最大投影深度处的 w，缩放因子的距离归一基）。
double getMinScaleViewW(Viewport& vp)
{
    std::optional<double> zHigh;
    // EQUIVALENCE：参考直读 vp.view.getCenter()/getZVector()（Marker.ts:48-49）
    // ——ViewState 基类即携两面（ViewState.ts:672/:993，基类 abstract
    // getRotation 上构造）；DanQing ViewState 基类无旋转面（ViewState3d 专属），
    // 经 AsViewState3d 护栏（本函数仅在 setPosition 的 isCameraOn 分支可达——
    // 相机视图恒 3d，护栏在实践中恒通过）。发散=非 3d 视图返回 0（参考为
    // 编译期不可达形）；验证=setPosition 缩放锁。
    ViewState3d const* view = (nullptr != vp.GetView()) ? vp.GetView()->AsViewState3d() : nullptr;
    if (nullptr == view)
        return 0.0;
    dqGeom::Point3d const origin = view->getCenter();          // :48
    dqGeom::Vector3d direction = view->GetZVec();              // :49
    direction.Scale(-1.0);                                     // :50 scaleInPlace(-1)
    IModelConnection* iModel = view->GetIModel();
    if (nullptr == iModel)
        return 0.0;
    // :51——projectExtents.corners()（Range3d.corners，Range.ts:601-619）。
    auto const corners = iModel->GetProjectExtents().Corners();
    for (auto const& corner : corners) {                       // :53-58
        dqGeom::Vector3d const delta = dqGeom::Vector3d::FromStartEnd(origin, corner);
        double const projection = delta.DotProduct(direction);
        if (!zHigh.has_value() || projection > *zHigh)
            zHigh = projection;
    }
    if (!zHigh.has_value())                                    // :59-60
        return 0.0;
    // :61——origin.plusScaled(direction, zHigh, origin)。
    dqGeom::Point3d const originAtDepth = dqGeom::Point3d::From(
        origin.x + direction.x * *zHigh,
        origin.y + direction.y * *zHigh,
        origin.z + direction.z * *zHigh);
    return vp.worldToView4d(originAtDepth).w;                  // :62
}

// ---------------------------------------------------------------------------
// Marker（Marker.ts:71-318）
// ---------------------------------------------------------------------------

// Ported from: Marker constructor（Marker.ts:164-167）。
Marker::Marker(dqGeom::Point3d const& worldLocation_, dqGeom::Point2d const& size_)
    : worldLocation(worldLocation_)
    , size(size_)
{
}

// XE14（C++ 显式所有权缝——参考依赖 GC）：marker 析构时清 ToolAdmin 的悬停
// 指针 + 清扫最近装饰视口的本 marker 条目（活列表 + 装饰缓存——修复轮 1
// Important-1 扩展），防悬垂（悬停中的 marker 所属 demo/集合销毁场景）。
// 边界如实登记：依赖 Application::Get() 单例在场——其为 Meyers 局部静态
//（进程期恒活，静态析构序 = 先建者后毁：Application 先于任何 marker 建立
//[首个 ToolAdmin/Viewport 触达即建]，故 marker 析构时恒在场）。
Marker::~Marker()
{
    Application::Get().GetToolAdmin().clearCanvasDecorationIfMarker(this);
    if (nullptr != m_lastDecorateViewport)
        m_lastDecorateViewport->RemoveCanvasDecorationsForMarker(*this);
}

// Ported from: Marker.onMouseEnter（Marker.ts:130-134）。
void Marker::onMouseEnter(BeButtonEvent& ev)
{
    m_isHilited = true;
    // :132——ev.viewport?.hilite.color ?? new Hilite.Settings().color。
    m_hiliteColor = (nullptr != ev.viewport)
        ? ev.viewport->GetHiliteColorDef()
        : dqCommon::HiliteSettings().color;
    // :133——IModelApp.accuSnap.clear()。
    Application::Get().GetAccuSnap().clear();
}

// Ported from: Marker.onMouseLeave（Marker.ts:137）。
void Marker::onMouseLeave()
{
    m_isHilited = false;
}

// Ported from: Marker.onMouseMove（Marker.ts:140-143——悬停即 tooltip）。
void Marker::onMouseMove(BeButtonEvent& ev)
{
    if (title.has_value() && nullptr != ev.viewport) {
        // :142——expectDefined(ev.viewport).openToolTip(title, ev.viewPoint,
        // tooltipOptions)。XE11：tooltipOptions 宿主不消费。
        ev.viewport->openToolTip(*title, &ev.viewPoint);
    }
}

// Ported from: Marker.onMouseButton（Marker.ts:145——参考为可选成员[undefined
// = 不吞事件]；默认实现 false = "未处理"，dispatch 处观测等价）。
bool Marker::onMouseButton(BeButtonEvent const& /*ev*/)
{
    return false;
}

// Ported from: Marker.pick（Marker.ts:148）。
bool Marker::pick(dqGeom::Point2d pt) const
{
    return rect.containsPoint(pt);
}

// Ported from: Marker.setScaleFactor（Marker.ts:155-158）。
void Marker::setScaleFactor(dqGeom::Range1d const& range)
{
    m_scaleFactorRange = range;
    m_scaleFactor = dqGeom::Point2d::From(1.0, 1.0);
}

// Ported from: Marker.setImage（Marker.ts:229-241 的同步形——Promise 面归
// XE2/XE9）。
void Marker::setImage(MarkerImage image_)
{
    image = image_;
}

// Ported from: Marker.setImageUrl（Marker.ts:244）+ imageElementFromUrl
//              （common/ImageUtil.ts:227-236 的 DanQing 承载，XE2）。
void Marker::setImageUrl(Viewport& vp, std::string const& url)
{
    QImageReader reader(QString::fromStdString(url));
    QImage const img = reader.read();
    if (img.isNull()) {
        // loadOne 的 catch 分支语义（IncidentMarkerDemo.ts:164-169——记日志后继续）。
        fprintf(stderr, "[MARKER] Could not load image %s\n", url.c_str());
        return;
    }
    // GLCanvasContext 的混合是 premultiplied（GL_ONE, ONE_MINUS_SRC_ALPHA——
    // GLCanvasContext.cpp:427）→ 预乘形上传（SVG 抗锯齿边缘正确性）。
    QImage const rgba = img.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    std::vector<uint8_t> bytes;
    bytes.reserve(static_cast<size_t>(rgba.width()) * static_cast<size_t>(rgba.height()) * 4u);
    for (int row = 0; row < rgba.height(); ++row) {
        auto const* scanline = rgba.constScanLine(row);
        bytes.insert(bytes.end(), scanline, scanline + static_cast<size_t>(rgba.width()) * 4u);
    }
    auto buffer = dqCommon::ImageBuffer::create(std::move(bytes),
                                                dqCommon::ImageBufferFormat::Rgba,
                                                rgba.width());
    if (!buffer.has_value())
        return;
    dqRender::CreateTextureArgs args{};
    args.imageBuffer = std::move(*buffer);
    MarkerImage markerImage;
    markerImage.texture = vp.createTexture(args);
    markerImage.width = static_cast<uint32_t>(rgba.width());
    markerImage.height = static_cast<uint32_t>(rgba.height());
    setImage(markerImage);
}

// Ported from: Marker.drawHilited（Marker.ts:187-192）——XE8：shadowBlur/
// shadowColor/ctx.scale 在 CanvasContext 子集无对应面（GLCanvasContext 为
// translate-only 变换）；恒 false（继续正常绘制）的参考控制流保留。
bool Marker::drawHilited(dqRender::CanvasContext& /*ctx*/)
{
    return false;
}

// Ported from: Marker.drawDecoration（Marker.ts:195-223——hilite 门 →
// scaleFactor → drawFunc → image → label 序）。
void Marker::drawDecoration(dqRender::CanvasContext& ctx)
{
    // :196-197
    if (m_isHilited && drawHilited(ctx))
        return;

    // :199-200——ctx.scale(m_scaleFactor.x, m_scaleFactor.y)。XE8：CanvasContext
    // 无 scale 面（translate-only——GLCanvasContext.h:128）；拾取域缩放已由
    // setPosition 的 rect.scaleAboutCenter 承载（:277），绘制期缩放登记为缺口。

    // :203-204——drawFunc 先行（绘制在 image/label 之下）。
    if (drawFunc)
        drawFunc(ctx);

    // :207-213——image（wantImage 门）。
    if (wantImage() && image.has_value()) {
        dqGeom::Point2d const drawSize = imageSize.has_value() ? *imageSize : size;
        // :209-211——offset = size/2 (+ imageOffset)。
        dqGeom::Point2d offset = dqGeom::Point2d::From(drawSize.x / 2.0, drawSize.y / 2.0);
        if (imageOffset.has_value()) {
            offset.x += imageOffset->x;
            offset.y += imageOffset->y;
        }
        // :212——ctx.drawImage(image, -offset.x, -offset.y, size.x, size.y)。
        // CanvasContext::drawImage 的 width/height 形参 = 目的矩形尺寸
        // （GLCanvasContext::drawImage → emitTexturedQuad(dx,dy,w,h)）。
        ctx.drawImage(image->texture, static_cast<uint32_t>(drawSize.x),
                      static_cast<uint32_t>(drawSize.y), -offset.x, -offset.y);
    }

    // :216-222——label 文本腿。XE8：CanvasContext 无 fillText/textAlign/
    // textBaseline/font 面（MeasureLabel 同款登记——dqApp/src/MeasureTool.cpp
    // :108-113）。label 字段族随数据面移植（title tooltip 链承载悬停文本）。
}

// Ported from: Marker.addMarker（Marker.ts:300-307）。
void Marker::addMarker(DecorateContext& context)
{
    // XE14 生命周期簿记——~Marker 清扫的视口定位。
    m_lastDecorateViewport = &context.GetViewport();
    // :301——context.addCanvasDecoration(this)。CanvasDecoration 结构体产出
    // （XE1/XE7——draw/pick 具名函子捕获 this；position = 视口像素位置，
    // Target.ts:1415-1423 的 save→translate(position)→drawDecoration→restore
    // 包裹语义由 GLCanvasContext 承载）。
    dqRender::CanvasDecoration decoration;
    decoration.position = dqGeom::Point2d::From(position.x, position.y);
    decoration.drawDecoration = MarkerDrawFunctor{this};
    decoration.pick = MarkerPickFunctor{this};
    context.AddCanvasDecoration(std::move(decoration));
    // :302-306——htmlElement 分支不移植（XE6）。
}

// Ported from: Marker.addDecoration（Marker.ts:314-317）。
void Marker::addDecoration(DecorateContext& context)
{
    if (setPosition(context.GetViewport()))
        addMarker(context);
}

// XE7 缝——CanvasDecoration 条目 → Marker 身份恢复（std::function::target<>）。
/*static*/ Marker* Marker::fromDecoration(dqRender::CanvasDecoration const& decoration)
{
    if (!decoration.pick)
        return nullptr;
    if (auto const* fn = decoration.pick.target<MarkerPickFunctor>())
        return fn->marker;
    return nullptr;
}

// ---------------------------------------------------------------------------
// XE7 具名函子（Marker.h 声明）——CanvasDecoration 结构体填充面。
// ---------------------------------------------------------------------------
void MarkerDrawFunctor::operator()(dqRender::CanvasContext& ctx) const
{
    if (nullptr != marker)
        marker->drawDecoration(ctx);
}

bool MarkerPickFunctor::operator()(dqGeom::Point2d pt) const
{
    return nullptr != marker && marker->pick(pt);
}

}  // namespace dqApp
