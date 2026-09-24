// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GltfReader implementation
// Authored: uses cgltf third-party library; not a port of itwinjs-core GltfReader.ts
//
// Glue layer: cgltf structs → dqGeom::IndexedPolyface.
#include "dqRender/GltfReader.h"

// cgltf implementation (define in exactly one .cpp)
#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/PolyfaceData.h>
#include <dqCommon/Image.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <string>
#include <cstdio>
#include <io.h>
#include <fcntl.h>
#endif

namespace dqRender {

static std::string sLastError;

// ---------------------------------------------------------------------------
// Helper: extract transform from cgltf_node
// ---------------------------------------------------------------------------
static dqGeom::Transform NodeTransform(cgltf_node const* node)
{
    // ← cgltf provides either a matrix or TRS decomposition
    if (node->has_matrix) {
        // Column-major 4x4 matrix → dqGeom::Transform
        float const* m = node->matrix;
        // cgltf stores column-major: m[col*4+row]
        dqGeom::Matrix3d rotation = dqGeom::Matrix3d::CreateRowValues(
            m[0], m[4], m[8],
            m[1], m[5], m[9],
            m[2], m[6], m[10]
        );
        dqGeom::Point3d origin = dqGeom::Point3d::From(m[12], m[13], m[14]);
        return dqGeom::Transform(origin, rotation);
    }

    // TRS decomposition
    double tx = 0, ty = 0, tz = 0;
    if (node->has_translation) {
        tx = node->translation[0];
        ty = node->translation[1];
        tz = node->translation[2];
    }

    // Rotation quaternion [x, y, z, w]
    dqGeom::Matrix3d rotation = dqGeom::Matrix3d::CreateIdentity();
    if (node->has_rotation) {
        double qx = node->rotation[0];
        double qy = node->rotation[1];
        double qz = node->rotation[2];
        double qw = node->rotation[3];

        // Quaternion to rotation matrix
        double xx = qx * qx, yy = qy * qy, zz = qz * qz;
        double xy = qx * qy, xz = qx * qz, yz = qy * qz;
        double wx = qw * qx, wy = qw * qy, wz = qw * qz;

        rotation = dqGeom::Matrix3d::CreateRowValues(
            1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy),
            2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx),
            2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy)
        );
    }

    // Scale
    double sx = 1, sy = 1, sz = 1;
    if (node->has_scale) {
        sx = node->scale[0];
        sy = node->scale[1];
        sz = node->scale[2];
    }

    // Compose: T * R * S
    if (sx != 1.0 || sy != 1.0 || sz != 1.0) {
        dqGeom::Matrix3d scale = dqGeom::Matrix3d::CreateScale(sx, sy, sz);
        rotation = rotation.MultiplyMatrix(scale);
    }

    return dqGeom::Transform(
        dqGeom::Point3d::From(tx, ty, tz), rotation);
}

// ---------------------------------------------------------------------------
// Helper: read a vec3 float from an accessor
// Uses accessor->stride (computed by cgltf from bufferView + type).
// ---------------------------------------------------------------------------
static bool ReadFloat3(cgltf_accessor const* accessor, cgltf_size index, float out[3])
{
    if (!accessor || accessor->type != cgltf_type_vec3 || index >= accessor->count)
        return false;

    if (!accessor->buffer_view || !accessor->buffer_view->buffer)
        return false;

    auto const* bufferData = static_cast<uint8_t const*>(accessor->buffer_view->buffer->data);
    if (!bufferData) return false;

    // accessor->stride is computed by cgltf and is always correct
    size_t stride = accessor->stride;
    if (stride == 0) stride = 3 * sizeof(float);

    size_t offset = accessor->offset + stride * index;
    auto const* src = reinterpret_cast<float const*>(bufferData + accessor->buffer_view->offset + offset);
    out[0] = src[0];
    out[1] = src[1];
    out[2] = src[2];
    return true;
}

