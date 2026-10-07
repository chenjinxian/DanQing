// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — glTF decoration tool implementation
// Ported from: itwinjs-core display-test-app GltfDecoration.ts
//              (createTransform :41-67 + createInstances :69-98 +
//              GltfDecorationTool :103-228).
#include "GltfDecorationTool.h"

#include <QFileDialog>
#include <QFileInfo>

#include <dqApp/Application.h>
#include <dqApp/GltfImport.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/NotificationManager.h>
#include <dqApp/ParseArgs.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqApp/GltfDecoration.h>

#include <dqGeom/Angle.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <dqRender/GltfReader.h>
#include <dqRender/RenderSystem.h>

#include <algorithm>
#include <cstdlib>
#include <random>

namespace Gui {
namespace {

// 随机源（参考 Math.random()；DanQing mt19937——seed==0 时非确定性播种，
// 测试以固定种子取得确定布局）。
std::mt19937& gltfRand(unsigned seed = 0)
{
    static std::mt19937 gen{std::random_device{}()};
    if (seed != 0)
        gen.seed(seed);
    return gen;
}

double rand01()
{
    static std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(gltfRand());
}

}  // namespace

// ---------------------------------------------------------------------------
// createTransform（Ported from: GltfDecoration.ts:41-67——createTransform
// (maxExtent, wantScale, wantRotate)）
// ---------------------------------------------------------------------------
dqGeom::Transform GltfDecorationTool::createGltfInstanceTransform(
    double maxExtent, bool wantScale, bool wantRotate)
{
    // :42-53 computeRandomPosition（applyRandomOffset ×3——origin 逐分量
    // ±maxExtent 随机偏移）。
    auto const applyRandomOffset = [maxExtent](double& coord) {
        double const r = rand01() * 2.0 * maxExtent - maxExtent;
        coord += r;
    };
    dqGeom::Point3d origin = dqGeom::Point3d::FromZero();
    applyRandomOffset(origin.x);
    applyRandomOffset(origin.y);
    applyRandomOffset(origin.z);

    // :56 translation。
    dqGeom::Transform translation = dqGeom::Transform::CreateTranslation(
        origin.x, origin.y, origin.z);

    // :58-61 scale（wantScale ? [0.25..2.5] 均匀随机 : 1）。
    constexpr double kMaxScale = 2.5;
    constexpr double kMinScale = 0.25;
    double const scaleFactor = wantScale
        ? rand01() * (kMaxScale - kMinScale) + kMinScale
        : 1.0;
    dqGeom::Transform const scale =
        dqGeom::Transform::CreateScaleAboutPoint(origin, scaleFactor);

    // :63-64 rotation（wantRotate ? 绕 Z 0-360° : 恒等——
    // createRotationAroundAxisIndex(AxisIndex.Z) 的 DanQing 等价物 =
    // CreateRotationAroundZ，角度经 DegreesToRadians）。
    double const zAngle = wantRotate ? rand01() * 360.0 : 0.0;
    dqGeom::Transform const rotation =
        dqGeom::Transform::CreateFixedPointAndMatrix(
            origin, dqGeom::Matrix3d::CreateRotationAroundZ(
                        dqGeom::Angle::DegreesToRadians(zAngle)));

    // :66 — translation.multiplyTransformTransform(scale).multiplyTransformTransform(rotation)。
    return translation.MultiplyTransform(scale).MultiplyTransform(rotation);
}

// ---------------------------------------------------------------------------
// parseAndRun（Ported from: GltfDecoration.ts:116-127）
// ---------------------------------------------------------------------------
bool GltfDecorationTool::parseAndRun(std::vector<std::string> const& inArgs)
{
    // :117 parseArgs（前端 devtools parseArgs——DanQing 等价物 dqApp::parseArgs）。
    auto const args = dqApp::parseArgs(inArgs);
    // :118 u —— §8.2 零网络：URL 取数不支持；有 url 走文件对话框等价面
    //（登记：参考 fetch(url) 分支网络，DanQing 恒本地选择）。
    if (auto url = args.get("u"))
        m_url = *url;
    // :119-124。
    m_numInstances = args.getInteger("i").value_or(1);
    m_wantScale = args.getBoolean("s").value_or(false);
    m_wantColor = args.getBoolean("c").value_or(false);
    m_wantRotate = args.getBoolean("r").value_or(false);
    m_forceUninstanced = args.getBoolean("f").value_or(false);
    // w>（useViewportRenderMode）：DanQing glTF 装饰无 RenderMode 覆盖面
    //（恒 SmoothShade 语义）——参数消费面登记，无作用（EQUIVALENCE 见头注）。

    return run();
}

// ---------------------------------------------------------------------------
// buildInstancedScene——run() 的实例化半边（Ported from: GltfDecoration.ts:69-98
// createInstances + :180-195 实例化/强制展开共用面）。
// 引擎 RenderInstances GPU 通道（参考 createGraphicFromTemplate 的 instances
// 形参）未移植——EQUIVALENCE: 参考源=GltfDecoration.ts:181-183
//（createRenderInstances + createGraphicFromTemplate GPU 实例化）；发散=
// DanQing CPU 展开烘顶点（每实例独立顶点拷贝，语义同参考 f> force-uninstanced
// 分支 :184-195——逐份 graphic + GraphicBranch transform 的合一份形态）；
// 验证法=GltfDecorationInstancesRenderSeparatedClusters 像素锁。
// ---------------------------------------------------------------------------
std::unique_ptr<dqRender::GltfScene> GltfDecorationTool::buildInstancedScene(
    dqRender::GltfScene const& src, int numInstances, double maxExtent,
    bool wantScale, bool wantColor, bool wantRotate, unsigned seed)
{
    // 测试确定性：固定种子重播（生产 seed=0 = 非确定性）。
    if (seed != 0)
        gltfRand(seed);
    // 七色（GltfDecoration.ts:76-84——green/blue/red/white/yellow/orange/black）。
    static float const kColors[7][3] = {
        {0.0f, 1.0f, 0.0f},   // green
        {0.0f, 0.0f, 1.0f},   // blue
        {1.0f, 0.0f, 0.0f},   // red
        {1.0f, 1.0f, 1.0f},   // white
        {1.0f, 1.0f, 0.0f},   // yellow（ColorByName.yellow）
        {1.0f, 0.65f, 0.0f},  // orange（ColorByName.orange ≈ 0xffa500）
        {0.0f, 0.0f, 0.0f},   // black
    };

    auto out = std::make_unique<dqRender::GltfScene>();
    for (int i = 0; i < numInstances; ++i) {
        dqGeom::Transform const tf =
            createGltfInstanceTransform(maxExtent, wantScale, wantRotate);
        for (auto const& mesh : src.meshes) {
            if (mesh.polyface.IsNull())
                continue;
            dqRender::GltfMesh copy;
            copy.name = mesh.name;
            // 烘变换（node.transform × 实例变换——逐顶点；GltfDecoration.cpp
            // :140-159 既有多实例分支同款：polyface 独立 clone 后烘、mesh
            // transform 复位恒等）。
            copy.transform = mesh.transform.MultiplyTransform(tf);
            // 实例色（:87-88——symbology { color: colors[i % 7] }，float RGB 形）。
            if (wantColor) {
                auto const& c = kColors[static_cast<size_t>(i) % 7];
                copy.baseColorFactor[0] = c[0];
                copy.baseColorFactor[1] = c[1];
                copy.baseColorFactor[2] = c[2];
                copy.baseColorFactor[3] = mesh.baseColorFactor[3];
            } else {
                std::copy(mesh.baseColorFactor, mesh.baseColorFactor + 4,
                          copy.baseColorFactor);
            }
            copy.metallicFactor = mesh.metallicFactor;
            copy.roughnessFactor = mesh.roughnessFactor;
            auto baseClone = mesh.polyface->clone();
            copy.polyface = dqBase::RefPtr<dqGeom::IndexedPolyface>(
                static_cast<dqGeom::IndexedPolyface*>(baseClone.Get()));
            // texture 随实例共享（ImageBuffer 可拷贝——GPU 侧 resolveTexture
            // 的 resolvedTextures 成员级缓存去重，GltfDecoration 既有面）。
            copy.baseColorTexture = mesh.baseColorTexture;
            copy.normalMapTexture = mesh.normalMapTexture;
            copy.texCoordSet = mesh.texCoordSet;

            auto& data = copy.polyface->Data();
            for (size_t pi = 0; pi < data.points.size(); ++pi)
                data.points[pi] = copy.transform.MultiplyPoint3d(data.points[pi]);
            for (size_t ni = 0; ni < data.normals.size(); ++ni)
                data.normals[ni] =
                    copy.transform.matrix.MultiplyVector(data.normals[ni]);
            copy.transform = dqGeom::Transform::CreateIdentity();  // 已烘
            out->meshes.push_back(std::move(copy));
        }
    }
    // 并集域（GltfReader::BuildSceneFromData 的装载尾调语义——消费者
    // InstallGltfDecoration 的 LookAtVolume(bounds) 依赖非 null 域）。
    out->computeBounds();
    return out;
}

// ---------------------------------------------------------------------------
// run（Ported from: GltfDecoration.ts:149-227）
// ---------------------------------------------------------------------------
bool GltfDecorationTool::run()
{
    // :150-152——无活动视口或实例数非法即返回。
    auto* vp = dqApp::Application::Get().GetViewManager().GetActiveViewport();
    if (!vp || m_numInstances < 1)
        return false;

    // :155-160 + :129-147 queryAsset——DanQing 本地文件选择（showOpenFilePicker
    // 的 .gltf/.glb 过滤等价 = QFileDialog）。u> URL 分支 §8.2 零网络不取数
    //（EQUIVALENCE 见头注——参考 queryAsset 本地分支忠实取用）。
    QString const path = QFileDialog::getOpenFileName(
        nullptr, QStringLiteral("Open glTF"), QString(),
        QStringLiteral("glTF (*.gltf *.glb)"));
    if (path.isEmpty())
        return false;  // :158-159（queryAsset 无 buffer → return false）

    // :162-174——readGltfTemplate 等价 = GltfReader::LoadFromFile（消费面
    // 同构：字节→GltfScene；pickable 的 id/modelId 经 InstallGltfDecoration
    // 内部 NextGltfPickableId 双发——与参考 transientIds.getNext() ×2 同语义）。
    auto scene = dqRender::GltfReader::LoadFromFile(path.toUtf8().toStdString());
    if (!scene || scene->meshes.empty())
        return false;  // :176-177（无 template → return false）

    // GltfDecoration.ts:19-23/:207 —— tooltip = `url ?? "glTF model"`；本地
    // 文件选择无 url → 恒 "glTF model"（2026-10-07 审计 B6：原先传文件名）。
    std::string const name = "glTF model";

    if (m_numInstances <= 1 && !m_forceUninstanced) {
        // 单实例 + 非强制展开 = M-O(1) 已有面（InstallGltfDecoration 直装）。
        // ← GltfDecoration.ts:180-204 的 instances=undefined 单模板路径 +
        //   createGraphicOwner + addDecorator + fit（GltfImport 内嵌）。
        auto decoration = dqApp::InstallGltfDecoration(*vp, std::move(scene), name);
        return decoration != nullptr;
    }

    // 多实例（:180-195 实例化分支；:74-75 maxExtent = projectExtents 对角线
    // 最小分量）。
    auto const& pe = vp->GetView()->GetIModel()->GetProjectExtents();
    auto const diag = pe.Diagonal();
    double const maxExtent = std::min(diag.x, std::min(diag.y, diag.z));

    auto instanced = buildInstancedScene(
        *scene, m_numInstances, maxExtent, m_wantScale, m_wantColor, m_wantRotate);

    auto decoration = dqApp::InstallGltfDecoration(*vp, std::move(instanced), name);
    return decoration != nullptr;
}

}  // namespace Gui
