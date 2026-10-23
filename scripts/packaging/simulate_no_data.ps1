# simulate_no_data.ps1 - P-4 gate: temporarily rename the five big-model dump
# roots to *.aside, run ctest (dump tests must SKIP not FAIL), then restore.
# try/finally guarantees restoration. PURE ASCII ONLY.
# Usage: powershell -ExecutionPolicy Bypass -File scripts/packaging/simulate_no_data.ps1 [-CtestArgs "..."]
param(
    [string]$CtestArgs = "--output-on-failure"
)
$ErrorActionPreference = "Stop"
$rpc = "D:\Github\DanQing\third_party\tile-sample-assets\rpc-dumps"
$roots = @(
    "joeshouse-v1","joeshouse-drill-v1","joeshouse-drill-v2","joeshouse-mag-missing-v1",
    "housemodel-v1","housemodel-drill-v1",
    "baytown-v1","baytown-drill-v1",
    "bridge-edit-v1","bridge-edit-drill-v1","bridge-edit-sweep-v1",
    "bridge-edit-mag-missing-v1","bridge-edit-mag-missing-v2","bridge-edit-mag-missing-v3","bridge-edit-mag-missing-v4",
    "bridge-edit-coverage-v1",
    "instances60-v1","instances60-imodel-v1","instances60-drill-v1","instances60-mag-missing-v1","instances60-placements-v1"
)
$renamed = @()
try {
    foreach ($r in $roots) {
        $src = Join-Path $rpc $r
        if (Test-Path $src) {
            Rename-Item $src ($r + ".aside")
            $renamed += $r
        }
    }
    Write-Output ("renamed aside: " + $renamed.Count + " roots -- running ctest...")
    & ctest --test-dir D:\Github\DanQing\build -C Debug ($CtestArgs -split " ")
    $script:ctestExit = $LASTEXITCODE
    Write-Output ("ctest exit = " + $ctestExit)
} finally {
    foreach ($r in $renamed) {
        $aside = Join-Path $rpc ($r + ".aside")
        if (Test-Path $aside) { Rename-Item $aside $r }
    }
    Write-Output ("restored: " + $renamed.Count + " roots")
}
exit $ctestExit
