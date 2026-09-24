// GltfParityHarness — 同一路径 glb × 双实现渲染对拍（2026-09-16）。
//
// 方法论（§11.11）：同文件绝对路径（每轮 MD5 记录）、内容互证（bbox 存在性 +
// 色彩分区直方图，非逐像素——AA/色彩管理不可比）、面朝向探针（标准视图方位）。
// 与 DTA 侧的 Page.captureScreenshot+canvas 裁剪产物（build/parity-dta-<model>.png）
// 做摘要对比。
//
// Authored: no reference test exists in itwinjs-core for dual-implementation
//           rendering parity (§5(g) 授权的像素级回归)。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"
#include <dqApp/StandardView.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewTool.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace { struct QtEnvP { QtEnvP() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvP s_qtP;

namespace {

void spinP(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

bool contentBBoxP(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                  uint32_t& minX, uint32_t& maxX, uint32_t& minY, uint32_t& maxY)
{
    uint8_t const* bg = &f[0];
    minX = w; maxX = 0; minY = h; maxY = 0;
    bool any = false;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            if (std::abs(int(p[0]) - bg[0]) + std::abs(int(p[1]) - bg[1]) + std::abs(int(p[2]) - bg[2]) > 60) {
                any = true;
                if (x < minX) minX = x; if (x > maxX) maxX = x;
                if (y < minY) minY = y; if (y > maxY) maxY = y;
            }
        }
    return any;
}

void saveBMP(char const* path, std::vector<uint8_t> const& f, uint32_t w, uint32_t h)
{
    FILE* fp = fopen(path, "wb");
    if (!fp) return;
    uint32_t const rowBytes = w * 4, dataSize = rowBytes * h, head = 54;
    unsigned char h54[54] = {};
    h54[0] = 'B'; h54[1] = 'M';
    *reinterpret_cast<uint32_t*>(&h54[2]) = head + dataSize;
    *reinterpret_cast<uint32_t*>(&h54[10]) = head;
    *reinterpret_cast<uint32_t*>(&h54[14]) = 40;
    *reinterpret_cast<uint32_t*>(&h54[18]) = w;
    *reinterpret_cast<uint32_t*>(&h54[22]) = h;
    *reinterpret_cast<uint16_t*>(&h54[26]) = 1;
    *reinterpret_cast<uint16_t*>(&h54[28]) = 32;
    fwrite(h54, 1, 54, fp);
    fwrite(f.data(), 1, dataSize, fp);  // row0 = screen bottom（BMP 底向上 ✓）
    fclose(fp);
}

// 3×3×3 色彩分区直方图（每分区像素占比）；与 DTA 摘要同构。
struct Hist { long cells[27]; long total; };
Hist histOf(std::vector<uint8_t> const& f, uint32_t w, uint32_t /*h*/,
            uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1)
{
    Hist hist; for (auto& c : hist.cells) c = 0; hist.total = 0;
    for (uint32_t y = y0; y < y1; y += 2)
        for (uint32_t x = x0; x < x1; x += 2) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            int const r = p[0] / 86, g = p[1] / 86, b = p[2] / 86;  // 0..2
            ++hist.cells[r * 9 + g * 3 + b];
            ++hist.total;
        }
    return hist;
}

}  // namespace

