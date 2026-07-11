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
        $gate = $output | Select-Object -Last 1 | ConvertFrom-Json
        $report = [pscustomobject][ordered]@{
            status = $gate.status
            scenario = $item.name
            mode = 'local'
            hardware = [bool]$item.hardware
            machine = Get-Priority5MachineInfo
            encoding_path = if ($item.hardware) { 'cuda-nvenc/h264_nvenc' } else { 'software/libx264' }
            duration_seconds = [double]$gate.elapsed_seconds
            loops = [int]$gate.loops
            output = $gate.output
        }
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
}

$realtimeReports = @($reports | Where-Object mode -ne 'local')
$result = [ordered]@{
    schema_version = 1
    generated_utc = [DateTime]::UtcNow.ToString('o')
    git_commit = (& git -C $repoRoot rev-parse HEAD).Trim()
    machine = Get-Priority5MachineInfo
    scenarios = @($reports)
    realtime_summary = if ($realtimeReports.Count -gt 0) { Merge-Priority5Reports -Reports $realtimeReports } else { $null }
}

if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $reportDirectory = Join-Path $repoRoot $config.report_directory
    New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
    $ReportPath = Join-Path $reportDirectory ("priority5-{0}.json" -f [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
}
$resolvedReportPath = if ([IO.Path]::IsPathRooted($ReportPath)) {
    [IO.Path]::GetFullPath($ReportPath)
} else {
    [IO.Path]::GetFullPath((Join-Path $repoRoot $ReportPath))
}
New-Item -ItemType Directory -Path (Split-Path -Parent $resolvedReportPath) -Force | Out-Null
$json = $result | ConvertTo-Json -Depth 8
[IO.File]::WriteAllText($resolvedReportPath, $json, [Text.UTF8Encoding]::new($false))
[ordered]@{ status='pass'; report=$resolvedReportPath; scenario_count=$reports.Count } | ConvertTo-Json -Compress
