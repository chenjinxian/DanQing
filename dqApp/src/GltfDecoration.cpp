// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — GltfDecoration implementation
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//
// The decorator builds a RenderGraphic from glTF scene data in SetScene(),
// then adds it via DecorateContext in Decorate(). This matches itwinjs-core's
// pattern where readGltfTemplate() produces a GraphicTemplate, which is then
// rendered via decorate(context) -> context.addDecoration(GraphicType.Scene, graphic).
#include "dqApp/GltfDecoration.h"

#include <dqApp/Viewport.h>

#include <dqRender/RenderSystem.h>
#include <dqRender/CreateTextureArgs.h>
#include <dqRender/GraphicBranch.h>
#include <dqRender/rhi/Driver.h>  // rhi::Driver (cache teardown destroyTexture)
#include <dqCommon/FeatureTable.h>
#include <dqBase/DqId.h>
#include <dqApp/GltfImport.h>
#include <dqGeom/IndexedPolyface.h>

#include <memory>

namespace dqApp {

GltfDecoration::GltfDecoration(uint32_t pickableId, QString const& name)
    : m_pickableId(pickableId)
    , m_name(name)
{
}

GltfDecoration::~GltfDecoration()
{
    // GraphicOwner does NOT dispose the owned graphic on destruction.
    // We must explicitly dispose it.
    if (m_graphicOwner) {
        m_graphicOwner->disposeGraphic();
        delete m_graphicOwner;
        m_graphicOwner = nullptr;
    }
    // m_graphic is owned by m_graphicOwner, don't delete separately
    m_graphic = nullptr;
    // Texture cache teardown AFTER the graphic dispose (consumers are gone;
    // the graphics held these handles external — none of them destroyed them).
    // TD-14: 1:1 GltfReader._resolvedTextures lifetime (reader lives as long
    // as its graphics).
    if (m_textureCacheDriver) {
        for (auto& e : m_resolvedTextures) {
            if (e.first != dqRender::rhi::TextureHandle{})
                m_textureCacheDriver->destroyTexture(e.first);
        }
    }
    m_resolvedTextures.clear();
}

void GltfDecoration::SetScene(std::unique_ptr<dqRender::GltfScene> scene, Viewport& vp)
{
    m_scene = std::move(scene);
    BuildGraphic(vp);
}

void GltfDecoration::BuildGraphic(Viewport& vp)
{
    if (!m_scene || m_scene->meshes.empty()) return;

    // Per-BuildGraphic resolved-texture cache — 1:1 GltfReader._resolvedTextures
    // (GltfReader.ts:533/2573-2583): meshes sharing an image decode+upload ONCE.
    // Member-level lifetime (m_resolvedTextures, destroyed in ~GltfDecoration
    // after the graphic dispose — a local cache destroyed at BuildGraphic's
    // return would kill the textures BEFORE the graphics ever render; the
    // consuming PolyfaceGraphics hold the handles with external-ownership
    // flags, CreateTextureArgs.ownership="external" semantics
    // CreateTextureArgs.ts:50-52). Driver = vp's GL driver (same one
    // createTexture used). (TD-14: previously every mesh re-created the same
    // texture — decode + upload per mesh.)
    dqRender::rhi::Driver* const driver = vp.getDriver();  // nullptr → cache disabled (can't own destroys)
    m_textureCacheDriver = driver;
    auto resolveTexture = [&](dqCommon::ImageBuffer const& image)
        -> dqRender::rhi::TextureHandle {
        if (driver) {
            for (auto const& e : m_resolvedTextures) {
                if (e.second && e.second->data == image.data && e.second->width == image.width)
                    return e.first;
            }
        }
        dqRender::CreateTextureArgs args{};
        args.imageBuffer = image;
        auto handle = vp.createTexture(args);
        if (driver && handle != dqRender::rhi::TextureHandle{})
            m_resolvedTextures.emplace_back(
                handle, std::make_unique<dqCommon::ImageBuffer>(image));
        return handle;
    };

    // Convert each glTF mesh to a renderable RenderGraphic via the viewport's RenderPipeline.
    // ← itwinjs-core: readGltfTemplate() produces GraphicTemplate nodes, then
    //    createGraphicFromTemplate() creates RenderGraphic. DanQing routes creation through the
    //    per-viewport GL driver (RenderSystem::get() is a no-op stub unless installed via setInstance).
    std::vector<dqRender::RenderGraphic*> graphics;
    size_t meshIdx = 0;
    for (auto const& mesh : m_scene->meshes) {
        if (getenv("DANQING_GLTF_TRACE"))
            printf("[GLTFDEC] mesh %zu/%zu pts=%zu idx=%zu tex=%d\n", meshIdx++, m_scene->meshes.size(),
                   mesh.polyface.IsNull() ? size_t(0) : mesh.polyface->Data().points.size(),
                   mesh.polyface.IsNull() ? size_t(0) : mesh.polyface->Data().pointIndex.size(),
                   mesh.baseColorTexture.has_value() ? 1 : 0);
        if (mesh.polyface.IsNull()) continue;

        // EXT_mesh_gpu_instancing 展开（Authored，Khronos spec；无参考实现——
        // itwinjs 不支持）：有 instances 的 mesh 展开为 N 份，每份的世界变换 =
        // node.worldTransform × instance(TRS)。单 mesh（无实例）保持原路径
        // （就地烘 node.transform，与 BoxTextured 等历史行为一致）。
        std::vector<dqGeom::Transform> worldTransforms;
        if (!mesh.instances.empty()) {
            for (auto const& it : mesh.instances) {
                // trsMatrix（GltfReader.ts:341-365）：T·R·S 顺序，quaternion 工厂
                // 后 transposeInPlace（参考 glTF/实例化布局约定）。
                auto rot = dqGeom::Matrix3d::CreateFromQuaternion(
                    it.rotation[0], it.rotation[1], it.rotation[2], it.rotation[3]).Transpose();
                dqGeom::Matrix3d const rs = rot.MultiplyMatrix(
                    dqGeom::Matrix3d::CreateScale(it.scale[0], it.scale[1], it.scale[2]));
                // instance TRS → Transform：origin=translation，matrix=R·S（T·(R·S)）。
                dqGeom::Transform const instTf(
                    dqGeom::Point3d::From(it.translation[0], it.translation[1], it.translation[2]),
                    rs);
                worldTransforms.push_back(mesh.transform.MultiplyTransform(instTf));
            }
        } else {
            worldTransforms.push_back(mesh.transform);
        }

        bool const isInstanced = !mesh.instances.empty();
        for (auto const& tf : worldTransforms) {
            // 单实例（非 instanced）：就地烘 transform（原路径——BoxTextured/Duck
            // 等单 mesh 模型的历史行为，clone 会破坏与后续 GetGraphicRange 共享的
            // 同一 polyface 对象的一致性）。
            // 多实例：每份独立顶点拷贝（共享 polyface 就地烘会二次变换）。
            dqGeom::IndexedPolyface* workPfRaw = nullptr;
            dqBase::RefPtr<dqGeom::IndexedPolyface> workPfOwned;
            if (isInstanced) {
                auto baseClone = mesh.polyface->clone();  // RefPtr<GeometryQuery>
                workPfOwned = dqBase::RefPtr<dqGeom::IndexedPolyface>(
                    static_cast<dqGeom::IndexedPolyface*>(baseClone.Get()));
                workPfRaw = workPfOwned.Get();
            } else {
                workPfRaw = mesh.polyface.Get();  // 就地（原语义）
            }
            auto& workPf = *workPfRaw;
            bool const isIdentity = tf.matrix.IsAlmostEqual(dqGeom::Matrix3d::CreateIdentity()) &&
                                    tf.origin.AlmostEqual(dqGeom::Point3d::FromZero());
            if (!isIdentity) {
                auto& data = workPf.Data();
                for (size_t i = 0; i < data.points.size(); ++i) {
                    data.points[i] = tf.MultiplyPoint3d(data.points[i]);
                }
                for (size_t i = 0; i < data.normals.size(); ++i) {
                    data.normals[i] = tf.matrix.MultiplyVector(data.normals[i]);
                }
            }

            // Convert baseColorFactor (float[4]) to uint32_t RGBA
            // Byte order matches PolyfaceGraphic::buildFromPolyface unpacking:
            //   R = (color >> 24) & 0xFF, G = (color >> 16) & 0xFF,
            //   B = (color >> 8) & 0xFF, A = (color >> 0) & 0xFF
            uint32_t defaultColor =
                (static_cast<uint32_t>(mesh.baseColorFactor[0] * 255.0f) << 24) |
                (static_cast<uint32_t>(mesh.baseColorFactor[1] * 255.0f) << 16) |
                (static_cast<uint32_t>(mesh.baseColorFactor[2] * 255.0f) << 8) |
                (static_cast<uint32_t>(mesh.baseColorFactor[3] * 255.0f) << 0);
            // Feature ID semantics: a_featureId carries the BATCH-LOCAL feature
            // index (reference vertex-table featureIndex), NOT the pickableId —
            // the pick value = featureIndex + batchId (SurfaceVariantCompiler
            // pick variant), translated back to the pickableId via BatchState.
            // The decoration is a single-feature batch → index 0.
            // ← itwinjs-core: GltfReader.ts:2758-2759
            //   FeatureTable(1, modelId) + insert(Feature(pickableId))
            // ← itwinjs-core GltfReader.ts createDisplayParams(:1356):
            //   baseColorFactor × baseColorTexture. Texture present -> upload once,
            //   use the textured factory; any texture failure degrades to the scalar
            //   path (resolveTexture miss — spec §6, never fails the import).
            uint32_t const kFeatureIndex = 0;
            // Normal map binding. Ported from: GltfReader.ts findTextureMapping
            // (:2586-2597 — greenUp is unconditionally true for glTF) +
            // Surface.ts addNormal (:546-549 — scale ?? 1.0, greenUp negates).
            // glTF never sets NormalMapParams.scale → the uniform value is -1.0.
            float const kNormalMapScale = -1.0f;
            dqRender::RenderGraphic* graphic = nullptr;
            dqRender::rhi::TextureHandle normalMapTex{};
            if (getenv("DANQING_NM_TRACE")) {
                printf("[NMDIAG] mesh=%s normalMapTexture=%s (%dx%d) baseColorTexture=%s\n",
                       mesh.name.c_str(),
                       mesh.normalMapTexture.has_value() ? "yes" : "no",
                       mesh.normalMapTexture.has_value() ? mesh.normalMapTexture->width : 0,
                       mesh.normalMapTexture.has_value() ? mesh.normalMapTexture->getHeight() : 0,
                       mesh.baseColorTexture.has_value() ? "yes" : "no");
            }
            if (mesh.normalMapTexture.has_value() && mesh.normalMapTexture->width > 0) {
                normalMapTex = resolveTexture(*mesh.normalMapTexture);
            }
            if (mesh.baseColorTexture.has_value() && mesh.baseColorTexture->width > 0) {
                dqRender::rhi::TextureHandle const tex = resolveTexture(*mesh.baseColorTexture);
                if (tex != dqRender::rhi::TextureHandle{}) {
                    graphic = vp.createGraphicFromPolyface(
                        &workPf, defaultColor, kFeatureIndex, tex, normalMapTex,
                        kNormalMapScale, /*textureExternal=*/true,
                        /*normalMapTextureExternal=*/true);
                }
            } else if (normalMapTex != dqRender::rhi::TextureHandle{}) {
                // Normal map WITHOUT a pattern texture. Reference net effect:
                // findTextureMapping (:2594-2596) sets texture = normalMap, then
                // MeshData (:63-66) demotes it — normalMap = texture, texture =
                // undefined → HasTexture=0 (base color = baseColorFactor),
                // HasNormalMap=1 (UVs still sampled via the HasNormalMap bit,
                // Surface.ts :465). Bound here as the equivalent end state:
                // no s_texture, normal map bound, greenUp scale.
                // EQUIVALENCE: 参考源=GltfReader.ts:2594-2596 + MeshData.ts:63-66；
                // 发散=无（两步合成为一步终态；逐维度核对：HasTexture=0 ✓
                // HasNormalMap=1 ✓ s_normalMap=法线图 ✓ u_normalMapScale=-1 ✓
                // baseColor=baseColorFactor ✓）。
                graphic = vp.createGraphicFromPolyface(
                    &workPf, defaultColor, kFeatureIndex, {}, normalMapTex,
                    kNormalMapScale, /*textureExternal=*/false,
                    /*normalMapTextureExternal=*/true);
            }
            if (!graphic) {
                graphic = vp.createGraphicFromPolyface(
                    &workPf, defaultColor, kFeatureIndex);
            }
            if (graphic) {
                graphics.push_back(graphic);
            }
        }
    }

    if (graphics.empty()) return;
    if (getenv("DANQING_GLTF_TRACE"))
        printf("[GLTFDEC] createGraphicList from %zu graphics\n", graphics.size());

    // Create a single RenderGraphic from the list
    // ← itwinjs-core: IModelApp.renderSystem.createGraphicFromTemplate({ template })
    auto* list = vp.createGraphicList(std::move(graphics));

    // Wrap the graphic in a Batch with a single-feature table — the reference's
    // pickable/hilite mechanism (Batch.isPickable = true, Graphic.ts:311; the
    // feature table drives the per-batch hilite LUT). The modelId MUST differ
    // from the pickable id for the decoration to be selectable & hilite-able
    // (DTA GltfDecoration.ts:163-164; Viewport.isPixelSelectable
    // Viewport.ts:2788-2796 rejects modelId === elementId).
    // ← itwinjs-core: GltfReader.ts:2756-2760 + System.ts:559-561
    //   (createGraphicFromTemplate → createBatch(graphic, featureTable, range))
    if (auto* system = vp.renderSystem()) {
        auto featureTable = std::make_unique<dqCommon::FeatureTable>(
            1, dqBase::DqId(static_cast<uint64_t>(NextGltfPickableId())),
            dqCommon::BatchType::Primary);
        featureTable->insert(dqCommon::Feature(
            dqBase::DqId(static_cast<uint64_t>(m_pickableId))));
        dqGeom::Range3d range;
        GetGraphicRange(range);
        m_graphic = system->createBatch(list, featureTable.get(), range);
    } else {
        m_graphic = list;  // no render system (headless) — unbatched fallback
    }

    // Wrap in GraphicOwner to prevent auto-disposal on decoration change
    // ← itwinjs-core: IModelApp.renderSystem.createGraphicOwner(graphic)
    if (m_graphic) {
        m_graphicOwner = vp.createGraphicOwner(m_graphic);
    }
    if (getenv("DANQING_GLTF_TRACE"))
        printf("[GLTFDEC] BuildGraphic done graphic=%d owner=%d\n",
               m_graphic ? 1 : 0, m_graphicOwner ? 1 : 0);
}

bool GltfDecoration::GetGraphicRange(dqGeom::Range3d& range) const
{
    // ← itwinjs-core GltfDecoration.ts:211-212
    //   const range = new Range3d(); graphic.unionRange(range);
    // BuildGraphic bakes each mesh's node transform into the polyface vertex
    // positions, so the scene's points are already in world space. Compute the
    // union range from them directly. (We can't use RenderGraphic::unionRange
    // here: the decoration graphic is a Primitive wrapping a PolyfaceGraphic, and
    // Primitive/CachedGeometry inherit Graphic's no-op unionRange — so
    // m_graphic->unionRange would leave `range` null.)
    if (!m_scene) return false;
    bool any = false;
    long nanCount = 0;
    for (auto const& mesh : m_scene->meshes) {
        if (mesh.polyface.IsNull()) continue;
        for (auto const& p : mesh.polyface->Data().points) {
            if (std::isnan(p.x) || std::isnan(p.y) || std::isnan(p.z) ||
                std::isinf(p.x) || std::isinf(p.y) || std::isinf(p.z)) { ++nanCount; continue; }
            range.ExtendPoint(p);
            any = true;
        }
    }
    if (getenv("DANQING_GLTF_TRACE"))
        printf("[GLTFDEC] GetGraphicRange any=%d nan/inf skipped=%ld range=[(%.2f,%.2f,%.2f),(%.2f,%.2f,%.2f)]\n",
               any ? 1 : 0, nanCount, range.low.x, range.low.y, range.low.z,
               range.high.x, range.high.y, range.high.z);
    return any;
}

void GltfDecoration::Decorate(DecorateContext& context)
{
    if (!m_graphic) return;

    // ← itwinjs-core: context.addDecoration(GraphicType.Scene, this._graphic)
    context.AddDecoration(dqRender::GraphicType::Scene, m_graphic);
}

bool GltfDecoration::TestDecorationHit(uint32_t featureId) const
{
    // ← itwinjs-core: id === this._pickableId
    return featureId == m_pickableId;
}

QString GltfDecoration::GetDecorationToolTip(uint32_t featureId) const
{
    if (featureId == m_pickableId) {
        return m_name;
    }
    return {};
}

}  // namespace dqApp
