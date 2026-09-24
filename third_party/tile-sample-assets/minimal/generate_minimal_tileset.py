#!/usr/bin/env python3
"""Generate the minimal 3D Tiles tileset used by TileTreeRenderTest.

Outputs: tileset.json, root.b3dm (GLB wrapped in b3dm), root.glb (diagnostic copy).

Authored: no reference test data exists in itwinjs-core for offline 3D Tiles
consumption (verified 2026-09-19: frontend unit tests use inline Uint8Array
fixtures; frontend-tiles has zero BENTLEY_BatchedTileSet fixtures on disk —
real tilesets come only from the mesh-export cloud service). This asset is
DanQing-authored per CLAUDE.md §5(g) (rendering regression authorization) and
§11.11 (asymmetrical markers pin orientation; new dedicated asset, never
mutate existing ones).

Design (mirrors the reference's own tile-content fixture idiom —
full-stack-tests/core/src/frontend/standalone/tile/data/TileIO.data.ts
"triangles": left=red middle=green right=blue colored primitives):

  root.b3dm   28-byte b3dm header + featureTableJSON padded to 8 + GLB.
              GLB holds three 1x1x1 boxes at world x = -1.5 / 0 / +1.5 with
              baseColorFactor red / green / blue (no textures — the tile
              content path colors via baseColorFactor).
  tileset.json  single root tile, content.uri = "root.b3dm",
              boundingVolume box around [-2.5..2.5, -0.5..0.5, -0.5..0.5],
              geometricError 100 (root always renders, never refines).

Layout contract this must satisfy (dqRender/src/tile/RealityTile.cpp
readContent): gltfOffset = 28 + ftJsonLen + ftBinLen + btJsonLen + btBinLen
with all-zero binary/batch-table lengths, i.e. GLB starts at byte 52.

Run:  python generate_minimal_tileset.py   (writes ./tileset.json + ./root.b3dm)
"""

import json
import struct
import zlib
from pathlib import Path

OUT_DIR = Path(__file__).resolve().parent

# --- geometry: unit cube, 24 verts / 36 indices (per-face normals) ----------
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

BOXES = [
    {"center": (-1.5, 0.0, 0.0), "color": [1.0, 0.0, 0.0, 1.0], "name": "red"},
    {"center": (0.0, 0.0, 0.0), "color": [0.0, 1.0, 0.0, 1.0], "name": "green"},
    {"center": (1.5, 0.0, 0.0), "color": [0.0, 0.0, 1.0, 1.0], "name": "blue"},
]


def build_box(center, scale=0.5):
    """Return (positions, normals, uvs, indices) for one box centered at `center`."""
    positions, normals, uvs, indices = [], [], [], []
    for quad, normal in zip(FACE_QUADS, FACE_NORMALS):
        base = len(positions)
        for corner in quad:
            positions.append(
                (center[0] + corner[0] * scale,
                 center[1] + corner[1] * scale,
                 center[2] + corner[2] * scale))
            normals.append(normal)
        uvs.extend([(0, 0), (1, 0), (1, 1), (0, 1)])
        indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])
    return positions, normals, uvs, indices


def make_png(rgb):
    """Minimal 1x1 RGB PNG (no external deps)."""
    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)  # 1x1, 8bit, truecolor
    idat = zlib.compress(b"\x00" + bytes(rgb))            # filter 0 + one pixel
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", idat) + chunk(b"IEND", b""))


