// NestedViewportDiagTest — 独立 Viewport vs 嵌套（QStackedWidget 容器）Viewport
// 的 Grid 渲染对照（View3DInventor 场景 band=0 的差异因子定位）。
// Authored: TEMP-DIAG 诊断用（无对应参考测试）。
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QStackedWidget>
#include <QThread>
#include <gtest/gtest.h>
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/Viewport.h>
#include <dqCommon/ViewFlags.h>
#include <dqGeom/Plane3dByOriginAndUnitNormal.h>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace { struct QtEnvN { QtEnvN() { if (!qApp) { static int argc=1; static char n[]="t"; static char* av[]={n,nullptr}; new QApplication(argc,av);} } }; }
static QtEnvN s_qtN;

namespace {
QSize g_probeSize(1001, 844);  // TEMP-DIAG 尺寸因子开关

void spin(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

int contrastBand(std::vector<uint8_t> const& f, uint32_t w, uint32_t h)
{
    uint8_t const* bg = &f[(static_cast<size_t>(h - 4) * w + 4) * 4];
    int count = 0;
    for (uint32_t y = h / 2 - h / 20; y < h / 2 + h / 20; ++y)
        for (uint32_t x = 0; x < w; x += 2) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            int const d = std::abs(int(p[0]) - int(bg[0]))
                        + std::abs(int(p[1]) - int(bg[1]))
                        + std::abs(int(p[2]) - int(bg[2]));
            if (d > 150) ++count;
        }
    return count;
}

// 公共视口建立 + Grid 勾选 + 采样（parent 决定独立/嵌套）。
int runGridProbe(QWidget* parent)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "NestedViewportDiag";
        opts.applicationVersion = "1.0";
        opts.noRender = true;
        EXPECT_TRUE(app.Startup(opts));
    }
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(
        dqGeom::Point3d::From(-1000, -1000, -100), dqGeom::Point3d::From(1000, 1000, 100));
    auto imodel = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(imodel.Get()).getDefaultView(imodel.Get());
    auto* vp = dqApp::Viewport::Create(parent, view);
    vp->resize(g_probeSize.width(), g_probeSize.height());
    QWidget* topLevel = parent ? parent : vp;
    topLevel->resize(g_probeSize.width(), g_probeSize.height());
    topLevel->show();
    spin(500);

    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    spin(400);
    vp->RenderFrame();

    // TEMP-DIAG: 调整后视锥深度范围（宽度→z 突变验证）。
    {
        dqCommon::Frustum const fr = vp->getFrustum(true);  // viewingSpace 调整后
        double zMin = 1e300, zMax = -1e300;
        double xMin = 1e300, xMax = -1e300;
        for (int i = 0; i < 8; ++i) {
            zMin = std::min(zMin, fr.points[i].z);
            zMax = std::max(zMax, fr.points[i].z);
            xMin = std::min(xMin, fr.points[i].x);
            xMax = std::max(xMax, fr.points[i].x);
        }
        printf("[NESTDIAG] adjustedFrustum x=[%.6g,%.6g] z=[%.6g,%.6g] spanZ=%.6g\n",
               xMin, xMax, zMin, zMax, zMax - zMin);

        // TEMP-DIAG: frustum∩z=0 多边形——BuildPlanarGridPolygon 的直接输入
        //（DecorateContext.cpp:119 getFrustum(true)，identity grid：uv = p.xy/40）。
        // 顶点数/UV 范围是否在 1100 逻辑宽（≈2200 物理）退化。
        auto plane = dqGeom::Plane3dByOriginAndUnitNormal::create(
            dqGeom::Point3d{0.0, 0.0, 0.0}, dqGeom::Vector3d{0.0, 0.0, 1.0});
        if (plane) {
            auto poly = fr.GetIntersectionWithPlane(*plane);
            if (poly && poly->size() >= 3) {
                double uMin = 1e300, uMax = -1e300, vMin = 1e300, vMax = -1e300;
                for (auto const& p : *poly) {
                    uMin = std::min(uMin, p.x / 40.0); uMax = std::max(uMax, p.x / 40.0);
                    vMin = std::min(vMin, p.y / 40.0); vMax = std::max(vMax, p.y / 40.0);
                }
                printf("[NESTDIAG] polygon verts=%zu uvExtent=(%.6g,%.6g) "
                       "uvRange=[%.4f,%.4f]x[%.4f,%.4f]\n",
                       poly->size(), uMax - uMin, vMax - vMin, uMin, uMax, vMin, vMax);
            } else {
                printf("[NESTDIAG] polygon NULL/degenerate (n=%d)\n",
                       poly ? static_cast<int>(poly->size()) : -1);
            }
        }
    }

    printf("[NESTDIAG] world=%zu sky=%d\n",
           vp->GetDecorations().world.size(),
           vp->GetDecorations().skyBox ? 1 : 0);
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    if (!vp->ReadFrameForTest(frame, w, h)) {
        printf("[NESTDIAG] readback FAILED\n");
        app.GetViewManager().DropViewport(vp);
        delete topLevel;
        return -1;
    }
    int const band = contrastBand(frame, w, h);
    printf("[NESTDIAG] %ux%u band=%d center=(%d,%d,%d)\n", w, h, band,
           frame[(static_cast<size_t>(h / 2) * w + w / 2) * 4],
           frame[(static_cast<size_t>(h / 2) * w + w / 2) * 4 + 1],
           frame[(static_cast<size_t>(h / 2) * w + w / 2) * 4 + 2]);
    // TEMP-DIAG: 存 PNG（帧对比肉眼确认）。
    {
        char path[128];
        std::snprintf(path, sizeof(path), "D:\\Github\\DanQing\\build\\nestdiag-%ux%u.ppm", w, h);
        if (FILE* dbg = std::fopen(path, "wb")) {
            std::fprintf(dbg, "P6\n%u %u\n255\n", w, h);
            for (size_t i = 0; i < frame.size() / 4; ++i)
                std::fwrite(&frame[i * 4], 1, 3, dbg);
            std::fclose(dbg);
        }
    }

    app.GetViewManager().DropViewport(vp);
    // 不 delete vp（parent 拥有时会双删）；直接关闭顶层。
    topLevel->close();
    spin(200);
    return band;
}
}  // namespace

TEST(NestedViewportDiag, TopLevelVsStacked)
{
    // 三点扫描：1080（历史"有"）、1100（历史"无"）、1200x800（MaximizeKeepsGridVisible
    // 基线 beforeBand>200 通过 = 有 Grid）——非单调（2200 物理无 vs 2400 物理有）
    // 则排除 GL 尺寸上限，指向几何/宽高比型突变。
    QSize const sizes[] = {QSize(1080, 844), QSize(1100, 844), QSize(1200, 800)};
    for (auto const& sz : sizes) {
        printf("[NESTDIAG] ===== probe %dx%d (logical) =====\n", sz.width(), sz.height());
        g_probeSize = sz;
        runGridProbe(nullptr);
    }
}
