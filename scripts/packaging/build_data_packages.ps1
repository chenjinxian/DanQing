# build_data_packages.ps1 - P-4: package the five big-model dump roots into
# release-asset zips under build/p4-data-packages/ (target repo: DanQing-Data,
# release data-v1). Emits scripts/data/dump_packages.json + build/p4-data-packages/checksums.md.
# PURE ASCII ONLY (PS 5.1 ANSI read - project lesson).
param(
    [string]$RpcDumps = "D:\Github\DanQing\third_party\tile-sample-assets\rpc-dumps",
    [string]$OutDir = "D:\Github\DanQing\build\p4-data-packages",
    [double]$PartCapMB = 1800
)
$ErrorActionPreference = "Stop"
$tar = "C:\Windows\System32\tar.exe"   # bsdtar (real zip output; GNU tar -a makes plain tar!)
if (-not (Test-Path $tar)) { throw "bsdtar not found at $tar" }
New-Item -ItemType Directory -Force $OutDir | Out-Null

# model key -> ordered root dirs (zip entries carry the root dir prefix so
# extraction target is rpc-dumps/ itself).
$packages = [ordered]@{
    "DanQing-Data-v1-joeshouse.zip" = @{
        model = "joeshouse"
        roots = @("joeshouse-v1","joeshouse-drill-v1","joeshouse-drill-v2","joeshouse-mag-missing-v1")
    }
    "DanQing-Data-v1-housemodel.zip" = @{
        model = "housemodel"
        roots = @("housemodel-v1","housemodel-drill-v1")
    }
    "DanQing-Data-v1-baytown.zip" = @{
        model = "baytown"
        roots = @("baytown-v1","baytown-drill-v1")
    }
    "DanQing-Data-v1-instances60.zip" = @{
        model = "instances60"
        roots = @("instances60-v1","instances60-imodel-v1","instances60-drill-v1","instances60-mag-missing-v1","instances60-placements-v1")
    }
    "DanQing-Data-v1-bridge-edit-base.zip" = @{
        model = "bridge-edit"
        roots = @("bridge-edit-v1","bridge-edit-drill-v1","bridge-edit-mag-missing-v1","bridge-edit-mag-missing-v2","bridge-edit-mag-missing-v3","bridge-edit-mag-missing-v4","bridge-edit-coverage-v1")
    }
}

$manifest = [ordered]@{
    release = "data-v1"
    repo = "chenjinxian/DanQing-Data"
    baseUrl = "https://github.com/chenjinxian/DanQing-Data/releases/download/data-v1"
    models = [ordered]@{}
}
$checksumLines = @("# data-v1 sha256 checksums", "")

function Add-ZipPackage([string]$name, [string[]]$relEntries) {
    # relEntries: paths relative to $RpcDumps, forward slashes.
    # NOTE: Write-Host only inside functions -- Write-Output pollutes the return
    # value (PS 5.1 gotcha; poisoned dump_packages.json on 2026-10-23).
    $listFile = Join-Path $env:TEMP ("p4-list-" + [Guid]::NewGuid().ToString("N") + ".txt")
    [System.IO.File]::WriteAllLines($listFile, $relEntries, (New-Object System.Text.UTF8Encoding($false)))
    $out = Join-Path $OutDir $name
    if (Test-Path $out) { Remove-Item $out -Force }
    Write-Host ("PACK " + $name + " (" + $relEntries.Count + " entries)...")
    & $tar -a -cf $out -C $RpcDumps -T $listFile
    if ($LASTEXITCODE -ne 0) { Remove-Item $listFile -Force; throw "tar failed for $name" }
    Remove-Item $listFile -Force
    $fi = Get-Item $out
    $hash = (Get-FileHash -Algorithm SHA256 $out).Hash.ToLower()
    Write-Host ("  -> " + [math]::Round($fi.Length / 1MB, 1) + " MB sha256=" + $hash.Substring(0,16) + "...")
    return @{ name = $name; sha256 = $hash; sizeBytes = $fi.Length }
}

