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

## newBridge fit+rotate 协议逐视图对比（2026-10-25——用户裁定协议，本节为权威版）

**协议（用户 2026-10-25 裁定）**：打开模型 → **Fit 一次** → 8 个标准视图旋转逐个抓
Debug Info。**零缩放操作**——上文 zoom 泵/定比缩放体协议废弃（"之前对比方式有问题"）。
瓦已进 `.Tiles` 缓存（用户手动测试 + 采集轮均已生成），Fit 态只有根瓦呈现，八视图
旋转零新请求——稳态纯渲染帧率对比。

**前置条件**：DTA 实例须前台可见（Electron 后台页 rAF 节流，tracker 全空；带
`--disable-backgrounding-occluded-windows` 等三 flag 的自管实例 + `Page.bringToFront`
后仍为真值——FPS=PerformanceMetrics.completeFrameTimings 50 帧滚动，遮挡只压低
采集便利性不影响真值，消融实验在案：前后台 15.4-16.9 vs 15.4-16.9 持平）。

### 双侧矩阵（newBridge，Fit → 8×View.Standard，2000×1400，2026-10-25）

| 视图 | DTA FPS | DanQing FPS | DTA 瓦态 | DanQing 瓦态 |
|---|---|---|---|---|
| Top | 16.67 | 15.30（首视图暖场） | Sel 1 / Elid 7 / **85 prims** | Sel 1 / **80 prims** |
| Bottom | 16.76 | 29.32 | 同上（八视图恒定） | 同上（八视图恒定） |
| Left | 16.85 | 29.69 | 同 | 同 |
| Right | 16.58 | 30.09 | 同 | 同 |
| Front | 15.38 | 30.02 | 同 | 同 |
| Back | 15.71 | 30.09 | 同 | 同 |
| IsoLeft | 16.30 | 29.57 | 同 | 同 |
| IsoRight | 16.76 | 28.98 | 同 | 同 |

- 两侧八视图瓦状态**逐字段同构**（Selected 1、旋转零请求、根瓦 LOD 呈现；85 vs 80
  prims = DTA 计入背景/装饰图元的细微差）。
- **FPS ≈ 2×**：DanQing 稳态 29-30 vs DTA 15.4-16.9（同窗径、同瓦态、同 LOD）。
  双侧首视图均偏低（着色器编译暖场：DanQing Top 15.3 / DTA 无明显首帧尖峰）。

### 本轮取证附赠：探针旋转机制根因（§11.8/§11.9 实录）

fitrotate 首版探针用裸 `SetRotation(matrix)` 换矩阵——**Top/Bottom/Front 三视图瓦
内容消失**（[SEL] cull 实录：根瓦球心恰=fit 域中心，若绕视心旋转永不被剔 → 旋转
没绕视心）。参考机制（StandardViewTool.onPostInstall，ViewTool.ts:3504-3522）=
**绕 `getDefaultTargetPointWorld`（视心）定点旋转 frustum 点集**（`Transform.
createFixedPointAndMatrix`）→ `setupFromFrustum` → synchWithView——frustum 刚体
旋转内容永在锥内。DanQing 的 `dqApp::StandardViewTool` 早已 1:1 移植（app 的
ViewTop 等按钮全走它，View3DInventor:280）——**引擎无缺陷，是探针绕过了参考机制**
（旧定比缩放协议的 LookAtVolume 重定心掩盖了该偏差）。探针已改走 StandardViewTool。

### 探针配方（fit+rotate，2026-10-25）

```bash
# DanQing（DebugWindowDiag.StandardViewDebugInfoProbe，DANQING_PROBE_PROTOCOL=fitrotate）
DANQING_PROBE_MODEL=newbridge DANQING_PROBE_PROTOCOL=fitrotate \
  ./build/samples/DisplayTestApp/Debug/DisplayTestAppDtaTest.exe \
  --gtest_filter='DebugWindowDiag.StandardViewDebugInfoProbe'
# DTA（debuginfo-capture.mjs 自管实例 + 防节流 flag + bringToFront；Fit 后 8×View.Standard）
cd D:/Github/danqing-rpc-tools
node debuginfo-capture.mjs --seed "D:/Github/itwinjs-core/test-apps/display-test-app/assets/newBridge.bim"
```

上文"Plan A 驻留生成"（park-wheel.ps1）**不再需要**——fit+rotate 协议下瓦态有界
（根瓦），生成债已由手动测试清偿。
