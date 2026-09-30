// JoesHouseEdgeTest — JoesHouse 面板边线（visible edges）像素锁（M-I Task 4：
// 边线两段清偿——compact 边真窗口零碎片修复 + hline 边色覆盖）。
//
// 断点（侦察实锤 + 本任务插桩取证，docs/阶段1-MI-打开性能渲染对齐与拾取-实现计划
// -2026-09-29.md Task 4 / 侦察结论 4）：DTA 打开 JoesHouse 每块面板有黑色边线，
// DanQing 完全没有边线（连白边都没有）。两段根因：
//
//   B 段（绘制机制，深）：compact 边展开为 indexed 形态（CompactEdges.ts
//     indexedEdgeParamsFromCompactEdges）后派发正常（[EDGE] dispatch tech=6
//     IndexedEdge、skip=0——DANQING_EDGE_TRACE 实测 48 次/帧面），但着色器
//     main 缺参考的 qpos 协议——取证件 = DANQING_SHADER_DUMP 落盘的
//     build/vshader-1-Edge-Opaque-Overrides.glsl：
//       ①computeQuantizedPosition()（含 indexed 边表解码——设 g_quadIndex/
//         g_otherIndexIndex/g_normals/g_isSilhouette）被定义却**零调用**
//         （参考 ShaderBuilder.ts:759 `vec3 qpos = computeQuantizedPosition();`
//         是 main 首行）→ g_quadIndex 恒 0 → 每边 6 顶点 clip 位置全同 →
//         零面积三角形 → 0 片元；
//       ②g_vertexLUTIndex = decodeUInt24(a_pos) 用边表索引当顶点表索引
//         （参考 initializeVertLUTCoords Vertex.ts:20-23 消费 qpos）；
//       ③addAnimation 的 AdjustRawPosition 覆盖了 addVertexTable 的 LUT 解码
//         （参考 ShaderBuilder.ts:770-774 是 computeVertexPosition(qpos) 主行
//         + adjustRawPosition 复合，不是替换）。
//
//   A 段（字段搬运）：imodel.json defaultViewState.displayStyleProps.styles
//     .hline.visible = {color:0(黑), ovrColor:true, pattern:0, width:1} +
//     transThreshold:0.3 + viewflags visEdges:true（两模型 dump 实测在案），
//     但 hline 段从未进 ViewState（ViewStateProps.h 登记面）→ EdgeSettings 恒
//     默认（无覆盖）→ 边色 = 网格色（白）→ 白边落白面板不可见。
//     参考链：DisplayStyleSettings.ts:1104（ctor HiddenLine.Settings.fromJSON）
//     → RenderPlan.ts:124（hline = style.is3d() ? settings.hiddenLineSettings）
//     → Target.ts:533（uniforms.branch.changeRenderPlan(vf,is3d,hline)）
//     → BranchState.ts:93-96（edgeSettings.init(hline)）
//     → EdgeSettings.ts:48-52/:99-101/:140-147（isOverridden：SmoothShade 门
//       visEdges）→ Target.ts:607-610 computeEdgeColor（覆盖优先）。
//
// 判据（§11.11 位置断言制度）：joeshouse saved 等轴测视图（JoesHouseColor
// 同款打开链 + 泵至静默 + 帧稳定等待）——
//   ①面板白区（min 通道 ≥180 的近白填充 bbox——实测 (308,228)-(1552,1202)
//     @2000×1400）内存在**细黑线像素**：全通道 <90（黑）且横轴 ±2 双邻皆
//     ≥120 或纵轴 ±2 双邻皆 ≥120（=跨亮填充的 1px 细线形态。亮邻门槛 120
//     而非 150：面板边大多坐落白面↔中灰侧面[128-140 实测]交界——DTA 同构；
//     窗洞暗斑/背景 (25,25,25) 边界的另一侧是暗斑本体不满足双亮，红管
//     #7B0303/蓝管 #0707B0 单通道 >90 被全通道门槛排除——两干扰形态实测
//     修复前仅 30 枚噪声命中）；合格像素分布经叠图目检恰好描出屋面/女儿墙/
//     窗棂/墙板边界线（build/edge-hits2-post.png 取证在案）。
//     修复前该计数 = 30（边几何退化零片元；即便 B 段独修也是白边落白面）；
//   ②WHERE 分布：合格像素横跨 ≥200 个不同行与 ≥350 个不同列（边线遍布
//     全屋面板边界，非单点伪影——首绿实测 457 行/870 列）；
//   ③黑度：合格像素 max 通道均值 <60（首绿实测 26.4——hline color=0 纯黑
//     覆盖 + AA 过渡；SolidFill 对比灰 0.699×255≈178 或白边 255 都不满足，
//     钉死 A 段覆盖色而非任意边线）。
//
// Authored: no reference test exists in itwinjs-core for imdl compact-edge
//           window pixel output（参考的边缘渲染覆盖在浏览器 WebGL 无离线回放
//           对应物；渲染像素回归授权 §5(g)：复现配方 = 打开链 saved 视图 +
//           泵至静默；证据链 = dump imodel.json hline 字段实测 + [EDGE]
//           dispatch 轨迹 + DANQING_SHADER_DUMP 着色器落盘取证 + DTA 对拍图
//           build/mi2-dta-joe.png 面板黑边）。dump 资产只读（§11.11）；
//           钉值 = 首绿实测。
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
#include <set>
#include <vector>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {

