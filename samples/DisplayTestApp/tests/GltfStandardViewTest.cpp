// GltfStandardViewForensics — 标准视图（Front/Back）下 glTF 箱体取景居中性取证。
//
// 用户报告（2026-09-15 DanQing vs DTA 并排对比）：标准旋转视图第 5(Front)/6(Back) 个
// 按钮下，箱体相对 ACS 一个在上一个在下。两侧按钮序/图标/工具机制已静态核对一致
// （DtaToolBars.cpp:185 vs StandardRotations.ts:9-18；StandardViewTool 逐行移植）。
// 本测试程序化复现：装 BoxTextured 装饰（自动 LookAtVolume 取景）→ 切 Front/Back →
// 渲染 → 输出相机数值（rotation/eye/target）与箱体像素包围盒中心 vs 画面中心偏差。
// 若 DanQing 侧箱体在标准视图下不居中（偏离 > 画面高度 10%），断言失败 = 异常复现。
//
// Authored: no reference test exists in itwinjs-core for standard-view framing of
//           decorations; 行为锚定 ViewTool.ts:3503-3525（rotate about target）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"

#include <dqApp/StandardView.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewTool.h>
#include <dqApp/ViewState.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#ifndef DANQING_GLTF_ASSETS_DIR
#define DANQING_GLTF_ASSETS_DIR "."
#endif

namespace { struct QtEnvR3 { QtEnvR3() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvR3 s_qtR3;

namespace {
void spin3(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 画面中"内容像素"（与背景差异显著）的包围盒；返回是否找到。
bool contentBBox(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                 uint32_t& minX, uint32_t& maxX, uint32_t& minY, uint32_t& maxY)
{
    // 背景参考 = 左上角像素
    uint8_t const* bg = &f[0];
    minX = w; maxX = 0; minY = h; maxY = 0;
    bool any = false;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            int const dr = std::abs(p[0] - bg[0]), dg = std::abs(p[1] - bg[1]), db = std::abs(p[2] - bg[2]);
            if (dr + dg + db > 60) {
                any = true;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }
    return any;
}
}  // namespace

static void RunStandardViewCase(dqApp::StandardViewId id, char const* name)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin3(400);

    ASSERT_TRUE(view.loadGltf(DANQING_GLTF_ASSETS_DIR "/BoxTextured/BoxTextured.gltf"));
    spin3(300);
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin3(300);
    spin3(400);
    view.getUeViewport()->RenderFrame();

    // 工具前状态插桩：LookAtVolume 取景动画是否已收敛。
    {
        auto* v0 = view.getUeViewport()->GetView()->AsViewState3d();
        auto const t0 = v0->GetTargetPoint();
        auto const e0 = v0->getEyePoint();
        auto const n0 = view.getUeViewport()->WorldToNpc(t0);
        printf("[GLTFSV] %s PRE-tool target=(%.2f,%.2f,%.2f) eye=(%.2f,%.2f,%.2f) targetNpc=(%.3f,%.3f,%.3f) org=(%.2f,%.2f,%.2f) extents=(%.2f,%.2f,%.2f) camOn=%d\n",
               name, t0.x, t0.y, t0.z, e0.x, e0.y, e0.z, n0.x, n0.y, n0.z,
               v0->GetOrigin().x, v0->GetOrigin().y, v0->GetOrigin().z,
               v0->GetExtents().x, v0->GetExtents().y, v0->GetExtents().z,
               v0->IsCameraOn() ? 1 : 0);
    }
    spin3(1600);  // 等 LookAtVolume 动画完全收敛后再切视图

    // 工具输入取证：vp.getFrustum() 的 8 个世界角点（ViewTool.cpp:1848 的直接输入）。
    {
        auto fr = view.getUeViewport()->getFrustum(true);
        dqGeom::Point3d lo(1e30, 1e30, 1e30), hi(-1e30, -1e30, -1e30);
        for (int i = 0; i < 8; ++i) {
            printf("[GLTFSV] %s inputFrustum[%d]=(%.2f,%.2f,%.2f)\n", name, i,
                   fr.points[i].x, fr.points[i].y, fr.points[i].z);
            lo.x = std::min(lo.x, fr.points[i].x); lo.y = std::min(lo.y, fr.points[i].y); lo.z = std::min(lo.z, fr.points[i].z);
            hi.x = std::max(hi.x, fr.points[i].x); hi.y = std::max(hi.y, fr.points[i].y); hi.z = std::max(hi.z, fr.points[i].z);
        }
        printf("[GLTFSV] %s inputFrustum AABB lo=(%.2f,%.2f,%.2f) hi=(%.2f,%.2f,%.2f) size=(%.2f,%.2f,%.2f)\n",
               name, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z,
               hi.x - lo.x, hi.y - lo.y, hi.z - lo.z);
    }

