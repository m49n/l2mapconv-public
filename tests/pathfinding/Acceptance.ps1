[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$Exe,
  [Parameter(Mandatory=$true)][string]$Case,
  [Parameter(Mandatory=$true)][string]$Profile,
  [Parameter(Mandatory=$true)][string]$Output,
  [string]$ExpectedReport
)
$ErrorActionPreference='Stop'
$exePath=(Resolve-Path -LiteralPath $Exe).Path
$casePath=(Resolve-Path -LiteralPath $Case).Path
$profilePath=(Resolve-Path -LiteralPath $Profile).Path
$root=[IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath $root) {throw 'Acceptance output must be new'}
[IO.Directory]::CreateDirectory($root) | Out-Null
function Write-NewJson($Path,$Value) {
  $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew)
  try {$bytes=[Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Depth 64));$stream.Write($bytes,0,$bytes.Length)} finally {$stream.Dispose()}
}
function Run-Case($Name,$CaseFile,$ProfileFile) {
  $directory=Join-Path $root $Name
  & $exePath --pathfinding-case $CaseFile --backend-profile $ProfileFile --output $directory
  if($LASTEXITCODE -ne 0) {throw "$Name failed: $LASTEXITCODE"}
  $report=Get-Content -LiteralPath (Join-Path $directory 'report.json') -Raw | ConvertFrom-Json
  if($report.results.Count -ne 2) {throw 'Missing backend result'}
  Write-Host "$Name : $($report.results[0].status) / $($report.results[1].status)"
  return $report
}
$forward=Run-Case 'forward' $casePath $profilePath
if($ExpectedReport) {
  $expected=Get-Content -LiteralPath $ExpectedReport -Raw | ConvertFrom-Json
  foreach($backend in @('l2j','navmesh')) {
    $before=@($expected.results | Where-Object backend -eq $backend)[0]
    $after=@($forward.results | Where-Object backend -eq $backend)[0]
    foreach($field in @('status','a','b','final_path','validated_samples')) {
      if(($before.$field | ConvertTo-Json -Depth 64 -Compress) -ne ($after.$field | ConvertTo-Json -Depth 64 -Compress)) {throw "UI/CLI replay differs for $backend $field"}
    }
  }
  Write-Host 'PASS saved UI case replays identical statuses/endpoints/routes'
}
$baseline=Get-Content -LiteralPath $casePath -Raw | ConvertFrom-Json
$hashes=@{}
foreach($result in $forward.results) {$hashes[$result.backend]=$result.identity.backend_manifest.sha256}
$baseline | Add-Member -NotePropertyName expected_backends -NotePropertyValue $hashes -Force
$baselinePath=Join-Path $root 'baseline.json';Write-NewJson $baselinePath $baseline
$same=Run-Case 'same-backends' $baselinePath $profilePath
if(($same.results.status -join ',') -ne ($forward.results.status -join ',')) {throw 'Matching baseline changed query outcome'}
$changedProfile=Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
$changedConfig=Join-Path $root 'changed-config.json'
$configStream=[IO.File]::Open($changedConfig,[IO.FileMode]::CreateNew)
try {
  $original=[IO.File]::ReadAllBytes($changedProfile.legacy_config)
  $configStream.Write($original,0,$original.Length)
  $comment=[Text.UTF8Encoding]::new($false).GetBytes("`n# backend baseline acceptance fixture`n")
  $configStream.Write($comment,0,$comment.Length)
} finally {$configStream.Dispose()}
$changedProfile.legacy_config=$changedConfig
$changedProfilePath=Join-Path $root 'changed-profile.json';Write-NewJson $changedProfilePath $changedProfile
$changed=Run-Case 'changed-backend' $baselinePath $changedProfilePath
if($changed.results[0].status -ne 'invalid_data' -or $changed.results[0].diagnostic -notmatch 'Backend identity changed' -or (Test-Path -LiteralPath (Join-Path $root 'changed-backend/l2j.log'))) {throw 'Changed backend was not rejected before Java execution'}
if($changed.results[1].status -ne $forward.results[1].status) {throw 'Changed legacy backend lost independent Navmesh result'}
$reverse=Get-Content -LiteralPath $casePath -Raw | ConvertFrom-Json
$point=$reverse.a;$reverse.a=$reverse.b;$reverse.b=$point
$reverse.case_id+='-reverse'
$reversePath=Join-Path $root 'reverse.json';Write-NewJson $reversePath $reverse
$back=Run-Case 'reverse' $reversePath $profilePath
$partial=Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
$partial.legacy_classpath=@();$partial.legacy_config=''
$partialPath=Join-Path $root 'one-backend.json';Write-NewJson $partialPath $partial
$one=Run-Case 'one-backend' $casePath $partialPath
if($one.results[0].status -ne 'backend_unavailable' -or $one.results[1].status -in @('failed','backend_unavailable')) {throw 'One backend failure lost the other result'}
$cancelDirectory=Join-Path $root 'cancelled'
$args='--pathfinding-case "'+$casePath+'" --backend-profile "'+$profilePath+'" --output "'+$cancelDirectory+'"'
$child=Start-Process -FilePath $exePath -ArgumentList $args -WindowStyle Hidden -PassThru
for($i=0;$i -lt 100 -and !(Test-Path -LiteralPath (Join-Path $cancelDirectory 'started.json'));$i++) {Start-Sleep -Milliseconds 25}
if(!(Test-Path -LiteralPath (Join-Path $cancelDirectory 'started.json'))) {throw 'Worker did not claim cancellation fixture'}
$marker=[IO.File]::Open((Join-Path $cancelDirectory 'cancel.request'),[IO.FileMode]::CreateNew);$marker.Dispose()
if(!$child.WaitForExit(30000)) {throw 'Owned worker failed to acknowledge cancellation within 30 seconds'}
if($child.ExitCode -ne 130) {throw "Cancellation exit was $($child.ExitCode), expected 130"}
$cancel=Get-Content -LiteralPath (Join-Path $cancelDirectory 'report.json') -Raw | ConvertFrom-Json
if(!($cancel.results.status -contains 'cancelled')) {throw 'Missing cancellation status'}
Write-Output 'PASS real forward/reverse, backend baseline/rejection, independent backend failure and cancellation'
