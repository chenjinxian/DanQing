# download_dumps.ps1 - P-4: fetch the externalized demo-model dump packages
# from the DanQing-Data repo release (data-v1), sha256-verify and extract into
# third_party/tile-sample-assets/rpc-dumps/. Idempotent; -Force re-downloads.
# PURE ASCII ONLY (PS 5.1 ANSI read).
# Usage: powershell -ExecutionPolicy Bypass -File scripts/data/download_dumps.ps1 [-Model instances60]
param(
    [string]$Model = "all",          # all | joeshouse | housemodel | baytown | bridge-edit | instances60
    [string]$DestDir = "",           # default <repo>/third_party/tile-sample-assets/rpc-dumps
    [string]$CacheDir = "",          # default <repo>/build/p4-download-cache
    [switch]$Force
)
$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ($DestDir -eq "") { $DestDir = Join-Path $repoRoot "third_party\tile-sample-assets\rpc-dumps" }
if ($CacheDir -eq "") { $CacheDir = Join-Path $repoRoot "build\p4-download-cache" }
New-Item -ItemType Directory -Force $DestDir | Out-Null
New-Item -ItemType Directory -Force $CacheDir | Out-Null
$tar = "C:\Windows\System32\tar.exe"

$pkg = Get-Content (Join-Path $PSScriptRoot "dump_packages.json") -Raw | ConvertFrom-Json
$models = @($pkg.models.PSObject.Properties.Name)
if ($Model -ne "all") {
    if ($models -notcontains $Model) { throw "unknown model '$Model' (have: $($models -join ', '))" }
    $models = @($Model)
}

function Get-Asset([string]$url, [string]$outFile) {
    if ((Test-Path $outFile) -and -not $Force) {
        Write-Output "  cached: $(Split-Path $outFile -Leaf)"; return
    }
    Write-Output "  download: $url"
    # Invoke-WebRequest first: Start-BitsTransfer stalls on GitHub release
    # assets (S3 redirect; job hangs silently - 2026-10-23 fresh-clone
    # dogfood). IWR is slower per-byte but reliable; BITS kept as fallback.
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $url -OutFile $outFile -UseBasicParsing -ErrorAction Stop
    } catch {
        Write-Output "  IWR failed, fallback Start-BitsTransfer..."
        Start-BitsTransfer -Source $url -Destination $outFile -ErrorAction Stop
    }
}

$totalAssets = 0; $doneAssets = 0
foreach ($m in $models) {
    $entry = $pkg.models.$m
    $roots = @($entry.roots)
    # idempotency: every root manifest present -> skip whole model
    $allPresent = $true
    foreach ($r in $roots) {
        if (-not (Test-Path (Join-Path $DestDir "$r\manifest.json"))) { $allPresent = $false; break }
    }
    if ($allPresent -and -not $Force) {
        Write-Output "SKIP $m (all $($roots.Count) roots present; -Force to redo)"
        continue
    }
    Write-Output "MODEL $m : $($entry.assets.Count) asset(s) -> $DestDir"
    foreach ($a in $entry.assets) {
        $totalAssets++
        $outFile = Join-Path $CacheDir $a.name
        Get-Asset ($pkg.baseUrl + "/" + $a.name) $outFile
        $hash = (Get-FileHash -Algorithm SHA256 $outFile).Hash.ToLower()
        if ($hash -ne $a.sha256) { throw "sha256 mismatch: $($a.name)`n  got $hash`n  want $($a.sha256)" }
        Write-Output "  sha256 ok ($([math]::Round($a.sizeBytes/1MB,1)) MB)"
        Write-Output "  extract..."
        if (Test-Path $tar) {
            & $tar -xf $outFile -C $DestDir
            if ($LASTEXITCODE -ne 0) { throw "tar extract failed: $($a.name)" }
        } else {
            Expand-Archive -Path $outFile -DestinationPath $DestDir -Force
        }
        $doneAssets++
    }
    # post-check: every root manifest must exist now
    foreach ($r in $roots) {
        if (-not (Test-Path (Join-Path $DestDir "$r\manifest.json"))) {
            throw "post-check failed: $r\manifest.json missing after extract"
        }
    }
    Write-Output "OK $m ($($roots.Count) roots verified)"
}
Write-Output "DONE ($doneAssets/$totalAssets assets processed)"
