param(
  [Parameter(Mandatory=$true)][string]$Java,
  [Parameter(Mandatory=$true)][string]$Classes,
  [Parameter(Mandatory=$true)][string]$DetourJar,
  [Parameter(Mandatory=$true)][string]$Mesh,
  [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $OutputDirectory -ErrorAction Stop | Out-Null
$progress = Join-Path $OutputDirectory 'progress.json'
$result = Join-Path $OutputDirectory 'result.json'
# Worst-case reader/antivirus contention: progress is expendable, search is not.
$reader = [IO.File]::Open($progress, [IO.FileMode]::CreateNew, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
  & $Java "-Dl2mapconv.progress=$progress" -cp "$Classes;$DetourJar" NavmeshRouteRunner --mesh $Mesh --request-id contention --case-id floor --from '-1920,-1920,1' --to '-128,-128,1' --snap-horizontal 8 --snap-vertical 32 --warmup 3 --iterations 2 --output $result
  if ($LASTEXITCODE -ne 0) { throw 'Progress reader contention aborted the benchmark' }
} finally { $reader.Dispose() }
$r = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
if ($r.status -ne 'reached' -or $r.metrics.benchmark_samples.Count -ne 2) { throw 'Progress contention lost measured results' }
Write-Output 'PASS progress contention cannot fail the measured series'
