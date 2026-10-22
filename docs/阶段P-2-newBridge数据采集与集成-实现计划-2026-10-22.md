# 阶段 P-2 newBridge.bim 数据采集与 Start View 集成 实现计划（2026-10-22）

> 用户指令（2026-10-22）：新开任务处理 `D:\test\newBridge.bim`（超大模型，tiles 可能几十 GB）
> 的数据采集，并集成到示例程序 Start View；先规划好再实施。
> 前置约束：M-X 集成收口进行中（A 道 fix round 2 + ContoursPanel 代理在跑）——**采集仓外并行，
> 仓内集成件排 M-X 收口提交之后**（避免全量门禁互相污染与 DtaToolBars/SurfaceFileTools 冲突）。
> 工具链：D:/Github/danqing-rpc-tools（M-D..P-1b 全程在役）；playbook=M-K（三模型采集）+
> P-1b 续（frontier 分块/续采）+ P-1b 补遗（key-list/live 面补采）。

## 0. 侦察结论（2026-10-22 实测）

- 源模型：`D:\test\newBridge.bim` = **3,166,023,680 B（2.95GB）**（2026-10-20）。
- 磁盘：D: 653GB 总量 / **376GB 可用**——几十 GB 采集余量充足。
- 工具：collector.mjs（落盘服务）/ rpc-dump.js（tileAdmin patch）/ imodel-rpc.js（iModelRpc
  数据面→imodel.json）/ sweep.js（BFS 全树，cap 参数化）/ capture.mjs（CDP 一键驱动：launch
  模式自动起 vite+electron+collector）/ curate.mjs（sha256 全量校验）/ inventory.mjs（特征盘点）。
- capture.mjs 旗标面：--seed/--out/--wait/--sweep/--sweep-max-{tiles,depth,bytes-mb,mag}/
  --sweep-keys/--drill/--views/--setview/--zoom/--shot/--placements/--strict/--mode。

## 1. 裁决（先例映射，主会话裁量）

| # | 决策 | 取值 | 先例 |
|---|---|---|---|
| R1 | 采集工作区 | `D:/Github/danqing-rpc-tools/dumps/newbridge-*`（仓外） | 全部前例 |
| R2 | 终态落位分档 | ≤~2GB 增量面（drill/imodel.json/补采）入 git（LFS 政策同现状）；**sweep 大体量根 .gitignore 本地持有** + README provenance/复现命令 | bridge-edit-sweep-v1 / frontier-d13-v1（P-1b LFS 指针缺瓦 saga 教训——新 sweep 根不入 LFS） |
| R3 | sweep 预算 cap | `--sweep-max-tiles 800000 --sweep-max-depth 12 --sweep-max-bytes-mb 20480 --sweep-max-mag 8`（首跑探测后可按实态调——cap 随 provenance.sweep.caps 落盘可追溯） | bridge-edit 400k/4.9GB 实测外推 ×4 体量 |
| R4 | 采集步序 | 四步：a 冒烟+特征盘点（小跑）→ b 主采（sweep+drill 并轮）→ c 验收（curate+inventory+域对账）→ d 集成 | M-K(1)/(2) 分段 |
| R5 | 集成时序 | 仓内件（dumpPackageForModel 映射+Start 卡片+锁+计数同步）**排 M-X 收口提交之后** | 本计划前置约束 |
| R6 | 卡片命名 | key=`newbridge`；显示名待 P2-a imodel.json nameProps 实测定（同 bridge-edit 中文名映射先例） | M-K(2) |
| R7 | 并发 | sweep 并发 32（64 实测后端连接失效） | P-1b 续 |
| R8 | 坑预案 | 坑 24 空域默认视图→`skipFirstTileWait:true`；坑 25 超大瓦→sweep 浅层预算优先+大瓦 .gitignore；changeset 非空→provenance 记 changesetId | M-K bridge-edit 三坑实录 |

## 2. 任务分解