// Helper: read a vec4 float from an accessor (EXT_mesh_gpu_instancing ROTATION).
// Authored: same accessor layout as ReadFloat3 (stride from cgltf).
static bool ReadFloat4(cgltf_accessor const* accessor, cgltf_size index, float out[4])
{
    if (!accessor || accessor->type != cgltf_type_vec4 || index >= accessor->count)
        return false;

    if (!accessor->buffer_view || !accessor->buffer_view->buffer)
        return false;

    auto const* bufferData = static_cast<uint8_t const*>(accessor->buffer_view->buffer->data);
    if (!bufferData) return false;

    size_t stride = accessor->stride;
    if (stride == 0) stride = 4 * sizeof(float);

    size_t offset = accessor->offset + stride * index;
    auto const* src = reinterpret_cast<float const*>(bufferData + accessor->buffer_view->offset + offset);
    out[0] = src[0];
    out[1] = src[1];
    out[2] = src[2];
    out[3] = src[3];
    return true;
}

// ---------------------------------------------------------------------------
// Helper: read a vec2 float from an accessor
// Reads TEXCOORD_n vec2 (Ported from: GltfReader.ts:1603-1604 texCoord read).
// ---------------------------------------------------------------------------
static bool ReadFloat2(cgltf_accessor const* accessor, cgltf_size index, float out[2])
{
    if (!accessor || accessor->type != cgltf_type_vec2 || index >= accessor->count)
        return false;
    if (!accessor->buffer_view || !accessor->buffer_view->buffer)
        return false;
    auto const* bufferData = static_cast<uint8_t const*>(accessor->buffer_view->buffer->data);
    if (!bufferData) return false;
    size_t stride = accessor->stride;
    if (stride == 0) stride = 2 * sizeof(float);
    size_t offset = accessor->offset + stride * index;
    auto const* src = reinterpret_cast<float const*>(bufferData + accessor->buffer_view->offset + offset);
    out[0] = src[0];
    out[1] = src[1];
    return true;
}

// ---------------------------------------------------------------------------
// Helper: read index value from accessor
// ---------------------------------------------------------------------------
static uint32_t ReadIndex(cgltf_accessor const* accessor, cgltf_size index)
{
    if (!accessor || index >= accessor->count)
        return 0;

    if (!accessor->buffer_view || !accessor->buffer_view->buffer)
        return 0;

    auto const* bufferData = static_cast<uint8_t const*>(accessor->buffer_view->buffer->data);
    if (!bufferData) return 0;

    size_t stride = accessor->stride;
    if (stride == 0) {
        switch (accessor->component_type) {
        case cgltf_component_type_r_8u: stride = 1; break;
        case cgltf_component_type_r_16u: stride = 2; break;
        case cgltf_component_type_r_32u: stride = 4; break;
        default: return 0;
        }
    }

    size_t offset = accessor->offset + stride * index;
    auto const* src = bufferData + accessor->buffer_view->offset + offset;

    switch (accessor->component_type) {
    case cgltf_component_type_r_8u:
        return *src;
    case cgltf_component_type_r_16u:
        return *reinterpret_cast<uint16_t const*>(src);
    case cgltf_component_type_r_32u:
        return *reinterpret_cast<uint32_t const*>(src);
    default:
        return 0;
    }
}