struct QtEnvJHE {
    QtEnvJHE()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvJHE s_qtJHE;

std::string const kDumpRootJHE = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

void spinJHE(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

void dumpBmpJHE(std::vector<uint8_t> const& frame, uint32_t w, uint32_t h,
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
        // BI_RGB 32bpp 文件字节序 = BGRX，内存帧 = RGBA（glReadPixels GL_RGBA）
        // ——逐像素换 R/B 落盘（M-J(3) 仪器修正，RpcDumpRenderTest.cpp dumpBmp
        // 同注：历史 dump 的 R/B 互换曾伪造"实例球行色发散"取证）。
        for (uint32_t x = 0; x < w; ++x)
            std::swap(row[x * 4 + 0], row[x * 4 + 2]);
        fwrite(row.data(), 1, rowBytes, f);
    }
    fclose(f);
    printf("[JHE] frame dumped to %s\n", path);
}

uint8_t minChannel(std::vector<uint8_t> const& f, uint32_t w, uint32_t x, uint32_t y)
{
    uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
    return std::min({p[0], p[1], p[2]});
}

uint8_t maxChannel(std::vector<uint8_t> const& f, uint32_t w, uint32_t x, uint32_t y)
{
    uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
    return std::max({p[0], p[1], p[2]});
}

}  // namespace

