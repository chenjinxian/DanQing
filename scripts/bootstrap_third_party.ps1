# bootstrap_third_party.ps1 —— 初始化第三方依赖（Windows）
#
# 做三件事：
#   1. 安装 aqtinstall（若未装）
#   2. 下载 Qt 6.11.1（qtbase，win64_msvc2022_64）到 third_party\qt\windows\  （gitignored）
#   3. 初始化 git submodule（GoogleTest）
#
# 用法（PowerShell）：.\scripts\bootstrap_third_party.ps1
# 幂等：已下载的步骤会跳过。

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path "$PSScriptRoot\.."
Set-Location $RepoRoot

$QtVersion = "6.11.1"
# GoogleTest 版本由 git submodule 指针固定
$PlatformDir = "third_party\qt\windows"

Write-Host "=== DanQing third-party bootstrap (Windows) ==="
Write-Host "repo root: $RepoRoot"
Write-Host ""

# --- 1. aqtinstall ---
Write-Host "=== [1/3] ensure aqtinstall ==="
# 注意：$ErrorActionPreference=Stop 下，native 命令 stderr 经 2>&1 会包装成
# ErrorRecord 抛出（aqt 未装时必然发生）——用 try/catch 吞掉再按退出码分支。
try { $AqtCheck = & python -m aqt --version 2>&1 } catch { $AqtCheck = $null }
if ($LASTEXITCODE -eq 0) {
    Write-Host "aqtinstall: already installed"
} else {
    Write-Host "aqtinstall: installing..."
    python -m pip install --user --quiet aqtinstall
}

# --- 2. 下载 Qt ---
Write-Host ""
Write-Host "=== [2/3] download Qt $QtVersion (windows/win64_msvc2022_64) ==="
if (Test-Path "$PlatformDir\$QtVersion\bin\qmake.exe") {
    Write-Host "Qt $QtVersion already present at $PlatformDir  (skip)"
} else {
    New-Item -ItemType Directory -Force -Path $PlatformDir | Out-Null
    # win64_msvc2022_64 对齐 FreeCAD-LibPack v3.5.2 的 MSVC 143
    # 注意：Qt 6.11+ 改用带工具链后缀的仓库布局（qt6_6111_msvc2022_64/
    #       qt.qt6.6111.win64_msvc2022_64/<ver-prefixed>.7z），aqtinstall 3.3.0
    #       尚未支持——失败时按下面"手动安装"提示操作。
    python -m aqt install-qt windows desktop $QtVersion win64_msvc2022_64 `
        --outputdir $PlatformDir
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path "$PlatformDir\$QtVersion\bin\qmake.exe")) {
        Write-Host @"
aqtinstall failed for Qt $QtVersion (likely the 6.11+ repo layout, unsupported by aqt 3.3.0).
手动安装（已验证可行，2026-09-09）：
  1. 浏览 https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6111/qt6_6111_msvc2022_64/qt.qt6.6111.win64_msvc2022_64/
     下载带版本前缀的 7z：qtbase / qtsvg / qttools / d3dcompiler_47-x64 / opengl32sw-...
     （qtdeclarative/qtdoc/qttranslations 可选）
  2. py7zr 解压全部到 third_party/qt/windows/$QtVersion/msvc2022_64/
     python -c "import py7zr; py7zr.SevenZipFile(r'<file>.7z').extractall(r'third_party\qt\windows\\$QtVersion\msvc2022_64')"
  3. 校验 bin/qmake.exe 存在后，configure 时：
     cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
       -DCMAKE_PREFIX_PATH=<repo>/third_party/qt/windows/$QtVersion/msvc2022_64
"@
        Write-Error "aqt install-qt failed (exit $LASTEXITCODE)"
        exit 1
    }
    Write-Host "Qt installed to $PlatformDir\$QtVersion"
}

# --- 3. git submodule ---
Write-Host ""
Write-Host "=== [3/3] init git submodules (GoogleTest) ==="
if (Test-Path "third_party\googletest\CMakeLists.txt") {
    Write-Host "googletest submodule already initialized (skip)"
} else {
    git submodule update --init --recursive third_party/googletest
    Write-Host "googletest initialized"
}

Write-Host ""
Write-Host "=== done ==="
Write-Host "Qt path:    $RepoRoot\$PlatformDir\$QtVersion"
Write-Host "CMake:      cmake -S . -B build -DCMAKE_PREFIX_PATH=`"$RepoRoot\$PlatformDir\$QtVersion`""
