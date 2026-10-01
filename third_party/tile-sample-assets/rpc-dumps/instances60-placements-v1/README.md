# instances60-placements-v1 — 元素 placement 数据面（M-N(2)）

> 用途：ZoomToSelectedElements 的离线回放源——`placements.json` 携带
> getPlacements 的 ECSQL 全集（DanQing 仓 §8.2 零网络，数据经打开链
> 文件回放进入）。

## Provenance

| 项 | 值 |
|---|---|
| 种子 | Properties_60InstancesWithUrl2.ibim |
| 采集 | 2026-10-01，danqing-rpc-tools capture.mjs `--placements`（placements.js 注入） |
| ECSQL | IModelConnection.ts:1293-1300 的 getPlacements 3d 分支原文（bis.GeometricElement3d，Origin/BBox 非 NULL） |
| 规模 | **62 placements**（60 球 + 2 伴随元素）/ 15,286 B |
| iModelId | dfac2750-4c3e-4c1-...（与 instances60-imodel-v1 同源——imodel.json 各自携带） |
| itwinjs | 88da8fb98647d37d32a5cef4d6bb6e2e5c007855 |

## placements.json 形态

```jsonc
{
  "iModelId": "...",
  "count": 62,
  "placements": [
    {
      "id": "0x38",                       // ECInstanceId（与特征表 elementId 同键）
      "origin": [-2.2701, 1.9146, 0],     // GeometricElement3d.Origin
      "bboxLow": [-0.531, -0.531, -0.531],// BBoxLow（elementAligned——世界域 = origin + YPR×box）
      "bboxHigh": [0.531, 0.531, 0.531],
      "angles": [0, 0, 90]                // Yaw/Pitch/Roll（度）
    }, ...
  ]
}
```

## 采集复现

```bash
cd /d/Github/danqing-rpc-tools
node capture.mjs --out dumps/instances60-placements-v1 \
  --seed "D:/Github/itwinjs-core/full-stack-tests/presentation/assets/datasets/Properties_60InstancesWithUrl2.ibim" \
  --placements --wait 60
```

## 已知限制

- 3d 分支全集（UNION 2d 分支未采——dump 域内模型均 3d，无影响）。
- Yaw/Pitch/Roll 的 NULL 行被 WHERE 过滤（与参考 getPlacements 同语义）。
- 采样含 2 个非球元素（0x38/0x39 族的第一行即球——全 62 行均可回放）。