// ---------------------------------------------------------------------------
// 锁：joeshouse saved 视图面板黑边线回归（compact 边上屏 + hline 黑色覆盖）。
// ---------------------------------------------------------------------------
TEST(JoesHouseEdge, PanelEdgesRenderBlackLinesFromHlineOverride)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "JoesHouseEdge";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinJHE(400);

    dta::DumpOpenPackage pkg;
    pkg.imodelRoot = kDumpRootJHE + "/joeshouse-v1";
    pkg.tileRoots = {kDumpRootJHE + "/joeshouse-v1", kDumpRootJHE + "/joeshouse-drill-v1",
                     kDumpRootJHE + "/joeshouse-drill-v2"};
    auto opened = dta::openDumpIModel(view, pkg);
    ASSERT_TRUE(opened.has_value()) << "open chain failed: " << pkg.imodelRoot;

    // 泵至静默（JoesHouseColor/DumpBrowse 同款强制选择帧形）。
    size_t lastLogSize = 0;
    int stable = 0;
    bool quiesced = false;
    for (int i = 0; i < 200 && !quiesced; ++i) {
        view.getUeViewport()->InvalidateController();
        spinJHE(100);
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

    // 帧稳定等待（连续两帧逐像素相等）。
    auto* vp = view.getUeViewport();
    vp->RenderFrame();
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    {
        std::vector<uint8_t> prev;
        bool frameStable = false;
        for (int i = 0; i < 40 && !frameStable; ++i) {
            spinJHE(150);
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
    dumpBmpJHE(frame, w, h,
               DANQING_TILE_ASSETS_DIR "/../../build/joeshouse-edge-saved.bmp");

    // 面板白区 bbox（近白填充 min≥180——saved 等轴测的屋面/墙面族）。
    uint32_t wx0 = w, wy0 = h, wx1 = 0, wy1 = 0;
    long whiteCount = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            if (minChannel(frame, w, x, y) >= 180) {
                ++whiteCount;
                wx0 = std::min(wx0, x);
                wy0 = std::min(wy0, y);
                wx1 = std::max(wx1, x);
                wy1 = std::max(wy1, y);
            }
        }
    ASSERT_GT(whiteCount, 10000l) << "no white panel region — scene did not render";
    printf("[JHE] white panel bbox=(%u,%u)-(%u,%u) count=%ld frame=%ux%u\n",
           wx0, wy0, wx1, wy1, whiteCount, w, h);

    // 细黑线像素：全通道 <90（黑——排除红/蓝管与 128+ 中灰面本身）且横或纵
    // ±2 双邻皆 ≥120（细线跨亮填充形态：面板边坐落白面↔中灰侧面交界，故
    // 亮邻门槛取 120 而非 150；窗洞暗斑/背景边界另一侧为暗部不满足双亮）。
    long edgePixels = 0;
    long long sumIntensity = 0;
    std::set<uint32_t> edgeRows, edgeCols;
    auto brightAt = [&](uint32_t x, uint32_t y) {
        return minChannel(frame, w, x, y) >= 120;
    };
    for (uint32_t y = wy0 + 2; y + 2 < wy1; ++y)
        for (uint32_t x = wx0 + 2; x + 2 < wx1; ++x) {
            if (maxChannel(frame, w, x, y) >= 90)
                continue;  // 非黑
            bool const horizBright = brightAt(x - 2, y) && brightAt(x + 2, y);
            bool const vertBright = brightAt(x, y - 2) && brightAt(x, y + 2);
            if (!horizBright && !vertBright)
                continue;  // 非细线（暗斑内部/暗背景上的线）
            ++edgePixels;
            sumIntensity += maxChannel(frame, w, x, y);
            edgeRows.insert(y);
            edgeCols.insert(x);
        }
    double const meanIntensity =
        edgePixels > 0 ? static_cast<double>(sumIntensity) / edgePixels : 255.0;
    printf("[JHE] edge-line pixels=%ld rows=%zu cols=%zu meanMaxChannel=%.1f\n",
           edgePixels, edgeRows.size(), edgeCols.size(), meanIntensity);

    // ①边线存在（修复前实测 = 30 噪声命中：indexed 边着色器 qpos 协议缺失
    //    → 6 顶点退化零片元；A 段未接时即便上屏也是白边落白面）。
    //    首绿实测 1540±1（终审独立复跑 1540——AA 抖动）；阈值 ≈ 0.45× 余量。
    //    M-M(1) 光照修复后重钉：u_sunDir legacy 双写拆除（方向光缺失 saga）→
    //    面板分面亮度重排——背光面 <120 使 ±2 亮邻门在暗面区失效，计数
    //    1540→319（边链完好：噪声 30 的 10×、均值 36.9 黑、行列仍铺开）。
    //    阈值 ≈ 0.45× 余量。
    EXPECT_GE(edgePixels, 150l)
        << "no black panel edge lines on panels — compact/indexed edges "
           "produce zero fragments (qpos protocol missing in function-call "
           "vertex main) or hline black override not wired";
    // ②WHERE 分布：边线遍布全屋面板边界（非单点伪影）。首绿实测
    //    457 行/870 列；M-M(1) 光照修复后 171 行/275 列（暗面区亮邻门失效，
    //    见①注）；阈值 ≈ 0.45× 余量。
    EXPECT_GE(edgeRows.size(), 80u)
        << "edge pixels not spread across rows — WHERE attribution failed";
    EXPECT_GE(edgeCols.size(), 130u)
        << "edge pixels not spread across columns — WHERE attribution failed";
    // ③黑度：hline.visible.color=0 纯黑覆盖（首绿实测 26.4；对比灰 178 /
    //    白边 255 均出局）。
    EXPECT_LT(meanIntensity, 60.0)
        << "edge lines are not black — hline visible color override "
           "(color:0, ovrColor:true) not consumed (EdgeSettings chain)";
}
