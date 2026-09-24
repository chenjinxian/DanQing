# minimal — 离线 3D Tiles 最小瓦片集（TileTreeRenderTest 专用资产）

**Authored**（CLAUDE.md §5(g) 渲染回归授权 + §11.11 不对称标记纪律）：
itwinjs-core 中不存在离线 3D Tiles 消费测试数据（2026-09-19 核实：frontend 单测
用内联 `Uint8Array` 夹具；frontend-tiles 包零 `BENTLEY_BatchedTileSet` 磁盘夹具，
真实 tileset 仅来自 mesh-export 云服务）。本资产为 DanQing 自建，新建专用资产、
不从既有资产突变（§11.11）。

## 内容

| 文件 | 说明 |
|---|---|
| `generate_minimal_tileset.py` | 一次性生成器（重跑可复现，输出下列三个文件） |
| `tileset.json` | 单 root tile：`content.uri = "root.b3dm"`，box 包围体，`geometricError = 100`（root 恒 ready、不 refine） |
| `root.b3dm` | 28B b3dm 头 + featureTableJSON（补空格到 8 对齐）+ GLB；GLB 内含三个 1×1×1 box：**红 @x=-1.5 / 绿 @x=0 / 蓝 @x=+1.5** |
| `root.glb` | b3dm 内嵌 GLB 的独立副本（诊断用：同一字节走 GltfDecoration 链对比 tile 链） |

## 设计要点

- **不对称色块钉 X 轴朝向**（§11.11 位置断言：红在画面左半、蓝在右半——交换即坐标系/变换缺陷暴露）。彩色图元排布沿参考 tile 夹具自身惯用法（`full-stack-tests/.../tile/data/TileIO.data.ts` "triangles"：left=red middle=green right=blue）。
- **每 box 一张 1×1 纯色 PNG baseColorTexture + 每面 UV**（2026-09-21 第 2 版）：第 1 版用纯 `baseColorFactor` 无 UV/纹理，暴露渲染器无纹理纯色路径缺陷（GltfDecoration 链渲染同 GLB 得全黑矩形——两链 `createGraphicFromPolyface` 的 defaultColor-only 路径均未点亮，登记 TD-17）；纹理路径是 GltfTexturePixelTest 已验证的主路径，也是参考生态 b3dm 内容的主流形态（Khronos BoxTextured）。
- material 的 `baseColorFactor` 置白（纹理供色）。

## 布局契约（与 `dqRender/src/tile/RealityTile.cpp` readContent 对齐）

```
gltfOffset = 28 + ftJsonLen + ftBinLen + btJsonLen + btBinLen = 28 + 24 + 0 + 0 + 0 = 52
```

## 再生成

```
python generate_minimal_tileset.py
```
