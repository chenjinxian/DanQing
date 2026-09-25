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
};

struct ImdlSurfaceProps {
    std::string indicesView;
    uint32_t type = 0;
};

struct ImdlPrimitiveProps {
    ImdlVertexTableProps vertices;
    ImdlSurfaceProps surface;
    bool isPlanar = false;  // ImdlSchema.ts:177 mesh primitive isPlanar → 参考侧
                            // 决定 OpaquePlanar pass 归属（Task 5 LUT 路径）
};

inline std::vector<ImdlPrimitiveProps> parseImdlMeshPrimitives(JsonValue const& doc)
{
    std::vector<ImdlPrimitiveProps> out;
    JsonValue const* meshes = doc.find("meshes");
    if (!meshes)
        return out;
    for (auto const& meshEntry : meshes->obj) {
        JsonValue const* primitives = meshEntry.second.find("primitives");
        if (!primitives)
            continue;
        for (auto const& prim : primitives->arr) {
            ImdlPrimitiveProps props;
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
            }
            if (JsonValue const* surf = prim.find("surface")) {
                if (JsonValue const* ind = surf->find("indices"))
                    props.surface.indicesView = ind->str;
                if (JsonValue const* t = surf->find("type"))
                    props.surface.type = static_cast<uint32_t>(t->number);
            }
            if (JsonValue const* pl = prim.find("isPlanar"))
                props.isPlanar = pl->boolean;
            out.push_back(std::move(props));
        }
    }
    return out;
}

}  // namespace tilejson

END_DQ_RENDER_NAMESPACE
