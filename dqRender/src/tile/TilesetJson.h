// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — shared minimal JSON parser (internal)
// Ported from: the browser JSON.parse + per-level field walk used by the
// reference's tileset and imdl consumers (RealityModelTileTree.getChildrenProps /
// ParseImdlDocument.ts) — see RealityTileTree.cpp's provenance note for the
// string-scanner history (replaced 2026-09-21 after the content-leak bug).
#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

#include <dqRender/tile/RealityTile.h>
#include <dqRender/tile/RealityTileTree.h>
#include <dqCommon/LinePixels.h>
#include <dqRender/tile/ImdlDocument.h>

#include "CompactEdges.h"  // U11(3)：parseImdlEdges 的 compact 兜底展开
                           //（ParseImdlDocument.ts:695-708 parseCompactEdges）

#include <map>
#include <optional>
#include <utility>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

namespace tilejson {

struct JsonValue {
    enum class Type : uint8_t { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;  // insertion-ordered

    JsonValue const* find(char const* key) const
    {
        for (auto const& kv : obj)
            if (kv.first == key)
                return &kv.second;
        return nullptr;
    }
};

struct JsonParser {
    std::string_view text;
    size_t pos = 0;
    bool failed = false;

    void skipWs()
    {
        while (pos < text.size()) {
            char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') pos++;
            else break;
        }
    }

    bool consume(char c)
    {
        skipWs();
        if (pos < text.size() && text[pos] == c) {
            pos++;
            return true;
        }
        failed = true;
        return false;
    }

    bool literal(char const* lit)
    {
        skipWs();
        size_t const n = std::strlen(lit);
        if (text.size() - pos >= n && text.substr(pos, n) == lit) {
            pos += n;
            return true;
        }
        return false;
    }

    JsonValue parseValue()
    {
        JsonValue v;
        skipWs();
        if (pos >= text.size()) { failed = true; return v; }
        char c = text[pos];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') { v.type = JsonValue::Type::String; v.str = parseString(); return v; }
        if (literal("true"))  { v.type = JsonValue::Type::Bool; v.boolean = true;  return v; }
        if (literal("false")) { v.type = JsonValue::Type::Bool; v.boolean = false; return v; }
        if (literal("null"))  { v.type = JsonValue::Type::Null; return v; }
        return parseNumber();
    }

    JsonValue parseObject()
    {
        JsonValue v;
        v.type = JsonValue::Type::Object;
        if (!consume('{')) return v;
        skipWs();
        if (pos < text.size() && text[pos] == '}') { pos++; return v; }
        while (true) {
            skipWs();
            std::string key = parseString();
            if (failed) return v;
            if (!consume(':')) return v;
            v.obj.emplace_back(std::move(key), parseValue());
            if (failed) return v;
            skipWs();
            if (pos < text.size() && text[pos] == ',') { pos++; continue; }
            if (pos < text.size() && text[pos] == '}') { pos++; return v; }
            failed = true;
            return v;
        }
    }

    JsonValue parseArray()
    {
        JsonValue v;
        v.type = JsonValue::Type::Array;
        if (!consume('[')) return v;
        skipWs();
        if (pos < text.size() && text[pos] == ']') { pos++; return v; }
        while (true) {
            v.arr.push_back(parseValue());
            if (failed) return v;
            skipWs();
            if (pos < text.size() && text[pos] == ',') { pos++; continue; }
            if (pos < text.size() && text[pos] == ']') { pos++; return v; }
            failed = true;
            return v;
        }
    }