def build_glb():
    """Pack the three colored boxes into a glTF 2.0 binary (GLB)."""
    bin_parts = []
    accessors, buffer_views, meshes, materials = [], [], [], []
    images, textures, samplers = [], [], [{"magFilter": 9728, "minFilter": 9728,
                                           "wrapS": 10497, "wrapT": 10497}]
    offset = 0

    def add_view(data_bytes):
        nonlocal offset
        while offset % 4:  # bufferView alignment
            bin_parts.append(b"\x00")
            offset += 1
        view_index = len(buffer_views)
        buffer_views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(data_bytes)})
        bin_parts.append(data_bytes)
        offset += len(data_bytes)
        return view_index

    for box in BOXES:
        positions, normals, uvs, indices = build_box(box["center"])

        # 1x1 solid-color baseColor texture (pure (255,0,0)/(0,255,0)/(0,0,255)).
        rgb = tuple(int(c * 255) for c in box["color"][:3])
        img_view = add_view(make_png(rgb))
        images.append({"bufferView": img_view, "mimeType": "image/png"})
        textures.append({"source": len(images) - 1, "sampler": 0})

        mat_index = len(materials)
        materials.append({"pbrMetallicRoughness": {
            "baseColorFactor": [1.0, 1.0, 1.0, 1.0],   # texture supplies the color
            "baseColorTexture": {"index": len(textures) - 1},
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
                          "min": [min(p[0] for p in positions), min(p[1] for p in positions), min(p[2] for p in positions)],
                          "max": [max(p[0] for p in positions), max(p[1] for p in positions), max(p[2] for p in positions)]})
        accessors.append({"bufferView": add_view(nrm_bytes), "componentType": 5126,
                          "count": len(normals), "type": "VEC3"})
        accessors.append({"bufferView": add_view(uv_bytes), "componentType": 5126,
                          "count": len(uvs), "type": "VEC2"})

        meshes.append({"name": box["name"], "primitives": [{
            "attributes": {"POSITION": len(accessors) - 3, "NORMAL": len(accessors) - 2,
                           "TEXCOORD_0": len(accessors) - 1},
            "indices": len(accessors) - 4, "mode": 4, "material": mat_index}]})

    while offset % 4:
        bin_parts.append(b"\x00")
        offset += 1
    bin_data = b"".join(bin_parts)

    gltf = {
        "asset": {"version": "2.0", "generator": "danqing-minimal-tileset"},
        "scene": 0,
        "scenes": [{"nodes": list(range(len(meshes)))}],
        "nodes": [{"mesh": i, "name": b["name"]} for i, b in enumerate(BOXES)],
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
        json_bytes += b" "  # JSON chunk padded with spaces

    json_header = struct.pack("<I", len(json_bytes)) + b"JSON"
    bin_header = struct.pack("<I", len(bin_data)) + b"BIN\x00"
    total = 12 + len(json_header) + len(json_bytes) + len(bin_header) + len(bin_data)
    glb = struct.pack("<III", 0x46546C67, 2, total) + json_header + json_bytes + bin_header + bin_data
    return glb


def build_b3dm(glb):
    """Wrap the GLB in a b3dm container (3D Tiles spec v1 header layout).

    Layout must match dqRender RealityTile::readContent:
    gltfOffset = 28 + ftJson + ftBin + btJson + btBin (all binary/batch = 0).
    """
    ft_json = b'{"BATCH_LENGTH":0}'
    while len(ft_json) % 8:
        ft_json += b" "
    body = ft_json + glb
    header = struct.pack(
        "<IIIIIII",
        0x6D643362,   # magic "b3dm"
        1,            # version
        28 + len(body),
        len(ft_json),  # featureTableJsonLength
        0, 0, 0,      # featureTableBinary / batchTableJson / batchTableBinary
    )
    return header + body


def main():
    glb = build_glb()
    (OUT_DIR / "root.b3dm").write_bytes(build_b3dm(glb))
    # Standalone copy of the embedded GLB — handy for bisecting the GltfDecoration
    # chain against the tile chain with the same bytes (diagnostics only).
    (OUT_DIR / "root.glb").write_bytes(glb)

    tileset = {
        "asset": {"version": "1.1"},
        "geometricError": 200,
        "root": {
            "boundingVolume": {"box": [0, 0, 0, 2.5, 0, 0, 0, 0.5, 0, 0, 0, 0.5]},
            "geometricError": 0.01,  # finest level: SSE = GE/pixelSize ≪ 16 → always ready (公式修正 2026-09-21：RealityTile.ts:535-542)
            "refine": "REPLACE",
            "content": {"uri": "root.b3dm"},
        },
    }
    (OUT_DIR / "tileset.json").write_text(json.dumps(tileset, indent=2), encoding="utf-8")
    print(f"root.b3dm: {(OUT_DIR / 'root.b3dm').stat().st_size} bytes, "
          f"tileset.json: {(OUT_DIR / 'tileset.json').stat().st_size} bytes")


if __name__ == "__main__":
    main()
