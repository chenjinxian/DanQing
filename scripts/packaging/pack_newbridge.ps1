# pack_newbridge.ps1 - pack the 8 newbridge roots into release-asset zips
# (<=1.8GB raw per zip; release asset cap is 2GB). Emits
# build/p4-data-packages/newbridge/ plus checksums-newbridge.md.
# PURE ASCII ONLY (PS 5.1 ANSI read).
param(
    [string]$RpcDumps = "D:\Github\danqing-rpc-tools\dumps",
    [string]$OutDir = "D:\Github\DanQing\build\p4-data-packages\newbridge",
    [double]$PartCapMB = 1800
)
$ErrorActionPreference = "Stop"
$tar = "C:\Windows\System32\tar.exe"   # bsdtar (real zip output; GNU tar -a makes plain tar!)
if (-not (Test-Path $tar)) { throw "bsdtar not found at $tar" }
New-Item -ItemType Directory -Force $OutDir | Out-Null

# ordered root list; multi-part roots extract into the same dir (entries carry
# the root-dir prefix, same convention as the other packages).
$roots = @(
    "newbridge-v1",
    "newbridge-sweep-v1",
    "newbridge-d9-v1",
    "newbridge-d9b-v1",
    "newbridge-d10-v1",
    "newbridge-d11-v1",
    "newbridge-d12-v1",
    "newbridge-d13-v1"
)
$capBytes = [int64]($PartCapMB * 1MB)

foreach ($root in $roots) {
    $rootPath = Join-Path $RpcDumps $root
    if (-not (Test-Path $rootPath)) { throw "missing root: $rootPath" }
    # greedy fill: metadata files first, then files/ sorted by name
    $items = New-Object System.Collections.Generic.List[object]
    foreach ($meta in @("manifest.json","imodel.json","README.md")) {
        $mp = Join-Path $rootPath $meta
        if (Test-Path $mp) { $items.Add(@{ rel = $root + "/" + $meta; size = (Get-Item $mp).Length }) }
    }
    $filesDir = Join-Path $rootPath "files"
    if (Test-Path $filesDir) {
        Get-ChildItem -File $filesDir | Sort-Object Name | ForEach-Object {
            $items.Add(@{ rel = $root + "/files/" + $_.Name; size = $_.Length })
        }
    }
    # greedy split
    $parts = New-Object System.Collections.Generic.List[object]
    $cur = New-Object System.Collections.Generic.List[string]
    $curSize = [int64]0
    foreach ($it in $items) {
        if ($curSize -ge $capBytes -and $cur.Count -gt 0) {
            $parts.Add($cur); $cur = New-Object System.Collections.Generic.List[string]; $curSize = 0
        }
        $cur.Add($it.rel); $curSize += $it.size
    }
    if ($cur.Count -gt 0) { $parts.Add($cur) }

    $total = $parts.Count
    for ($i = 0; $i -lt $total; ++$i) {
        if ($total -eq 1) { $name = "DanQing-Data-v1-$root.zip" }
        else { $name = "DanQing-Data-v1-$root-part" + ($i + 1) + "of$total.zip" }
        $listFile = Join-Path $env:TEMP ("nb-list-" + [Guid]::NewGuid().ToString("N") + ".txt")
        [System.IO.File]::WriteAllLines($listFile, $parts[$i].ToArray(), (New-Object System.Text.UTF8Encoding($false)))
        $out = Join-Path $OutDir $name
        if (Test-Path $out) { Remove-Item $out -Force }
        Write-Host ("PACK " + $name + " (" + $parts[$i].Count + " entries)...")
        & $tar -a -cf $out -C $RpcDumps -T $listFile
        if ($LASTEXITCODE -ne 0) { Remove-Item $listFile -Force; throw "tar failed for $name" }
        Remove-Item $listFile -Force
        $fi = Get-Item $out
        $hash = (Get-FileHash -Algorithm SHA256 $out).Hash.ToLower()
        Write-Host ("  -> " + [math]::Round($fi.Length / 1MB, 1) + " MB sha256=" + $hash.Substring(0, 16) + "...")
        Add-Content -Path (Join-Path $OutDir "checksums-newbridge.md") `
            -Value ("``" + $name + "``  " + $hash + "  (" + [math]::Round($fi.Length / 1MB, 1) + " MB)")
    }
    Write-Host ("ROOT DONE " + $root + ": " + $total + " zip(s)")
}
Write-Output "ALL-DONE"