    std::string parseString()
    {
        std::string out;
        skipWs();
        if (pos >= text.size() || text[pos] != '"') { failed = true; return out; }
        pos++;
        while (pos < text.size()) {
            char c = text[pos++];
            if (c == '"') return out;
            if (c == '\\' && pos < text.size()) {
                char e = text[pos++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (text.size() - pos < 4) { failed = true; return out; }
                        unsigned code = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = text[pos++];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= unsigned(h - '0');
                            else if (h >= 'a' && h <= 'f') code |= unsigned(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= unsigned(h - 'A' + 10);
                            else { failed = true; return out; }
                        }
                        // UTF-8 encode (BMP only — tileset.json keys/URIs are ASCII)
                        if (code < 0x80) {
                            out += char(code);
                        } else if (code < 0x800) {
                            out += char(0xC0 | (code >> 6));
                            out += char(0x80 | (code & 0x3F));
                        } else {
                            out += char(0xE0 | (code >> 12));
                            out += char(0x80 | ((code >> 6) & 0x3F));
                            out += char(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: failed = true; return out;
                }
            } else {
                out += c;
            }
        }
        failed = true;
        return out;
    }

    JsonValue parseNumber()
    {
        JsonValue v;
        v.type = JsonValue::Type::Number;
        size_t start = pos;
        if (pos < text.size() && (text[pos] == '-' || text[pos] == '+')) pos++;
        bool any = false;
        while (pos < text.size()) {
            char c = text[pos];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E'
                || c == '-' || c == '+') {
                pos++;
                if (c >= '0' && c <= '9') any = true;
            } else break;
        }
        if (!any) { failed = true; return v; }
        v.number = std::strtod(std::string(text.substr(start, pos - start)).c_str(), nullptr);
        return v;
    }
};

inline std::unique_ptr<JsonValue> parseJsonDocument(std::string_view text)
{
    auto doc = std::make_unique<JsonValue>();
    JsonParser p{text};
    *doc = p.parseValue();
    p.skipWs();
    if (p.failed || p.pos != text.size())
        return nullptr;
    return doc;
}

// Parse a bounding volume from the structured JSON tree.
// Supports "box" (12 floats), "sphere" (4 floats), "region" (6 floats).
inline BoundingVolume parseBoundingVolume(JsonValue const& bv)
{
    BoundingVolume out;
    JsonValue const* box = bv.find("box");
    JsonValue const* sphere = bv.find("sphere");
    JsonValue const* region = bv.find("region");
    JsonValue const* arr = nullptr;

    if (box) { out.type = BoundingVolumeType::Box; arr = box; }
    else if (sphere) { out.type = BoundingVolumeType::Sphere; arr = sphere; }
    else if (region) { out.type = BoundingVolumeType::Region; arr = region; }
    else return out;

    for (auto const& el : arr->arr)
        out.values.push_back(static_cast<float>(el.number));
    return out;
}

// Parse a single tileset node from the structured JSON tree.
inline TilesetNode parseTilesetNode(JsonValue const& node)
{
    TilesetNode out;
    if (JsonValue const* bv = node.find("boundingVolume"))
        out.boundingVolume = parseBoundingVolume(*bv);
    if (JsonValue const* ge = node.find("geometricError"))
        out.geometricError = static_cast<float>(ge->number);
    if (JsonValue const* refine = node.find("refine"))
        out.refine = refine->str;
    if (JsonValue const* content = node.find("content")) {
        // 3D Tiles 1.0 "url" / 1.1 "uri" — both accepted (spec transition).
        if (JsonValue const* uri = content->find("uri"))
            out.contentUri = uri->str;
        else if (JsonValue const* url = content->find("url"))
            out.contentUri = url->str;
    }
    if (JsonValue const* children = node.find("children"))
        for (auto const& child : children->arr)
            out.children.push_back(parseTilesetNode(child));
    return out;
}

// Compute a Range3d from a bounding volume.
[[maybe_unused]] inline dqGeom::Range3d rangeFromBoundingVolume(BoundingVolume const& bv)
{
    if (bv.type == BoundingVolumeType::Box && bv.values.size() >= 12) {
        // Box: center(3) + x-axis(3) + y-axis(3) + z-axis(3)
        float cx = bv.values[0], cy = bv.values[1], cz = bv.values[2];
        float ex = bv.values[3] + bv.values[6] + bv.values[9];
        float ey = bv.values[4] + bv.values[7] + bv.values[10];
        float ez = bv.values[5] + bv.values[8] + bv.values[11];
        return dqGeom::Range3d::CreateXYZXYZ(
            cx - ex, cy - ey, cz - ez,
            cx + ex, cy + ey, cz + ez);
    }

    if (bv.type == BoundingVolumeType::Sphere && bv.values.size() >= 4) {
        float cx = bv.values[0], cy = bv.values[1], cz = bv.values[2], r = bv.values[3];
        return dqGeom::Range3d::CreateXYZXYZ(cx - r, cy - r, cz - r, cx + r, cy + r, cz + r);
    }

    // Default: unit range.
    return dqGeom::Range3d::CreateXYZXYZ(-1, -1, -1, 1, 1, 1);
}

// parseImdlMeshes — extract the mesh primitives' vertex-table and surface
// metadata from an imdl document's glTF JSON (the subset ImdlGraphics reads:
// meshes.{name}.primitives[].vertices/surface/material). Layout verified
// byte-level against the recorded v1.1 fixtures (2026-09-23): vertex tables
// are texture-layout RGBA rows, numRgbaPerVertex×4 bytes per vertex, with
// the first three u16s the 16-bit quantized (x,y,z); surface indices are
// 24-bit little-endian.
struct ImdlVertexTableProps {
    std::string bufferView;
    uint32_t count = 0;
    uint32_t width = 0;
    uint32_t height = 1;
    uint32_t numRgbaPerVertex = 4;
    double decodedMin[3] = {0, 0, 0};
    double decodedMax[3] = {0, 0, 0};
    uint32_t uniformColor = 0;  // packed 0xRRGGBB? (reference fillColor int)
    bool hasUniformColor = false;
    // TD-27（M-M(4)）：非量化顶点表（numRgbaPerVertex=5，20B/顶点——位置为
    // 跨 4 texel .w 通道转置重组的 IEEE f32，VertexTableBuilder.ts Unquantized
    // 命名空间）。ParseImdlDocument.ts:1031 原样透传。
    bool usesUnquantizedPositions = false;
};

// Ported from: ImdlSchema.ts:86-97 SurfaceMaterialParams（内联形态——
// alpha / diffuse{color,weight} / specular{color,weight,exponent}；颜色为
// [0..1] 三分量数组，ColorDef 域换算 v*255+0.5 在消费点，
// ParseImdlDocument.ts:1105-1107 colorDefFromMaterialJson）。
struct ImdlSurfaceMaterialProps {
    bool isInline = false;  // false = string 键（key 有效）
    std::string key;        // renderMaterials 表键
    std::optional<float> alpha;
    std::optional<float> diffuseWeight;
    std::optional<float> diffuseColor[3];
    std::optional<float> specularWeight;
    std::optional<float> specularColor[3];
    std::optional<float> specularExponent;
};

// Ported from: ImdlSchema.ts:90-110 RenderMaterialJson（renderMaterials 表项
// ——ImdlGraphicsCreator.ts:196-241 getMaterial 的全字段消费面；颜色 [0..1]
// 三分量；transparency → alpha=1-t；reflect*/refract/shadows/ambient 参考侧
// 解析后进 RenderMaterialParams 但 shader 不消费（core-common 标注 Currently
// unused）——此处提取保留、换算层丢弃）。textureMapping 归 textured 变体
// （M-M(3)），此处不提取。
struct ImdlRenderMaterialProps {
    bool valid = false;
    std::optional<float> diffuseWeight;
    std::optional<float> diffuseColor[3];
    std::optional<float> specularWeight;
    std::optional<float> specularColor[3];
    std::optional<float> specularExponent;
    std::optional<float> transparency;  // alpha = 1 - transparency
};

struct ImdlSurfaceProps {
    std::string indicesView;
    uint32_t type = 0;
    // M-M(1)：surface 材质（ImdlSchema.ts:92-104 SurfaceMaterial——string 键
    // 指向 renderMaterials 表，或内联 SurfaceMaterialParams；atlas 形态当前
    // 采面未命中，登记）。原始字段提取，换算归消费点（ImdlGraphics.cpp）。
    std::optional<ImdlSurfaceMaterialProps> material;
    // M-M(3)：textured surface 的 UV 量化参数（ImdlSchema.ts SurfaceParams
    // .uvParams——QParams2dProps {decodedMin/Max[2]}；surface.type 2/3
    // Textured/TexturedLit 携带）。
    bool hasUvParams = false;
    double uvDecodedMin[2] = {0.0, 0.0};
    double uvDecodedMax[2] = {0.0, 0.0};
};

// ---------------------------------------------------------------------------
// U11(1)：ImdlMeshEdges 四形态（JSON 侧——视图持 bufferView 名，字节区间由
// parseImdlEdges 解析期换入 ImdlDocument.h 的 ImdlEdgeParams 族）。
// Ported from: itwinjs-core ImdlSchema.ts:195-286（字段名逐一 1:1）。
// ---------------------------------------------------------------------------

// Ported from: ImdlSchema.ts:195-202 ImdlSegmentEdges（:197 indices / :201
//              endPointAndQuadIndices——bufferView 名 → *View 后缀，照本文件
//              ImdlSurfaceProps.indicesView 先例）。
struct ImdlSegmentEdgesProps {
    std::string indicesView;
    std::string endPointAndQuadIndicesView;
};

// Ported from: ImdlSchema.ts:209-212 ImdlSilhouetteEdges（extends
//              ImdlSegmentEdges + :211 normalPairs）。
struct ImdlSilhouetteEdgesProps : ImdlSegmentEdgesProps {
    std::string normalPairsView;
};

// Ported from: ImdlSchema.ts:219-232 ImdlIndexedEdges（:221 indices / :223
//              edges / :225 width / :227 height / :229 numSegments / :231
//              silhouettePadding）。
struct ImdlIndexedEdgesProps {
    std::string indicesView;
    std::string edgeTableView;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t numSegments = 0;
    uint32_t silhouettePadding = 0;
};

// Ported from: ImdlSchema.ts:257-272 ImdlCompactEdges（:262 visibility /
//              :267 normalPairs?——无 silhouette 时 undefined / :271
//              numVisible）。
struct ImdlCompactEdgesProps {
    std::string visibilityView;
    std::optional<std::string> normalPairsView;
    uint32_t numVisible = 0;
};

// Ported from: ImdlSchema.ts:277-286 ImdlMeshEdges（:285 polylines 随
//              polyline 图元落 Task 6——TODO ParseImdlDocument.ts:716
//              parseTesselatedPolyline）。
struct ImdlMeshEdgesProps {
    std::optional<ImdlSegmentEdgesProps> segments;
    std::optional<ImdlSilhouetteEdgesProps> silhouettes;
    std::optional<ImdlIndexedEdgesProps> indexed;
    std::optional<ImdlCompactEdgesProps> compact;
};

// ---------------------------------------------------------------------------
// TD-25：ImdlInstances——per-primitive 实例放置参数（JSON 侧——bufferView 名，
// 字节区间由消费点 createImdlLutGraphics 用 findBufferView 换入）。
// Ported from: itwinjs-core ImdlSchema.ts:156-166（ImdlInstances 字段逐一）+
//              ParseImdlDocument.ts parseInstances（:1045-1087——count/
//              transformCenter[3]/featureIds/transforms bufferView 名提取，
//              symbologyOverrides 可选）。
// ---------------------------------------------------------------------------

// Ported from: ImdlSchema.ts:160-166 ImdlInstances（:161 count / :162
//              transformCenter / :163 featureIds / :164 transforms /
//              :165 symbologyOverrides?——bufferView 名 → *View 后缀，照
//              ImdlSurfaceProps.indicesView 先例）。
struct ImdlInstancesProps {
    uint32_t count = 0;
    double transformCenter[3] = {0.0, 0.0, 0.0};
    std::string featureIdsView;
    std::string transformsView;
    std::optional<std::string> symbologyOverridesView;
};

// M-M(3)：namedTextures 表（ImdlSchema.ts:56-80 ImdlNamedTexture——名字 →
// 图像载荷/元数据）。bufferView 存在 = 图像字节嵌入瓦内（loadNamedTexture
// :70-80 直读 tile 内容）；缺席 = 外部请求（本采面未命中——housemodel 36
// 纹理瓦全为嵌入 glyph atlas gta0）。
struct ImdlNamedTextureProps {
    bool valid = false;
    std::string bufferView;
    uint32_t format = 0;       // ImageSourceFormat（Jpeg=0/Png=2/Svg=3）
    uint32_t transparency = 0; // ImdlTextureTransparency
    uint32_t width = 0;
    uint32_t height = 0;
    bool isGlyph = false;
    bool isTileSection = false;
};

struct ImdlPrimitiveProps {
    ImdlVertexTableProps vertices;
    ImdlSurfaceProps surface;
    bool isPlanar = false;  // ImdlSchema.ts:177 mesh primitive isPlanar → 参考侧
                            // 决定 OpaquePlanar pass 归属（Task 5 LUT 路径）
    std::string material;   // ImdlSchema.ts:173 material——关联 ImdlDisplayParams 的 Id
    // M-M(3)：DisplayParams.materials[material].texture.name（housemodel 实测
    // 形态——纹理引用在 DisplayParams 级的 texture 字段：{name, params}，非
    // surface.textureMapping；ParseImdlDocument.ts:1218-1231 parseDisplayParams
    // 的 texture 分支）。textureWeight = params.weight（0 = 纯纹理采样）。
    std::string textureName;
    double textureWeight = 0.0;
    std::optional<ImdlMeshEdgesProps> edges;  // ImdlSchema.ts:277-286（mesh primitive）
    std::optional<ImdlInstancesProps> instances;  // ImdlSchema.ts:181 instances?