foreach ($pkgName in $packages.Keys) {
    $p = $packages[$pkgName]
    $entries = New-Object System.Collections.Generic.List[string]
    foreach ($root in $p.roots) {
        $rootPath = Join-Path $RpcDumps $root
        if (-not (Test-Path $rootPath)) { throw "missing root: $rootPath" }
        Get-ChildItem -Recurse -File $rootPath | ForEach-Object {
            $rel = $_.FullName.Substring($RpcDumps.Length + 1).Replace("\", "/")
            $entries.Add($rel)
        }
    }
    $a = Add-ZipPackage $pkgName $entries.ToArray()
    if (-not $manifest.models.Contains($p.model)) {
        $manifest.models[$p.model] = @{ assets = (New-Object System.Collections.Generic.List[object]); roots = $p.roots }
    }
    $manifest.models[$p.model].assets.Add($a)
    $checksumLines += ("``" + $a.name + "``  " + $a.sha256 + "  (" + [math]::Round($a.sizeBytes / 1MB, 1) + " MB)")
}

# bridge-edit sweep split into plain zip parts (NOT split-format volumes):
# entries keep the bridge-edit-sweep-v1/ prefix; parts extract into the same dir.
$sweep = "bridge-edit-sweep-v1"
$sweepPath = Join-Path $RpcDumps $sweep
$capBytes = [int64]($PartCapMB * 1MB)
$parts = New-Object System.Collections.Generic.List[object]
$cur = New-Object System.Collections.Generic.List[string]
$curSize = [int64]0
# part 1 also carries the root metadata files
foreach ($meta in @("manifest.json","imodel.json","README.md")) {
    $mp = Join-Path $sweepPath $meta
    if (Test-Path $mp) { $cur.Add($sweep + "/" + $meta); $curSize += (Get-Item $mp).Length }
}
Get-ChildItem -File (Join-Path $sweepPath "files") | Sort-Object Name | ForEach-Object {
    if ($curSize -ge $capBytes) {
        $parts.Add($cur); $cur = New-Object System.Collections.Generic.List[string]; $curSize = 0
    }
    $cur.Add($sweep + "/files/" + $_.Name); $curSize += $_.Length
}
if ($cur.Count -gt 0) { $parts.Add($cur) }
Write-Output ("sweep split into " + $parts.Count + " parts")
if ($parts.Count -gt 9) { throw "unexpected part count (cap too small?)" }
$bridgeRoots = @($packages["DanQing-Data-v1-bridge-edit-base.zip"].roots) + @("bridge-edit-sweep-v1")
for ($i = 0; $i -lt $parts.Count; ++$i) {
    $pn = "DanQing-Data-v1-bridge-edit-sweep-part" + ($i + 1) + "of" + $parts.Count + ".zip"
    $a = Add-ZipPackage $pn $parts[$i].ToArray()
    $manifest.models["bridge-edit"].assets.Add($a)
    $checksumLines += ("``" + $a.name + "``  " + $a.sha256 + "  (" + [math]::Round($a.sizeBytes / 1MB, 1) + " MB)")
}
$manifest.models["bridge-edit"].roots = $bridgeRoots

# emit dump_packages.json (consumed by download_dumps.ps1)
$jsonPath = "D:\Github\DanQing\scripts\data\dump_packages.json"
# PS 5.1 ConvertTo-Json depth + ordered structure
$json = $manifest | ConvertTo-Json -Depth 6
[System.IO.File]::WriteAllText($jsonPath, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("manifest -> " + $jsonPath)

$checksumLines += ""
$checksumLines += ("generated: " + (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ") + "  host: build_data_packages.ps1")
[System.IO.File]::WriteAllLines((Join-Path $OutDir "checksums.md"), $checksumLines, (New-Object System.Text.UTF8Encoding($false)))
Write-Output "DONE"