// ---------------------------------------------------------------------------
// Helper: resolve texture image bytes → ImageSource
// ← itwinjs-core GltfReader.ts resolveImage(:2487-2521): image bytes from
//   bufferView (embedded) / external URI file / data URI (base64 via
//   cgltf_load_buffer_base64). Format sniffed from mime_type then magic bytes.
//   Returns nullopt on any miss — the caller degrades, never fails the import.
// ---------------------------------------------------------------------------
static std::optional<dqCommon::ImageSource> ResolveImageSource(
    cgltf_image const* image, std::string const& baseDir)
{
    if (!image) return std::nullopt;
    std::vector<uint8_t> bytes;
    if (image->buffer_view && image->buffer_view->buffer && image->buffer_view->buffer->data) {
        auto const* bv = image->buffer_view;
        auto const* base = static_cast<uint8_t const*>(bv->buffer->data) + bv->offset;
        bytes.assign(base, base + bv->size);
    } else if (image->uri) {
        std::string uri = image->uri;
        if (uri.rfind("data:", 0) == 0) {
            size_t comma = uri.find(',');
            if (comma == std::string::npos) return std::nullopt;
            char const* b64 = uri.c_str() + comma + 1;
            cgltf_size b64Len = static_cast<cgltf_size>(uri.size() - comma - 1);
            // length: base64 chars -> bytes (b64Len without padding). The size
            // passed to cgltf_load_buffer_base64 is the decoded byte count —
            // same as cgltf's own data-URI buffer path (cgltf.h:1457) — since
            // the decoder emits exactly `size` bytes and treats '=' as invalid.
            size_t pad = 0;
            if (b64Len >= 1 && uri[uri.size() - 1] == '=') ++pad;
            if (b64Len >= 2 && uri[uri.size() - 2] == '=') ++pad;
            size_t decoded = b64Len / 4 * 3 - pad;
            void* out = nullptr;
            cgltf_options opts = {};
            if (decoded == 0 ||
                cgltf_load_buffer_base64(&opts, static_cast<cgltf_size>(decoded), b64, &out) != cgltf_result_success ||
                !out)
                return std::nullopt;
            bytes.assign(static_cast<uint8_t const*>(out), static_cast<uint8_t const*>(out) + decoded);
            free(out);
        } else {
            std::ifstream file(baseDir + uri, std::ios::binary | std::ios::ate);
            if (!file.is_open()) return std::nullopt;  // resolveUrl miss — degrade
            size_t size = static_cast<size_t>(file.tellg());
            file.seekg(0, std::ios::beg);
            bytes.resize(size);
            file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
        }
    } else {
        return std::nullopt;
    }
    if (bytes.empty()) return std::nullopt;

    dqCommon::ImageSourceFormat format = dqCommon::ImageSourceFormat::Png;
    if (image->mime_type) {
        if (strstr(image->mime_type, "jpeg")) format = dqCommon::ImageSourceFormat::Jpeg;
    } else if (bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8) {
        format = dqCommon::ImageSourceFormat::Jpeg;  // JPEG magic
    }
    return dqCommon::ImageSource{ std::move(bytes), format };
}

