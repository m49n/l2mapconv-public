param(
  [Parameter(Mandatory=$true)][string]$Java,
  [Parameter(Mandatory=$true)][string]$Classes,
  [Parameter(Mandatory=$true)][string]$DetourJar,
  [Parameter(Mandatory=$true)][string]$Mesh,
  [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
New-Item -ItemType Directory -Path $OutputDirectory -ErrorAction Stop | Out-Null
$result = Join-Path $OutputDirectory 'result.json'
$progress = Join-Path $OutputDirectory 'progress.json'
& $Java "-Dl2mapconv.progress=$progress" '-Dl2mapconv.requestId=test-benchmark' '-Dl2mapconv.backend=navmesh' -cp "$Classes;$DetourJar" NavmeshRouteRunner --mesh $Mesh --request-id test-benchmark --case-id floor --from '-1920,-1920,1' --to '-128,-128,1' --snap-horizontal 8 --snap-vertical 32 --warmup 3 --iterations 2 --output $result
if ($LASTEXITCODE -ne 0) { throw 'Benchmark CLI failed: expected one loaded mesh, three warmups and two measurements' }
$r = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
if ($r.status -ne 'reached' -or $r.metrics.benchmark_samples.Count -ne 2) { throw 'Warmup leaked into measurements or route changed' }
if ($r.metrics.benchmark_warmup -ne 3 -or $r.metrics.mesh_load_count -ne 1) { throw 'Mesh was not loaded exactly once' }
foreach ($s in $r.metrics.benchmark_samples) {
  if ($s.status -ne 'reached' -or $s.search_ns -lt 0 -or $s.total_ns -lt $s.search_ns) { throw 'Bad measured sample' }
}
$p = Get-Content -LiteralPath $progress -Raw | ConvertFrom-Json
if ($p.request_id -ne 'test-benchmark' -or $p.backend -ne 'navmesh' -or $p.phase -ne 'measurement' -or $p.completed -ne 2 -or $p.total -ne 2 -or $p.pid -le 0) { throw 'Final progress is incomplete or not bound to the JVM/request' }
Write-Output 'PASS same-JVM Navmesh warmup, exact measured count, single data load and progress'
