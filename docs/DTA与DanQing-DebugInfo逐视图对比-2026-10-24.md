# DTA 与 DanQing Debug Info 逐视图对比（2026-10-24）

> 模型：`编辑大桥测试.bim`（1.5km 桥——双方同一种子）。协议：8 标准旋转
> （Top/Bottom/Left/Right/Front/Back/IsoLeft/IsoRight——`StandardRotations.ts`
> 的 `View.Standard` idx 0-7 同序）× 同一缩放档（默认视图起 ×2⁴ 泵，2000×1400
> 窗），逐视图瓦静默后读 DiagnosticsPanel 全字段。
>
> - DanQing 侧：`DebugWindowDiag.StandardViewDebugInfoProbe`（进程内，逐视图
>   reset 统计 = 视图增量）。
> - DTA 侧：`danqing-rpc-tools/debuginfo-capture.mjs`（vite+electron 直启 +
>   CDP 直构 DiagnosticsPanel；统计为会话累计——逐视图看增量）。

## 一、DTA 实测矩阵（会话累计；括号 = 视图增量）

| 视图 | Selected | Ready | Completed | Elided | Cache Misses | Dispatched | Decoding max | 图元/批/分支 | FPS |
|---|---|---|---|---|---|---|---|---|---|
| Top | 1 | 1 | 1 | 19 | 2 | 4 | 522ms | 23/4/7 | 56.4 |
| Bottom | 1 | 1 | 1 | 19 | 2 | 4 | 522 | 23/4/7 | 56.5 |
| Left | 2 | 3 | 3 | 26 | 4 | 8 | 522 | 32/7/7 | 56.4 |
| Right | 2 | 3 | 3 | 26 | 4 | 8 | 522 | 32/7/7 | 56.6 |
| Front | 1 | 2 | 3 | 26 | 4 | 8 | 522 | 23/4/7 | 56.6 |
| Back | 1 | 2 | 3 | 26 | 4 | 8 | 522 | 23/4/7 | 56.5 |
| IsoLeft | 1 | 2 | 3 | 26 | 4 | 8 | 522 | 23/4/7 | 56.6 |
| IsoRight | 1 | 2 | 3 | 26 | 4 | 8 | 522 | 23/4/7 | 56.2 |

（DTA 无逐视图 reset 面——`resetStatistics()` 是 tileAdmin 级；累计差分即视图增量。）

## 二、DanQing 实测矩阵（逐视图 reset = 视图增量）

| 视图 | Selected | Ready | Completed | Elided | Decoding max | 图元/批/分支 | FPS |
|---|---|---|---|---|---|---|---|
| Top | 2 | 0 | 11 | 0 | 1345ms | 26/3/6 | 26.4（装载中）|
| Bottom | 2 | 0 | 0 | 0 | — | 26/3/6 | 62.5 |
| Left | 14 | 0 | 52 | 14 | 613ms | 64/11/14 | 22.9（装载中）|
| Right | 14 | 0 | 0 | 0 | — | 64/11/14 | 62.5 |
| Front | 7 | 0 | 6 | 0 | 80ms | 34/4/7 | 60.6 |
| Back | 7 | 0 | 0 | 0 | — | 34/4/7 | 62.5 |
| Iso | 7 | 0 | 2 | 0 | 15ms | 33/4/7 | 62.5 |
| RightIso | 7 | 0 | 0 | 0 | — | 33/4/7 | 62.5 |

## 三、逐字段一致性判定

**机制级（一致）**：
1. **全部 18 项统计 + 3 项渲染命令 + FPS 双侧活值**——无死字段（P-7a 修复后）。
2. **对侧视图对称**：双侧同构（DTA Left/Right=32/7/7 同、Top/Bottom=23/4/7 同；
   DanQing 同型对称）——同一批瓦反看的必然结构，双侧都呈现。
3. **Left 最重视角**：双侧一致（DTA 32 图元 > 23；DanQing 64 > 26/34）。
4. **Elided 机制活**：双侧非零且随视图变化（DTA 19→26；DanQing 0→14）——
   子代派生空子域计数链两侧同构。
5. **稳态 FPS**：双侧 56-62（DTA 56=vsync 60 捕获负载下；DanQing 62.5=无 vsync
   测试窗）——同一数量级。
6. **Ready:0 的语义**：DanQing 的 Ready=0 为 `!canSkipThisTile` 门忠实值
   （IModelTile.ts:322）；DTA Ready=1-3 因其树 props 的 skip 预算来自后端
   实时派生（与 dump 固化 props 不同源）——**数据源差异非机制差异**。

**数据级（预期差异，非一致性缺陷）**：
1. **绝对数量级**：DTA Selected=1-2 / Completed=1-3 vs DanQing 2-14 / 0-52——
   双方数据源不同（DTA=后端实时 generateTileContent[瓦大而少]；DanQing=
   rpc-dump 固化采集域[瓦小而多]）+ 放大绝对档位不同（DTA 默认视图 ×2⁴ 泵
   vs DanQing 取景域 /16）。
2. **Failed=1 / Cache Misses=2-4（仅 DTA）**：DTA 后端对域外请求回 404；
   DanQing dump 域内零缺失（采集域=请求域超集）。
3. **解码时间**：DTA max 522ms 恒定（后端首制大瓦）；DanQing 15-1345ms 随
   视图新瓦变化（dump 字节本地即时解析）。

## 四、复跑配方

```bash
# DTA 侧（静机；vite 冷变换首跑数分钟）
cd D:/Github/danqing-rpc-tools
node debuginfo-capture.mjs                      # 默认编辑大桥测试.bim
node debuginfo-capture.mjs --seed "D:/Github/itwinjs-core/test-apps/display-test-app/assets/newBridge.bim"

# DanQing 侧
ctest --test-dir build -C Debug -R StandardViewDebugInfoProbe
```
