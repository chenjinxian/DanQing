#!/usr/bin/env python3
"""Enumerate the next spatial frontier layer of the bridge tile tree from the
captured dump contents.

Reference semantics (itwinjs-core@88da8fb9):
- computeChildTileProps (core/common/src/tile/TileMetadata.ts:777-853)
  * magnification: content decode yields sizeMultiplier -> one same-range child
    with multiplier*2 (we only need the m2 key here);
  * spatial: 8 children, skip emptySubRangeMask bits (1<<(i+j*2+k*4)) and child
    ranges not intersecting tree.contentRange when the parent is not wholly
    inside it;
  * child ranges: bisectTileRange3d/2d shrink ONE endpoint of the range's
    CURRENT longest axis per call (three successive calls for 3d).
- decodeTileContentDescription (:880-940) leaf/magnification heuristics.
"""
import json, os, struct, sys

DUMP = r"D:\Github\DanQing\third_party\tile-sample-assets\rpc-dumps\bridge-edit-sweep-v1"
TARGET_DEPTH = int(sys.argv[1]) if len(sys.argv) > 1 else 12
OUT = sys.argv[2] if len(sys.argv) > 2 else r"D:\Github\DanQing\build\frontier-keys.txt"

man = json.load(open(os.path.join(DUMP, "manifest.json"), encoding="utf-8"))
tree = man["trees"][0]
tree_id = tree["treeId"]
props = json.load(open(os.path.join(DUMP, tree["propsFile"]), encoding="utf-8"))
root_range = props["rootTile"]["range"]
content_range = props.get("contentRange")

# 合并根（FRONTIER_ROOTS 分号分隔）——后根覆盖同键；headers 从各自根读。
ROOTS = [r.strip() for r in (os.environ.get("FRONTIER_ROOTS") or DUMP).split(";") if r.strip()]
by_key = {}
for _root in ROOTS:
    _m = json.load(open(os.path.join(_root, "manifest.json"), encoding="utf-8"))
    for _t in _m["tiles"]:
        by_key[_t["contentId"]] = (_root, _t)


def spec(cid):
    p = cid.split("-")
    return int(p[2], 16), int(p[3], 16), int(p[4], 16), int(p[5], 16), int(p[6], 16)


def range_of(cid):
    d, i, j, k, _ = spec(cid)
    lo = list(root_range["low"])
    hi = list(root_range["high"])
    for lv in range(1, d + 1):
        shift = d - lv
        for bit in ((i >> shift) & 1, (j >> shift) & 1, (k >> shift) & 1):
            takeUpper = (bit == 0)   # i/j/k == 0 keeps the lower half
            dx, dy, dz = hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]
            if dx > dy and dx > dz:
                if takeUpper: hi[0] = 0.5 * (lo[0] + hi[0])
                else: lo[0] = 0.5 * (lo[0] + hi[0])
            elif dy > dz:
                if takeUpper: hi[1] = 0.5 * (lo[1] + hi[1])
                else: lo[1] = 0.5 * (lo[1] + hi[1])
            else:
                if takeUpper: hi[2] = 0.5 * (lo[2] + hi[2])
                else: lo[2] = 0.5 * (lo[2] + hi[2])
    return lo, hi


def contains(outer, lo, hi):
    return (outer["low"][0] <= lo[0] and outer["low"][1] <= lo[1] and outer["low"][2] <= lo[2]
            and outer["high"][0] >= hi[0] and outer["high"][1] >= hi[1] and outer["high"][2] >= hi[2])


def intersects(a_lo, a_hi, b_lo, b_hi):
    return not (a_hi[0] < b_lo[0] or a_lo[0] > b_hi[0]
                or a_hi[1] < b_lo[1] or a_lo[1] > b_hi[1]
                or a_hi[2] < b_lo[2] or a_lo[2] > b_hi[2])


