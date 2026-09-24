// GltfNormalMapPixelTest — 导入带 normalTexture 的 BoxNormalMap 后，不对称法线图
//（左半 n=+0.7x、右半 n=−0.7x）必须在屏幕左右产生光照不对称（readPixels 断言，
// §12.8 教训 3 + §11.11 位置断言：不对称标记 → 左右明暗差）。
// Authored: no reference test exists（渲染输出级回归，参考无对应测试；行为锚定
// GltfReader.ts findTextureMapping(:2586-2597 greenUp 恒 true) + Surface.ts
// finalizeNormalNormalMap(:412-442 TBN)+ u_normalMapScale(:541-550 greenUp 取负)。
// 复现配方：File→Import BoxNormalMap.gltf == loadGltf() 路径（View3DInventor.cpp:353）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"

#include <dqApp/Viewport.h>

#include <cmath>
#include <cstdio>
#include <vector>

namespace { struct QtEnvNM { QtEnvNM() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvNM s_qtNM;

#ifndef DANQING_GLTF_ASSETS_DIR
#define DANQING_GLTF_ASSETS_DIR "."
#endif

namespace {
void spinNM(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}
}  // namespace

TEST(GltfNormalMapPixel, AsymmetricNormalMapShadesLeftRightUnevenly)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinNM(400);

    ASSERT_TRUE(view.loadGltf(DANQING_GLTF_ASSETS_DIR "/BoxNormalMap/BoxNormalMap.gltf"));
    // loadGltf 按参考 GltfDecoration.ts:214 走 animateFrustumChange 渐进 fit
    //（~1000ms）——等动画收敛后再取帧（GltfTexturePixelTest 同款时序）。
    spinNM(2000);
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));

    // 采样区域与 GltfTexturePixelTest 的中央盒一致（x∈[25%,75%]、y∈[10%,90%]
    // 严格落在立方体上，不混天空边距），分左右两半统计亮度。
    double lSum = 0, rSum = 0;
    size_t lCnt = 0, rCnt = 0;
    for (uint32_t y = h / 10; y < h - h / 10; y += 2) {
        for (uint32_t x = w / 4; x < w - w / 4; x += 2) {
            size_t px = (static_cast<size_t>(y) * w + x) * 4;
            double const lum = 0.299 * frame[px + 0] + 0.587 * frame[px + 1] + 0.114 * frame[px + 2];
            if (x < w / 2) { lSum += lum; ++lCnt; }
            else           { rSum += lum; ++rCnt; }
        }
    }
    ASSERT_GT(lCnt, 0u);
    ASSERT_GT(rCnt, 0u);
    double const L = lSum / lCnt;
    double const R = rSum / rCnt;

    printf("[NM] frame=%ux%u leftLum=%.2f rightLum=%.2f diff=%.2f\n",
           w, h, L, R, std::abs(L - R));

    // 判据（WHERE 断言）：法线贴图起效 → 左右两半亮度显著不等（不对称标记：
    // 法线图左半 +X、右半 −X，方向光下必产生一侧偏亮一侧偏暗）。
    // 法线未用上（HasNormalMap=0 / 纹理未绑 / 门关）→ 左右都由几何法线决定 → 相等。
    // 阈值校准（2026-09-17 [NM] 探针实测，2000x1400）：
    //   法线起效（全链接通）→ leftLum=154.35 rightLum=145.00 diff=9.35
    //   法线断链（修复前实录，两轮）→ diff=0.00（解析断 / viewFlags 未进
    //     branch stack 致 wantNormalMaps 的 SmoothShade 门关，左右完全相等）
    //   幅度说明：法线 (±0.7,0,0.7) 仅偏转 45°，响应幅度取决于太阳方向与
    //   切向量的几何关系——9.35 是光照几何的结果，不是链路部分起效。
    double const kMinDiff = 4.0;
    EXPECT_GT(std::abs(L - R), kMinDiff)
        << "leftLum=" << L << " rightLum=" << R
        << " — normal map not applied (left/right equally lit)";

    view.close();
    spinNM(200);
}