// ---------------------------------------------------------------------------
// Helper: extract a single mesh primitive into IndexedPolyface
// ---------------------------------------------------------------------------
static dqBase::RefPtr<dqGeom::IndexedPolyface> ExtractPrimitive(
    cgltf_primitive const* primitive, int texCoordSet)
{
    // Find POSITION accessor
    cgltf_accessor const* posAccessor = nullptr;
    cgltf_accessor const* normalAccessor = nullptr;
    cgltf_accessor const* uvAccessor = nullptr;

    for (cgltf_size a = 0; a < primitive->attributes_count; ++a) {
        auto const& attr = primitive->attributes[a];
        if (attr.type == cgltf_attribute_type_position)
            posAccessor = attr.data;
        else if (attr.type == cgltf_attribute_type_normal)
            normalAccessor = attr.data;
        else if (attr.type == cgltf_attribute_type_texcoord && static_cast<int>(attr.index) == texCoordSet)
            uvAccessor = attr.data;
    }

    if (getenv("DANQING_GLTF_TRACE")) printf("[EXT] pos=%p cnt=%d type=%d bv=%p bufData=%p | uv=%p uvcnt=%d | indices=%p idxcnt=%d\n",
        (void const*)posAccessor, posAccessor ? (int)posAccessor->count : -1, posAccessor ? (int)posAccessor->type : -1,
        posAccessor ? (void const*)posAccessor->buffer_view : nullptr,
        (posAccessor && posAccessor->buffer_view && posAccessor->buffer_view->buffer) ? posAccessor->buffer_view->buffer->data : nullptr,
        (void const*)uvAccessor, uvAccessor ? (int)uvAccessor->count : -1,
        (void const*)primitive->indices, primitive->indices ? (int)primitive->indices->count : -1);
    if (!posAccessor || posAccessor->count == 0)
        return nullptr;

    // create IndexedPolyface
    bool needNormals = (normalAccessor != nullptr);
    bool needParams = (uvAccessor != nullptr);
    auto polyface = dqGeom::IndexedPolyface::create(needNormals, false, false, /*needParams=*/needParams);

    cgltf_size vertexCount = posAccessor->count;

    // add points
    for (cgltf_size i = 0; i < vertexCount; ++i) {
        float pos[3];
        if (!ReadFloat3(posAccessor, i, pos))
            return nullptr;
        polyface->AddPoint(dqGeom::Point3d::From(pos[0], pos[1], pos[2]));
    }

    // add normals
    if (normalAccessor && normalAccessor->count == vertexCount) {
        for (cgltf_size i = 0; i < vertexCount; ++i) {
            float nrm[3];
            if (ReadFloat3(normalAccessor, i, nrm))
                polyface->AddNormal(dqGeom::Vector3d::From(nrm[0], nrm[1], nrm[2]));
        }
    }

    // add UV params (TEXCOORD_n)
    if (uvAccessor && uvAccessor->count == vertexCount) {
        for (cgltf_size i = 0; i < vertexCount; ++i) {
            float uv[2];
            if (ReadFloat2(uvAccessor, i, uv)) {
                polyface->AddParam(dqGeom::Point2d::From(uv[0], uv[1]));
            } else {
                polyface->AddParam(dqGeom::Point2d::From(0.0, 0.0));
            }
        }
    }

    // add indices and facets (triangles)
    if (primitive->indices && primitive->indices->count > 0) {
        cgltf_size indexCount = primitive->indices->count;

        for (cgltf_size i = 0; i < indexCount; ++i) {
            // cgltf indices are 0-based, IndexedPolyface uses 1-based
            int32_t idx = static_cast<int32_t>(ReadIndex(primitive->indices, i) + 1);
            polyface->AddPointIndex(idx);

            if (needNormals) {
                polyface->AddNormalIndex(idx);
            }

            if (needParams) {
                polyface->AddParamIndex(idx);
            }

            // Every 3 indices = one triangle facet
            if ((i + 1) % 3 == 0) {
                polyface->TerminateFacet();
            }
        }
    } else {
        // ← itwinjs-core GltfReader.ts:2116-2126 — non-indexed primitives render
        //   with drawArrays semantics: sequential indices 0..vertexCount-1.
        cgltf_size indexCount = (vertexCount / 3) * 3;  // whole triangles only
        for (cgltf_size i = 0; i < indexCount; ++i) {
            int32_t idx = static_cast<int32_t>(i + 1);  // 0-based -> 1-based
            polyface->AddPointIndex(idx);
            if (needNormals) {
                polyface->AddNormalIndex(idx);
            }
            if (needParams) {
                polyface->AddParamIndex(idx);
            }
            if ((i + 1) % 3 == 0) {
                polyface->TerminateFacet();
            }
        }
    }

    return polyface;
}

