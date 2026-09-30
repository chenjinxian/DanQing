// SPDX-License-Identifier: LGPL-2.1-or-later
// DanQing DisplayTestApp — ReadMe 展示页实现（M-L(3) Task B；设计说明见
// ReadMeView.h——素材与分析报告 §4 同源，判据测试名全部在树）。
#include "ReadMeView.h"

#include <QTextBrowser>
#include <QVBoxLayout>

namespace StartGui {

ReadMeView::ReadMeView(QWidget* parent)
    : Gui::MDIView(nullptr, parent)
{
    setObjectName(QLatin1String("ReadMeView"));

    m_browser = new QTextBrowser(this);
    m_browser->setObjectName(QStringLiteral("ReadMeBrowser"));
    m_browser->setOpenExternalLinks(false);
    m_browser->setFrameShape(QFrame::NoFrame);

    // StartView 同款装配（setCentralWidget——MDIView 自带中央控件布局，再挂
    // QVBoxLayout 会被 Qt 拒绝，浏览器得不到布局悬空成小方块——首跑实测）。
    setCentralWidget(m_browser);

    // 素材 = docs/DTA功能对照与专业化分析-2026-09-30.md §4.1（按用户价值排序的
    // 12 条已锁能力）；判据测试名 = 各锁现行命名（树内检索可复核）。
    QString html = QStringLiteral(
        "<h1>%1</h1>"
        "<p>%2</p>"

        "<h2>%3</h2>"
        // 1 真实后端数据回放渲染
        "<h3>%4</h3><p>%5</p><p><code>%6</code></p>"
        // 2 LUT 直传
        "<h3>%7</h3><p>%8</p><p><code>%9</code></p>"
        // 3 实例化
        "<h3>%10</h3><p>%11</p><p><code>%12</code></p>"
        // 4 顶点色
        "<h3>%13</h3><p>%14</p><p><code>%15</code></p>"
        // 5 边渲染
        "<h3>%16</h3><p>%17</p><p><code>%18</code></p>"
        // 6 打开性能
        "<h3>%19</h3><p>%20</p><p><code>%21</code></p>"
    )
        .arg(tr("DanQing DisplayTestApp"))
        .arg(tr("A pure-client rendering engine reference implementation — this app "
                "replays real-backend captured iModel data locally (dump replay, "
                "zero network). Every capability listed here is pinned by an "
                "automated test (pixel locks / isomorphism locks)."))
        .arg(tr("A. Real iModel rendering pipeline"))
        .arg(tr("1. Real backend data replay rendering"))
        .arg(tr("Joe's House (10 tile trees / 173,876 tiles) and "
                "Properties_60InstancesWithUrl2 (1 tree / 3,587 tiles) — real "
                "backend imdl bytes replayed locally onto screen."))
        .arg("RpcDumpRender.MirukuruRendersRealBackendTile · "
             "DumpOpenChain.OpensJoesHouseWithTenModelTrees")
        .arg(tr("2. imdl vertex-table LUT upload"))
        .arg(tr("Quantized vertices uploaded verbatim as an RGBA8 LUT — zero CPU "
                "decode, GPU shader dequantization (16 B/vertex vs 52 B/corner)."))
        .arg("ImdlGraphicsTest.LutPathUploadsVertexTableVerbatim · "
             "ImdlGraphicsTest.LutPathUsesLessGpuMemoryThanVboPath")
        .arg(tr("3. Instancing (instances)"))
        .arg(tr("Shared geometry placed per instance — 60 instances drawn from one "
                "tree's instanced primitives."))
        .arg("RpcDumpRender.Instances60RendersAllInstances")
        .arg(tr("4. Non-uniform vertex colors (color tables)"))
        .arg(tr("Per-vertex color-table sampling — pipeline pipes render blue/red "
                "instead of flat white."))
        .arg("JoesHouseColor.PipeTilesRenderColorTableNotWhite")
        .arg(tr("5. Edge rendering"))
        .arg(tr("Compact/indexed edge tables plus hline color override (black panel "
                "edges) — consumed end to end from the tile format."))
        .arg("TileTreeRender.ImdlEdgesRenderContrastingRingInSolidFill · "
             "ImdlSilhouetteEdgesRenderAtExtremesAndCulledOnFace · "
             "JoesHouseEdge.PanelEdgesRenderBlackLinesFromHlineOverride")
        .arg(tr("6. Open performance"))
        .arg(tr("Manifest parsed once and shared with the tile fetcher plus a hash "
                "index — Debug open time 18.7 s → 2.76 s (measured on the "
                "Joe's House dump)."))
        .arg("DumpTileFetcherTest (manifest single-parse) · [OPEN] chain-done trace");

    QString htmlB = QStringLiteral(
        "<h2>%1</h2>"
        // 7 点选高亮
        "<h3>%2</h3><p>%3</p><p><code>%4</code></p>"
        // 8 LOD 浏览
        "<h3>%5</h3><p>%6</p><p><code>%7</code></p>"
        // 9 视口交互
        "<h3>%8</h3><p>%9</p><p><code>%10</code></p>"
        // 10 装饰/glTF
        "<h3>%11</h3><p>%12</p><p><code>%13</code></p>"
    )
        .arg(tr("B. Interaction and picking"))
        .arg(tr("7. Pick → selection → hilite"))
        .arg(tr("Pixel-accurate pick feeds the SelectionSet; selected elements "
                "hilite on screen and restore on clear; hover flash brightens the "
                "target only."))
        .arg("PickDumpScene (3 locks) · PickHiliteSelection (6 locks) · "
             "JoesHouseHover.FlashBrightensTargetAndSparesOtherComponents")
        .arg(tr("8. Zero-missing LOD browsing"))
        .arg(tr("Zoom-drill, zoom-out and pan across both models reconcile every "
                "tile request against the captured manifest — zero NotFound inside "
                "the captured domain."))
        .arg("DumpBrowse.Instances60BrowseSessionZeroMissing · "
             "DumpBrowse.JoesHouseBrowseSessionZeroMissing · "
             "RpcDumpRender.Instances60DrillReplaysViewportChain")
        .arg(tr("9. Full viewport interaction set"))
        .arg(tr("Pan / rotate / wheel zoom / Ctrl+middle Look / Fit / Window Area / "
                "8 standard views / view undo·redo / camera toggle / view picker."))
        .arg("EventDispatchTest · LookToolTest · WindowAreaTest · ViewUndoTest · "
             "GltfStandardViewTest")
        .arg(tr("10. Decoration geometry and glTF"))
        .arg(tr("Decoration Geometry Example (ported line-by-line from the "
                "reference) plus glTF import with textures and normal maps."))
        .arg("DecoGeometryPixel.GridRendersAndRowsDiffer · GltfParityTest · "
             "GltfTexturePixelTest · GltfNormalMapPixelTest");

    QString htmlC = QStringLiteral(
        "<h2>%1</h2>"
        // 11 诊断窗
        "<h3>%2</h3><p>%3</p><p><code>%4</code></p>"
        // 12 回归 harness
        "<h3>%5</h3><p>%6</p><p><code>%7</code></p>"
        "<hr/>"
        "<p><small>%8</small></p>"
    )
        .arg(tr("C. Diagnostics"))
        .arg(tr("11. Diagnostics window suite"))
        .arg(tr("FPS / tile-request statistics / tile memory / render-command "
                "statistics / GPU profile (chrome tracing JSON) / tool settings — "
                "aligned with the reference DiagnosticsPanel. Key-in field, FPS "
                "monitor, tile-load indicator and viewport sync are wired in the "
                "status bar (M-L(3))."))
        .arg("DebugWindowDiag.PanelHasSevenSectionsInOrder · "
             "DebugWindowDiag.TileStatisticsTrackerWired · "
             "DebugWindowDiag.GpuProfilerDeliversRealResults")
        .arg(tr("12. Pixel-level regression harness"))
        .arg(tr("readPixels position-assertion discipline across the whole app — "
                "the gate runs ~2500 tests with zero failures before every "
                "milestone lands."))
        .arg("TileTreeRenderTest · RpcDumpRenderTest · DumpOpenChainTest · "
             "DumpBrowseTest")
        .arg(tr("Data note: all iModel content shown in this app is local replay "
                "of captured real-backend data (dump packages). No network, no "
                "arbitrary .bim file support."));

    m_browser->setHtml(html + htmlB + htmlC);
}

}  // namespace StartGui
