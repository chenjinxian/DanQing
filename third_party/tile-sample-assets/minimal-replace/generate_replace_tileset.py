#!/usr/bin/env python3
"""Generate the REPLACE-refinement 3D Tiles tileset used by
TileTreeRender.ReplaceRefinementSwapsParentForChildren.

Outputs: tileset.json, parent-green.b3dm, child-red.b3dm, child-blue.b3dm.

Authored: no reference test data exists in itwinjs-core for offline 3D Tiles
consumption (see ../minimal/README.md). CLAUDE.md §5(g) + §11.11 — new
dedicated asset (minimal/ = single-level lock, minimal-lod/ = descend lock).

Structure (REPLACE refinement — parent content is substituted by children):

  root          HAS content: a large GREEN box (4.4 x 1 x 1) covering the
                whole extent, geometricError 0.1 — coarse stand-in layer.
  ├─ child0     red 1x1x1 box @ x=-1.5, GE 0.01 (fine)
  └─ child1     blue 1x1x1 box @ x=+1.5, GE 0.01 (fine)

SSE math (SSE = GE / pixelSize, refine when > 16):
  far view  (frustum height 10 → pixelSize ≈ 10/700):  root SSE ≈ 7 ≤ 16 →
            root displays (green), children never requested.
  near view (frustum height 2.2 → pixelSize ≈ 0.0031): root SSE ≈ 32 > 16 →
            refine: children display (red+blue), parent hidden (REPLACE).

Layout contract (dqRender/src/tile/RealityTile.cpp readContent): each b3dm is
28-byte header + featureTableJSON padded to 8 + GLB (GLB at byte 52).

Run:  python generate_replace_tileset.py
"""

import json
import struct
import zlib
from pathlib import Path

OUT_DIR = Path(__file__).resolve().parent

FACE_NORMALS = [
    (0, 0, 1), (0, 0, -1), (1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0),
]
FACE_QUADS = [
    ((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)),      # +Z
    ((-1, -1, -1), (-1, 1, -1), (1, 1, -1), (1, -1, -1)),  # -Z
    ((1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)),      # +X
    ((-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)),  # -X
    ((-1, 1, -1), (1, 1, -1), (1, 1, 1), (-1, 1, 1)),      # +Y
    ((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)),  # -Y
]


def build_box(center, half):
    positions, normals, uvs, indices = [], [], [], []
    for quad, normal in zip(FACE_QUADS, FACE_NORMALS):
        base = len(positions)
        for corner in quad:
            positions.append(
                (center[0] + corner[0] * half[0],
                 center[1] + corner[1] * half[1],
                 center[2] + corner[2] * half[2]))
            normals.append(normal)
        uvs.extend([(0, 0), (1, 0), (1, 1), (0, 1)])
        indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])
    return positions, normals, uvs, indices


def make_png(rgb):
    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)
    idat = zlib.compress(b"\x00" + bytes(rgb))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", idat) + chunk(b"IEND", b""))