### P2-a 冒烟+特征盘点（先行，~0.5-1h）
1. `cp /d/test/newBridge.bim D:/Github/itwinjs-core/test-apps/display-test-app/assets/`（3GB 拷贝）。
2. capture.mjs 基础面小跑（**无 --sweep**：imodel.json 数据面 + drill 默认面）→
   探测：模型/树数、默认视图健康（坑 24 检查）、瓦体量分布（avg/p95/max bytes）、
   instances/纹理/polyline 形态首查（inventory.mjs）、changeset。
3. 产出：`dumps/newbridge-v1/imodel.json` + drill 面 + 探测报告（回填本计划 §4）→
   **cap 终裁**（R3 值按实态修正）。

### P2-b 主采（长跑，数小时——仓外后台，与 M-X 集成并行）
4. sweep 全树 BFS（R3 caps）→ `dumps/newbridge-sweep-v1`；分块+续采预案（run_frontier_chunks
   模式备而不用）；中断恢复=按 manifest 幂等重跑。
5. drill 深潜面 → `dumps/newbridge-drill-v1`（视口自然请求面——DumpOpenChain 锁的请求域基准）。
6. 过冲/放大越界键补采（P-1b 补遗 playbook：key-list/live 面），迭代至域内零 NotFound。

### P2-c 验收（采集收口门）
7. curate.mjs 全量 sha256 校验 0 mismatch；inventory.mjs 特征盘点表；
   域完整性=manifest 键域 vs 树 props 派生域对账（enumerate_frontier.py 参考算法）。
8. 磁盘终态报告（各根体量/合计/剩余空间）→ README（provenance/caps/复现命令/已知限制）。

### P2-d 仓内集成（M-X 收口后）
9. `dumpPackageForModel("newbridge")` 多根映射（主+drill+sweep[条件接入]+补采根——
   bridge-edit 9 根先例形态）+ Start Views 卡片 + MDI 标题映射（R6）。
10. 大瓦 .gitignore 登记 + `SurfaceFileTools.DumpPackageForModelTableIntact` 计数同步 +
    DumpOpenChain 型锁（framed-view 取景面+请求面钉值——按 P2-a 实态裁剪判据）。
11. 真实 app 冒烟（打开→LOD 逐级→回退；stderr 零 NotFound/零 SEH）+ 全量门禁（随 M-X 后首轮）。

## 3. 风险登记
- 3.17GB 源 → 树数可能多（bridge-edit 单树 380k 瓦/5GB 的数倍级）——sweep 800k cap 可能截断
  （depth 12 内未尽）：如实登记 caps 截断面，深域按视口可达面策略（P-1b ⑥遗留界同语义）。
- collector 落盘速率/后端限流未知 → P2-a 探测吞吐后修正 P2-b 时长预期。
- M-X 集成若与 P2-b 同时跑全量门禁 → 门禁机满载抖动族（TD-26/29）风险升高——P2-b 为仓外
  node 进程不占 GL/窗口面，实测影响小，如实记录。