    // 切标准视图（DtaToolBars.cpp:203 同款：StandardViewTool + run）。
    auto* tool = new dqApp::StandardViewTool(view.getUeViewport(), id);
    if (!tool->run())
        delete tool;
    spin3(1500);  // 等待 animateFrustumChange 动画收敛
    view.getUeViewport()->RenderFrame();

    // 相机数值取证。
    auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    auto const rot = view3d->getRotation();
    auto const eye = view3d->getEyePoint();
    auto const target = view3d->GetTargetPoint();
    printf("[GLTFSV] %s cameraOn=%d focus=%.3f org=(%.2f,%.2f,%.2f) extents=(%.2f,%.2f,%.2f)\n",
           name, view3d->IsCameraOn() ? 1 : 0, view3d->getFocusDistance(),
           view3d->GetOrigin().x, view3d->GetOrigin().y, view3d->GetOrigin().z,
           view3d->GetExtents().x, view3d->GetExtents().y, view3d->GetExtents().z);
    printf("[GLTFSV] %s rot=(%.3f %.3f %.3f / %.3f %.3f %.3f / %.3f %.3f %.3f) eye=(%.2f,%.2f,%.2f) target=(%.2f,%.2f,%.2f)\n",
           name,
           rot.coffs[0], rot.coffs[1], rot.coffs[2],
           rot.coffs[3], rot.coffs[4], rot.coffs[5],
           rot.coffs[6], rot.coffs[7], rot.coffs[8],
           eye.x, eye.y, eye.z, target.x, target.y, target.z);

    // 像素居中性取证。
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));

    // 存 BMP 供人工目检（无压缩，直接写）。
    {
        char path[128];
        snprintf(path, sizeof(path), "build/gltf-standardview-%s.bmp", name);
        FILE* f = fopen(path, "wb");
        if (f) {
            uint32_t const rowBytes = w * 4;
            uint32_t const dataSize = rowBytes * h;
            uint32_t const fileHead = 54;
            unsigned char head[54] = {};
            head[0] = 'B'; head[1] = 'M';
            *reinterpret_cast<uint32_t*>(&head[2]) = fileHead + dataSize;
            *reinterpret_cast<uint32_t*>(&head[10]) = fileHead;
            *reinterpret_cast<uint32_t*>(&head[14]) = 40;
            *reinterpret_cast<uint32_t*>(&head[18]) = w;
            *reinterpret_cast<uint32_t*>(&head[22]) = h;
            *reinterpret_cast<uint16_t*>(&head[26]) = 1;
            *reinterpret_cast<uint16_t*>(&head[28]) = 32;
            fwrite(head, 1, 54, f);
            std::vector<unsigned char> row(rowBytes);
            // 行序：ReadFrameForTest = glReadPixels 语义（frame 行 0 = 屏幕**底**行）。
            // BMP 自底向上（文件行 0 = 图像底行）——正序写入 frame[0..h-1] 恰好得到
            // 与屏幕一致的显示。此前倒序写入（假设 frame 行 0 = 屏顶）令所有取证
            // BMP 上下颠倒（2026-09-15 贴图朝向 saga：制造了"Z/Y 面 V 翻转、X 面
            // 正确"的幻影分裂——横向特征不受垂直翻转影响）。
            // （BGRA 视为 RGBA 写入——仅人工目检色彩通道序差异可接受。）
            for (uint32_t y = 0; y < h; ++y) {
                memcpy(row.data(), &frame[static_cast<size_t>(y) * rowBytes], rowBytes);
                fwrite(row.data(), 1, rowBytes, f);
            }
            fclose(f);
            printf("[GLTFSV] %s frame saved to %s\n", name, path);
        }
    }
    uint32_t minX, maxX, minY, maxY;
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY));
    double const cx = (minX + maxX) / 2.0, cy = (minY + maxY) / 2.0;
    printf("[GLTFSV] %s content bbox=(%u,%u)-(%u,%u) center=(%.1f,%.1f) frame center=(%.1f,%.1f) dy=%.1f%% dx=%.1f%%\n",
           name, minX, minY, maxX, maxY, cx, cy, w / 2.0, h / 2.0,
           100.0 * (cy - h / 2.0) / h, 100.0 * (cx - w / 2.0) / w);

    // 断言：标准视图绕 target 旋转后，内容（箱体+ACS）应保持画面居中。
    // 阈值 10% 画面高/宽（LookAtVolume 有视角 padding，内容略小于画面）。
    EXPECT_LT(std::abs(cy - h / 2.0), h * 0.10)
        << name << ": content vertically off-center after standard view";
    EXPECT_LT(std::abs(cx - w / 2.0), w * 0.10)
        << name << ": content horizontally off-center after standard view";

    view.close();
    spin3(200);
}

