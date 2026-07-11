Set-StrictMode -Version Latest

function Get-Percentile {
    param(
        [Parameter(Mandatory = $true)][double[]]$Values,
        [Parameter(Mandatory = $true)][ValidateRange(0.0, 1.0)][double]$Percentile
    )
    if ($Values.Count -eq 0) { throw 'percentile requires at least one value' }
    $sorted = @($Values | Sort-Object)
    $index = [Math]::Min($sorted.Count - 1, [Math]::Max(0, [Math]::Ceiling($sorted.Count * $Percentile) - 1))
    return [double]$sorted[$index]
}

function Get-Priority5MachineInfo {
    [ordered]@{
        name = [Environment]::MachineName
        os = [Environment]::OSVersion.VersionString
        logical_processors = [Environment]::ProcessorCount
        powershell = $PSVersionTable.PSVersion.ToString()
    }
}

function Assert-ReportField {
    param([object]$Report, [string]$Name)
    if ($null -eq $Report.PSObject.Properties[$Name] -or $null -eq $Report.$Name) {
        throw "priority5 report requires field '$Name'"
    }
}

function Test-Priority5RealtimeReport {
    param(
        [Parameter(Mandatory = $true)][object]$Report,
        [Parameter(Mandatory = $true)][bool]$Hardware
    )
    $required = @(
        'status','scenario','machine','encoding_path','duration_seconds',
        'average_cli_cpu_percent','p95_cli_cpu_percent','peak_working_set_bytes',
        'peak_thread_count','queue_high_watermark','max_progress_gap_ms',
        'dropped_buffers','worker_errors','runtime_errors','stalled_intervals','av_drift_ms'
    )
    foreach ($name in $required) { Assert-ReportField -Report $Report -Name $name }
    if ($Report.status -ne 'pass') { throw 'priority5 report status is not pass' }
    if ($Hardware -and $Report.encoding_path -ne 'cuda-nvenc/h264_nvenc') { throw 'hardware report did not use CUDA/NVENC' }
    if (-not $Hardware -and $Report.encoding_path -ne 'software/libx264') { throw 'software report did not use libx264' }
    $cpuLimit = if ($Hardware) { 5.0 } else { 25.0 }
    if ([double]$Report.average_cli_cpu_percent -gt $cpuLimit) { throw 'average CLI CPU gate failed' }
    if ([double]$Report.av_drift_ms -gt 100.0) { throw 'A/V drift gate failed' }
    if ([double]$Report.max_progress_gap_ms -gt 5000.0) { throw 'progress gap gate failed' }
    foreach ($name in @('dropped_buffers','worker_errors','runtime_errors','stalled_intervals')) {
        if ([uint64]$Report.$name -ne 0) { throw "nonzero runtime failure metric '$name'" }
    }
    return $true
}

function Test-Priority5LocalReport {
    param(
        [Parameter(Mandatory = $true)][object]$Report,
        [Parameter(Mandatory = $true)][bool]$Hardware
    )
    $required = @(
        'status','scenario','machine','encoding_path','duration_seconds','loops',
        'average_cli_cpu_percent','p95_cli_cpu_percent','peak_working_set_bytes',
        'peak_thread_count','queue_high_watermark','maximum_loop_duration_ms',
        'dropped_buffers','worker_errors','runtime_errors','stalled_intervals','maximum_av_drift_ms'
    )
    foreach ($name in $required) { Assert-ReportField -Report $Report -Name $name }
    if ($Report.status -ne 'pass') { throw 'priority5 local report status is not pass' }
    if ($Hardware -and $Report.encoding_path -ne 'cuda-nvenc/h264_nvenc') { throw 'local hardware report did not use CUDA/NVENC' }
    if (-not $Hardware -and $Report.encoding_path -ne 'software/libx264') { throw 'local software report did not use libx264' }
    if ([double]$Report.maximum_av_drift_ms -gt 100.0) { throw 'local A/V drift gate failed' }
    foreach ($name in @('dropped_buffers','worker_errors','runtime_errors','stalled_intervals')) {
        if ([uint64]$Report.$name -ne 0) { throw "nonzero local runtime failure metric '$name'" }
    }
    return $true
}

function Merge-Priority5Reports {
    param([Parameter(Mandatory = $true)][object[]]$Reports)
    if ($Reports.Count -eq 0) { throw 'at least one report is required' }
    [ordered]@{
        status = if (@($Reports | Where-Object status -ne 'pass').Count -eq 0) { 'pass' } else { 'fail' }
        run_count = $Reports.Count
        total_duration_seconds = [double](($Reports | Measure-Object duration_seconds -Sum).Sum)
        total_dropped_buffers = [uint64](($Reports | Measure-Object dropped_buffers -Sum).Sum)
        total_worker_errors = [uint64](($Reports | Measure-Object worker_errors -Sum).Sum)
        total_runtime_errors = [uint64](($Reports | Measure-Object runtime_errors -Sum).Sum)
        total_stalled_intervals = [uint64](($Reports | Measure-Object stalled_intervals -Sum).Sum)
        maximum_av_drift_ms = [double](($Reports | Measure-Object av_drift_ms -Maximum).Maximum)
        maximum_progress_gap_ms = [double](($Reports | Measure-Object max_progress_gap_ms -Maximum).Maximum)
        peak_working_set_bytes = [uint64](($Reports | Measure-Object peak_working_set_bytes -Maximum).Maximum)
        peak_thread_count = [uint64](($Reports | Measure-Object peak_thread_count -Maximum).Maximum)
        queue_high_watermark = [uint64](($Reports | Measure-Object queue_high_watermark -Maximum).Maximum)
    }
}

Export-ModuleMember -Function Get-Percentile,Get-Priority5MachineInfo,Test-Priority5RealtimeReport,Test-Priority5LocalReport,Merge-Priority5Reports
