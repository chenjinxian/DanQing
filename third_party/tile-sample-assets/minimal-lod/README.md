# minimal-lod — 两层 LOD 瓦片集（TileTreeRender.LodTilesetRendersChildren 专用资产）

**Authored**（CLAUDE.md §5(g) + §11.11；新建专用资产，不突变 `../minimal/`——单层锁保持可复现）。

## 内容

| 文件 | 说明 |
|---|---|
| `generate_lod_tileset.py` | 一次性生成器（输出下列文件） |
| `tileset.json` | **root 无 content**（合法 3D Tiles LOD 中间层，box 全包，GE=100）+ 2 children |
| `child-red.b3dm` / `child-blue.b3dm` | 各含一个 1×1×1 box（红 @x=-1.5 / 蓝 @x=+1.5），GE=0.01（最细层） |

## 锁定的机制（失败即回归）

1. **无 content 中间层下钻**：root 不显示自己、选择必须穿透到 children（参考 `RealityTile.selectRealityTiles`，RealityTile.ts:340-390）。
2. **结构化 JSON 解析**：字段不得跨层泄漏（2026-09-21 曾因手写扫描器"第一个 content 归 root"使 root 冒领 child-red 的内容、蓝 child 永不加载）。
3. **SSE LOD 判定**：`SSE = geometricError / pixelSize`，`> 16` refine（RealityTile.ts:535-542 + TileDrawArgs.ts:279）。children GE=0.01 → 恒 ready；root 无 content → 恒下钻。

## 几何注意

子内容的世界偏移**烘焙进几何**（DanQing 尚未实现 3D Tiles 节点 transformToRoot；与单层资产同策略）。transformToRoot 支持落地后可改用节点 transform 表达。

## 再生成

```
python generate_lod_tileset.py
```
