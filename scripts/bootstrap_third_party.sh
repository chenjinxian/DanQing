#!/usr/bin/env bash
# bootstrap_third_party.sh —— 初始化第三方依赖（macOS / Linux）
#
# 做三件事：
#   1. 安装 aqtinstall（若未装）
#   2. 下载 Qt 6.11.1（qtbase）到 third_party/qt/<platform>/  （gitignored）
#   3. 初始化 git submodule（GoogleTest）
#
# 用法：./scripts/bootstrap_third_party.sh
# 幂等：已下载的步骤会跳过。

set -euo pipefail

# 切到仓库根
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

QT_VERSION="6.11.1"
# GoogleTest 版本由 git submodule 指针（gitlink）固定，无需在此 checkout 指定 hash。
# 当前固定于 973323e（master，对齐可用的 GoogleTest；如需精确对齐 FreeCAD-LibPack 的
# 7140cd4，可在 submodule 内 git fetch 后 checkout，再 bump gitlink）。

echo "=== DanQing third-party bootstrap (macOS/Linux) ==="
echo "repo root: $REPO_ROOT"
echo ""

# --- 平台检测 ---
case "$(uname -s)" in
    Darwin*)
        PLATFORM_DIR="third_party/qt/mac"
        AQT_OS="mac"
        AQT_ARCH="clang_64"
        ;;
    Linux*)
        PLATFORM_DIR="third_party/qt/linux"
        AQT_OS="linux"
        AQT_ARCH="gcc_64"
        ;;
    *)
        echo "ERROR: unsupported OS $(uname -s). Use bootstrap_third_party.ps1 on Windows."
        exit 1
        ;;
esac

# --- 1. aqtinstall ---
echo "=== [1/3] ensure aqtinstall ==="
if python3 -m aqt --version >/dev/null 2>&1; then
    echo "aqtinstall: already installed"
else
    echo "aqtinstall: installing (pip install --user aqtinstall)..."
    python3 -m pip install --user --quiet aqtinstall
fi

# --- 2. 下载 Qt ---
echo ""
echo "=== [2/3] download Qt $QT_VERSION ($AQT_OS/$AQT_ARCH) ==="
if [ -d "$PLATFORM_DIR/$QT_VERSION" ]; then
    echo "Qt $QT_VERSION already present at $PLATFORM_DIR/  (skip)"
else
    mkdir -p "$PLATFORM_DIR"
    # qtbase 含 Core/Gui/Widgets/Network/Concurrent/Sql/PrintSupport 等
    python3 -m aqt install-qt "$AQT_OS" desktop "$QT_VERSION" "$AQT_ARCH" \
        --outputdir "$PLATFORM_DIR"
    echo "Qt installed to $PLATFORM_DIR/$QT_VERSION"
fi

# --- 3. git submodule ---
echo ""
echo "=== [3/3] init git submodules (GoogleTest) ==="
if [ -f "third_party/googletest/CMakeLists.txt" ]; then
    echo "googletest submodule already initialized (skip)"
else
    git submodule update --init --recursive third_party/googletest
    echo "googletest at $(git -C third_party/googletest rev-parse --short HEAD)"
fi

echo ""
echo "=== done ==="
echo "Qt 位置:    $REPO_ROOT/$PLATFORM_DIR/$QT_VERSION"
echo "配置 CMake: cmake -S . -B build -DCMAKE_PREFIX_PATH=\"$REPO_ROOT/$PLATFORM_DIR/$QT_VERSION\""
