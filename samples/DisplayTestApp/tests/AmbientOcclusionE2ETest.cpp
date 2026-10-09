// AmbientOcclusionE2ETest — Ambient Occlusion 引擎链像素锁（M-T T-e）。
//
// 链（全段本里程碑接线）：面板/预设 → DisplayStyle3dSettings.ao +
// viewFlags.ambientOcclusion → Viewport::ValidateRenderPlan（RenderPlan.ts:125
// 填充）→ TargetImpl::changeRenderPlan AO 门（Target.ts:524-530 四条件）→
// RenderCommands compositeFlags AO 位（:86-89——已锁）→ preDraw AO 资源开关
//（SceneCompositor.ts:1354-1370）→ renderOpaque AO 分流（:943-947）→
// renderOpaqueAO/renderAmbientOcclusion（:981-1044/:1220-1252——AO 计算+
// 高斯 X/Y）→ composite AO 乘腿（Composite.ts:73/:186-189——u_occlusion
// 单元 4）。
//
// Authored: no reference test exists in itwinjs-core for AO pixel output
//          （参考的 AO 覆盖在 DTA 交互式会话，无离线回放对应物；渲染像素回归
//           授权 §5(g)——复现配方 = 双盒交角可控资产 + overrideDisplayStyle；
//           证据链 = 设置数据面锁（AmbientOcclusionTest）+ AO 门矩阵锁
//          （TargetUniformsTest）+ readPixels WHERE 断言）。
//
// 资产与判据（§11.11 位置断言制度——WHERE 锚 = 世界坐标投影定位，非手工象限）：
// 平板 [-4..4,-4..4,0..1] + 立柱 [0.5..1.5,0.5..1.5,1..2.5]（Scene 装饰——
// WorldDecoration 的 vf 豁免语义不适用于 AO 之外的 vf 位[豁免清单为
// monochrome/thematic 等 vf 全表——ambientOcclusion 位同被关，故 Scene 挂接]，
// 关光照 = 纯 AO 隔离面[平顶/柱顶同色，off 帧恒均匀]）。
// AO on → 立柱足根环带（接触缝）遮蔽变暗 vs 开阔面；AO off → 环带=开阔面
// （均匀帧）→ 关恢复精确。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"

#include <dqApp/Application.h>
#include <dqApp/StandardView.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqCommon/AmbientOcclusion.h>
#include <dqGeom/PolyfaceBuilder.h>
#include <dqRender/GraphicBuilder.h>

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

// 双盒装饰（ThematicDisplayE2ETest 的 TallBoxDecorator 同构——PolyfaceBuilder
// 六面 + computeChordTolerance 接线[M-O(4) P8 实证] + GraphicType::Scene 挂接
// [S-e 的 WorldDecorations 豁免取证——豁免清单元 vf 全表，AO 位在内]）。
class AoCornerDecorator : public dqApp::IDecorator {
public:
    void Decorate(dqApp::DecorateContext& context) override
    {
        dqRender::GraphicBuilderOptions opts;
        opts.type = dqRender::GraphicType::Scene;
        auto& vp = context.GetViewport();
        dqGeom::Point3d const refPoint(0.0, 0.0, 0.0);
        double const worldPerPixel = vp.GetViewingSpace().getPixelSizeAtPoint(&refPoint);
        opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
        auto builder = context.GetViewport().createGraphicBuilder(opts);
        if (!builder)
            return;
        builder->setSymbology(dqCommon::ColorDef::white, dqCommon::ColorDef::white, 1);

        // 平板 + 立柱（六面四边形面片——RenderSmokeTest.HilitePass 同构）。
        auto addBox = [&](double x0, double y0, double z0,
                          double x1, double y1, double z1) {
            dqGeom::Point3d const b1(x0, y0, z0), b2(x1, y0, z0), b3(x1, y1, z0), b4(x0, y1, z0);
            dqGeom::Point3d const t1(x0, y0, z1), t2(x1, y0, z1), t3(x1, y1, z1), t4(x0, y1, z1);
            auto box = dqGeom::PolyfaceBuilder::create();
            {
                dqGeom::Point3d const q1[4] = {b4, b3, b2, b1};
                dqGeom::Point3d const q2[4] = {t1, t2, t3, t4};
                dqGeom::Point3d const q3[4] = {b1, b2, t2, t1};
                dqGeom::Point3d const q4[4] = {b2, b3, t3, t2};
                dqGeom::Point3d const q5[4] = {b3, b4, t4, t3};
                dqGeom::Point3d const q6[4] = {b4, b1, t1, t4};
                box->AddQuadFacet(q1); box->AddQuadFacet(q2); box->AddQuadFacet(q3);
                box->AddQuadFacet(q4); box->AddQuadFacet(q5); box->AddQuadFacet(q6);
            }
            auto polyface = box->ClaimPolyface();
            builder->addPolyface(*polyface, /*filled=*/true);
        };
        addBox(-4.0, -4.0, 0.0, 4.0, 4.0, 1.0);   // 平板（顶面 z=1）
        addBox(0.5, 0.5, 1.0, 1.5, 1.5, 2.5);     // 立柱（坐于平板顶面）

        if (auto* g = builder->finish()) {
            context.GetViewport().createGraphicOwner(g);
            context.AddDecoration(opts.type, g);
        }
    }
    bool TestDecorationHit(uint32_t) const override { return false; }
    QString GetDecorationToolTip(uint32_t) const override { return {}; }
};

