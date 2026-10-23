# build_app_package.ps1 - P-4: stage + zip the prebuilt DisplayTestApp demo
# package (bin/ + assets/ + data/ placeholder + run.cmd + docs).
# PURE ASCII ONLY (PS 5.1 ANSI read).
param(
    [string]$Version = "v0.1.0",
    [string]$Repo = "D:\Github\DanQing",
    [string]$BuildOut = "D:\Github\DanQing\build\samples\DisplayTestApp\Release",
    [string]$OutDir = "D:\Github\DanQing\build\p4-app-package"
)
$ErrorActionPreference = "Stop"
$pkgName = "DanQing-DisplayTestApp-$Version-win64"
$stage = Join-Path $OutDir $pkgName
$qtBin = "D:\Github\DanQing\third_party\qt\windows\6.11.1\msvc2022_64\bin"
$tar = "C:\Windows\System32\tar.exe"

if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force "$stage\bin" | Out-Null
New-Item -ItemType Directory -Force "$stage\assets" | Out-Null
New-Item -ItemType Directory -Force "$stage\data" | Out-Null

# 1) exe + Qt deployment (fresh windeployqt into the staging bin)
Copy-Item (Join-Path $BuildOut "DisplayTestApp.exe") "$stage\bin\"
& (Join-Path $qtBin "windeployqt.exe") --release --no-translations "$stage\bin\DisplayTestApp.exe" | Out-Null
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }
# 1a) explicit asserts: windeployqt has missed these before (plan risk register)
foreach ($dll in @("Qt6OpenGL.dll","Qt6OpenGLWidgets.dll","Qt6PrintSupport.dll")) {
    if (-not (Test-Path "$stage\bin\$dll")) {
        $src = Join-Path $qtBin $dll
        if (-not (Test-Path $src)) { throw "missing $dll in staging AND Qt bin" }
        Copy-Item $src "$stage\bin\"
        Write-Output "  explicit copy: $dll (windeployqt missed)"
    }
}
# 1b) VC143 CRT (redistributable per VS license)
$crtRoot = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022" -Recurse -Directory -Filter "Microsoft.VC143.CRT" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match "x64" } | Select-Object -First 1
if (-not $crtRoot) { throw "VC143 CRT redist dir not found" }
Copy-Item (Join-Path $crtRoot.FullName "*.dll") "$stage\bin\"
Write-Output ("  CRT dlls: " + (Get-ChildItem "$stage\bin\vcruntime*","$stage\bin\msvcp*" -ErrorAction SilentlyContinue).Count)

# 2) runtime assets (env-overridable dirs, P-4c)
Copy-Item -Recurse "$Repo\third_party\itwinjs-sprites" "$stage\assets\itwinjs-sprites"
Copy-Item -Recurse "$Repo\third_party\itwinjs-cursors" "$stage\assets\itwinjs-cursors"
Copy-Item -Recurse "$Repo\samples\DisplayTestApp\assets" "$stage\assets\dta"

# 3) run.cmd + docs (README text lives in package-README.txt next to this
#    script -- PS 5.1 reads .ps1 as ANSI, so no CJK literals in this file)
$runCmd = @"
@echo off
set ROOT=%~dp0
set DANQING_RPC_DUMP=%ROOT%data\rpc-dumps
set DANQING_SPRITE_ASSETS_DIR=%ROOT%assets\itwinjs-sprites
set DANQING_CURSOR_ASSETS_DIR=%ROOT%assets\itwinjs-cursors
set DANQING_DTA_ASSETS_DIR=%ROOT%assets\dta
start "" "%ROOT%bin\DisplayTestApp.exe"
"@
[System.IO.File]::WriteAllText("$stage\run.cmd", $runCmd, [System.Text.Encoding]::ASCII)

Copy-Item "$Repo\scripts\packaging\package-README.txt" "$stage\README.txt"
[System.IO.File]::WriteAllText("$stage\data\README.txt",
    "Put downloaded rpc-dumps model packages here (see ..\README.txt).`r`n",
    (New-Object System.Text.UTF8Encoding($false)))
Copy-Item "$Repo\LICENSE" "$stage\"
Copy-Item "$Repo\NOTICE" "$stage\"

# 4) zip (bsdtar; entry prefix = package folder)
$zip = Join-Path $OutDir ($pkgName + ".zip")
if (Test-Path $zip) { Remove-Item $zip -Force }
& $tar -a -cf $zip -C $OutDir $pkgName
if ($LASTEXITCODE -ne 0) { throw "tar zip failed" }
$fi = Get-Item $zip
Write-Output ("PACKAGE " + $zip + "  " + [math]::Round($fi.Length/1MB,1) + " MB")
