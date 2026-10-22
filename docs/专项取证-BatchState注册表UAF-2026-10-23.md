# M-X 合并后四真窗缩放测试 SEH 0xc0000005 根因裁决报告

日期：2026-10-22 ｜ 代理：AV 根因专案（worktree `agent-a3e6a03dcbda68d08` @ X-a1 7382427196 清建）
范围：`WheelZoomCoalesceTest.{DeepZoomInClampKeepsDecorationsAndFit, AcsDiscSurvivesDeepZoomPixels}`（dqAppTest）+ `ZoomBlackBoxSeq.{RealMainWindowMdiPath, MdiAcsDiscSurvivesZoomPixels}`（DisplayTestAppDtaTest）

---

## 1. 裁决（一句话）

**H1 变体（新 H4）：非 M-X 引入、非经典 TD-26——是「TD-35（2026-10-21）引入的 ACS 原点 pickable 每帧 Batch churn」×「BatchState::m_batches 注册表缺 C++ 生命周期适配（TD-22 同族 GC 语义缺口）+ 缺参考的 `batchId==0` 幂等守卫」的预存潜伏 UAF；M-X（含纯数据层 X-a1）只是布局/时序漂移把失败率从"基线侥幸 0/25"推到 ~10-15% 可见区。**

- H2（M-X 内存 bug）：**排除**。全部崩溃栈在 pre-M-X 代码；X-a1（diff=数据载体+事件监听，无渲染路径代码）清空重建即崩。
- H1（TD-26 滚轮竞态被放大）：**方向对但对象错**——同测试家族确有预存缺陷，但不是 TD-26 的像素/呈现分裂缺陷，而是 TD-35×注册表 UAF。经典 TD-26（呈现分裂 `fboBlues>0/screenBlues=0` 像素断言败）在本专案会话中独立复现一次（nopick-35 轮），与 AV 无关。
- H3（纯环境）：**排除**——堆损坏/UAF 栈在干净单进程复现，代码级可定位。

## 2. 证据链（全部本会话实测，X-a1 干净构建）

### 2.1 崩溃样本（~35 轮 instrumented，6 崩 ≈15%）

| 轮 | 测试 | 时刻 | 形态 | 崩溃栈（DanQing 侧） |
|---|---|---|---|---|
| avcap-1 | DeepZoom | 2.61s | 0xc0000374 堆损坏 fastfail | （进程直死，无栈） |
| avcap-9 | DeepZoom | 3.52s | AV 0xc0000005 读 0x1000001B9 | **site A**：`Viewport::RenderFrame→drawOverlays→drawPass:2839→PolylineGeometry::draw(SurfaceGeometry.cpp:391)→OpenGLDriver::drawArrays:881→Intel 驱动内部`（RegisterProcTableCallback+… 8 帧，op=0 读） |
| avcap-28 | AcsDisc | 2.43s | AV 同地址 0x1000001B9 | 同 site A 逐帧一致（驱动模块 ASLR 平移、偏移相同） |
| avcap-30 | DeepZoom | 3.85s | AV | **site B**：`renderOpaque→drawPass:1788→Batch::getFeatureOverrideLUT()->anyOverridden→vector::_Unchecked_begin`（FeatureOverrideLUT.h:177，读已释放的 `m_overrides` 向量） |
| avcap-32 | DeepZoom | ~3.9s | 0xc0000409 fastfail | （v2 探针轮；GLLIFE 死集大量命中——后甄别含回收假阳性） |
| avcap-34 | DeepZoom | 1.75s | AV 读 0xFFFFFFFFFFFFFFFF | **site C**：`renderOpaque→drawPass:2954(PushBatch case)→Batch::updateFeatureStates(Batch.cpp:95)→PackedFeatureTable::getNumFeatures`——**经 `findBatch(batchId)` 从注册表找回的 Batch 已释放**（0xFF…=freed 填充） |

环境：Intel Arc B390 GL 驱动 32.0.101.8359（[GLINFO] 历史档）；gtest 捕获 SEH（`SetUnhandledExceptionFilter` 永不触发——本报告改用 `AddVectoredExceptionHandler` 先于帧级 `__except` 截获并 dbghelp 符号化，仪器自检经 `CrashProbeSelfTest` 故意空指针验证）。

### 2.2 逐层排除/锁定（探针三轮迭代）

1. **GL 对象死集（v1/v2）**：bind/draw 时 VAO/buffer/纹理名大量"***"命中——但 GL 名回收产生假阳性（每帧删旧名→glGen 立即复用同名，avcap-30 单轮 6832 条 delete）。**ACS 三轴 GL 对象确以同名高频删/建（每帧 ~40 对象）**——churn 画像实锤，但责任未定。
2. **末事件追踪（v3：per 名/地址记 created/destroyed，只报"末事件=destroyed"）**：
   - VAO 名：**零真悬垂**（v2 的 vao 告警全部消失）→ site A 不是"我们 bind 死 VAO 名"。
   - Batch（ctor 置 true/dtor 置 false）：drawPass:1788 读点与 `BatchState::reset` 遍历点在采样崩溃轮**未报真悬垂**（avcap-32 的地址复用假阳性被消除）。
   - 但 site C 崩栈直接证明：**`findBatch` 返回/解引用了已释放 Batch**（读 0xFFFF…）。
