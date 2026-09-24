# 丹青（DanQing）图形引擎架构设计

> 2026-09-24 重写：替代旧的五模块 SDK 架构设计（BIM/CAD 底座定位时期）。
> 本文是唯一现行架构描述；规则权威为 CLAUDE.md，模块级 API 以各模块 `PublicAPI/` 代码为准（规划期的 `*-模块API设计.md` 已随 2026-09-24 历史文档清理删除）。

## 定位

丹青是**纯客户端图形引擎**，是天工开物平台命名家族（天工开物 / 鲁班CAD / 真形 / 绳墨）中的渲染层：

- **双源血统**：
  - **itwinjs-core** —— 数字孪生大体量渲染：Tile 系统（iMdl/瓦片树/LRU/四级失效级联）、24 步帧管线、多 Pass 合成（WBOIT OIT）、拾取与 Hilite
  - **Filament** —— 高质量实时渲染与全平台能力：RHI 后端抽象、平台呈现层（WGL 权威参考 `PlatformWGL.cpp`）、材质与光照
- **职责边界**：丹青只管渲染与视口交互宿主。几何内核归**真形**（私有仓，自主研发）；数据/事务归 imodel-native；CAD 应用归鲁班CAD。
- 不含：BRep 建模内核、BIM 数据引擎、CAD 应用框架（旧 zoBRep/zoData/zoPlatform 规划已随定位废弃）。

## 模块架构（5 个已实现模块 + 测试宿主）

```
samples/DisplayTestApp（像素级回归 harness，非 SDK）
        │ 只使用 PublicAPI
        ▼
┌─────────────────────────────────────────────┐
│ dqApp —— 渲染宿主层（允许 Qt）                │
│ Application 单例 / ViewManager / Viewport    │
│ （24 步帧管线）/ ToolAdmin / Decorations     │
└──────┬──────────────────┬───────────────────┘
       │                  │
       ▼                  │
┌─────────────┐           │
│ dqRender    │           │
│ 渲染引擎     │           │
│ RHI/Tile/   │           │
│ 多Pass/Shader│          │
└──────┬──────┘           │
       │                  ▼
       │            （dqApp 亦直接消费 dqCommon/dqGeom）
       ▼
┌─────────────┐   ┌─────────────┐
│ dqGeom      │   │ dqCommon    │
│ 纯数学几何   │   │ 共享渲染类型 │
│ （零内核依赖）│   │ ColorDef/   │
│             │   │ ViewFlags/  │
│             │   │ Frustum/    │
│             │   │ FeatureTable│
└──────┬──────┘   └──────┬──────┘
       └────────┬─────────┘
                ▼
            dqBase（共享基础：RefPtr/Result/容器/事件，零领域知识）
```

## 依赖硬约束（详见 CLAUDE.md §8）

- 依赖只许向下，永不反向；`dqRender → dqGeom` 是唯一允许的引擎间边
- 引擎 SDK（dqBase/dqGeom/dqCommon/dqRender）公开 API **零 Qt**；dqApp 是 Qt 的唯一桥接点
- 公开 API 仅在 `PublicAPI/<模块名>/`；跨模块通信只经 SDK 接口

## 渲染管线与平台层

- 帧管线：dqApp `Viewport::renderFrame()` 24 步管线（对齐 itwinjs `Viewport.ts`）
- 多 Pass 合成：dqRender `SceneCompositor`（WBOIT OIT / Hilite / 7 变体合成）
- Windows 呈现层：对齐 filament `PlatformWGL.cpp`（swapchain 自持 DC / resize=表面过期 / 句柄生命周期归应用层）
- 渲染系统全流程分析见 `docs/itwinjs-core-渲染系统执行流程分析.md`

## 测试体系

- 单元测试：各模块 `tests/`（GoogleTest，移植自参考项目，溯源规则见 CLAUDE.md §5）
- 像素级回归：samples/DisplayTestApp 真窗口 harness（View3DResizeTest / ZoomBlackBoxSeqTest 等）
- 参考优先级：imodel-native C++ 测试 > itwinjs-core TS 测试（CLAUDE.md §5）

## 参考项目

| 参考项目 | 角色 |
|---------|------|
| **itwinjs-core** | 渲染栈主参考：渲染管线、前端 API、Tile 系统、glTF |
| **filament** | RHI 驱动、OpenGL 后端、平台呈现层权威参考 |
| **imodel-native** | dqGeom 几何参考 |
| **FreeCAD** | 仅测试宿主（DisplayTestApp）Gui 命令框架参考 |

## 演进方向

- Filament 侧吸收深化：RHI 后端抽象（Vulkan/Metal）、PBR 材质、更多平台
- itwinjs-core 侧持续对齐：随 tiangong-kaiwu 仓的 subtree 同步更新参考基线
- 代码标识符已完成 dq* 化（2026-09-24）：命名空间/宏 DQ_*/环境变量 DANQING_*/类型前缀 Dq*