def decode_header(root, entry):
    b = open(os.path.join(root, entry["file"]), "rb").read(96)
    flags = struct.unpack_from("<I", b, 12)[0]
    tol = struct.unpack_from("<d", b, 64)[0]
    inc, exc = struct.unpack_from("<I", b, 72)[0], struct.unpack_from("<I", b, 76)[0]
    mask = struct.unpack_from("<I", b, 84)[0]
    return flags, tol, inc, exc, mask


def derived(cid, size_multiplier):
    """Return ('leaf', None) | ('mag', child_cid) | ('spatial', [child_cids])."""
    d, i, j, k, m = spec(cid)
    root, entry = by_key[cid]
    flags, tol, inc, exc, mask = decode_header(root, entry)
    complete = (flags & 4) == 0
    if complete and inc == 0 and exc == 0:
        return "leaf", None
    if size_multiplier:
        # Magnification branch (parent carries a multiplier from props/content).
        parts = cid.split("-")
        parts[-1] = format(int(parts[-1], 16) * 2, "x")
        return "mag", "-".join(parts)
    can_skip = (flags & 8) == 0 and tol <= 1.0
    if can_skip:
        if complete and exc == 0 and inc <= 100:
            if flags & 1:
                parts = cid.split("-")
                parts[-1] = format(int(parts[-1], 16) * 2, "x")
                return "mag", "-".join(parts)
            return "leaf", None
        elif inc + exc <= 100:
            parts = cid.split("-")
            parts[-1] = format(int(parts[-1], 16) * 2, "x")
            return "mag", "-".join(parts)
    lo, hi = range_of(cid)
    test_range = content_range is not None and not contains(content_range, lo, hi)
    kids = []
    for ci in range(2):
        for cj in range(2):
            for ck in range(2):
                if mask & (1 << (ci + cj * 2 + ck * 4)):
                    continue
                clo, chi = list(lo), list(hi)
                # Three bisect calls exactly (i then j then k), each choosing
                # the longest axis at call time.
                for bit in (ci, cj, ck):
                    dx, dy, dz = chi[0] - clo[0], chi[1] - clo[1], chi[2] - clo[2]
                    if dx > dy and dx > dz:
                        if bit == 0: chi[0] = 0.5 * (clo[0] + chi[0])
                        else: clo[0] = 0.5 * (clo[0] + chi[0])
                    elif dy > dz:
                        if bit == 0: chi[1] = 0.5 * (clo[1] + chi[1])
                        else: clo[1] = 0.5 * (clo[1] + chi[1])
                    else:
                        if bit == 0: chi[2] = 0.5 * (clo[2] + chi[2])
                        else: clo[2] = 0.5 * (clo[2] + chi[2])
                if test_range and not intersects(clo, chi, content_range["low"], content_range["high"]):
                    continue
                kids.append("-".join([cid.split("-")[0], cid.split("-")[1],
                                      format(d + 1, "x"), format(i * 2 + ci, "x"),
                                      format(j * 2 + cj, "x"), format(k * 2 + ck, "x"),
                                      cid.split("-")[6]]))
    return "spatial", kids


parents = [cid for cid, (root, t) in by_key.items()
           if spec(cid)[0] == TARGET_DEPTH - 1 and spec(cid)[4] == 1]
print(f"parents at depth {TARGET_DEPTH-1} (m1): {len(parents)}")
known = set(by_key)
missing, mag_children = set(), set()
for cid in parents:
    kind, kids = derived(cid, None)
    if kind == "mag":
        if kids not in known:
            mag_children.add(kids)
    elif kind == "spatial":
        for kid in kids:
            if kid not in known:
                missing.add(kid)
print(f"missing depth-{TARGET_DEPTH} spatial keys: {len(missing)}")
print(f"m2 magnification children not captured: {len(mag_children)}")
lines = [f"{tree_id}/{k}" for k in sorted(missing | mag_children)]
open(OUT, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + ("\n" if lines else ""))
print("wrote", OUT, len(lines))