3. **静态对读参考（§11.8，itwinjs @ D:/Github/itwinjs-core）**：
   - 参考 `BatchState.getBatchId(batch, allowAdd)`（BatchState.ts:107-114）：`if (allowAdd && 0 === batch.batchId)` 才 `_batches.push`——**幂等守卫**；DanQing `BatchState::assignBatchId`（Batch.cpp:305-314）**无守卫，每次 addBatch 无条件 push**。
   - 参考注册表由 **GC 保活**（死批不可达问题在 JS 不存在）；`Batch.resetContext()`（置 batchId=0）仅在 `reset()` 时批量执行——`resetBatchState()` 调用点=Target.ts:731/825/926（drawForReadPixels 尾/readPixels 尾/endReadPixels）。
   - DanQing `reset()` 仅两路：`TargetImpl::endReadPixels`（TargetImpl.cpp:1079——AcsDisc 的 `ReadFrameForTest` 每步触发）与 `DrawCommand.cpp:157 PopBatchCommand::execute`——**后者是死代码路径**（compositor `drawPass` 以内联 switch 执行命令：`case PopBatch: m_batchState.pop()`，不置空注册表）。
   - 参考 `Target.reset`（Target.ts:583-590）在清命令时对 `_batches` 逐批 `onTargetDisposed(this)` 通知——DanQing 无对应物。

### 2.3 统一机制（一个写入者、多面 victim）

```
每失效帧（深缩放动画每帧 InvalidateScene→InvalidateDecorations）：
  AcsTriadDecorator::Decorate（AcsTriadDecorator.cpp:296-310）
    dispose 旧 pick graphic（TD-35 pickable 臂批——本测试面唯一每帧 Batch）→ ~Batch（释放）
    builder->finish() 建新批
  drawFrame 命令构建：RenderCommands::addBatch → assignBatchId（无守卫）→ m_batches.push_back(&新批)
    —— 注册表条目永不注销；除最新数条外全部悬垂（DeepZoom 无 readPixels → 永不 reset）
  drawPass 执行 PushBatch（SceneCompositorImpl.cpp:2936-2966）：
    findBatch(batchId) 在悬垂条目上读 batch->getBatchId()（垃圾值，lowerBound 比较）
    → 可匹配/返回死批 → push(*死批) + 惰性更新臂：
        updateFeatureStates / applySubCategoryVisibility / updateAppearanceOverrides
        ——经死指针【写】已释放内存（LUT 向量 resize/版本计数）
  下游表现：
    读侧触页 → site C（getNumFeatures）/ site B（anyOverridden 向量）
    写侧污染分配器 → form B（0xc0000374 堆损坏 fastfail——驱动/CRT 堆检测）
    邻块污染 → site A（Intel 驱动在 drawArrays 内解引用被污染的内部结构，
      确定性垃圾地址 0x1000001B9={1,441} 跨进程一致）+ 0xc0000409
  闪烁率 ~10-15%：释放堆块通常仍可读写（分配器不解绑小页）→ 多数轮静默
  读垃圾/写无害；仅当块被复用/归还 OS 或分配器校验时崩 —— 桌面/时序敏感的观测画像来源
```

先例档案：`Viewport.cpp:1115-1121` 注释自证 **2026-09-19 同族崩溃**（doZoom→InvalidateScene→缓存图形删除→悬空列表→pick 遍历虚调用 SEH）——当日修复清了 `m_decorations`，但批注册表当时无每帧批（pre-TD-35）故未暴露。**TD-35（8fb09eb8a0，2026-10-21，M-X 前一日）引入 pickable ACS 原点批 = 本测试面首个每帧 Batch**，激活潜伏缺口。

### 2.4 因果验证（DANQING_NO_PICK_BATCH 门控，TEMP-DIAG）

关闭 TD-35 pickable 臂（AcsTriadDecorator.cpp pickOpts.pickableId 不设 → 点串不批包裹 → 本测试面零 Batch → 注册表空）后跑轮：
- **轮 35-56 共 22 轮：AV/堆损坏签名零复发**（P(门控无效|15% 基率) = 0.85²² ≈ 2.8%）。轮 35/38 的两次非零 rc=**经典 TD-26 呈现分裂像素断言败**（`fboBlues=48/screenBlues=0`——独立已知间歇族，非本专案对象；循环仅对 crash 签名停表，本会话的意外 TD-26 复现样本）。
- 对照：无门控 instrumented 轮 6 崩/35 轮 ≈ 15%。
（门控构建/探针改动全部 TEMP-DIAG 于 worktree 分支，未提交、不入主干。）

### 2.5 M-X 的角色（放大器而非作者）

