param(
    [Parameter(Mandatory = $true)][string]$ExecutableDirectory,
    [Parameter(Mandatory = $true)][string]$InputPath,
    [int]$DurationSeconds = 60,
    [switch]$DisableHw
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Priority5Acceptance.psm1') -Force
$normalizedPath = $env:Path
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $normalizedPath, 'Process')
$cli = Join-Path $ExecutableDirectory 'media_transcode_local_video_cli.exe'
$ffmpeg = Join-Path $ExecutableDirectory 'ffmpeg.exe'
$ffprobe = Join-Path $ExecutableDirectory 'ffprobe.exe'
$tag = if ($DisableHw) { 'sw' } else { 'hw' }
$output = Join-Path $ExecutableDirectory "priority4_local_$tag.mp4"
$log = Join-Path $ExecutableDirectory "priority4_local_$tag.log"
$errorLog = $log + '.err'
$timer = [Diagnostics.Stopwatch]::StartNew()
$loops = 0
$logicalProcessors = [Environment]::ProcessorCount
$cpuSamples = [Collections.Generic.List[double]]::new()
$peakWorkingSet = [uint64]0
$peakThreadCount = 0
$queueHighWatermark = [uint64]0
$droppedBuffers = [uint64]0
$workerErrors = [uint64]0
$runtimeErrors = [uint64]0
$stalledIntervals = [uint64]0
$maximumLoopDurationMs = 0.0
$maximumAvDriftMs = 0.0

function Last-PacketPts([string]$Selector) {
    $values = & $ffprobe -v error -select_streams $Selector -show_entries packet=pts_time -of csv=p=0 $output
    if ($LASTEXITCODE -ne 0) { throw "local packet timestamp probe failed for $Selector" }
    $last = $values | Where-Object { $_ -match '^-?\d+(\.\d+)?$' } | Select-Object -Last 1
    if ($null -eq $last) { throw "local packet timestamp missing for $Selector" }
    return [double]::Parse($last, [Globalization.CultureInfo]::InvariantCulture)
}

function Update-MaximumMetric([string]$Text, [string]$Pattern, [ref]$Target, [string]$Name) {
    $matches = [regex]::Matches($Text, $Pattern)
    if ($matches.Count -eq 0) { throw "local runtime report metric missing: $Name" }
    $value = [uint64](($matches | ForEach-Object { [uint64]$_.Groups[1].Value } | Measure-Object -Maximum).Maximum)
    if ($value -gt $Target.Value) { $Target.Value = $value }
}