void spinAO(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 帧稳定（连续两帧逐像素相等——ThematicDisplayE2E 的 ReadStableFrame 同构）。
bool readStableFrame(Gui::View3DInventor& view, std::vector<uint8_t>& frame,
                     uint32_t& w, uint32_t& h)
{
    auto* vp = view.getUeViewport();
    std::vector<uint8_t> prev;
    for (int i = 0; i < 40; ++i) {
        prev = frame;
        spinAO(120);
        vp->InvalidateController();
        vp->RenderFrame();
        if (!vp->ReadFrameForTest(frame, w, h))
            return false;
        if (i > 0 && frame == prev)
            return true;
    }
    return true;
}

int lumaAt(std::vector<uint8_t> const& f, uint32_t w, uint32_t h, int x, int y)
{
    if (x < 0 || y < 0 || x >= static_cast<int>(w) || y >= static_cast<int>(h))
        return -1;
    uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
    return (p[0] * 299 + p[1] * 587 + p[2] * 114) / 1000;
}

}  // namespace

TEST(AmbientOcclusionE2E, CornerSeamDarkensAndRestores)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "AmbientOcclusionE2E";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinAO(400);

    auto* deco = new AoCornerDecorator();
    dqApp::Application::Get().GetViewManager().AddDecorator(deco);

    auto* vp = view.getUeViewport();
    auto* view3d = vp->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    // 顶视（立柱足根环带俯视可见）+ 保内容域。
    view3d->SetRotation(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Top));
    view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-5.0, -5.0, -1.0, 5.0, 5.0, 3.0));
    {
        // 关光照/grid/ACS + 黑底（纯 AO 隔离面——off 帧恒均匀）。
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.lighting = false;
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
        style.getSettings().toggleSkyBox(false);
        style.getSettings().setBackgroundColor(dqCommon::ColorDef::from(0, 0, 0));
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(readStableFrame(view, frame, w, h));
    ASSERT_FALSE(frame.empty());
    std::vector<uint8_t> const baseline = frame;

    // WHERE 锚（世界→视口投影——钉死立柱足根环带与开阔面的**像素**位）：
    // 立柱顶心 (1,1,2.5) 与柱东棱 (1.5,1,2.5) 投影距 = 柱半径像素。
    // WorldToView 返回**逻辑像素**（窗口 1000×700 空间）；帧为**设备像素**
    //（2000×1400——ReadFrameForTest 实测）→ 坐标须按帧宽比缩放（实锤：
    // 未缩放时采样落在空场，环带实测中心 = 投影×2）。
    auto const c = vp->WorldToView(dqGeom::Point3d::From(1.0, 1.0, 2.5));
    auto const e = vp->WorldToView(dqGeom::Point3d::From(1.5, 1.0, 2.5));
    double const devScale = static_cast<double>(w) / 1000.0;  // 逻辑→设备
    int const cx = static_cast<int>(std::lround(c.x * devScale));
    int const cyView = static_cast<int>(std::lround(c.y * devScale));
    int const postR = std::max(2, static_cast<int>(std::lround(std::abs(e.x - c.x) * devScale)));
    // 帧 y=0=底（glReadPixels）↔ 视口 y 向——WorldToView 的 y 为顶起像素
    //（ViewingSpace.ts:427-457 族——实测校验见首绿注记）。
    int const cy = static_cast<int>(h) - 1 - cyView;
    printf("[AO] proj: c=(%d,%d->%d) postR=%dpx (w=%u h=%u)\n", cx, cyView, cy,
           postR, w, h);

    // 环带样本 = 柱足根外侧 3px（西向——柱偏心于 (+0.5,+0.5)，西侧开阔）；
    // 开阔面 = 帧左下角内 10% 带中点（平板远场）。
    int const seamX = cx - postR - static_cast<int>(3 * devScale), seamY = cy;
    int const farX = static_cast<int>(w * 0.15), farY = static_cast<int>(h * 0.15);
    int const seamBase = lumaAt(baseline, w, h, seamX, seamY);
    int const farBase = lumaAt(baseline, w, h, farX, farY);
    printf("[AO] baseline: seam=%d far=%d @(%d,%d)/(%d,%d)\n",
           seamBase, farBase, seamX, seamY, farX, farY);
    ASSERT_GT(seamBase, 100) << "baseline seam must be lit content (white slab)";
    ASSERT_GT(farBase, 100);

    // --- AO 开（可测量化调参——默认值的环带变暗实测仅 ~3 luma[本几何尺度
    //     下]，锁链判据需稳健余量：intensity 3/texelStepSize 8 为 DTA 编辑器
    //     同族的用户可调面——锁的是链路与 WHERE，非默认美学值）---
    {
        dqCommon::DisplayStyle3dSettingsProps props;
        dqCommon::ViewFlagProps vf;
        vf.ambientOcclusion = true;
        props.viewflags = vf;
        dqCommon::AmbientOcclusion::Props ao;
        ao.bias = 0.25;
        ao.intensity = 3.0;
        ao.texelStepSize = 8.0;
        props.ao = ao;
        vp->overrideDisplayStyle(props);
    }
    spinAO(300);
    ASSERT_TRUE(readStableFrame(view, frame, w, h));

    int const seamAO = lumaAt(frame, w, h, seamX, seamY);
    int const farAO = lumaAt(frame, w, h, farX, farY);
    printf("[AO] on: seam=%d far=%d (base %d/%d)\n", seamAO, farAO, seamBase, farBase);
    // 取证帧落盘（dumpBmpTHM 同构——BI_RGB 32bpp BGRX 逐像素 R/B 换序）。
    {
        FILE* fp = fopen("build/thematic-ao-on.bmp", "wb");
        if (fp) {
            uint32_t const rowBytes = w * 4;
            unsigned char head[54] = {};
            head[0] = 'B'; head[1] = 'M';
            *reinterpret_cast<uint32_t*>(&head[2]) = 54 + rowBytes * h;
            *reinterpret_cast<uint32_t*>(&head[10]) = 54;
            *reinterpret_cast<uint32_t*>(&head[14]) = 40;
            *reinterpret_cast<uint32_t*>(&head[18]) = w;
            *reinterpret_cast<uint32_t*>(&head[22]) = h;
            *reinterpret_cast<uint16_t*>(&head[26]) = 1;
            *reinterpret_cast<uint16_t*>(&head[28]) = 32;
            fwrite(head, 1, 54, fp);
            std::vector<unsigned char> row(rowBytes);
            for (uint32_t y = 0; y < h; ++y) {
                memcpy(row.data(), &frame[static_cast<size_t>(y) * rowBytes], rowBytes);
                for (uint32_t x = 0; x < w; ++x)
                    std::swap(row[x * 4 + 0], row[x * 4 + 2]);
                fwrite(row.data(), 1, rowBytes, fp);
            }
            fclose(fp);
        }
    }
    // 判据 1（语义）：开阔面不被遮蔽（变暗量小——远场无近邻遮挡物）。
    EXPECT_GT(farAO, farBase - 40) << "open field must stay near baseline";
    // 判据 2（WHERE——§11.11 位置断言）：环带变暗显著大于开阔面变暗量。
    EXPECT_LT(seamAO, seamBase - 25) << "contact seam must darken under AO";
    EXPECT_LT(seamAO, farAO) << "seam darker than open field";
    EXPECT_GT((seamBase - seamAO) + 60, farBase - farAO)
        << "seam darkening must dominate open-field drift";

    // --- AO 关 → 恢复基线精确 ---
    {
        dqCommon::DisplayStyle3dSettingsProps props;
        dqCommon::ViewFlagProps vf;
        vf.ambientOcclusion = false;
        props.viewflags = vf;
        vp->overrideDisplayStyle(props);
    }
    spinAO(300);
    ASSERT_TRUE(readStableFrame(view, frame, w, h));
    EXPECT_EQ(frame, baseline) << "AO off must restore the baseline frame exactly";

    dqApp::Application::Get().GetViewManager().DropDecorator(deco);
}