// 单模型全视图采集：产物 build/parity-danqing-<model>-<view>.bmp + 摘要行。
// rootDir 含尾随分隔符；model 是不带扩展名的文件名。
static void RunModelViews(char const* rootDir, char const* model, bool keepBmp)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinP(400);
    std::string path = std::string(rootDir) + model + ".glb";
    printf("[PARITY] model=%s\n", model);
    if (!view.loadGltf(QString::fromStdString(path))) {
        printf("[PARITY] %s LOAD-FAIL\n", model);
        ADD_FAILURE() << model << " failed to load";
        view.close();
        return;
    }
    spinP(300);
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false; p.acsTriad = false; p.backgroundMap = false;  // 关背景图——
        // bgMap 椭球深度（ECEF 米制 ~3 万）混入 viewingSpace z 平面会把装饰盒
        // 发散到不可见（视图切换以 viewingSpace 盒为下次旋转输入，滚雪球）。
        // 判据对象是 glTF 装饰渲染，不是背景图（参考侧同配置同行为）。
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spinP(1600);

    // 等动画收敛：view rotation 连续两帧不再变化（frustum 会在动画尾段微动，
    // 用 rotation 作判据更稳）。超时保底。
    auto waitSettle = [&]() {
        double prevR0 = -99.0, prevR4 = -99.0;
        for (int i = 0; i < 60; ++i) {
            spinP(60);
            view.getUeViewport()->RenderFrame();
            auto* v3 = view.getUeViewport()->GetView()->AsViewState3d();
            double const r0 = v3->getRotation().coffs[0], r4 = v3->getRotation().coffs[4];
            if (std::abs(r0 - prevR0) < 1e-9 && std::abs(r4 - prevR4) < 1e-9)
                return;
            prevR0 = r0; prevR4 = r4;
        }
    };

    struct { dqApp::StandardViewId id; char const* name; } views[] = {
        {dqApp::StandardViewId::Top, "Top"}, {dqApp::StandardViewId::Bottom, "Bottom"},
        {dqApp::StandardViewId::Front, "Front"}, {dqApp::StandardViewId::Back, "Back"},
        {dqApp::StandardViewId::Left, "Left"}, {dqApp::StandardViewId::Right, "Right"},
        {dqApp::StandardViewId::Iso, "Iso"}, {dqApp::StandardViewId::RightIso, "RightIso"},
    };
    for (auto const& v : views) {
        auto* tool = new dqApp::StandardViewTool(view.getUeViewport(), v.id);
        if (!tool->run()) delete tool;
        waitSettle();
        spinP(200);
        view.getUeViewport()->RenderFrame();
        std::vector<uint8_t> frame; uint32_t w = 0, h = 0;
        ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));
        uint32_t minX, maxX, minY, maxY;
        bool found = contentBBoxP(frame, w, h, minX, maxX, minY, maxY);
        if (keepBmp) {
            char path2[160];
            snprintf(path2, sizeof(path2), "build/parity-danqing-%s-%s.bmp", model, v.name);
            saveBMP(path2, frame, w, h);
        }
        if (found) {
            auto hist = histOf(frame, w, h, minX, maxX + 1, minY, maxY + 1);
            printf("[PARITY] %s/%s bbox=(%u,%u)-(%u,%u) size=%ux%u hist=[", model, v.name,
                   minX, minY, maxX, maxY, maxX - minX + 1, maxY - minY + 1);
            for (auto c : hist.cells) printf("%.2f ", hist.total ? double(c) / hist.total : 0.0);
            printf("]\n");
        } else {
            printf("[PARITY] %s/%s EMPTY-FRAME\n", model, v.name);
        }
    }
    view.close();
    spinP(200);
}

TEST(GltfParity, Box)            { RunModelViews("D:\\Github\\DanQing\\build\\glb-mirror\\", "Box", true); }
TEST(GltfParity, BoxTextured)    { RunModelViews("D:\\Github\\DanQing\\build\\glb-mirror\\", "BoxTextured", true); }
TEST(GltfParity, Avocado)        { RunModelViews("D:\\Github\\DanQing\\build\\glb-mirror\\", "Avocado", true); }
TEST(GltfParity, DamagedHelmet)  { RunModelViews("D:\\Github\\DanQing\\build\\glb-mirror\\", "DamagedHelmet", true); }