// ---------------------------------------------------------------------------
// Helper: recursively traverse scene graph and collect meshes
// Cycle guard: Ported from: itwinjs-core core/frontend/src/common/gltf/GltfSchema.ts
//              traverseGltfNodes (L332-346) — a `traversed` set persists across the
//              WHOLE traversal; revisiting any node (cycle OR DAG-shared node)
//              throws "Cycle detected while traversing glTF nodes". DanQing's error
//              channel is sLastError + aborted traversal (core engine: no throw).
// ---------------------------------------------------------------------------
static bool TraverseNode(
    cgltf_node const* node,
    dqGeom::Transform const& parentTransform,
    std::vector<GltfMesh>& outMeshes,
    std::string const& baseDir,
    std::set<cgltf_node const*>& traversed)
{
    if (!node) return true;

    if (traversed.count(node) != 0) {
        sLastError = "Cycle detected while traversing glTF nodes";
        return false;
    }
    traversed.insert(node);

    // KHR_node_visibility（Khronos spec；Authored——itwinjs 与上游 cgltf 未支持）：
    // visible=false 的节点**连同其整个子树**不渲染（子树不再遍历/提取）。
    // 未设置 has_node_visibility = 可见（spec 默认 true）。
    if (node->has_node_visibility && !node->node_visibility_visible)
        return true;  // 合法跳过（非错误）

    // Accumulate transform
    dqGeom::Transform localTransform = NodeTransform(node);
    dqGeom::Transform worldTransform = parentTransform.MultiplyTransform(localTransform);

    // Extract mesh if this node has one
    if (getenv("DANQING_GLTF_TRACE")) printf("[TRV] node mesh=%p prims=%d\n", (void*)node->mesh, (int)(node->mesh ? node->mesh->primitives_count : -1));
    if (node->mesh) {
        for (cgltf_size p = 0; p < node->mesh->primitives_count; ++p) {
            cgltf_primitive const* primitive = &node->mesh->primitives[p];

            // Extract material BEFORE geometry — texCoordSet must be known
            // when ExtractPrimitive selects the TEXCOORD_n accessor
            // (← GltfReader.ts:1601-1605: texCoordIndex is read before
            //  readUVParams(TEXCOORD_${texCoordIndex})).
            GltfMesh mesh;
            cgltf_texture const* baseColorTex = nullptr;
            cgltf_texture const* normalMapTex = nullptr;
            int texCoordSet = 0;
            if (primitive->material) {
                auto const* mat = primitive->material;
                if (mat->has_pbr_metallic_roughness) {
                    auto const& pbr = mat->pbr_metallic_roughness;
                    mesh.baseColorFactor[0] = static_cast<float>(pbr.base_color_factor[0]);
                    mesh.baseColorFactor[1] = static_cast<float>(pbr.base_color_factor[1]);
                    mesh.baseColorFactor[2] = static_cast<float>(pbr.base_color_factor[2]);
                    mesh.baseColorFactor[3] = static_cast<float>(pbr.base_color_factor[3]);
                    mesh.metallicFactor = static_cast<float>(pbr.metallic_factor);
                    mesh.roughnessFactor = static_cast<float>(pbr.roughness_factor);
                }

                // ← itwinjs-core GltfReader.ts extractTextureId(:1228-1252):
                //   baseColorTexture, falling back to emissiveTexture.
                if (mat->has_pbr_metallic_roughness &&
                    mat->pbr_metallic_roughness.base_color_texture.texture) {
                    baseColorTex = mat->pbr_metallic_roughness.base_color_texture.texture;
                    texCoordSet = static_cast<int>(mat->pbr_metallic_roughness.base_color_texture.texcoord);
                } else if (mat->emissive_texture.texture) {
                    baseColorTex = mat->emissive_texture.texture;
                }

                // ← itwinjs-core GltfReader.ts extractNormalMapId(:1254-1262):
                //   material.normalTexture.index (glTF 1.0 materials have none —
                //   isGltf1Material returns undefined). The normal map shares the
                //   baseColor UV chain: the reference's UV accessor selection
                //   reads only the pattern texture's texCoord index (:1601-1605)
                //   and findTextureMapping never reads normalTexture.texcoord.
                if (mat->normal_texture.texture) {
                    normalMapTex = mat->normal_texture.texture;
                }
            }

            auto polyface = ExtractPrimitive(primitive, texCoordSet);
            if (getenv("DANQING_GLTF_TRACE")) printf("[TRV] prim p=%d polyface=%d pts=%d\n", (int)p, polyface.IsValid() ? 1 : 0, polyface.IsValid() ? (int)polyface->Data().PointCount() : 0);
            if (polyface) {
                if (node->mesh->name) {
                    mesh.name = node->mesh->name;
                }
                mesh.transform = worldTransform;
                mesh.polyface = polyface;

                // EXT_mesh_gpu_instancing 提取（Khronos spec；无参考实现——
                // Authored，语义 = instance world = node.worldTransform × instance(TRS)）。
                // 属性 accessor 名 → TRANSLATION(VEC3)/ROTATION(VEC4)/SCALE(VEC3)。
                if (node->has_mesh_gpu_instancing) {
                    auto const& inst = node->mesh_gpu_instancing;
                    cgltf_accessor const* accT = nullptr;
                    cgltf_accessor const* accR = nullptr;
                    cgltf_accessor const* accS = nullptr;
                    for (cgltf_size a = 0; a < inst.attributes_count; ++a) {
                        auto const& at = inst.attributes[a];
                        if (!at.name) continue;
                        if (strcmp(at.name, "TRANSLATION") == 0) accT = at.data;
                        else if (strcmp(at.name, "ROTATION") == 0) accR = at.data;
                        else if (strcmp(at.name, "SCALE") == 0) accS = at.data;
                    }
                    cgltf_size const instCount = accT ? accT->count
                        : (accR ? accR->count : (accS ? accS->count : 0));
                    for (cgltf_size ii = 0; ii < instCount; ++ii) {
                        GltfMesh::InstanceTransform it;
                        if (accT) { float v[3]; if (ReadFloat3(accT, ii, v)) { it.translation[0]=v[0]; it.translation[1]=v[1]; it.translation[2]=v[2]; } }
                        if (accR) { float v[4]; if (ReadFloat4(accR, ii, v)) { it.rotation[0]=v[0]; it.rotation[1]=v[1]; it.rotation[2]=v[2]; it.rotation[3]=v[3]; } }
                        if (accS) { float v[3]; if (ReadFloat3(accS, ii, v)) { it.scale[0]=v[0]; it.scale[1]=v[1]; it.scale[2]=v[2]; } }
                        mesh.instances.push_back(it);
                    }
                    if (getenv("DANQING_GLTF_TRACE"))
                        printf("[TRV] EXT_mesh_gpu_instancing: %zu instances (T=%p R=%p S=%p)\n",
                               mesh.instances.size(), (void const*)accT, (void const*)accR, (void const*)accS);
                }

                // ← resolveTexture(:2544-2565): decode once at load; failure
                //   degrades to the scalar baseColorFactor path (never fails
                //   the whole import — spec §6).
                if (baseColorTex && baseColorTex->image) {
                    if (auto source = ResolveImageSource(baseColorTex->image, baseDir)) {
                        if (auto decoded = dqCommon::DecodeImage(*source)) {
                            mesh.baseColorTexture = std::move(decoded);
                            mesh.texCoordSet = texCoordSet;
                        }
                    }
                }

                // ← findTextureMapping(:2578-2583): the normal map resolves
                //   independently of the pattern texture (isTransparent always
                //   false). Decode failure degrades to "no normal map"
                //   (resolveTexture miss — spec §6, never fails the import);
                //   greenUp=true (:2587) is applied at the binding site.
                if (normalMapTex && normalMapTex->image) {
                    if (auto source = ResolveImageSource(normalMapTex->image, baseDir)) {
                        if (auto decoded = dqCommon::DecodeImage(*source)) {
                            mesh.normalMapTexture = std::move(decoded);
                        }
                    }
                }

                outMeshes.push_back(std::move(mesh));
            }
        }
    }

    // Recurse into children
    for (cgltf_size c = 0; c < node->children_count; ++c) {
        if (!TraverseNode(node->children[c], worldTransform, outMeshes, baseDir, traversed))
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// GltfScene
// ---------------------------------------------------------------------------
void GltfScene::computeBounds()
{
    bounds = dqGeom::Range3d::CreateNull();
    for (auto const& mesh : meshes) {
        if (mesh.polyface) {
            for (size_t i = 0; i < mesh.polyface->Data().points.size(); ++i) {
                dqGeom::Point3d const& pt = mesh.polyface->Data().points[i];
                dqGeom::Point3d worldPt = mesh.transform.MultiplyPoint3d(pt);
                bounds.ExtendPoint(worldPt);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Helper: build GltfScene from parsed cgltf_data
// ---------------------------------------------------------------------------
static std::unique_ptr<GltfScene> BuildSceneFromData(cgltf_data* gltfData, std::string const& baseDir)
{
    auto scene = std::make_unique<GltfScene>();
    if (gltfData->scene && gltfData->scene->name) {
        scene->name = gltfData->scene->name;
    }

    // Root transform = the mandatory yAxisUp conversion (rotX(+90°): (x,y,z)->(x,-z,y)).
    // Ported from: itwinjs-core readGltfTemplate (GltfReader.ts:2687-2690):
    //   const props = GltfReaderProps.create(args.gltf, TRUE, baseUrl);
    //   // glTF supports exactly one coordinate system with y axis up.
    // ... -> GltfReader._yAxisUp -> getTileTransform (:591-592):
    //   transform = transform.multiplyTransformMatrix3d(
    //       Matrix3d.createRotationAroundVector(Vector3d.create(1,0,0), Angle.piOver2));
    // The tileTransform wraps ALL node transforms (rotX90 · nodeWorld). DanQing 的
    // glTF 入口唯一且即装饰路径（= readGltfTemplate），故恒为 true。BoxTextured
    // 自带 rotX(-90°) 节点矩阵与该转换互逆抵消——参考 DTA 实际显示的是原始
    // glTF 立方体（2026-09-15 贴图朝向 saga：漏此转换令除 Front/Back 外全部
    // 视图与 DTA 差 90°/180°）。
    dqGeom::Matrix3d const yAxisUpRot = dqGeom::Matrix3d::CreateRowValues(
        1.0, 0.0, 0.0,
        0.0, 0.0, -1.0,
        0.0, 1.0, 0.0);
    dqGeom::Transform const rootTransform(
        dqGeom::Point3d::FromZero(), yAxisUpRot);

    // Traverse all root nodes in the default scene
    // (traversed set shared across roots — reference GltfSchema.ts:301/332)
    std::set<cgltf_node const*> traversed;
    bool traversalOk = true;
    if (gltfData->scene) {
        for (cgltf_size i = 0; i < gltfData->scene->nodes_count; ++i) {
            if (!TraverseNode(gltfData->scene->nodes[i], rootTransform, scene->meshes, baseDir, traversed)) {
                traversalOk = false;
                break;
            }
        }
    } else if (gltfData->nodes_count > 0) {
        // No scene defined, use all root nodes
        for (cgltf_size i = 0; i < gltfData->nodes_count && traversalOk; ++i) {
            bool isRoot = true;
            for (cgltf_size j = 0; j < gltfData->nodes_count && isRoot; ++j) {
                for (cgltf_size c = 0; c < gltfData->nodes[j].children_count; ++c) {
                    if (gltfData->nodes[j].children[c] == &gltfData->nodes[i]) {
                        isRoot = false;
                        break;
                    }
                }
            }
            if (isRoot) {
                if (!TraverseNode(&gltfData->nodes[i], rootTransform, scene->meshes, baseDir, traversed))
                    traversalOk = false;
            }
        }
    }
    if (!traversalOk)
        return nullptr;

    scene->computeBounds();
    return scene;
}

// ---------------------------------------------------------------------------
// GltfReader::LoadFromMemory
// ---------------------------------------------------------------------------
std::unique_ptr<GltfScene> GltfReader::LoadFromMemory(
    uint8_t const* data, size_t dataSize,
    std::string const& baseDir)
{
    sLastError.clear();

    if (!data || dataSize == 0) {
        sLastError = "Invalid input data";
        return nullptr;
    }

    // Parse with cgltf
    cgltf_options options = {};
    memset(&options, 0, sizeof(options));
    cgltf_data* gltfData = nullptr;

    cgltf_result result = cgltf_parse(&options, data, dataSize, &gltfData);
    if (result != cgltf_result_success) {
        sLastError = "cgltf_parse failed: " + std::to_string(static_cast<int>(result));
        return nullptr;
    }

    if (!gltfData) {
        sLastError = "cgltf_parse returned null data";
        return nullptr;
    }

    // Load buffer data (resolves external URIs)
    char const* basePath = baseDir.empty() ? nullptr : baseDir.c_str();
    result = cgltf_load_buffers(&options, gltfData, basePath);
    if (result != cgltf_result_success) {
        sLastError = "cgltf_load_buffers failed: " + std::to_string(static_cast<int>(result));
        cgltf_free(gltfData);
        return nullptr;
    }

    // Build scene
    auto scene = BuildSceneFromData(gltfData, baseDir);
    cgltf_free(gltfData);
    return scene;
}

// ---------------------------------------------------------------------------
// GltfReader::LoadFromFile
// ---------------------------------------------------------------------------
std::unique_ptr<GltfScene> GltfReader::LoadFromFile(std::string const& filePath)
{
    sLastError.clear();

    if (filePath.empty()) {
        sLastError = "Empty file path";
        return nullptr;
    }

    // Read file into memory (more reliable than cgltf_parse_file across platforms)
    // Unicode 路径（2026-09-16，Unicode❤♻Test）：MSVC 窄字符 ifstream 按 ANSI
    // 代码页解释路径——UTF-8 字节串（❤♻ 等）无法解码 → 打开失败。Windows 上把
    // UTF-8 转 UTF-16 走宽字符打开（_wfopen + fdopen 等价流）；非 Windows 用原
    // 窄字符路径（POSIX 文件系统是字节序，UTF-8 原样可用）。
    std::ifstream file;
#ifdef _WIN32
    FILE* fp = nullptr;
    {
        // UTF-8 → UTF-16（Windows API；引擎 SDK 层不依赖 Qt）。
        int const wlen = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0);
        if (wlen > 0) {
            std::wstring wpath(static_cast<size_t>(wlen - 1), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, wpath.data(), wlen);
            fp = _wfopen(wpath.c_str(), L"rb");
        }
    }
    if (fp) {
        // 用 fd 直接读入内存（不经 ifstream 的路径重开——文件已由宽路径打开）。
        std::fseek(fp, 0, SEEK_END);
        long const fsize = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);
        if (fsize > 0) {
            std::vector<uint8_t> buf(static_cast<size_t>(fsize));
            size_t const got = std::fread(buf.data(), 1, buf.size(), fp);
            std::fclose(fp);
            if (got == buf.size()) {
                // 复用 LoadFromMemory 的解析链。
                std::string baseDir;
                size_t const ls = filePath.find_last_of("/\\");
                if (ls != std::string::npos) {
                    baseDir = filePath.substr(0, ls + 1);
                    for (auto& c : baseDir) if (c == '\\') c = '/';
                }
                return LoadFromMemory(buf.data(), buf.size(), baseDir);
            }
        } else {
            std::fclose(fp);
        }
    }
    // 宽路径失败才回落窄字符 ifstream（保持原错误语义）。
    file.open(filePath, std::ios::binary | std::ios::ate);
#else
    file.open(filePath, std::ios::binary | std::ios::ate);
#endif
    if (!file.is_open()) {
        sLastError = "Failed to open file: " + filePath;
        return nullptr;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        sLastError = "Failed to read file: " + filePath;
        return nullptr;
    }

    // ← itwinjs-core GltfReaderProps.create baseUrl — external buffers/textures
    //   resolve relative to the .gltf file's directory (resolveUrl semantics).
    std::string baseDir;
    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseDir = filePath.substr(0, lastSlash + 1);  // keep trailing separator
        // Normalize to forward slashes for cgltf_combine_paths.
        for (auto& c : baseDir) {
            if (c == '\\') c = '/';
        }
    }

    return LoadFromMemory(buffer.data(), fileSize, baseDir);
}

// ---------------------------------------------------------------------------
// GltfReader::getLastError
// ---------------------------------------------------------------------------
std::string const& GltfReader::getLastError()
{
    return sLastError;
}

}  // namespace dqRender
