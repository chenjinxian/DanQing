// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — glTF/GLB reader
// Ported from: itwinjs-core core/frontend/src/tile/GltfReader.ts
// APPROVED DEVIATION (audit F2b, §8.3 cgltf note, 架构师签字 2026-07-10): itwinjs
// GltfReader.ts (2975 lines) is the §0 reference; this C++ port parses via cgltf
// (jkuhlmann/cgltf, third_party/cgltf/) instead of re-porting the reference's Bentley
// glTF stack. glTF 2.0 is a Khronos open standard (the actual "reference"); GltfReader.ts
// is one implementation of it, and cgltf is a mature spec-conformant parser — behaviorally
// equivalent, no Bentley-proprietary algorithm/data-structure. Registered as an approved
// third-party parser substitution (NOT a §0 violation). Mesh geometry extraction
// (→ IndexedPolyface) stays behavior-aligned with the reference.
//
// Uses cgltf (single-header C99 library) to parse glTF 2.0 files.
// Extracts mesh geometry into IndexedPolyface for rendering.
#pragma once

#include "Export.h"

#include <dqBase/RefCounted.h>
#include <dqCommon/Image.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dqRender {

// ---------------------------------------------------------------------------
// GltfMesh — a single mesh extracted from a glTF file
// ---------------------------------------------------------------------------
struct GltfMesh {
    std::string name;
    dqGeom::Transform transform;  // world transform (accumulated from scene graph)
    dqBase::RefPtr<dqGeom::IndexedPolyface> polyface;

    // EXT_mesh_gpu_instancing（Khronos spec：节点级实例化，无参考实现——itwinjs
    // 与 cgltf 参考均未实现 GPU 实例化渲染；此处按 spec 语义展开为每实例一个
    // 世界变换，由 GltfDecoration 展开为 N 份 mesh）。空 = 非实例化节点。
    // Authored: no reference implementation exists in itwinjs-core for
    //           EXT_mesh_gpu_instancing; semantics per Khronos spec
    //           (instance world = node.worldTransform × instance(TRS))。
    struct InstanceTransform {
        float translation[3] = {0, 0, 0};
        float rotation[4] = {0, 0, 0, 1};  // quaternion (x,y,z,w)
        float scale[3] = {1, 1, 1};
    };
    std::vector<InstanceTransform> instances;

    // Material info (basic for Phase 1)
    float baseColorFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.5f;

    // baseColor texture (spec C2). Populated only when the primitive's material
    // has pbr.baseColorTexture (falling back to emissiveTexture per
    // extractTextureId order); empty optional = scalar baseColorFactor path.
    // Ported from: itwinjs-core GltfReader.ts extractTextureId(:1228-1252) +
    //               resolveImage(:2487-2521) + resolveTexture(:2544-2565).
    std::optional<dqCommon::ImageBuffer> baseColorTexture;
    int texCoordSet = 0;  // baseColorTexture.texCoord (usually 0)

    // Normal map texture. Populated only when the primitive's material has
    // normalTexture; empty optional = no normal mapping. The reference resolves
    // it independently of the pattern texture (isTransparent = false always)
    // and always binds it greenUp — GltfReader.ts extractNormalMapId(:1254-1262)
    // + findTextureMapping(:2578-2597). Decode failure degrades to "no normal
    // map" (resolveTexture miss, spec §6 — never fails the import).
    // The normal map shares the baseColor UV chain: the reference's UV accessor
    // selection reads only the pattern texture's texCoord index (GltfReader.ts
    // :1601-1605) and findTextureMapping never reads normalTexture.texcoord.
    std::optional<dqCommon::ImageBuffer> normalMapTexture;
};

// ---------------------------------------------------------------------------
// GltfScene — all meshes loaded from a glTF file
// ---------------------------------------------------------------------------
struct GltfScene {
    std::string name;
    std::vector<GltfMesh> meshes;
    dqGeom::Range3d bounds;  // world-space bounding box of all meshes

    /// Compute bounds from all meshes.
    void computeBounds();
};

// ---------------------------------------------------------------------------
// GltfReader — loads glTF/GLB files and extracts mesh geometry
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT GltfReader {
public:
    /// Load a glTF or GLB file from disk.
    /// @param filePath Path to .gltf or .glb file.
    /// @return Loaded scene, or nullptr on error. Call getLastError() for details.
    static std::unique_ptr<GltfScene> LoadFromFile(std::string const& filePath);

    /// Load glTF from a memory buffer (GLB binary or JSON).
    /// @param data Pointer to file data.
    /// @param dataSize Size in bytes.
    /// @param baseDir Base directory for resolving external resources (textures, buffers).
    /// @return Loaded scene, or nullptr on error.
    static std::unique_ptr<GltfScene> LoadFromMemory(
        uint8_t const* data, size_t dataSize,
        std::string const& baseDir = "");

    /// Get the last error message.
    static std::string const& getLastError();
};

}  // namespace dqRender
