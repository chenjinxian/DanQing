# regen_dump_packages_json.ps1 - rebuild dump_packages.json + checksums.md from
# the EXISTING zips in build/p4-data-packages (the zip content is unchanged; only
# the manifest was poisoned by the Write-Output-in-function PS gotcha).
# PURE ASCII ONLY.
$ErrorActionPreference = "Stop"
$pkgDir = "D:\Github\DanQing\build\p4-data-packages"

$models = [ordered]@{
    "joeshouse" = @{
        assets = @("DanQing-Data-v1-joeshouse.zip")
        roots = @("joeshouse-v1","joeshouse-drill-v1","joeshouse-drill-v2","joeshouse-mag-missing-v1")
    }
    "housemodel" = @{
        assets = @("DanQing-Data-v1-housemodel.zip")
        roots = @("housemodel-v1","housemodel-drill-v1")
    }
    "baytown" = @{
        assets = @("DanQing-Data-v1-baytown.zip")
        roots = @("baytown-v1","baytown-drill-v1")
    }
    "instances60" = @{
        assets = @("DanQing-Data-v1-instances60.zip")
        roots = @("instances60-v1","instances60-imodel-v1","instances60-drill-v1","instances60-mag-missing-v1","instances60-placements-v1")
    }
    "bridge-edit" = @{
        assets = @("DanQing-Data-v1-bridge-edit-base.zip",
                   "DanQing-Data-v1-bridge-edit-sweep-part1of3.zip",
                   "DanQing-Data-v1-bridge-edit-sweep-part2of3.zip",
                   "DanQing-Data-v1-bridge-edit-sweep-part3of3.zip")
        roots = @("bridge-edit-v1","bridge-edit-drill-v1","bridge-edit-mag-missing-v1","bridge-edit-mag-missing-v2","bridge-edit-mag-missing-v3","bridge-edit-mag-missing-v4","bridge-edit-coverage-v1","bridge-edit-sweep-v1")
    }
}

$doc = [ordered]@{
    release = "data-v1"
    repo = "chenjinxian/DanQing-Data"
    baseUrl = "https://github.com/chenjinxian/DanQing-Data/releases/download/data-v1"
    models = [ordered]@{}
}
$checksumLines = @("# data-v1 sha256 checksums", "")
foreach ($m in $models.Keys) {
    $assets = New-Object System.Collections.Generic.List[object]
    foreach ($name in $models[$m].assets) {
        $f = Join-Path $pkgDir $name
        if (-not (Test-Path $f)) { throw "missing zip: $f" }
        $fi = Get-Item $f
        $hash = (Get-FileHash -Algorithm SHA256 $f).Hash.ToLower()
        $assets.Add(@{ name = $name; sha256 = $hash; sizeBytes = $fi.Length })
        $checksumLines += ("``" + $name + "``  " + $hash + "  (" + [math]::Round($fi.Length/1MB,1) + " MB)")
        Write-Host ("HASH " + $name + " " + [math]::Round($fi.Length/1MB,1) + " MB " + $hash.Substring(0,16) + "...")
    }
    $doc.models[$m] = @{ assets = $assets; roots = $models[$m].roots }
}
$json = $doc | ConvertTo-Json -Depth 6
[System.IO.File]::WriteAllText("D:\Github\DanQing\scripts\data\dump_packages.json", $json, (New-Object System.Text.UTF8Encoding($false)))
[System.IO.File]::WriteAllLines((Join-Path $pkgDir "checksums.md"), $checksumLines, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "DONE"