TEST(GltfStandardView, FrontViewKeepsCubeCentered)
{
    RunStandardViewCase(dqApp::StandardViewId::Front, "Front");
}

TEST(GltfStandardView, BackViewKeepsCubeCentered)
{
    RunStandardViewCase(dqApp::StandardViewId::Back, "Back");
}

// 全 8 视图取证（贴图朝向 saga）：与 DTA 同序列截图对照（build/dta-view-*.png）。
TEST(GltfStandardView, TopViewTexture)
{
    RunStandardViewCase(dqApp::StandardViewId::Top, "Top");
}

TEST(GltfStandardView, BottomViewTexture)
{
    RunStandardViewCase(dqApp::StandardViewId::Bottom, "Bottom");
}

TEST(GltfStandardView, LeftViewTexture)
{
    RunStandardViewCase(dqApp::StandardViewId::Left, "Left");
}

TEST(GltfStandardView, RightViewTexture)
{
    RunStandardViewCase(dqApp::StandardViewId::Right, "Right");
}

TEST(GltfStandardView, IsoViewTexture)
{
    RunStandardViewCase(dqApp::StandardViewId::Iso, "Iso");
}

TEST(GltfStandardView, RightIsoViewTexture)
{
    RunStandardViewCase(dqApp::StandardViewId::RightIso, "RightIso");
}

// 深度反转回归锁（2026-09-16 贴图镜像 saga，取证见 build/gltf-mirror-forensics.md）：
// 正交投影 m22 必须为负（参考 FrustumUniforms.changeFrustum → lookIn+ortho(0,depth)，
// 近平面→depth 0）。曾由 setViewportTransform 把 worldToNdc（m22 恒正）当投影 →
// LEQUAL 下最远面获胜 → Top 视图渲染 -Z 面（其 u=1@世界+X 与 +Z 面相反）→ 贴图呈
// 左右镜像的假象。BoxTexturedDots（DanQing-authored，见资产 README）的蓝/红点是
// 无歧义面朝向标记：+Z 面忠实场 = 蓝点在面右侧、红点在左侧；深度反转时互换。
//
// Authored: no reference test exists in itwinjs-core for viewport depth ordering
//           of decorations (browser WebGL 渲染无此缺陷面)。
TEST(GltfStandardView, TopViewRendersTopFaceNotBottom)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin3(400);

    ASSERT_TRUE(view.loadGltf(DANQING_GLTF_ASSETS_DIR "/BoxTexturedDots/BoxTextured.gltf"));
    spin3(300);
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin3(1600);  // 等 LookAtVolume 动画收敛

    auto* tool = new dqApp::StandardViewTool(view.getUeViewport(), dqApp::StandardViewId::Top);
    if (!tool->run())
        delete tool;
    spin3(1500);
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
    uint32_t minX, maxX, minY, maxY;
    ASSERT_TRUE(contentBBox(frame, w, h, minX, maxX, minY, maxY));
    double const faceCx = (minX + maxX) / 2.0;

    // 蓝/红 blob 质心（row0=屏幕底）。
    double bsx = 0, bsy = 0; long bn = 0;
    double rsx = 0; long rn = 0;
    for (uint32_t y = minY; y <= maxY; ++y)
        for (uint32_t x = minX; x <= maxX; ++x) {
            uint8_t const* p = &frame[(static_cast<size_t>(y) * w + x) * 4];
            int const r = p[0], g = p[1], b = p[2];
            if (b > 120 && b > r + 60 && b > g + 60) { bsx += x; bsy += y; ++bn; }
            else if (r > 120 && r > b + 60 && r > g + 60) { rsx += x; ++rn; }
        }
    ASSERT_GT(bn, 30) << "blue dot not found on top face";
    ASSERT_GT(rn, 30) << "red dot not found on top face";
    bsx /= bn; rsx /= rn; bsy /= bn;
    printf("[DEPTHLOCK] blue=(%.0f,%.0f) red=(%.0f,*) faceCx=%.0f frame=%ux%u\n",
           bsx, bsy, rsx, faceCx, w, h);

    // +Z 面（face0）忠实场：蓝点（image 左上，col≈0.05 → u≈0.05 → 世界 x≈+0.425）
    // 在面右侧；红点在左侧。深度反转（看到 -Z 面 face5）时互换。
    EXPECT_GT(bsx, faceCx) << "depth inverted: top view shows the BOTTOM face "
                              "(ortho m22>0 — see build/gltf-mirror-forensics.md)";
    EXPECT_LT(rsx, faceCx) << "depth inverted: top view shows the BOTTOM face";
    // v 不受深度影响：两 dot 的 image 顶行 → v≈0.05 → 世界 y≈-0.45 → 屏幕下半。
    EXPECT_LT(bsy, (minY + maxY) / 2.0) << "blue dot not in lower half (v axis changed?)";

    view.close();
    spin3(200);
}
