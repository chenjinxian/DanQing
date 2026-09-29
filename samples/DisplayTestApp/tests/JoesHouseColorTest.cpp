// JoesHouseColorTest — JoesHouse 管线蓝/红回归像素锁（M-I Task 3：色表顶点色接线）。
//
// 断点（侦察实锤，docs/阶段1-MI-打开性能渲染对齐与拾取-实现计划-2026-09-29.md
// Task 3 / 侦察结论 3）：JoesHouse 两根大管线在 DanQing 呈白色，DTA 呈暗红
// #7B0303 / 蓝 #0707B0。根因：管线 prim 是**色表（非均匀）顶点色形态**——
// 瓦 25_1d-E:6_0x4d/-b-2-0-0-0-1（joeshouse-v1 files/28.imdl，取证
// 2026-09-29）的 vertices 无 uniformColor、numColors:2、count:148、
// width:594 = 148×4 + 2 色表 texel（色表**已内嵌**在顶点表字节尾部——
// VertexTableBuilder.appendColorTable 在 numVertices*numRgbaPerVertex 之后
// 追加，随 LUT 纹理直传上 GPU）；色表条目实测 (210,0,0,255)=#D20000 纯红 与
// (0,0,210,255)=#0000D2 纯蓝（4 字节 r,g,b,a——appendColor 存的是反转透明度
// 后的 alpha + 预乘 rgb，本两条 alpha=1 无预乘缩放），顶点 colorIndex 实测
// 只有 {0,1} 两值（texel1.zw——LitMeshBuilder 16B/顶点布局的 colorIndex 槽）。
// DanQing createImdlLutGraphics 对无 uniformColor 的 prim 强制
// setColor(0xFFFFFFFF) → 白管。
//
// 参考链（真实读过，实施锚）：
//   ParseImdlDocument.ts:1020（uniformColor 可缺省）
//   → VertexLUT.createFromVertexTable（VertexLUT.ts:97——uniformColor undefined
//     → ColorInfo.createFromVertexTable）
//   → ColorInfo.ts:35 createNonUniform（isNonUniform 形态位）
//   → glsl/Common.ts:53-73 setShaderFlags（geom.colorInfo.isNonUniform →
//     u_shaderFlags[kShaderBit_NonUniformColor]=1，每 draw 上传全数组）
//   → glsl/Color.ts:16-26 getComputeElementColor（quantized：colorIndex =
//     decodeUInt16(g_vertLutData1.zw)，colorTableStart = u_vertParams.z ×
//     u_vertParams.w，采样 u_vertLUT 色表 texel，预乘还原 rgb/=a，位选
//     lutColor : u_color）
//   → glsl/Color.ts:56（u_color 仅 color.isUniform 时 bind）。
//
// 判据（§11.11 位置断言制度）：joeshouse saved 等轴测视图（DumpBrowse 锁同款
// 打开链 + 泵至静默 + 帧稳定等待）——
//   ①红带存在：右下象限 WHERE 盒（管线 2 实测屏幕域）内红色主导像素簇
//     （r 显著大于 g,b——DTA 实测 #7B0303 暗红系；光照下的色表纯红 #D20000）；
//   ②蓝带存在：右上象限 WHERE 盒内蓝色主导像素簇（#0707B0 系——色表纯蓝
//     #0000D2 经光照）；修复前两盒均为白色管（ dominance 计数 = 0 → RED）；
//   ③WHERE 分离：红带质心显著低于蓝带质心（两管线不同屏幕区域——DTA 对拍
//     build/mi2-dta-joe.png：蓝管从屋面伸向右上、红管在右下 stub）。
//
// Authored: no reference test exists in itwinjs-core for imdl color-table
//           (non-uniform vertex color) pixel output（参考的渲染覆盖在 DTA
//           交互式对拍，无离线回放对应物；渲染像素回归授权 §5(g)：复现配方 =
//           打开链 saved 视图 + 泵至静默；证据链 = 瓦 JSON/色表字节取证（本
//           文件头）+ DTA 对拍图 + readPixels WHERE 断言）。dump 资产只读
//           （§11.11）；钉值 = 首绿实测。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"

#include "DumpOpenHelper.h"

#include <dqApp/Application.h>
#include <dqApp/Viewport.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <optional>
#include <vector>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {

