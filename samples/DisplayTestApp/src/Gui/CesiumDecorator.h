// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Cesium 装饰图元陈列馆（EmptyExample 的 8 族装饰形态）
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/EmptyExample.ts
//              （CesiumDecorator :50-496 + start/stop :40-48）
//
// 8 族（decorate :58-71 的派发序）：point / lineString / shape / arc / path /
// loop / polyface / solidPrimitive——每族 1-3 例，WorldDecoration+WorldOverlay
// 双挂载 + 2d 形（addPointString2d/addLineString2d/addShape2d/addArc2d）。
// 布局：projectExtents.center 基 + 参考米级偏移（±50000 级）。
//
// EQUIVALENCE（§11.10）：
//   - Arc3d.createCircularStartMiddleEnd（:293——路径族的 90° 过渡弧）：DanQing
//     Arc3d 无该工厂——本文件持 XY 共面三分点圆弧构造（圆心=外接圆心 +
//     FromVectors[vector90 = v0 面内 CCW 90°] + sweep 走中点方向）；发散 =
//     参考支持一般平面三分点，陈列馆的三点恒 XY 共面（z 同值——构造域内
//     等价）；验证法 = CesiumGalleryTest 弧几何锁（三分点回代距离 ≤1e-9）。
//   - Arc3d.createScaledXYColumns（:205/:218/:231/:252）：经 FromVectors
//     （vector0=(rx,0,0)、vector90=(0,ry,0)）等价组装（参考 create 的 axes
//     承载同缩放列向量——Arc3d.h FromVectors 即该 ctor 的向量形）。
//   - xOffset -220000（:52——把 path/loop/polyface/solid 族左移避叠）：DanQing
//     保留同值（blank 视域 projectExtents.center 基下同效）。
#pragma once

#include <dqApp/Decorator.h>
#include <dqApp/IModelConnection.h>

#include <dqGeom/Arc3d.h>

#include <memory>

namespace dqApp {
class DecorateContext;
}

namespace Gui {

// CesiumDecorator — 8 族装饰形态陈列。
// Ported from: EmptyExample.ts CesiumDecorator (:50-496).
class CesiumDecorator final : public dqApp::IDecorator {
public:
    // Ported from: CesiumDecorator.start (:266-269 + :40-48——addDecorator)。
    static CesiumDecorator* start(dqApp::IModelConnection* imodel);
    // Ported from: CesiumDecorator.stop (:493-495——dropDecorator)。
    void stop();

    // Ported from: CesiumDecorator.decorate (:58-71——8 族派发)。
    void Decorate(dqApp::DecorateContext& context) override;
    bool TestDecorationHit(uint32_t /*featureId*/) const override { return false; }
    QString GetDecorationToolTip(uint32_t /*featureId*/) const override
    {
        return {};
    }

private:
    // XY 共面三分点圆弧（EQUIVALENCE 见文件头——createCircularStartMiddleEnd
    // 的共面构造）。失败（共线）返回空。
    static dqBase::RefPtr<dqGeom::Arc3d> arcFromStartMiddleEnd(
        dqGeom::Point3d const& start, dqGeom::Point3d const& mid,
        dqGeom::Point3d const& end);

    // 8 族（:73-491 逐族 1:1）。
    void createPointDecorations(dqApp::DecorateContext& context);      // :73-98
    void createLineStringDecorations(dqApp::DecorateContext& context); // :100-148
    void createShapeDecorations(dqApp::DecorateContext& context);      // :150-197
    void createArcDecorations(dqApp::DecorateContext& context);        // :199-264
    void createPathDecorations(dqApp::DecorateContext& context);       // :272-327
    void createLoopDecorations(dqApp::DecorateContext& context);       // :329-349
    void createPolyfaceDecorations(dqApp::DecorateContext& context);   // :351-385
    void createSolidPrimitiveDecorations(dqApp::DecorateContext& context); // :443-491

    dqApp::IModelConnection* m_iModel = nullptr;
    double m_xOffset = -220000.0;  // :52
};

}  // namespace Gui