do {
    $arguments = @(
        '--input',$InputPath,'--output',$output,
        '--metadata-queue','1','--packet-queue','256','--frame-queue','128','--mux-queue','256',
        '--video-codec','h264','--width','1280','--height','720','--bitrate','2000',
        '--audio-codec','aac','--audio-bitrate','128','--sample-rate','48000','--channels','2'
    )
    if ($DisableHw) { $arguments += '--disable-hw' }
    Remove-Item $log,$errorLog -Force -ErrorAction SilentlyContinue
    $loopTimer = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $cli -ArgumentList $arguments -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput $log -RedirectStandardError $errorLog
    $previousCpu = $process.TotalProcessorTime.TotalSeconds
    $sampleTimer = [Diagnostics.Stopwatch]::StartNew()
    while (-not $process.HasExited) {
        Start-Sleep -Milliseconds 500
        $process.Refresh()
        $elapsed = [Math]::Max($sampleTimer.Elapsed.TotalSeconds, 0.001)
        $currentCpu = $process.TotalProcessorTime.TotalSeconds
        $cpuSamples.Add(([Math]::Max(0.0, $currentCpu-$previousCpu) / $elapsed / $logicalProcessors) * 100.0)
        $previousCpu = $currentCpu
        $sampleTimer.Restart()
        $peakWorkingSet = [Math]::Max($peakWorkingSet, [uint64]$process.WorkingSet64)
        $peakThreadCount = [Math]::Max($peakThreadCount, $process.Threads.Count)
    }
    $process.WaitForExit()
    $process.Refresh()
    $cliExitCode = $process.ExitCode
    if ($null -ne $cliExitCode -and $cliExitCode -ne 0) { throw "local CLI failed with exit code $cliExitCode" }
    $logText = (Get-Content -LiteralPath $log -Raw) + (Get-Content -LiteralPath $errorLog -Raw)
    if ($logText -notmatch '\[CLI\] done: .*completed=true') { throw 'local CLI success marker missing' }
    if (-not $DisableHw -and $logText -notmatch 'selected_chain=cuda-nvenc.*encoder=h264_nvenc') { throw 'hardware gate did not select CUDA/NVENC' }
    if ($DisableHw -and $logText -notmatch 'selected_chain=software.*encoder=libx264') { throw 'software gate did not select libx264' }
    Update-MaximumMetric $logText 'peakQueued=(\d+)' ([ref]$queueHighWatermark) 'queue'
    Update-MaximumMetric $logText 'droppedBuffers=(\d+)' ([ref]$droppedBuffers) 'drop'
    Update-MaximumMetric $logText 'workerErrors=(\d+)' ([ref]$workerErrors) 'worker'
    Update-MaximumMetric $logText 'errors=(\d+)' ([ref]$runtimeErrors) 'error'
    Update-MaximumMetric $logText 'stalledIntervals=(\d+)' ([ref]$stalledIntervals) 'stall'
    $streams = & $ffprobe -v error -show_entries stream=codec_type,codec_name,sample_rate,width,height -of json $output | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw "local stream probe failed on loop $($loops+1)" }
    $video = $streams.streams | Where-Object codec_type -eq 'video'
    $audio = $streams.streams | Where-Object codec_type -eq 'audio'
    if ($video.codec_name -ne 'h264' -or $video.width -ne 1280 -or $video.height -ne 720) { throw "local video contract failed on loop $($loops+1)" }
    if ($audio.codec_name -ne 'aac' -or $audio.sample_rate -ne '48000') { throw "local audio contract failed on loop $($loops+1)" }
    & $ffmpeg -v error -i $output -f null NUL
    if ($LASTEXITCODE -ne 0) { throw "local full decode failed on loop $($loops+1)" }
    $maximumAvDriftMs = [Math]::Max($maximumAvDriftMs, [Math]::Abs((Last-PacketPts 'v:0') - (Last-PacketPts 'a:0')) * 1000.0)
    $maximumLoopDurationMs = [Math]::Max($maximumLoopDurationMs, $loopTimer.Elapsed.TotalMilliseconds)
    $loops++
} while ($timer.Elapsed.TotalSeconds -lt $DurationSeconds)

$report = [pscustomobject][ordered]@{
    status = 'pass'
    scenario = "local_$tag"
    mode = 'local'
    hardware = -not $DisableHw
    machine = Get-Priority5MachineInfo
    encoding_path = if ($DisableHw) { 'software/libx264' } else { 'cuda-nvenc/h264_nvenc' }
    duration_seconds = $timer.Elapsed.TotalSeconds
    loops = $loops
    average_cli_cpu_percent = ($cpuSamples | Measure-Object -Average).Average
    p95_cli_cpu_percent = Get-Percentile -Values @($cpuSamples) -Percentile 0.95
    peak_working_set_bytes = $peakWorkingSet
    peak_thread_count = $peakThreadCount
    queue_high_watermark = $queueHighWatermark
    maximum_loop_duration_ms = $maximumLoopDurationMs
    dropped_buffers = $droppedBuffers
    worker_errors = $workerErrors
    runtime_errors = $runtimeErrors
    stalled_intervals = $stalledIntervals
    maximum_av_drift_ms = $maximumAvDriftMs
    output = $output
}
Test-Priority5LocalReport -Report $report -Hardware (-not $DisableHw) | Out-Null
$report | ConvertTo-Json -Depth 4 -Compress
