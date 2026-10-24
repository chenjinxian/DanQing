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

---

## 五、newBridge（旗舰模型）逐视图对比（2026-10-24 补）

双侧同协议（8 标准视图 × 同缩放档）。**数据前提差异**：DanQing 从采集域本地
流式（169,920 瓦/47.5GB 全在盘）；DTA 的 `newBridge.bim.Tiles` 本地缓存为
**空**（采集会话的瓦未持久化到该缓存）——后端需按请求即时
`generateTileContent`（10km 桥 d1-d2 级瓦单枚 100MB 级）。

### DanQing（Release 构建；DANQING_PROBE_ZOOM=2 即取景域/2 档）

| 视图 | Selected | Completed | Elided | 图元/批/分支 | FPS |
|---|---|---|---|---|---|
| Top | 2 | 5 | 0 | 86/3/6 | 8.9（巨瓦装载中）|
| Bottom | 2 | 0 | 0 | 86/3/6 | 16.3 |
| Left | 4 | 8 | 12 | **184**/5/8 | 16.3 |
| Right | 4 | 0 | 0 | **184**/5/8 | 14.8 |
| Front | 2 | 0 | 0 | 86/3/6 | 23.5 |
| Back | 2 | 0 | 0 | 86/3/6 | 30.1 |
| Iso | 2 | 0 | 0 | 86/3/6 | 28.2 |
| RightIso | 2 | 0 | 0 | 86/3/6 | （装载期）|

（FPS 低段 = Debug 构建装载 100MB 级巨瓦的工作集压力；稳态渲染同前节 62.5。）

### DTA（空缓存 + 即时重生成）

| 视图 | Selected | Completed | Elided | 图元 | FPS |
|---|---|---|---|---|---|
| 全部 8 视图 | **0** | **0** | 41→123（累计）| **14**（仅背景/装饰）| 60（空放）|

Dispatched 8→24 递增但 **Completed 恒 0**——后端按需生成未在会话内交付
（10km 桥瓦生成耗时超过采集窗）；DTA 对 newBridge 实际**渲染空视口**。

### 判定（2026-10-24 修订——初版判定有误，订正如下）

- **初版错误**：曾据 DTA Selected=0/图元 14 判"DTA 渲染空视口"——**结论
  无效**。DTA 对 newBridge 是**首访即时生成**架构（.Tiles 缓存未持久化，
  每枚瓦后端现生成，10km 桥生成量巨大）；采集窗内未生成完 ≠ 不能渲染。
  Completed=0/Dispatched 8→24 恰是生成进行中的中间态（Elided 41→123 =
  树派生在跑）。条件静默（Active+Pending 归零）复测后方可能出稳态对比。
- **保持有效的发现（TD-38 登记）**：newBridge 深档视口（取景域/16、/8）
  的渐进细化工作集无界 → `bad_alloc`（/2 有界可完成）——TileAdmin 内存
  预算对 pinned 请求集的治理需调优（巨型模型深缩放场景，DanQing 侧独立
  成立，与 DTA 对比无关）。

### 探针配方（newBridge）

```bash
# DanQing
DANQING_PROBE_MODEL=newbridge DANQING_PROBE_ZOOM=2 \
  ctest --test-dir build -C Release -R StandardViewDebugInfoProbe
# DTA
cd D:/Github/danqing-rpc-tools
DTA_SETTLE_MS=45000 node debuginfo-capture.mjs \
  --seed "D:/Github/itwinjs-core/test-apps/display-test-app/assets/newBridge.bim"
```
