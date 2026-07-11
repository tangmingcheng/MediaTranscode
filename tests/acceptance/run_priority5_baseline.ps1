param(
    [string]$ConfigPath = (Join-Path $PSScriptRoot 'priority5_scenarios.json'),
    [string[]]$Scenario,
    [int]$DurationSeconds,
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Priority5Acceptance.psm1') -Force

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$config = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json
if ($config.schema_version -ne 1) { throw "unsupported priority5 scenario schema: $($config.schema_version)" }
$executableDirectory = Join-Path $repoRoot $config.executable_directory
$inputPath = Join-Path $repoRoot $config.sample
if (-not (Test-Path $inputPath)) { throw "priority5 sample is missing: $inputPath" }
if (-not (Test-Path (Join-Path $executableDirectory 'media_transcode_realtime_video_cli.exe'))) { throw "priority5 executables are missing: $executableDirectory" }

$selected = @($config.scenarios)
if ($Scenario.Count -gt 0) {
    $selected = @($selected | Where-Object { $Scenario -contains $_.name })
    foreach ($name in $Scenario) {
        if ($selected.name -notcontains $name) { throw "unknown priority5 scenario: $name" }
    }
}
if ($selected.Count -eq 0) { throw 'priority5 requires at least one selected scenario' }

if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $reportDirectory = Join-Path $repoRoot $config.report_directory
    $ReportPath = Join-Path $reportDirectory ("priority5-{0}.json" -f [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
}
$resolvedReportPath = if ([IO.Path]::IsPathRooted($ReportPath)) {
    [IO.Path]::GetFullPath($ReportPath)
} else {
    [IO.Path]::GetFullPath((Join-Path $repoRoot $ReportPath))
}
$resolvedReportDirectory = Split-Path -Parent $resolvedReportPath
New-Item -ItemType Directory -Path $resolvedReportDirectory -Force | Out-Null

function Write-Priority5Report([object]$Value) {
    $temporaryPath = Join-Path $resolvedReportDirectory ((Split-Path -Leaf $resolvedReportPath) + '.tmp')
    $json = $Value | ConvertTo-Json -Depth 8
    [IO.File]::WriteAllText($temporaryPath, $json, [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporaryPath -Destination $resolvedReportPath -Force
}

$reports = [Collections.Generic.List[object]]::new()
foreach ($item in $selected) {
    $duration = if ($PSBoundParameters.ContainsKey('DurationSeconds')) { $DurationSeconds } else { [int]$item.stability_seconds }
    if ($duration -le 0) { throw "scenario '$($item.name)' requires positive duration" }
    if ($item.mode -eq 'local') {
        $arguments = @(
            '-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'run_priority4_local_gate.ps1'),
            '-ExecutableDirectory',$executableDirectory,'-InputPath',$inputPath,'-DurationSeconds',"$duration"
        )
        if (-not $item.hardware) { $arguments += '-DisableHw' }
        $output = & powershell.exe @arguments
        if ($LASTEXITCODE -ne 0) { throw "priority5 scenario failed: $($item.name)" }
        $report = $output | Select-Object -Last 1 | ConvertFrom-Json
        Test-Priority5LocalReport -Report $report -Hardware ([bool]$item.hardware) | Out-Null
    } else {
        $arguments = @(
            '-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'run_priority4_realtime_gate.ps1'),
            '-Mode',$item.mode,'-ExecutableDirectory',$executableDirectory,'-InputPath',$inputPath,
            '-InputPort',"$($item.input_port)",'-OutputPort',"$($item.output_port)",'-DurationSeconds',"$duration"
        )
        if (-not $item.hardware) { $arguments += '-DisableHw' }
        $output = & powershell.exe @arguments
        if ($LASTEXITCODE -ne 0) { throw "priority5 scenario failed: $($item.name)" }
        $report = $output | Select-Object -Last 1 | ConvertFrom-Json
        Test-Priority5RealtimeReport -Report $report -Hardware ([bool]$item.hardware) | Out-Null
    }
    $reports.Add($report)
    Write-Priority5Report ([ordered]@{
        schema_version = 1
        status = 'running'
        generated_utc = [DateTime]::UtcNow.ToString('o')
        git_commit = (& git -C $repoRoot rev-parse HEAD).Trim()
        machine = Get-Priority5MachineInfo
        completed_scenarios = $reports.Count
        requested_scenarios = $selected.Count
        scenarios = @($reports)
    })
}

$realtimeReports = @($reports | Where-Object mode -ne 'local')
$result = [ordered]@{
    schema_version = 1
    status = 'pass'
    generated_utc = [DateTime]::UtcNow.ToString('o')
    git_commit = (& git -C $repoRoot rev-parse HEAD).Trim()
    machine = Get-Priority5MachineInfo
    scenarios = @($reports)
    realtime_summary = if ($realtimeReports.Count -gt 0) { Merge-Priority5Reports -Reports $realtimeReports } else { $null }
}

Write-Priority5Report $result
[ordered]@{ status='pass'; report=$resolvedReportPath; scenario_count=$reports.Count } | ConvertTo-Json -Compress
