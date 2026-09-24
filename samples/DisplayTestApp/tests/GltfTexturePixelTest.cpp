// GltfTexturePixelTest — 导入带 baseColorTexture 的 BoxTextured 后，屏幕中心
// 像素须反映纹理内容（readPixels 断言内容存活，§12.8 教训 3）。
// Authored: no reference test exists（渲染输出级回归，参考无对应测试；spec §7.2 层 2）。
// 复现配方：File→Import BoxTextured.gltf == loadGltf() 路径（View3DInventor.cpp:353）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include <dqCommon/Image.h>

#include "View3DInventor.h"

#include <dqApp/Viewport.h>

#include <cmath>
#include <fstream>
#include <vector>

namespace { struct QtEnvR2 { QtEnvR2() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvR2 s_qtR2;

#ifndef DANQING_GLTF_ASSETS_DIR
#define DANQING_GLTF_ASSETS_DIR "."
#endif

namespace {
void spin2(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}
}  // namespace

TEST(GltfTexturePixel, TexturedBoxRendersTextureNotFlatColor)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin2(400);

    ASSERT_TRUE(view.loadGltf(DANQING_GLTF_ASSETS_DIR "/BoxTextured/BoxTextured.gltf"));
    // loadGltf 现按参考 GltfDecoration.ts:214 走 animateFrustumChange 渐进 fit
    //（从空白连接的大范围缩放到模型，~1000ms）——等动画收敛后再取帧。
    spin2(2000);
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));

    // 自校准判据：解码资产 PNG，取其网格均值色（期望主导色）；与屏幕中心带
    // 均值色比对。纹理上屏 → 中心带趋近纹理均值；标量降级/纹理丢失 → 停在
    // baseColorFactor(1,1,1) 白色或背景色，与纹理均值差显著。
    // 纹理文件名 = 上游真实名 CesiumLogoFlat.png（BoxTextured.gltf 引用之；
    // 规划期假设的 BoxTextured.png 在上游不存在，见 third_party/gltf-sample-assets/README.md）。
    std::ifstream pf(DANQING_GLTF_ASSETS_DIR "/BoxTextured/CesiumLogoFlat.png", std::ios::binary);
    ASSERT_TRUE(pf.is_open());
    std::vector<uint8_t> pngBytes((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());
    auto decoded = dqCommon::DecodeImage({ pngBytes, dqCommon::ImageSourceFormat::Png });
    ASSERT_TRUE(decoded.has_value());

    double texR = 0, texG = 0, texB = 0;
    size_t const n = static_cast<size_t>(decoded->width) * decoded->getHeight();
    ASSERT_GT(n, 0u);
    for (size_t i = 0; i < n; ++i) {
        texR += decoded->data[i * 4 + 0];
        texG += decoded->data[i * 4 + 1];
        texB += decoded->data[i * 4 + 2];
    }
    texR /= n * 255.0; texG /= n * 255.0; texB /= n * 255.0;

    // 纹理中心带均值（与屏幕中心带同相对宽度的空间对齐采样）。2026-09-15 取景
    // 修复（loadGltf 补传 viewRect.aspect，GltfDecoration.ts:213 对齐）后顶面完整
    // 可见——中心带落在 Cesium logo 的灰色波纹区，与整图均值（绿系）恒差 ≈0.35，
    // 不是纹理丢失。判据改为带-带对比（同一套自校准方法论，不随取景裁剪漂移）。
    int const tw = decoded->width, th = decoded->getHeight();
    double tbR = 0, tbG = 0, tbB = 0; size_t tbCnt = 0;
    for (int y = th / 2 - th / 20; y < th / 2 + th / 20; ++y) {
        for (int x = tw / 2 - tw / 20; x < tw / 2 + tw / 20; ++x) {
            if (x < 0 || y < 0 || x >= tw || y >= th) continue;
            size_t i = (static_cast<size_t>(y) * tw + x) * 4;
            tbR += decoded->data[i + 0]; tbG += decoded->data[i + 1]; tbB += decoded->data[i + 2];
            ++tbCnt;
        }
    }
    ASSERT_GT(tbCnt, 0u);
    tbR /= tbCnt * 255.0; tbG /= tbCnt * 255.0; tbB /= tbCnt * 255.0;

    // 屏幕中央盒均值（RGBA8）。取景修复后（aspect 撑宽到 1.49）立方体占屏
    // x∈[16.5%,83.5%]、y∈[2%,98%]——采样盒 x∈[25%,75%]、y∈[10%,90%] 严格落在
    // 顶面纹理内，不混天空边距。
    double fr = 0, fg = 0, fb = 0; size_t cnt = 0;
    for (uint32_t y = h / 10; y < h - h / 10; y += 2) {
        for (uint32_t x = w / 4; x < w - w / 4; x += 2) {
            size_t px = (static_cast<size_t>(y) * w + x) * 4;
            fr += frame[px + 0]; fg += frame[px + 1]; fb += frame[px + 2];
            ++cnt;
        }
    }
    ASSERT_GT(cnt, 0u);
    fr /= cnt * 255.0; fg /= cnt * 255.0; fb /= cnt * 255.0;

    // 校准/取证探针（[MAXGRID] 同款模式）：中心带均值、纹理均值、纹理中心带、背景角像素。
    size_t const corner = (static_cast<size_t>(h - 4) * w + 4) * 4;
    printf("[GLTFTEX] frame=%ux%u center=(%.3f,%.3f,%.3f) texMean=(%.3f,%.3f,%.3f) texBand=(%.3f,%.3f,%.3f) corner=(%d,%d,%d)\n",
           w, h, fr, fg, fb, texR, texG, texB, tbR, tbG, tbB,
           frame[corner], frame[corner + 1], frame[corner + 2]);

    // 判据：中央盒与纹理整图的 **L1 归一化色度**（色调方向）曼哈顿差 < 0.06。
    // 光照是均匀标量乘法，归一化后精确抵消——判据不随光照强度与取景裁剪漂移。
    // 校准依据（2026-09-15 取景修复后实测，[GLTFTEX] 探针）：
    //   纹理上屏 → chromaDist=0.036、lightFactor=0.622 PASS
    //   标量降级（baseColorFactor 白 (1,1,1)）→ dir(0.577,0.577,0.577) vs
    //     texMean dir(0.299,0.360,0.341) → ≈ 0.11 FAIL
    //   天空占满中央盒（箱体消失/纹理全丢）→ sky(142,205,255) dir(0.280,0.405,0.506)
    //     → ≈ 0.29 FAIL
    // 历史判据（中心带 vs 整图均值色差 0.228→0.97/2.03）在取景修复后失效两次：
    // 先是顶面被裁使中心带含绿色区、修复后中心带恒落 logo 波纹灰区 + 光照因子
    // 直乘（band 0.63×texBand → dist 0.985）——见 texBand 注释与 [GLTFTEX] 探针。
    double const fSum = fr + fg + fb;
    double const tSum = texR + texG + texB;
    ASSERT_GT(fSum, 1e-9);
    ASSERT_GT(tSum, 1e-9);
    double const chromaDist =
        std::abs(fr / fSum - texR / tSum) +
        std::abs(fg / fSum - texG / tSum) +
        std::abs(fb / fSum - texB / tSum);
    double const lightFactor = fSum / tSum;   // 取证：均匀光照标量（≈0.63）
    printf("[GLTFTEX] chromaDist=%.4f lightFactor=%.3f\n", chromaDist, lightFactor);
    EXPECT_LT(chromaDist, 0.06)
        << "frame box (" << fr << "," << fg << "," << fb << ") vs texture mean chroma ("
        << texR / tSum << "," << texG / tSum << "," << texB / tSum << ")";

    view.close();
    spin2(200);
}
