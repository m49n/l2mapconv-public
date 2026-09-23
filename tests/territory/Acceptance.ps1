param(
    [Parameter(Mandatory=$true)][string]$App,
    [Parameter(Mandatory=$true)][string]$Client,
    [Parameter(Mandatory=$true)][string]$Output,
    [string]$BaselineGeo,
    [switch]$SkipGeo
)
$ErrorActionPreference = 'Stop'
$appPath = (Resolve-Path -LiteralPath $App).Path
$clientPath = (Resolve-Path -LiteralPath $Client).Path
$outputPath = [System.IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $outputPath) { throw 'Acceptance output must be a new directory.' }
$null = New-Item -ItemType Directory -Path $outputPath
$compare = Join-Path (Split-Path -Parent $appPath) 'territory_png_compare.exe'
$testApp = Join-Path (Split-Path -Parent $appPath) 'territory_tests.exe'
$results = @{}
foreach ($resolution in 4096,8192,16384) {
    $jsonText = & $appPath --render-territory --client-root $clientPath --output $outputPath --resolution $resolution -- 22_22 2> (Join-Path $outputPath "render-$resolution.log")
    if ($LASTEXITCODE -ne 0) { throw "Render failed: $resolution" }
    $result = ($jsonText -join "`n") | ConvertFrom-Json
    if ($result.schema_version -ne 1 -or $result.state -notin @('completed','completed_with_warnings')) { throw 'Invalid terminal result' }
    $images = @($result.files | Where-Object { $_ -like '*.png' })
    if ($images.Count -ne 1) { throw 'Expected exactly one image per selected map' }
    $image = Join-Path $result.output_directory $images[0]
    $decoded = (& $compare $image | Out-String) | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or $decoded.width -ne $resolution -or $decoded.height -ne $resolution -or $decoded.channels -ne 3) { throw 'Independent PNG validation failed' }
    $report = Get-Content -LiteralPath $result.report -Raw | ConvertFrom-Json
    $inventory = $report.maps[0].material_inventory
    if (-not $inventory -or $inventory.textures.Count -lt 100 -or @($inventory.materials | Where-Object { $_.state -eq 'supported' -and $_.reference_chain.Count -gt 1 -and $_.surfaces.Count -gt 0 }).Count -eq 0) { throw 'Production material inventory is missing resolved chains and texture identities' }
    if (@($report.issues | Where-Object { $_.kind -eq 'missing_object' -and $_.source -ieq 'T_texture.Texture.G_01' }).Count -ne 0) { throw 'G_01 must resolve its lowercase client export' }
    if (@($inventory.textures | Where-Object { $_.source -ieq 'T_texture.Texture.g_01' -and $_.width -eq 256 -and $_.height -eq 256 -and $_.material_ids.Count -gt 0 }).Count -eq 0) { throw 'Expected the real G_01 texture payload in used materials' }
    $results["$resolution"] = @{ result=$result; decoded=$decoded; image=$image }
    Write-Output "Verified native raster $resolution x $resolution at $image"
}
$offText = & $appPath --render-territory --client-root $clientPath --output $outputPath --resolution 4096 --no-water -- 22_22 2> (Join-Path $outputPath 'water-off.log')
if ($LASTEXITCODE -ne 0) { throw 'Water-off render failed' }
$off = ($offText -join "`n") | ConvertFrom-Json
$waterDelta = (& $compare $results['4096'].image (Join-Path $off.output_directory '22_22_4096.png') | Out-String) | ConvertFrom-Json
if ($LASTEXITCODE -ne 0 -or $waterDelta.delta.changed_pixels -eq 0) { throw 'Confirmed water did not affect image pixels' }
Write-Output ($waterDelta | ConvertTo-Json -Depth 5 -Compress)
& $testApp --gpu-raster (Join-Path $outputPath 'synthetic')
if ($LASTEXITCODE -ne 0) { throw 'Synthetic tiled/color/water checks failed' }
if (-not $SkipGeo) {
    if (-not $BaselineGeo) { throw 'BaselineGeo required unless SkipGeo is set' }
    $geoDirectory = Join-Path $outputPath 'geodata'
    $null = New-Item -ItemType Directory -Path $geoDirectory
    Push-Location -LiteralPath $geoDirectory
    try {
        & $appPath --build --client-root $clientPath -- 24_18 25_19 *> (Join-Path $outputPath 'geodata-build.log')
        if ($LASTEXITCODE -ne 0) { throw 'Geodata regression build failed' }
        foreach ($map in '24_18','25_19') {
            $actual = Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $geoDirectory "output/$map.l2j")
            $expected = Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $BaselineGeo "$map.l2j")
            if ($actual.Hash -ne $expected.Hash) { throw "Geodata regression: $map" }
            Write-Output "Unchanged geodata: $map $($actual.Hash)"
        }
    } finally { Pop-Location }
}
Write-Output 'Acceptance checks passed. UI interaction and client asset identity checks are separate evidence.'
