#!/usr/bin/env python3
"""Robust d13 frontier capture driver: 50k-key chunks with per-chunk retries.

Reuses tiles already present in bridge-edit-frontier-d13-p*/ dirs; for each
chunk it requests only the still-missing keys. A chunk that loses its iModel
connection mid-run is retried (new dir) with only the remaining keys.
"""
import json, os, subprocess, sys, time

RPC = r"D:\Github\DanQing\third_party\tile-sample-assets\rpc-dumps"
TOOLS = r"D:\Github\danqing-rpc-tools"
SEED = r"D:/Github/itwinjs-core/test-apps/display-test-app/assets/编辑大桥测试.bim"
KEYS = r"D:\Github\DanQing\build\frontier-d13-keys.txt"
LOG = r"D:\Github\DanQing\build\frontier-driver.log"
CHUNK = 50000
MAX_RETRY = 4


def log(*a):
    msg = " ".join(str(x) for x in a)
    print(msg, flush=True)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(time.strftime("[%H:%M:%S] ") + msg + "\n")


def collected_from(prefix):
    got = set()
    for name in os.listdir(RPC):
        if not (name.startswith(prefix) and os.path.isdir(os.path.join(RPC, name))):
            continue
        mp = os.path.join(RPC, name, "manifest.json")
        if not os.path.exists(mp):
            continue
        try:
            m = json.load(open(mp, encoding="utf-8"))
        except Exception:
            continue
        for t in m.get("tiles", []):
            got.add(t["treeId"] + "/" + t["contentId"])
    return got


keys = [line.strip() for line in open(KEYS, encoding="utf-8") if line.strip()]
log("total keys", len(keys))
got = collected_from("bridge-edit-frontier-d13-p")
got |= collected_from("bridge-edit-frontier-d13-c")
log("already collected", len(got))

chunks = [keys[i:i + CHUNK] for i in range(0, len(keys), CHUNK)]
log("chunks", len(chunks))

for ci, chunk in enumerate(chunks):
    missing = [k for k in chunk if k not in got]
    if not missing:
        log("chunk", ci, "complete")
        continue
    for attempt in range(MAX_RETRY + 1):
        missing = [k for k in chunk if k not in got]
        if not missing:
            break
        tag = "c%02d" % ci + ("" if attempt == 0 else "r%d" % attempt)
        out = os.path.join(RPC, "bridge-edit-frontier-d13-" + tag)
        kf = r"D:\Github\DanQing\build\frontier-chunk-%s.txt" % tag
        open(kf, "w", encoding="utf-8", newline="\n").write("\n".join(missing) + "\n")
        cmd = ["node", "capture.mjs", "--seed", SEED, "--out", out,
               "--sweep", "--sweep-keys", kf,
               "--sweep-max-tiles", "1500000", "--sweep-max-bytes-mb", "200000",
               "--sweep-max-depth", "16", "--sweep-max-mag", "8", "--wait", "1200"]
        log("chunk", ci, "attempt", attempt, "missing", len(missing), "->", tag)
        t0 = time.time()
        try:
            r = subprocess.run(cmd, cwd=TOOLS, timeout=1800,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            rc = r.returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
        got |= collected_from("bridge-edit-frontier-d13-" + tag)
        log("chunk", ci, "attempt", attempt, "rc", rc, "elapsed", int(time.time() - t0),
            "collected", len(got))

log("DONE collected", len(got), "of", len(keys))
