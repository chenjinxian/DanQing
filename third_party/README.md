# 第三方依赖管理（third_party）

> **策略：FreeCAD-LibPack 模式** —— 小库以源码入 git（git submodule），大库（Qt）以版本固定的下载脚本获取（二进制不进 git）。
> 版本基线对齐 **FreeCAD-LibPack v3.5.2**：Qt `6.11.1`、GoogleTest `7140cd4`。

## 目录约定

```
third_party/
├── README.md              # 本文件（策略说明，入 git）
├── qt/                    # ★ gitignored —— aqtinstall 下载的 Qt 6.11.1 预编译包
│   ├── mac/               #   macOS clang_64
│   ├── linux/             #   Linux gcc_64
│   └── windows/           #   Windows win64_msvc2022_64
└── googletest/            # ★ git submodule（源码入 git）
```

**入 git 的**：本 README、`scripts/bootstrap_third_party.*`、git submodule 指针（`.gitmodules` + gitlink）、CMake 中的版本固定。
**不入 git 的**：`third_party/qt/**`（下载产物，每平台 ~400MB–1GB）。

## 版本基线（单一事实来源）

| 依赖 | 版本 | 来源 | 入 git 方式 |
|------|------|------|------------|
| Qt | **6.11.1** | FreeCAD-LibPack v3.5.2（`git://code.qt.io/qt/qt5.git` v6.11.1） | aqtinstall 脚本下载，不入 git |
| GoogleTest | `7140cd416cecd7462a8aae488024abeee55598e4` | FreeCAD-LibPack 同版本 | git submodule（源码入 git） |

## 首次初始化

```bash
# macOS / Linux
./scripts/bootstrap_third_party.sh

# Windows (PowerShell)
.\scripts\bootstrap_third_party.ps1
```

脚本会：
1. 安装 `aqtinstall`（若未装）
2. 下载 Qt 6.11.1（qtbase，含 Core/Gui/Widgets/Network/Concurrent/Sql）到 `third_party/qt/<platform>/`
3. 初始化并更新 git submodule（GoogleTest）

完成后，CMake 配置时指定 Qt 位置：

```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="$PWD/third_party/qt/mac/6.11.1/macos" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## 为什么 Qt 不入 git

- Qt 6.11.1 预编译包每平台 ~400MB–1GB，4 平台 = 4–8GB。
- 即使用 Git LFS，仓库克隆/拉取成本过高。
- **FreeCAD 自己也不把 LibPack 二进制提交进 git** —— LibPack 是独立仓库，产出可下载的 7z 制品，用户设 `FREECAD_LIBPACK_DIR` 指向本地副本。
- 我们采用同模型：**配方（脚本 + 版本固定）入 git，二进制不入 git**。

## 跨平台 Qt 来源

| 平台 | aqtinstall 规格 | 备注 |
|------|----------------|------|
| macOS (clang_64) | `aqt install-qt mac desktop 6.11.1 clang_64` | Apple Silicon / Intel 通用 |
| Linux (gcc_64) | `aqt install-qt linux desktop 6.11.1 gcc_64` | |
| Windows (msvc2022_64) | `aqt install-qt windows desktop 6.11.1 win64_msvc2022_64` | 对齐 LibPack MSVC 143 |

## 新增第三方库的流程

1. **小库**（源码可编译，<50MB）：加为 git submodule（`git submodule add <url> third_party/<name>`），checkout 固定 tag/hash，登记本 README 版本表。
2. **大库**（预编译二进制）：在 `bootstrap_third_party.*` 增加下载步骤，版本固定，目录命名 `third_party/<name>/<platform>/`，加入 `.gitignore`。
3. 任何新依赖必须：① 有许可证记录（与商用兼容）；② 有封装层（不在业务代码直接暴露第三方类型）。