- X-a1 在四测试的运行时足迹：DisplayStyle/DisplayStyleSettings 布局变大（堆分配模式漂移）+ AttachToView 多注册一个从不触发的监听器——**不在任何崩溃栈上**。
- HEAD 的 A 道运行时足迹（contours want 门关、无 Marker）活跃差=更大 shader 程序+每 draw 矩阵上传——时序类漂移。
- 统计：基线（含 TD-35 的 10e670e384）0/25 vs X-a1 ~15% ——Fisher p≈0.24，基线零失败与 ~10% 潜伏率相容（运气轮）。失败率方向性上升=布局/时序把"写垃圾落点"从无害区推向可观测区的概率调制。
- 主干门禁 5 败中的 DumpOpenChain.OpensJoesHouse 已由父会话隔离 ×2 干净（TD-29 照旧），与本机制无关。

## 3. 缺陷点定位（file:line，主干 HEAD 对应）

| # | 缺陷 | 位置 | 参考锚 |
|---|---|---|---|
| 1 | `assignBatchId` 缺幂等守卫（每帧重复注册） | `dqRender/src/render/Batch.cpp:305-314` | BatchState.ts:107-114（`0 === batch.batchId` 门） |
| 2 | 注册表裸指针无注销/无死集清扫（TD-22 族 GC 适配缺口） | `dqRender/src/render/Batch.h:321`（`std::vector<Batch*> m_batches`）+ `~Batch` 无反注册 | 参考 GC 保活 + Target.ts:583-590 `onTargetDisposed` 通知面 |
| 3 | 崩溃消费点（惰性更新臂经死批写/读） | `dqRender/src/render/SceneCompositorImpl.cpp:2936-2966`（PushBatch case：findBatch+updateFeatureStates/applySubCategoryVisibility/updateAppearanceOverrides）+ `:1786-1790`（getCurrentBatch→anyOverridden） | DrawCommand.ts PushBatch/FeatureOverrides.ts:412-441 |
| 4 | churn 源（每帧 dispose+重建批） | `dqApp/src/AcsTriadDecorator.cpp:296-310`（dispose 旧）+ `:388-410`（TD-35 pickable 臂新建批） | 参考语义=每帧重建（AuxCoordSys.ts:301-305），差异=浏览器 GC 延迟回收 vs DanQing 同步销毁 |
| 5 | 保真度伴生项：drawPass 内联 `case PopBatch: m_batchState.pop()` ≠ 参考 `popBatch()→clearCurrentBatch()`；`DrawCommand.cpp:157` 的全量 `reset()` 反而偏离参考且为死路径 | SceneCompositorImpl.cpp:3028-3030 / DrawCommand.cpp:154-158 | Target.ts:379-381（popBatch） |

## 4. 修复建议（未动主干，报告为准）

1. **归位参考守卫（最小正交）**：`assignBatchId` 补 `batch.getBatchId()==0` 门（仅未注册批入表）——消除每帧重复注册与 batchId 段位错配。
2. **C++ 生命周期适配（TD-22 模式，必做）**：`Batch` 增 `BatchState*` 反指针（`setContext` 时登记），`~Batch` 从 `m_batches` 摘除自身（或注册表条目改弱形态+reset/changeDecorations 时清扫——反指针法确定性最高、无死集判别歧义）。两件合做后注册表任意时刻只含活批，site A/B/C 与堆损坏形态一并根除。
3. **伴生保真**（顺路，非崩溃必需）：PopBatch 内联路径对齐参考 `clearCurrentBatch` 语义。
4. **验证协议**：修复后四测试 ×40 轮（预期 0 crash，TD-26 呈现分裂族可能仍偶发——属另一缺陷面）+ 全量门禁；本报告的 vectored 打栈器/GLLIFE 探针已在本 worktree TEMP-DIAG 在案可复用。

## 5. 仪器与产物登记（TEMP-DIAG，均在 worktree `agent-a3e6a03dcbda68d08`，不提交）

- `dqApp/tests/WheelZoomCoalesceTest.cpp`：vectored 崩溃打栈器（`DANQING_CRASH_STACK`）升级 + `CrashProbeSelfTest` 自检（`DANQING_CRASH_PROBE`）；stdout 无缓冲化。
- `samples/DisplayTestApp/tests/ZoomBlackBoxSeqTest.cpp`：同款打栈器（未构建 DtaTest 目标——dqAppTest 已足够定案）。
- `dqRender/src/rhi/opengl/OpenGLDriver.cpp`：GL 对象末事件探针（`DANQING_GL_LIFETIME`）。
- `dqRender/src/render/Batch.{h,cpp}`+`SceneCompositorImpl.cpp`：Batch 末事件探针（同 env）。
- `dqApp/src/AcsTriadDecorator.cpp`：`DANQING_NO_PICK_BATCH` 因果门。
- 崩溃轮日志：`/tmp/avcap-{1..34}.log`、门控轮 `/tmp/nopick-{35..}.log`。