// ---------------------------------------------------------------------------
// 批跑：D:\Github\glTF-Sample-Assets\Models 全量 glb。
// DANQING_PARITY_BATCH=Models根目录 时启用（默认 skip——批跑 ~119 模型 × 8 视图，
// 数量级小时）。每模型独立 View3DInventor（加载失败不阻塞后续），摘要落
// build/parity-batch-report.txt（✅/❌/⚠️ 分类）。
// ---------------------------------------------------------------------------
TEST(GltfParity, ProbeTextureSettingsTest)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinP(400);
    bool ok = view.loadGltf(QString("D:/Github/glTF-Sample-Assets/Models/TextureSettingsTest/glTF-Binary/TextureSettingsTest.glb"));
    printf("[PROBE] load=%d\n", ok?1:0);
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false; p.acsTriad = false; p.backgroundMap = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spinP(1800);
    view.getUeViewport()->RenderFrame();
    {
        auto* v3 = view.getUeViewport()->GetView()->AsViewState3d();
        auto const org = v3->GetOrigin(); auto const ext = v3->GetExtents();
        printf("[PROBE] view org=(%.2f,%.2f,%.2f) ext=(%.2f,%.2f,%.2f) camOn=%d\n",
               org.x,org.y,org.z, ext.x,ext.y,ext.z, v3->IsCameraOn()?1:0);
    }
    std::vector<uint8_t> frame; uint32_t w=0,h=0;
    view.getUeViewport()->ReadFrameForTest(frame, w, h);
    uint32_t minX,maxX,minY,maxY;
    bool found = contentBBoxP(frame, w, h, minX, maxX, minY, maxY);
    printf("[PROBE] content found=%d bbox=(%u,%u)-(%u,%u) frame=%ux%u\n",
           found?1:0, minX,minY,maxX,maxY, w, h);
    saveBMP("build/probe-texsettings.bmp", frame, w, h);
    view.close();
    SUCCEED();
}

TEST(GltfParity, SingleABeautifulGameLoadOnly)
{
    // 二分：只加载+BuildGraphic+一帧（Top），不切 8 视图——隔离 BuildGraphic
    // 崩溃 vs 视图切换渲染崩溃。
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(640, 480);
    view.show();
    spinP(300);
    bool ok = view.loadGltf(QString(getenv("DANQING_PROBE_MODEL") ? getenv("DANQING_PROBE_MODEL") : "D:/Github/glTF-Sample-Assets/Models/ABeautifulGame/glTF-Binary/ABeautifulGame.glb"));
    printf("[PROBE] loadGltf=%d\n", ok ? 1 : 0);
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false; p.acsTriad = false; p.backgroundMap = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spinP(1500);  // 等动画收敛（loadGltf 内部 animateFrustumChange）
    view.getUeViewport()->RenderFrame();
    printf("[PROBE] one frame rendered\n");
    view.close();
    SUCCEED();
}

TEST(GltfParity, SingleABeautifulGame)
{
    RunModelViews("D:/Github/glTF-Sample-Assets/Models/ABeautifulGame/glTF-Binary/", "ABeautifulGame", false);
}

