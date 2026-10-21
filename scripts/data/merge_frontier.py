#!/usr/bin/env python3
"""Merge captured frontier chunk dumps into one compact local-only root.

- Hardlinks each chunk tile into <out>/files/<n>.imdl (no extra disk; NTFS).
- Writes a compact manifest: tiles entries carry only
  {treeId, contentId, byteLength, file} (the DumpTileTreeProps parser
  requires treeId/contentId/file; byteLength feeds the multi-root
  availability gate). sha256/guid are dropped to keep the 800k-entry manifest
  near 100MB instead of ~320MB.
- The root is local-only (added to .gitignore); the app includes it
  conditionally when its manifest exists.

Env overrides: FRONTIER_OUT, FRONTIER_CHUNKS (";"-separated), FRONTIER_PROPS.
"""
import json, os, sys

RPC = "D:\\Github\\DanQing\\third_party\\tile-sample-assets\\rpc-dumps"
OUT = os.environ.get("FRONTIER_OUT") or (RPC + "\\bridge-edit-frontier-d13-v1")
_ch = os.environ.get("FRONTIER_CHUNKS")
CHUNKS = ([c.strip() for c in _ch.split(";") if c.strip()] if _ch else
          [RPC + "\\bridge-edit-frontier-d13-p%d" % i for i in range(1, 10)])
SRC_PROPS = os.environ.get("FRONTIER_PROPS") or (RPC + "\\bridge-edit-sweep-v1")

os.makedirs(os.path.join(OUT, "files"), exist_ok=True)
props = json.load(open(os.path.join(SRC_PROPS, "manifest.json"), encoding="utf-8"))
tree_entry = props["trees"][0]

seen = {}
n = 0
bytes_total = 0
for chunk in CHUNKS:
    mp = os.path.join(chunk, "manifest.json")
    if not os.path.exists(mp):
        print("skip missing chunk", chunk)
        continue
    m = json.load(open(mp, encoding="utf-8"))
    print(chunk, "tiles", len(m["tiles"]))
    for t in m["tiles"]:
        cid = t["contentId"]
        if cid in seen:
            continue
        src = os.path.join(chunk, t["file"])
        if not os.path.exists(src) or os.path.getsize(src) != t["byteLength"]:
            print("MISSING/BAD", src, t["byteLength"])
            continue
        dst = os.path.join(OUT, "files", "%d.imdl" % n)
        if not os.path.exists(dst):
            try:
                os.link(src, dst)
            except OSError:
                import shutil
                shutil.copyfile(src, dst)
        seen[cid] = (t["treeId"], os.path.getsize(dst))
        bytes_total += os.path.getsize(dst)
        n += 1

print("merged tiles", n, "bytes", bytes_total)
with open(os.path.join(OUT, "manifest.json"), "w", encoding="utf-8", newline="\n") as f:
    f.write('{"provenance":{"tool":"danqing-rpc-tools capture.mjs + merge_frontier.py",')
    f.write('"note":"d13 spatial frontier + d12 m2 magnification (live backend, hardlinked chunks)",')
    f.write('"chunks":%d},' % len([c for c in CHUNKS if os.path.exists(os.path.join(c, "manifest.json"))]))
    f.write('"stats":{"trees":1,"tiles":%d,"bytes":%d},' % (n, bytes_total))
    f.write('"trees":[%s],"tiles":[' % json.dumps(tree_entry, ensure_ascii=False, separators=(",", ":")))
    first = True
    for i, (cid, (tree_id, size)) in enumerate(seen.items()):
        if not first:
            f.write(",")
        first = False
        f.write('{"treeId":%s,"contentId":%s,"byteLength":%d,"file":"files/%d.imdl"}'
                % (json.dumps(tree_id), json.dumps(cid), size, i))
    f.write("]}")
print("wrote", os.path.join(OUT, "manifest.json"))