## 4. 完成实录（回填区）
- **P2-a 冒烟（2026-10-22 完成）**：imodel.json 到手——name=newBridge.bim / **3 views / 默认视图 0x5387 / 1 model / gaps 0**；默认视图=小取景域（12.4×6.7×46.2m @ z≈-31，正交，rotation identity——有人工取景痕迹）。树 props：**根域 ±4940 × ±425 × ±1294（约 10km 跨度大桥——"超级大"实锤）**，根为叶（子瓦由内容派生=冷启动细分形态），root contentId `0/0/0/0/1`，maxInitialTilesToSkip=3。
- **P2-a 判读**：0 瓦=基础面采集器在 10km 桥根内容到达前退出（等待窗不足），非坑 24 空域（视图有实域）。**裁决：主采直上 sweep（BFS 不依赖视口——默认视图小取景域/坑 16 取景死亡机制均不适用）**；drill 自然请求面另行补采（newbridge-drill-v1，坑 16 skipFrame 路线）。
- **P2-b sweep 主采（2026-10-22 启动，后台长跑）**：`--sweep --sweep-max-tiles 800000 --sweep-max-depth 12 --sweep-max-bytes-mb 20480 --sweep-max-mag 8`（R3 caps 原值维持——根域 10km 级，800k/20GB 预算可能截断，如实登记）。
- P2-a 时长观测：开模型→树 props≈40s（09:58 页起→09:58:55 defaultView+props）；根内容未及到达（≈3min 窗）。
- **P2-b 首跑受挫实录（10:07）**：根瓦 `-b-0-0-0-0-1` = **135,983,728B（129.68MB 单瓦）**；d1 子请求在途 60s 触 STALL 探针（坑 19 仅记录不中止）——**真因=`--wait 180` 覆盖了 sweep 缺省 420s 的 done 等待**（capture.mjs:73），BFS 未完成即按失败收口。单瓦超时=180s 晚到仍补采派生（坑 19 机制完备，无误跳风险）。部分成果不可跨运行续用（collector 去重为内存态）→ 删部分目录重跑。
- **P2-b 重跑（10:1x 启动，后台）**：同 caps + `--wait 14400`（4h 安全天花板，BFS done 即提前收口）→ `dumps/newbridge-sweep-v1`（fresh）。根瓦 129MB 量级外推：全树数十 GB/数小时——与用户预估吻合。
- **P2-b 二跑定音（10:13）**：d1 `-b-1-0-0-0-1` 两轮均未在 180s 内到达（根 ~2min 到）→ 超时项离开 pending 集 → BFS 队列空提前 DONE（1 瓦收场）。**真因=180s 单瓦超时对本模型系统性过紧**（瓦体量 ~1000× 常规模型）。**工具补丁**：sweep.js `REQ_TIMEOUT_MS` 参数化（capture.mjs 新旗标 `--sweep-req-timeout-sec`，缺省 180 保持旧行为；占位符注入走既有 caps 机制进 provenance）。另登记：**两轮根瓦字节不同**（135,983,728 vs 107,944,096——后端生成非字节稳定，sha256 按轮内校验）。
- **P2-b 三跑（10:2x 启动，后台长跑）**：`--sweep-req-timeout-sec 900 --wait 86400`（单瓦 15min/24h done 天花板）→ fresh `newbridge-sweep-v1`。20GB cap ÷ ~100MB 均瓦 ≈ ~200 瓦预算，顺序 BFS 时长数小时-十余小时量级。
- **三跑中途健康（11:03 定音）**：tiles=60 / 4,998,611,292B（~4.65GB）/ **errs=0**，已下潜 depth-5（`-b-5-0-2-{a,c}-1`）；节奏 ~82MB/min；900s 超时修正生效（d1 早过）。外推 20GB cap ≈ 4-4.5h 触顶（budget-capped 收口）。
- **P2-b 收口（13:0x，定案）**：`done:budget-capped`——**5407 瓦 / 21,475,290,400B（20GB cap 触顶）/ 全程 errs=0**（70 叶 + 5337 枝待派生；dup 仅 5）。均瓦 3.9MB（深处瓦远小于根瓦 100-130MB——20GB 买到全深度结构）。
- **P2-c 前沿枚举（enumerate_frontier.py + FRONTIER_DUMP 覆写补丁，定音）**：**d1-d8 完整**（逐层 0 missing）——截断发生在 d9 进入处：
  - d9 前沿 = 1,938 空间 + 167 mag ≈ **2,105 键**（估 ~1GB/12min @ sweep 吞吐）
  - d10 前沿 = 10,296 空间 + 1,254 mag ≈ **11,550 键**（d9 补齐后还会增长——d9 内容揭示更多 d10 子代）
  - d11+ 逐层待 d10 后再枚举；对齐用户"几十个G"预期，**裁决=按 frontier 波次续采**（key-list 模式 `--sweep-keys`，增量不重取；每波独立验收；与原始指令一致可随时叫停）
- **M-X 全量门禁并行进行中**（b25wyxj5r）——续采波次排队于门禁完成后（避免桌面满载抬高 TD-26/29 族）。