TEST(GltfParity, BatchAllSampleModels)
{
    // 单模型模式：DANQING_PARITY_ONE=<glb 绝对路径> 时只跑该模型（shell 循环驱动，
    // 渲染 SEH 只杀当前进程，报告逐行 append）。否则 DANQING_PARITY_BATCH 全量
    // （同进程，遇渲染崩溃模型会中断——仅用于无崩溃模型的快速通道）。
    if (char const* one = getenv("DANQING_PARITY_ONE"); one && *one) {
        std::string const glbPath = one;
        Gui::View3DInventor view(nullptr, nullptr, nullptr);
        view.resize(640, 480);
        view.show();
        spinP(250);
        if (!view.loadGltf(QString::fromStdString(glbPath))) {
            printf("[BATCH] %s LOAD-FAIL\n", glbPath.c_str());
            FILE* r = fopen("build/parity-batch-report.txt", "a");
            if (r) { fprintf(r, "LOAD-FAIL  %s\n", glbPath.c_str()); fclose(r); }
            view.close();
            return;
        }
        spinP(200);
        {
            auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
            auto p = style.getViewFlags().Properties();
            p.grid = false; p.acsTriad = false; p.backgroundMap = false;
            style.setViewFlags(dqCommon::ViewFlags(p));
        }
        view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
        spinP(1400);
        view.getUeViewport()->RenderFrame();
        std::vector<uint8_t> frame; uint32_t w = 0, h = 0;
        bool ok = view.getUeViewport()->ReadFrameForTest(frame, w, h);
        uint32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
        bool found = ok && contentBBoxP(frame, w, h, minX, maxX, minY, maxY);
        FILE* r = fopen("build/parity-batch-report.txt", "a");
        if (r) {
            fprintf(r, found ? "OK         %s\n" : "EMPTY      %s\n", glbPath.c_str());
            fclose(r);
        }
        printf("[BATCH] %s %s\n", glbPath.c_str(), found ? "OK" : "EMPTY");
        view.close();
        return;
    }

    char const* batchRoot = getenv("DANQING_PARITY_BATCH");
    if (!batchRoot || !*batchRoot)
        GTEST_SKIP() << "set DANQING_PARITY_BATCH=<Models root> or DANQING_PARITY_ONE=<glb>";

    FILE* report = fopen("build/parity-batch-report.txt", "w");
    ASSERT_NE(report, nullptr);

    // 枚举 <root>/<Model>/glTF-Binary/<Model>.glb
    std::vector<std::string> models;
    {
        std::string const root = batchRoot;
        WIN32_FIND_DATAA fd;
        std::string const pat = root + "\\*";
        HANDLE hFind = FindFirstFileA(pat.c_str(), &fd);
        ASSERT_NE(hFind, INVALID_HANDLE_VALUE);
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (fd.cFileName[0] == '.') continue;
            std::string const glbDir = root + "\\" + fd.cFileName + "\\glTF-Binary";
            WIN32_FIND_DATAA fd2;
            HANDLE h2 = FindFirstFileA((glbDir + "\\*.glb").c_str(), &fd2);
            if (h2 != INVALID_HANDLE_VALUE) {
                std::string glb = fd2.cFileName;
                FindClose(h2);
                // model name = glb basename without extension
                models.push_back(std::string(fd.cFileName) + "\\glTF-Binary\\" +
                                 glb.substr(0, glb.size() - 4));
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    std::sort(models.begin(), models.end());
    printf("[BATCH] %zu models found under %s\n", models.size(), batchRoot);
    fprintf(report, "# glTF parity batch report — %zu models under %s\n", models.size(), batchRoot);

    int nOk = 0, nLoadFail = 0, nEmpty = 0;
    for (auto const& m : models) {
        std::string const glbPath = std::string(batchRoot) + "\\" + m + ".glb";
        Gui::View3DInventor view(nullptr, nullptr, nullptr);
        view.resize(640, 480);
        view.show();
        spinP(250);
        if (!view.loadGltf(QString::fromStdString(glbPath))) {
            printf("[BATCH] %s LOAD-FAIL\n", m.c_str());
            fprintf(report, "LOAD-FAIL  %s\n", m.c_str());
            ++nLoadFail;
            view.close();
            spinP(100);
            continue;
        }
        spinP(200);
        {
            auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
            auto p = style.getViewFlags().Properties();
            p.grid = false; p.acsTriad = false; p.backgroundMap = false;
            style.setViewFlags(dqCommon::ViewFlags(p));
        }
        view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
        spinP(1200);
        view.getUeViewport()->RenderFrame();

        // 内容存在性（初始 Top 视图 + 一次 Iso 视图）。
        bool anyEmpty = false;
        struct { dqApp::StandardViewId id; char const* name; } probes[] = {
            {dqApp::StandardViewId::Top, "Top"}, {dqApp::StandardViewId::Iso, "Iso"},
        };
        for (auto const& v : probes) {
            auto* tool = new dqApp::StandardViewTool(view.getUeViewport(), v.id);
            if (!tool->run()) delete tool;
            spinP(900);
            view.getUeViewport()->RenderFrame();
            std::vector<uint8_t> frame; uint32_t w = 0, h = 0;
            if (!view.getUeViewport()->ReadFrameForTest(frame, w, h)) { anyEmpty = true; break; }
            uint32_t minX, maxX, minY, maxY;
            if (!contentBBoxP(frame, w, h, minX, maxX, minY, maxY)) anyEmpty = true;
        }
        if (anyEmpty) {
            printf("[BATCH] %s EMPTY\n", m.c_str());
            fprintf(report, "EMPTY      %s\n", m.c_str());
            ++nEmpty;
        } else {
            printf("[BATCH] %s OK\n", m.c_str());
            fprintf(report, "OK         %s\n", m.c_str());
            ++nOk;
        }
        view.close();
        spinP(100);
    }
    fprintf(report, "# summary: OK=%d LOAD-FAIL=%d EMPTY=%d (total=%zu)\n",
            nOk, nLoadFail, nEmpty, models.size());
    fclose(report);
    printf("[BATCH] summary: OK=%d LOAD-FAIL=%d EMPTY=%d (total=%zu)\n",
           nOk, nLoadFail, nEmpty, models.size());
}
