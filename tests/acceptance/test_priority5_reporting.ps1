$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'Priority5Acceptance.psm1') -Force

function Assert-Equal($Expected, $Actual, [string]$Message) {
    if ($Expected -ne $Actual) { throw "$Message expected=$Expected actual=$Actual" }
}

$values = @(9.0, 1.0, 5.0, 3.0, 7.0)
Assert-Equal 9.0 (Get-Percentile -Values $values -Percentile 0.95) 'P95'

$valid = [pscustomobject][ordered]@{
    status = 'pass'
    scenario = 'rtp_hw'
    machine = [ordered]@{ logical_processors = 16; os = 'Windows' }
    encoding_path = 'cuda-nvenc/h264_nvenc'
    duration_seconds = 60
    average_cli_cpu_percent = 2.0
    p95_cli_cpu_percent = 4.0
    peak_working_set_bytes = 1048576
    peak_thread_count = 20
    queue_high_watermark = 4
    max_progress_gap_ms = 1000
    dropped_buffers = 0
    worker_errors = 0
    runtime_errors = 0
    stalled_intervals = 0
    av_drift_ms = 20
}
Assert-Equal $true (Test-Priority5RealtimeReport -Report $valid -Hardware $true) 'valid realtime report'

$invalid = $valid.PSObject.Copy()
$invalid.dropped_buffers = 1
$failed = $false
try { Test-Priority5RealtimeReport -Report $invalid -Hardware $true | Out-Null } catch { $failed = $true }
Assert-Equal $true $failed 'drop gate'

$local = [pscustomobject][ordered]@{
    status='pass';scenario='local_hw';machine=$valid.machine;encoding_path='cuda-nvenc/h264_nvenc'
    duration_seconds=60;loops=8;average_cli_cpu_percent=2;p95_cli_cpu_percent=4
    peak_working_set_bytes=1048576;peak_thread_count=20;queue_high_watermark=4
    maximum_loop_duration_ms=9000;dropped_buffers=0;worker_errors=0;runtime_errors=0
    stalled_intervals=0;maximum_av_drift_ms=20
}
Assert-Equal $true (Test-Priority5LocalReport -Report $local -Hardware $true) 'valid local report'

$summary = Merge-Priority5Reports -Reports @([pscustomobject]$valid, [pscustomobject]$valid)
Assert-Equal 2 $summary.run_count 'aggregate run count'
Assert-Equal 0 $summary.total_dropped_buffers 'aggregate drops'

$config = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'priority5_scenarios.json') -Raw | ConvertFrom-Json
Assert-Equal 1 $config.schema_version 'scenario schema'
Assert-Equal 6 $config.scenarios.Count 'scenario count'
Assert-Equal 'local_hw,local_sw,mpegts_hw,mpegts_sw,rtp_hw,rtp_sw' (($config.scenarios.name | Sort-Object) -join ',') 'scenario names'
Assert-Equal 1800 ($config.scenarios | Where-Object name -eq 'local_hw').stability_seconds 'local stability duration'
Assert-Equal 3600 ($config.scenarios | Where-Object name -eq 'rtp_hw').stability_seconds 'RTP stability duration'
Assert-Equal 3600 ($config.scenarios | Where-Object name -eq 'mpegts_hw').stability_seconds 'MPEG-TS stability duration'

Write-Output 'priority5 reporting tests passed'