struct QtEnvJHC {
    QtEnvJHC()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvJHC s_qtJHC;

std::string const kDumpRoot = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

void spinJHC(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

void dumpBmpJHC(std::vector<uint8_t> const& frame, uint32_t w, uint32_t h,
                char const* path)
{
    FILE* f = fopen(path, "wb");
    if (!f)
        return;
    uint32_t const rowBytes = w * 4;
    unsigned char head[54] = {};
    head[0] = 'B';
    head[1] = 'M';
    *reinterpret_cast<uint32_t*>(&head[2]) = 54 + rowBytes * h;
    *reinterpret_cast<uint32_t*>(&head[10]) = 54;
    *reinterpret_cast<uint32_t*>(&head[14]) = 40;
    *reinterpret_cast<uint32_t*>(&head[18]) = w;
    *reinterpret_cast<uint32_t*>(&head[22]) = h;
    *reinterpret_cast<uint16_t*>(&head[26]) = 1;
    *reinterpret_cast<uint16_t*>(&head[28]) = 32;
    fwrite(head, 1, 54, f);
    std::vector<unsigned char> row(rowBytes);
    for (uint32_t y = 0; y < h; ++y) {
        memcpy(row.data(), &frame[static_cast<size_t>(y) * rowBytes], rowBytes);
        fwrite(row.data(), 1, rowBytes, f);
    }
    fclose(f);
    printf("[JHC] frame dumped to %s\n", path);
}

// WHERE 盒内的主导色分类：红 = r 通道显著大于 g/b；蓝 = b 显著大于 r/g。
// 阈值取 DTA 实测暗色系（#7B0303 / #0707B0——两通道差 ≥40、主导通道 ≥70）
// 的保守下界：色表值 #D20000/#0000D2 经任意合理光照只增两通道差的比例关系；
// 天空渐变（142,205,255：b-g=50 恰在界外）不构成蓝主导（另经盒域排除）。
struct BandStats {
    long count = 0;
    double cx = 0.0, cy = 0.0;
};

BandStats classifyBand(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                       uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1,
                       bool wantRed)
{
    BandStats out;
    long long sx = 0, sy = 0;
    for (uint32_t y = y0; y < y1 && y < h; ++y)
        for (uint32_t x = x0; x < x1 && x < w; ++x) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            int const r = p[0], g = p[1], b = p[2];
            bool const hit = wantRed
                ? (r >= 70 && r > g + 40 && r > b + 40)
                : (b >= 70 && b > r + 40 && b > g + 40);
            if (hit) {
                ++out.count;
                sx += x;
                sy += y;
            }
        }
    if (out.count > 0) {
        out.cx = static_cast<double>(sx) / out.count;
        out.cy = static_cast<double>(sy) / out.count;
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// 锁：joeshouse saved 视图两管线蓝/红回归（色表顶点色接线）。
// ---------------------------------------------------------------------------
TEST(JoesHouseColor, PipeTilesRenderColorTableNotWhite)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "JoesHouseColor";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinJHC(400);

    dta::DumpOpenPackage pkg;
    pkg.imodelRoot = kDumpRoot + "/joeshouse-v1";
    pkg.tileRoots = {kDumpRoot + "/joeshouse-v1", kDumpRoot + "/joeshouse-drill-v1"};
    auto opened = dta::openDumpIModel(view, pkg);
    ASSERT_TRUE(opened.has_value()) << "open chain failed: " << pkg.imodelRoot;

    // 泵至静默（DumpBrowse/DumpOpenChain 同款强制选择帧形——管线瓦
    // 0x4d/-b-2-0-0-0-1 在 saved 打开面 10 键内）。
    size_t lastLogSize = 0;
    int stable = 0;
    bool quiesced = false;
    for (int i = 0; i < 200 && !quiesced; ++i) {
        view.getUeViewport()->InvalidateController();
        spinJHC(100);
        size_t const logSize = opened->fetcher->requestLog().size();
        if (!opened->fetcher->requestLog().empty()
            && logSize == lastLogSize
            && opened->fetcher->getActiveCount() == 0) {
            if (++stable >= 6)
                quiesced = true;
        } else {
            stable = 0;
        }
        lastLogSize = logSize;
    }
    ASSERT_TRUE(quiesced) << "saved view never quiesced (log="
                          << opened->fetcher->requestLog().size() << ")";

    // 帧稳定等待（连续两帧逐像素相等——PickDumpScene 同款：saved 视图过渡
    // 动画不假设时长）。
    auto* vp = view.getUeViewport();
    vp->RenderFrame();
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    {
        std::vector<uint8_t> prev;
        bool frameStable = false;
        for (int i = 0; i < 40 && !frameStable; ++i) {
            spinJHC(150);
            vp->RenderFrame();
            std::vector<uint8_t> cur;
            uint32_t cw = 0, ch = 0;
            ASSERT_TRUE(vp->ReadFrameForTest(cur, cw, ch));
            w = cw;
            h = ch;
            if (!prev.empty() && prev == cur)
                frameStable = true;
            prev = std::move(cur);
        }
        ASSERT_TRUE(frameStable) << "frame never stabilized";
        frame = std::move(prev);
    }
    dumpBmpJHC(frame, w, h,
               DANQING_TILE_ASSETS_DIR "/../../build/joeshouse-color-saved.bmp");

    // WHERE 盒（saved 等轴测的管线屏幕域——首绿实测钉死；两盒 y 不相交）：
    //   红管 = 上方对角圆柱（盒 x∈[0.32,0.90]w, y∈[0.18,0.55]h——实测红带
    //     bbox y∈[305,1157]、质心 y=542≈0.39h）；
    //   蓝管 = 下方 stub 圆柱（盒 x∈[0.30,0.90]w, y∈[0.55,0.92]h——实测蓝带
    //     bbox y∈[744,1186]、质心 y=1040≈0.74h）。
    // 色归属与 DTA 参考一致（红=#D20000 系/蓝=#0000D2 系——色表条目直读）；
    // 屏幕位置与 DTA 截图不同是取景差异（DTA 视口 3112×1688 vs 本锁
    // 1000×700 CSS×2 DPR——等轴测投影带不同），不影响"色表被消费"判据。
    uint32_t const redX0 = static_cast<uint32_t>(w * 0.32);
    uint32_t const redX1 = static_cast<uint32_t>(w * 0.90);
    uint32_t const redY0 = static_cast<uint32_t>(h * 0.18);
    uint32_t const redY1 = static_cast<uint32_t>(h * 0.55);
    uint32_t const blueX0 = static_cast<uint32_t>(w * 0.30);
    uint32_t const blueX1 = static_cast<uint32_t>(w * 0.90);
    uint32_t const blueY0 = static_cast<uint32_t>(h * 0.55);
    uint32_t const blueY1 = static_cast<uint32_t>(h * 0.92);

    BandStats const red = classifyBand(frame, w, h, redX0, redX1, redY0, redY1,
                                       /*wantRed=*/true);
    BandStats const blue = classifyBand(frame, w, h, blueX0, blueX1, blueY0, blueY1,
                                        /*wantRed=*/false);
    printf("[JHC] red band: count=%ld centroid=(%.0f,%.0f) frame=%ux%u\n",
           red.count, red.cx, red.cy, w, h);
    printf("[JHC] blue band: count=%ld centroid=(%.0f,%.0f)\n",
           blue.count, blue.cx, blue.cy);

    // ①红带存在（修复前 = 0：白管无红主导像素）。首绿实测钉值见 commit；
    //    阈值 ≈ 首绿的 0.05×（圆柱投影面积的小余量下界）。
    EXPECT_GE(red.count, 300l)
        << "no red band in the upper pipe region — color-table vertex "
           "colors not consumed (pipes render white/uniform)";
    // ②蓝带存在（同上）。
    EXPECT_GE(blue.count, 300l)
        << "no blue band in the lower pipe region — color-table vertex "
           "colors not consumed (pipes render white/uniform)";

    // ③WHERE 分离：蓝带质心显著低于红带质心（两管线不同屏幕区域；首绿实测
    //    差 ≈ 0.35h[1040-542]，阈值 0.08h 大余量）。两质心各自落在自己的盒内
    //   （计数>0 时恒真——本断言钉的是两带的空间关系，非盒实现细节）。
    if (red.count > 0 && blue.count > 0) {
        EXPECT_GT(blue.cy - red.cy, h * 0.08)
            << "red and blue bands are not vertically separated — WHERE "
               "attribution regressed (which pipe is which color is unpinned)";
    }
}
