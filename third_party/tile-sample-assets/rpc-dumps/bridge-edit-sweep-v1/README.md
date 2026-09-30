# bridge-edit-sweep-v1 — 编辑大桥测试.bim 全树 BFS sweep（M-M(5)）

> 采集指令背景：用户解除采集存储限制（"获取模型离线 imdl 数据时可以不限制存储"，
> 2026-09-30）——M-K(1) 时代的 ≤2GB 预算硬门（sweep 跳过、仅 imodel+drill）按此
> 解除，本 dump 为全树无限制 sweep 首例。

## Provenance

| 项 | 值 |
|---|---|
| 种子 | 编辑大桥测试.bim（ASCII 目录名映射，坑 26 四层编码链） |
| 采集 | 2026-09-30，danqing-rpc-tools capture.mjs `--sweep` |
| caps | tiles=400000 / depth=12 / bytes=200000MB / mag=×8（预算随 provenance.sweep.caps 落盘） |
| 终态 | **done:budget-capped**（400k 瓦 cap 触顶；errs=0） |
| 规模 | **1 树 380,738 瓦 / 4,955.79 MB**（+dup 19,262 跳过） |
| itwinjs | 88da8fb98647d37332a5cef4d6bb6e2e5c007855（7e57d018 后代，光照/材质/imdl 路径 diff 为空） |
| iModel | dbd4826b-4f723c-b4fa-ff8205f2bae7；projectExtents low[-10.88,-16.03,-10.10] high[1714.21,313.50,110.81] |

## 为什么要这个 dump

M-K(1) 时 d2 瓦 34/141MB 实证超 ≤2GB 硬门 → sweep 跳过，浏览深度界 = drill
30 瓦（桥跨两端 d2→d6）。本 dump 把**浏览深度界从 drill 域扩到全树域**——
未采位置/深度不再依赖父瓦 LOD 兜底（hasMissingTiles 语义）。

## 大瓦本地持有（GitHub 100MB 硬门先例）

| 瓦 | 文件 | 大小 | 处置 |
|---|---|---|---|
| `-b-2-0-0-0-1` | `files/12.imdl` | 134.2MB | **git 不入库本地持有**（.gitignore；与 bridge-edit-v1/-drill-v1 的 `files/8.imdl` 同一字节——跨会话确定性）。manifest sha256 不变；回放缺瓦走父瓦兜底（桥锁请求面为 d3 键不依赖本瓦）。 |

## 接入

`main.cpp dumpPackageForModel("bridge-edit")` 的 fallback 根第三位（
`bridge-edit-v1` → `bridge-edit-drill-v1` → `bridge-edit-sweep-v1`）。
树 props 仅取自主根（同 treeId 语义单源）。

## 采集复现

```bash
cd /d/Github/danqing-rpc-tools
node capture.mjs --out dumps/bridge-edit-sweep-v1 \
  --seed "D:/Github/itwinjs-core/test-apps/display-test-app/assets/编辑大桥测试.bim" \
  --sweep --sweep-max-tiles 400000 --sweep-max-depth 12 \
  --sweep-max-bytes-mb 200000 --wait 20000
```

（大模型后端生成 >15s/瓦——总时长 ~1.5h；tile cache 加速重采。）