def build_glb(rgb, center, half):
    bin_parts = []
    accessors, buffer_views, meshes, materials = [], [], [], []
    images, textures, samplers = [], [], [{"magFilter": 9728, "minFilter": 9728,
                                           "wrapS": 10497, "wrapT": 10497}]
    offset = 0

    def add_view(data_bytes):
        nonlocal offset
        while offset % 4:
            bin_parts.append(b"\x00")
            offset += 1
        view_index = len(buffer_views)
        buffer_views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(data_bytes)})
        bin_parts.append(data_bytes)
        offset += len(data_bytes)
        return view_index

    # World offset baked into geometry (no transformToRoot support yet).
    positions, normals, uvs, indices = build_box(center, half)
    img_view = add_view(make_png(rgb))
    images.append({"bufferView": img_view, "mimeType": "image/png"})
    textures.append({"source": 0, "sampler": 0})
    materials.append({"pbrMetallicRoughness": {
        "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
        "baseColorTexture": {"index": 0},
        "metallicFactor": 0.0, "roughnessFactor": 0.9}})

    idx_bytes = struct.pack(f"<{len(indices)}H", *indices)
    pos_bytes = struct.pack(f"<{len(positions) * 3}f", *[c for p in positions for c in p])
    nrm_bytes = struct.pack(f"<{len(normals) * 3}f", *[c for n in normals for c in n])
    uv_bytes = struct.pack(f"<{len(uvs) * 2}f", *[c for uv in uvs for c in uv])

    accessors.append({"bufferView": add_view(idx_bytes), "componentType": 5123,
                      "count": len(indices), "type": "SCALAR",
                      "min": [min(indices)], "max": [max(indices)]})
    accessors.append({"bufferView": add_view(pos_bytes), "componentType": 5126,
                      "count": len(positions), "type": "VEC3",
                      "min": [center[0] - half[0], center[1] - half[1], center[2] - half[2]],
                      "max": [center[0] + half[0], center[1] + half[1], center[2] + half[2]]})
    accessors.append({"bufferView": add_view(nrm_bytes), "componentType": 5126,
                      "count": len(normals), "type": "VEC3"})
    accessors.append({"bufferView": add_view(uv_bytes), "componentType": 5126,
                      "count": len(uvs), "type": "VEC2"})
    meshes.append({"name": "box", "primitives": [{
        "attributes": {"POSITION": 1, "NORMAL": 2, "TEXCOORD_0": 3},
        "indices": 0, "mode": 4, "material": 0}]})

    while offset % 4:
        bin_parts.append(b"\x00")
        offset += 1
    bin_data = b"".join(bin_parts)

    gltf = {
        "asset": {"version": "2.0", "generator": "danqing-minimal-replace-tileset"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "box"}],
        "meshes": meshes,
        "materials": materials,
        "textures": textures,
        "images": images,
        "samplers": samplers,
        "accessors": accessors,
        "bufferViews": buffer_views,
        "buffers": [{"byteLength": len(bin_data)}],
    }

    json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    while len(json_bytes) % 4:
        json_bytes += b" "
    json_header = struct.pack("<I", len(json_bytes)) + b"JSON"
    bin_header = struct.pack("<I", len(bin_data)) + b"BIN\x00"
    total = 12 + len(json_header) + len(json_bytes) + len(bin_header) + len(bin_data)
    return struct.pack("<III", 0x46546C67, 2, total) + json_header + json_bytes + bin_header + bin_data


def build_b3dm(glb):
    ft_json = b'{"BATCH_LENGTH":0}'
    while len(ft_json) % 8:
        ft_json += b" "
    body = ft_json + glb
    return struct.pack("<IIIIIII", 0x6D643362, 1, 28 + len(body),
                       len(ft_json), 0, 0, 0) + body


def main():
    # Parent content: large green slab covering the whole extent.
    (OUT_DIR / "parent-green.b3dm").write_bytes(
        build_b3dm(build_glb((0, 255, 0), (0.0, 0.0, 0.0), (2.2, 0.5, 0.5))))
    # Fine children.
    (OUT_DIR / "child-red.b3dm").write_bytes(
        build_b3dm(build_glb((255, 0, 0), (-1.5, 0.0, 0.0), (0.5, 0.5, 0.5))))
    (OUT_DIR / "child-blue.b3dm").write_bytes(
        build_b3dm(build_glb((0, 0, 255), (1.5, 0.0, 0.0), (0.5, 0.5, 0.5))))

    tileset = {
        "asset": {"version": "1.1"},
        "geometricError": 200,
        "root": {
            "boundingVolume": {"box": [0, 0, 0, 2.5, 0, 0, 0, 0.5, 0, 0, 0, 0.5]},
            "geometricError": 0.1,
            "refine": "REPLACE",
            "content": {"uri": "parent-green.b3dm"},
            "children": [
                {
                    "boundingVolume": {"box": [-1.5, 0, 0, 0.5, 0, 0, 0, 0.5, 0, 0, 0, 0.5]},
                    "geometricError": 0.01,
                    "content": {"uri": "child-red.b3dm"},
                },
                {
                    "boundingVolume": {"box": [1.5, 0, 0, 0.5, 0, 0, 0, 0.5, 0, 0, 0, 0.5]},
                    "geometricError": 0.01,
                    "content": {"uri": "child-blue.b3dm"},
                },
            ],
        },
    }
    (OUT_DIR / "tileset.json").write_text(json.dumps(tileset, indent=2), encoding="utf-8")
    print("parent-green.b3dm / child-red.b3dm / child-blue.b3dm / tileset.json written")


if __name__ == "__main__":
    main()