    // M-M(2)：图元类型（MeshPrimitiveType——core/frontend/src/common/internal/
    // render/MeshPrimitive.ts:13-17：Mesh=0 / Polyline=1 / Point=2）。
    uint32_t primType = 0;
    // polyline 图元（ImdlSchema.ts:47-61 PolylineParams——tesselated 三视图在
    // primitive 顶层，与 mesh 的 surface/edges 嵌套不同）。
    std::string polylineIndicesView;              // TesselatedPolyline.indices
    std::string polylinePrevIndicesView;          // TesselatedPolyline.prevIndices
    std::string polylineNextIndicesAndParamsView; // TesselatedPolyline.nextIndicesAndParams
    // point 图元（ImdlSchema.ts:41-45 PointStringParams——顶层 indices）。
    std::string pointIndicesView;

    // DisplayParams 子集（width/linePixels）——materials[material] 的
    // lineWidth/linePixels（parsePrimitive ParseImdlDocument.ts:800-802 +
    // parseDisplayParams :1206-1207；ImdlEdgeParams.weight/linePixels 的来源
    // :730-731）。默认值照 DisplayParams 构造（DisplayParams.ts:36——width=0、
    // linePixels=Solid）。参考 :803 无 displayParams 的 primitive 整体丢弃——
    // 丢弃决策归消费方（Task 5/6 图形 pass），本提取层不丢（hasDisplayParams
    // 记录 materials[material] 是否存在）。
    uint32_t width = 0;
    dqCommon::LinePixels linePixels = dqCommon::LinePixels::Solid;
    bool hasDisplayParams = false;
};

inline std::vector<ImdlPrimitiveProps> parseImdlMeshPrimitives(JsonValue const& doc)
{
    std::vector<ImdlPrimitiveProps> out;
    JsonValue const* meshes = doc.find("meshes");
    if (!meshes)
        return out;
    // displayParams 查找源（parsePrimitive ParseImdlDocument.ts:801——
    // this._document.materials[materialName]）。
    JsonValue const* materials = doc.find("materials");
    for (auto const& meshEntry : meshes->obj) {
        JsonValue const* primitives = meshEntry.second.find("primitives");
        if (!primitives)
            continue;
        for (auto const& prim : primitives->arr) {
            ImdlPrimitiveProps props;
            // 图元类型（MeshPrimitive.ts:13-17——mesh=0/polyline=1/point=2；
            // M-M(2) 前只解析 mesh 形态）。
            if (JsonValue const* pt = prim.find("type"))
                if (pt->type == JsonValue::Type::Number)
                    props.primType = static_cast<uint32_t>(pt->number);
            if (props.primType == 1u) {
                // TesselatedPolyline（ImdlSchema.ts:47-51 + ParseImdlDocument.ts
                // parsePrimitive 的 polyline 分支——顶层三视图）。
                if (JsonValue const* v = prim.find("indices"))
                    props.polylineIndicesView = v->str;
                if (JsonValue const* v = prim.find("prevIndices"))
                    props.polylinePrevIndicesView = v->str;
                if (JsonValue const* v = prim.find("nextIndicesAndParams"))
                    props.polylineNextIndicesAndParamsView = v->str;
            } else if (props.primType == 2u) {
                if (JsonValue const* v = prim.find("indices"))
                    props.pointIndicesView = v->str;
            }
            if (JsonValue const* verts = prim.find("vertices")) {
                if (JsonValue const* bv = verts->find("bufferView"))
                    props.vertices.bufferView = bv->str;
                if (JsonValue const* c = verts->find("count"))
                    props.vertices.count = static_cast<uint32_t>(c->number);
                if (JsonValue const* w = verts->find("width"))
                    props.vertices.width = static_cast<uint32_t>(w->number);
                if (JsonValue const* h = verts->find("height"))
                    props.vertices.height = static_cast<uint32_t>(h->number);
                if (JsonValue const* nr = verts->find("numRgbaPerVertex"))
                    props.vertices.numRgbaPerVertex = static_cast<uint32_t>(nr->number);
                if (JsonValue const* params = verts->find("params")) {
                    if (JsonValue const* mn = params->find("decodedMin"))
                        for (int i = 0; i < 3 && i < static_cast<int>(mn->arr.size()); ++i)
                            props.vertices.decodedMin[i] = mn->arr[i].number;
                    if (JsonValue const* mx = params->find("decodedMax"))
                        for (int i = 0; i < 3 && i < static_cast<int>(mx->arr.size()); ++i)
                            props.vertices.decodedMax[i] = mx->arr[i].number;
                }
                if (JsonValue const* uc = verts->find("uniformColor")) {
                    props.vertices.uniformColor = static_cast<uint32_t>(uc->number);
                    props.vertices.hasUniformColor = true;
                }
                if (JsonValue const* uq = verts->find("usesUnquantizedPositions")) {
                    if (uq->type == JsonValue::Type::Bool)
                        props.vertices.usesUnquantizedPositions = uq->boolean;
                }
            }
            if (JsonValue const* surf = prim.find("surface")) {
                if (JsonValue const* ind = surf->find("indices"))
                    props.surface.indicesView = ind->str;
                if (JsonValue const* t = surf->find("type"))
                    props.surface.type = static_cast<uint32_t>(t->number);
                // M-M(3)：uvParams（QParams2d——textured surface）。
                if (JsonValue const* uvp = surf->find("uvParams")) {
                    if (JsonValue const* mn = uvp->find("decodedMin"))
                        for (int i = 0; i < 2 && i < static_cast<int>(mn->arr.size()); ++i)
                            props.surface.uvDecodedMin[i] = mn->arr[i].number;
                    if (JsonValue const* mx = uvp->find("decodedMax"))
                        for (int i = 0; i < 2 && i < static_cast<int>(mx->arr.size()); ++i)
                            props.surface.uvDecodedMax[i] = mx->arr[i].number;
                    props.surface.hasUvParams =
                        uvp->find("decodedMin") != nullptr && uvp->find("decodedMax") != nullptr;
                }
                // M-M(1)：surface.material（ImdlSchema.ts:104 SurfaceMaterial =
                // string 键 | 内联 SurfaceMaterialParams；ParseImdlDocument.ts
                // :467-475 convertMaterial 的两形态）。
                if (JsonValue const* m = surf->find("material")) {
                    ImdlSurfaceMaterialProps sm;
                    if (m->type == JsonValue::Type::String) {
                        sm.key = m->str;
                    } else if (m->type == JsonValue::Type::Object) {
                        sm.isInline = true;
                        if (JsonValue const* v = m->find("alpha"))
                            if (v->type == JsonValue::Type::Number)
                                sm.alpha = static_cast<float>(v->number);
                        if (JsonValue const* d = m->find("diffuse")) {
                            if (JsonValue const* v = d->find("weight"))
                                if (v->type == JsonValue::Type::Number)
                                    sm.diffuseWeight = static_cast<float>(v->number);
                            if (JsonValue const* c = d->find("color"))
                                if (c->type == JsonValue::Type::Array)
                                    for (int i = 0; i < 3 && i < static_cast<int>(c->arr.size()); ++i)
                                        sm.diffuseColor[i] = static_cast<float>(c->arr[i].number);
                        }
                        if (JsonValue const* s = m->find("specular")) {
                            if (JsonValue const* v = s->find("weight"))
                                if (v->type == JsonValue::Type::Number)
                                    sm.specularWeight = static_cast<float>(v->number);
                            if (JsonValue const* v = s->find("exponent"))
                                if (v->type == JsonValue::Type::Number)
                                    sm.specularExponent = static_cast<float>(v->number);
                            if (JsonValue const* c = s->find("color"))
                                if (c->type == JsonValue::Type::Array)
                                    for (int i = 0; i < 3 && i < static_cast<int>(c->arr.size()); ++i)
                                        sm.specularColor[i] = static_cast<float>(c->arr[i].number);
                        }
                    }
                    props.surface.material = std::move(sm);
                }
            }
            if (JsonValue const* pl = prim.find("isPlanar"))
                props.isPlanar = pl->boolean;
            // material + displayParams（parsePrimitive ParseImdlDocument.ts
            // :800-802——materialName 为空则无 displayParams）。
            if (JsonValue const* mat = prim.find("material"))
                props.material = mat->str;
            if (materials) {
                if (JsonValue const* dp = materials->find(props.material.c_str())) {
                    props.hasDisplayParams = true;
                    if (JsonValue const* w = dp->find("lineWidth"))
                        props.width = static_cast<uint32_t>(w->number);
                    if (JsonValue const* lp = dp->find("linePixels"))
                        props.linePixels =
                            static_cast<dqCommon::LinePixels>(static_cast<uint32_t>(lp->number));
                    // M-M(3)：DisplayParams 级纹理引用（{name, params{weight}}）。
                    if (JsonValue const* tex = dp->find("texture")) {
                        if (JsonValue const* nm = tex->find("name"))
                            props.textureName = nm->str;
                        if (JsonValue const* ps = tex->find("params"))
                            if (JsonValue const* wt = ps->find("weight"))
                                props.textureWeight = wt->number;
                    }
                }
            }
            // ImdlSchema.ts:277-286 edges 四形态（字段逐一）。
            if (JsonValue const* edgesJson = prim.find("edges")) {
                ImdlMeshEdgesProps edges;
                if (JsonValue const* seg = edgesJson->find("segments")) {  // :278
                    ImdlSegmentEdgesProps s;
                    if (JsonValue const* v = seg->find("indices"))
                        s.indicesView = v->str;
                    if (JsonValue const* v = seg->find("endPointAndQuadIndices"))
                        s.endPointAndQuadIndicesView = v->str;
                    edges.segments = std::move(s);
                }
                if (JsonValue const* sil = edgesJson->find("silhouettes")) {  // :279
                    ImdlSilhouetteEdgesProps s;
                    if (JsonValue const* v = sil->find("indices"))
                        s.indicesView = v->str;
                    if (JsonValue const* v = sil->find("endPointAndQuadIndices"))
                        s.endPointAndQuadIndicesView = v->str;
                    if (JsonValue const* v = sil->find("normalPairs"))
                        s.normalPairsView = v->str;
                    edges.silhouettes = std::move(s);
                }
                if (JsonValue const* idx = edgesJson->find("indexed")) {  // :283
                    ImdlIndexedEdgesProps s;
                    if (JsonValue const* v = idx->find("indices"))
                        s.indicesView = v->str;
                    if (JsonValue const* v = idx->find("edges"))
                        s.edgeTableView = v->str;
                    if (JsonValue const* v = idx->find("width"))
                        s.width = static_cast<uint32_t>(v->number);
                    if (JsonValue const* v = idx->find("height"))
                        s.height = static_cast<uint32_t>(v->number);
                    if (JsonValue const* v = idx->find("numSegments"))
                        s.numSegments = static_cast<uint32_t>(v->number);
                    if (JsonValue const* v = idx->find("silhouettePadding"))
                        s.silhouettePadding = static_cast<uint32_t>(v->number);
                    edges.indexed = std::move(s);
                }
                if (JsonValue const* cmp = edgesJson->find("compact")) {  // :284
                    ImdlCompactEdgesProps s;
                    if (JsonValue const* v = cmp->find("visibility"))
                        s.visibilityView = v->str;
                    if (JsonValue const* v = cmp->find("normalPairs"))
                        s.normalPairsView = v->str;
                    if (JsonValue const* v = cmp->find("numVisible"))
                        s.numVisible = static_cast<uint32_t>(v->number);
                    edges.compact = std::move(s);
                }
                props.edges = std::move(edges);
            }
            // ImdlSchema.ts:160-181 instances 修饰（parseInstances
            // ParseImdlDocument.ts:1045-1087——字段原样提取，本层不丢弃）。
            // 语义差异登记：参考 parseInstances 对 count<=0（:1050-1052）与
            // transformCenter 长度≠3（:1054-1056）是解析层直接 return
            // undefined（放弃实例化）；DanQing 本层把 count 与
            // transformCenter（不足 3 分量零填充）照字段提取，长度≠3 的
            // 放弃语义在消费点不可复现（原始长度信息于此丢失）——采集资产
            // 的 instances 均带合法 3 分量 transformCenter，当前不可达。
            // 消费点（createImdlLutGraphics）复现的归零 = count<=0 +
            // bufferView 缺失/尺寸不符（参考 :1060-1071 的 findBuffer 与
            // %12 assert 的 C++ 显式防御）。
            if (JsonValue const* instJson = prim.find("instances")) {
                ImdlInstancesProps inst;
                if (JsonValue const* c = instJson->find("count"))  // :161
                    inst.count = static_cast<uint32_t>(c->number);
                if (JsonValue const* tc = instJson->find("transformCenter")) {  // :162
                    for (int i = 0; i < 3 && i < static_cast<int>(tc->arr.size()); ++i)
                        inst.transformCenter[i] = tc->arr[i].number;
                }
                if (JsonValue const* f = instJson->find("featureIds"))  // :163
                    inst.featureIdsView = f->str;
                if (JsonValue const* t = instJson->find("transforms"))  // :164
                    inst.transformsView = t->str;
                if (JsonValue const* s = instJson->find("symbologyOverrides"))  // :165
                    inst.symbologyOverridesView = s->str;
                props.instances = std::move(inst);
            }
            out.push_back(std::move(props));
        }
    }
    return out;
}

// parseImdlNamedTextures — namedTextures 表（M-M(3)）。ImdlSchema.ts:56-80
// （名字 → bufferView/format/transparency/dims/isGlyph——bufferView 存在即
// 嵌入载荷）。
inline std::map<std::string, ImdlNamedTextureProps> parseImdlNamedTextures(JsonValue const& doc)
{
    std::map<std::string, ImdlNamedTextureProps> out;
    JsonValue const* table = doc.find("namedTextures");
    if (!table || table->type != JsonValue::Type::Object)
        return out;
    for (auto const& entry : table->obj) {
        ImdlNamedTextureProps nt;
        nt.valid = true;
        if (JsonValue const* v = entry.second.find("bufferView"))
            nt.bufferView = v->str;
        if (JsonValue const* v = entry.second.find("format"))
            nt.format = static_cast<uint32_t>(v->number);
        if (JsonValue const* v = entry.second.find("transparency"))
            nt.transparency = static_cast<uint32_t>(v->number);
        if (JsonValue const* v = entry.second.find("width"))
            nt.width = static_cast<uint32_t>(v->number);
        if (JsonValue const* v = entry.second.find("height"))
            nt.height = static_cast<uint32_t>(v->number);
        if (JsonValue const* v = entry.second.find("isGlyph"))
            if (v->type == JsonValue::Type::Bool)
                nt.isGlyph = v->boolean;
        if (JsonValue const* v = entry.second.find("isTileSection"))
            if (v->type == JsonValue::Type::Bool)
                nt.isTileSection = v->boolean;
        out[entry.first] = nt;
    }
    return out;
}

// parseImdlRenderMaterials — renderMaterials 表（M-M(1)）。// Ported from: ImdlGraphicsCreator.ts:192-227 getMaterial 的 string 键分支
//（`document.json.renderMaterials[mat]` → RenderMaterialParams 全字段；本层
// 只提取 shader 消费面字段，flat 键名 1:1 ImdlSchema.ts RenderMaterialJson）。
// 表缺失/键缺席 → 空 map（消费方落 Material.default，参考
// getMaterial undefined 语义）。
inline std::map<std::string, ImdlRenderMaterialProps> parseImdlRenderMaterials(JsonValue const& doc)
{
    std::map<std::string, ImdlRenderMaterialProps> out;
    JsonValue const* table = doc.find("renderMaterials");
    if (!table || table->type != JsonValue::Type::Object)
        return out;
    auto colorArr = [](JsonValue const* c, std::optional<float> dst[3]) {
        if (c && c->type == JsonValue::Type::Array)
            for (int i = 0; i < 3 && i < static_cast<int>(c->arr.size()); ++i)
                dst[i] = static_cast<float>(c->arr[i].number);
    };
    for (auto const& entry : table->obj) {
        ImdlRenderMaterialProps m;
        m.valid = true;
        if (JsonValue const* v = entry.second.find("diffuse"))
            if (v->type == JsonValue::Type::Number)
                m.diffuseWeight = static_cast<float>(v->number);
        colorArr(entry.second.find("diffuseColor"), m.diffuseColor);
        if (JsonValue const* v = entry.second.find("specular"))
            if (v->type == JsonValue::Type::Number)
                m.specularWeight = static_cast<float>(v->number);
        colorArr(entry.second.find("specularColor"), m.specularColor);
        if (JsonValue const* v = entry.second.find("specularExponent"))
            if (v->type == JsonValue::Type::Number)
                m.specularExponent = static_cast<float>(v->number);
        if (JsonValue const* v = entry.second.find("transparency"))
            if (v->type == JsonValue::Type::Number)
                m.transparency = static_cast<float>(v->number);
        out[entry.first] = m;
    }
    return out;
}

// Locate a bufferView's byte span inside the imdl document's binary section.
// （原 ImdlGraphics.cpp 匿名 namespace 既有实现——U11(1) 起为 JSON 层共享，
// parseImdlEdges 与图形创建路径共用同一视图语义。）
// 守卫对齐参考 findBuffer（ParseImdlDocument.ts:1093-1100）：
//   - 视图名为空 → undefined（:1094 `0 === bufferViewId.length`——C++ 侧
//     参数已无类型区分，仅空名对应参考的 typeof/length 双检）；
//   - byteLength === 0 → undefined（:1099-1100）——0 长视图不是合法字节
//     区间（2026-09-27 U11(2) 补齐，评审 ②）。
inline bool findBufferView(JsonValue const& doc, std::string const& name,
                           std::vector<uint8_t> const& binary,
                           uint8_t const*& outData, size_t& outSize)
{
    if (name.empty())
        return false;  // :1094
    JsonValue const* views = doc.find("bufferViews");
    if (!views)
        return false;
    JsonValue const* view = views->find(name.c_str());
    if (!view)
        return false;
    JsonValue const* off = view->find("byteOffset");
    JsonValue const* len = view->find("byteLength");
    if (!off || !len)
        return false;
    size_t const offset = static_cast<size_t>(off->number);
    size_t const length = static_cast<size_t>(len->number);
    if (length == 0)
        return false;  // :1099-1100
    if (offset + length > binary.size())
        return false;
    outData = binary.data() + offset;
    outSize = length;
    return true;
}

// parseImdlEdges——imdl 边缘 JSON → 字节区间参数（Task 5/6 消费）。
// Ported from: ParseImdlDocument.ts:710-727 parseEdges（优先级与归零语义）：
//   - segments/silhouettes 直取（:714-715 → parseSegmentEdges :665-669 /
//     parseSilhouetteEdges :671-675——bufferView 缺失 → 该成员 undefined）；
//   - indexed 直取（:718 → parseIndexedEdges :677-693）；
//   - compact 兜底展开（:719-720 → parseCompactEdges :695-708 →
//     CompactEdges.ts indexedEdgeParamsFromCompactEdges——U11(3) 落地：
//     :720 `new VertexIndices(indices)` 的输入即 primitive 的 surface.indices
//     （:710 parseEdges 的 indices 形参），maxEdgeTableDimension = 选项
//     maxVertexTableSize（ImdlReader.ts:98 = renderSystem.maxTextureSize）；
//     展开产物字节由 ImdlIndexedEdgeParams 的 owned 向量持有）。
//   - 四形态全空 → undefined/nullopt（:722-723）；
//   - weight = displayParams.width、linePixels 直传（:730-731）。
inline std::optional<ImdlEdgeParams> parseImdlEdges(
    JsonValue const& doc, ImdlDocument const& imdl, ImdlPrimitiveProps const& props,
    uint32_t maxEdgeTableDimension = 2048)
{
    if (!props.edges)
        return std::nullopt;  // :711-712（imdl undefined）
    ImdlMeshEdgesProps const& edges = *props.edges;

    // findBuffer 视图语义（ParseImdlDocument.ts findBuffer——bufferView 名 →
    // 字节区间；DanQing 侧等价物 findBufferView）。
    auto resolve = [&doc, &imdl](ImdlByteView& out, std::string const& name) {
        uint8_t const* data = nullptr;
        size_t size = 0;
        if (!findBufferView(doc, name, imdl.binary, data, size))
            return false;
        out = ImdlByteView{data, size};
        return true;
    };

    ImdlEdgeParams out;
    out.weight = props.width;
    out.linePixels = props.linePixels;

    if (edges.segments) {  // parseSegmentEdges :665-669（双视图全需）
        ImdlSegmentEdgeParams s;
        if (resolve(s.indices, edges.segments->indicesView)
            && resolve(s.endPointAndQuadIndices, edges.segments->endPointAndQuadIndicesView))
            out.segments = std::move(s);
    }

    if (edges.silhouettes) {  // parseSilhouetteEdges :671-675（segments && normalPairs）
        ImdlSilhouetteParams s;
        if (resolve(s.indices, edges.silhouettes->indicesView)
            && resolve(s.endPointAndQuadIndices,
                       edges.silhouettes->endPointAndQuadIndicesView)
            && resolve(s.normalPairs, edges.silhouettes->normalPairsView))
            out.silhouettes = std::move(s);
    }

    if (edges.indexed) {  // parseIndexedEdges :677-693（双视图全需 + 尺寸字段原样）
        ImdlIndexedEdgeParams ix;
        if (resolve(ix.indices, edges.indexed->indicesView)
            && resolve(ix.edges.data, edges.indexed->edgeTableView)) {
            ix.edges.width = edges.indexed->width;
            ix.edges.height = edges.indexed->height;
            ix.edges.numSegments = edges.indexed->numSegments;
            ix.edges.silhouettePadding = edges.indexed->silhouettePadding;
            out.indexed = std::move(ix);
        }
    }

    if (!out.indexed && edges.compact) {  // :719-720（indexed 优先，compact 兜底）
        // parseCompactEdges（:695-708）：visibility 必需（:696-698），normalPairs
        // 可选（:700），顶点索引 = surface.indices（:720 的形参 indices）。
        ImdlByteView visibility;
        if (resolve(visibility, edges.compact->visibilityView)) {
            std::optional<ImdlByteView> normalPairs;
            {
                ImdlByteView np;
                if (edges.compact->normalPairsView
                    && resolve(np, *edges.compact->normalPairsView))
                    normalPairs = np;
            }
            ImdlByteView surfaceIndices;
            if (resolve(surfaceIndices, props.surface.indicesView)) {
                CompactEdgeParams compact;
                compact.numVisibleEdges = edges.compact->numVisible;  // :702
                compact.visibility = visibility;                      // :703
                compact.vertexIndices = VertexIndices(surfaceIndices);  // :704/:720
                compact.normalPairs = normalPairs;  // :705（Uint32Array 重解释 →
                                                    // 展开内按 LE u32 读）
                compact.maxEdgeTableDimension = maxEdgeTableDimension;  // :706
                out.indexed = indexedEdgeParamsFromCompactEdges(compact);
            }
        }
    }

    // TODO(Task 6+)：polylines 形态（:716 `imdl.polylines ?
    // parseTesselatedPolyline(imdl.polylines) : undefined` → :722 全空析取含
    // polylines）。参考 parseEdges 的全空检查是**四形态析取**
    // （:722 `!segments && !silhouettes && !indexed && !polylines`）——本层
    // 未解析 polylines（:711-716 的 parseTesselatedPolyline 三视图
    // indices/prevIndices/nextIndicesAndParams），故析取缺 polylines 极。
    // 注意：polylines 是 EdgeParams.polylines（TesselatedPolyline 视图，
    // EdgeParams.ts PolylineEdgeGroup 的 tesselate 输入是它而非本层视图），
    // 消费在 createEdgeParams（EdgeParams.ts:396-410 tesselatePolylineList），
    // 与 segments 直通不同——接线时按参考消费链评估，勿从 segments 旁路合成。

    if (!out.segments && !out.silhouettes && !out.indexed)
        return std::nullopt;  // :722-723（参考析取含 polylines——见上 TODO）
    return out;
}

}  // namespace tilejson

END_DQ_RENDER_NAMESPACE
