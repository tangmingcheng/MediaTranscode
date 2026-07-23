param(
    [Parameter(Mandatory = $true)][ValidateSet('rtp','mpegts')][string]$Mode,
    [Parameter(Mandatory = $true)][string]$ExecutableDirectory,
    [Parameter(Mandatory = $true)][string]$InputPath,
    [Parameter(Mandatory = $true)][int]$InputPort,
    [Parameter(Mandatory = $true)][int]$OutputPort,
    [int]$DurationSeconds = 60,
    [switch]$DisableHw,
    [switch]$Diagnostics
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Priority5Acceptance.psm1') -Force
$normalizedPath = $env:Path
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $normalizedPath, 'Process')
$cli = Join-Path $ExecutableDirectory 'media_transcode_realtime_video_cli.exe'
$ffmpeg = Join-Path $ExecutableDirectory 'ffmpeg.exe'
$ffprobe = Join-Path $ExecutableDirectory 'ffprobe.exe'
$tag = if ($DisableHw) { 'sw' } else { 'hw' }
$name = "priority4_${Mode}_$tag"
$cliLog = Join-Path $ExecutableDirectory "$name.cli.log"
$senderLog = Join-Path $ExecutableDirectory "$name.sender.log"
$receiverLog = Join-Path $ExecutableDirectory "$name.receiver.log"
$captureExtension = if ($Mode -eq 'rtp') { 'ts' } else { 'mkv' }
$capture = Join-Path $ExecutableDirectory "$name.capture.$captureExtension"
$sdp = Join-Path $ExecutableDirectory "$name.sdp"
$captureSeconds = [Math]::Max(1, $DurationSeconds - 10)
$receiverLogLevel = if ($Diagnostics) { 'verbose' } else { 'warning' }
$logicalProcessors = [Environment]::ProcessorCount
$processes = [Collections.Generic.List[Diagnostics.Process]]::new()

function Start-GateProcess([string]$FilePath, [string[]]$Arguments, [string]$LogPath) {
    $process = Start-Process -FilePath $FilePath -ArgumentList $Arguments -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput $LogPath -RedirectStandardError ($LogPath + '.err')
    $processes.Add($process)
    return $process
}

function Stop-GateProcesses {
    foreach ($process in $processes) {
        if ($process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            $process.WaitForExit()
        }
    }
}

function Stop-GateProcess([Diagnostics.Process]$Process) {
    if ($Process -and -not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit()
    }
}

function PacketPts([string]$Selector, [switch]$First) {
    $values = & $ffprobe -v error -select_streams $Selector -show_entries packet=pts_time -of default=nw=1:nk=1 $capture
    if ($LASTEXITCODE -ne 0) { throw "packet timestamp probe failed for $Selector" }
    $valid = @($values | Where-Object { $_ -match '^-?\d+(\.\d+)?$' })
    if ($valid.Count -eq 0) { throw "packet timestamp missing for $Selector" }
    $selected = if ($First) { $valid[0] } else { $valid[$valid.Count-1] }
    return [double]::Parse($selected, [Globalization.CultureInfo]::InvariantCulture)
}

try {
    Remove-Item $cliLog,($cliLog+'.err'),($cliLog+'.warmup'),($cliLog+'.warmup.err'),$senderLog,($senderLog+'.err'),($senderLog+'.warmup'),($senderLog+'.warmup.err'),$receiverLog,($receiverLog+'.err'),$capture,$sdp -Force -ErrorAction SilentlyContinue
    $common = @(
        '--media-id',"$name-gate",
        '--metadata-queue','1','--packet-queue','256','--frame-queue','128','--mux-queue','256',
        '--open-timeout-ms','5000','--read-timeout-ms','5000','--analyze-duration-us','1000000','--probe-size','1048576',
        '--video-codec','h264','--width','1280','--height','720','--bitrate','2000',
        '--audio-codec','aac','--sample-rate','48000','--audio-bitrate','128',
        '--startup-max-video-unit-bytes','4194304','--startup-max-audio-unit-bytes','1048576','--startup-max-gap-ms','40',
        '--max-duration',"$DurationSeconds",'--progress-timeout-ms','5000','--poll-interval-ms','250'
    )
    if (-not $Diagnostics) { $common += '--quiet-graph' }
    if ($DisableHw) { $common += '--disable-hw' }

    if ($Mode -eq 'rtp') {
        $cliArguments = @(
            '--input-type','rtp','--input-layout','separate','--output-layout','separate',
            '--video-rtp-url',"rtp://127.0.0.1:$InputPort",'--video-rtp-codec','h264','--video-rtp-payload-type','96','--video-rtp-clock-rate','90000',
            '--video-rtp-fmtp','packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032',
            '--audio-rtp-url',"rtp://127.0.0.1:$($InputPort+2)",'--audio-rtp-codec','aac','--audio-rtp-payload-type','97','--audio-rtp-clock-rate','44100','--audio-rtp-channels','2',
            '--audio-rtp-fmtp','profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210',
            '--rtp-host','127.0.0.1','--rtp-port',"$OutputPort",'--sdp',$sdp,'--packet-size','1200'
        ) + $common
        $warmupCli = Start-GateProcess $cli $cliArguments ($cliLog + '.warmup')
        Start-Sleep -Milliseconds 750
        $warmupSender = Start-GateProcess $ffmpeg @('-hide_banner','-loglevel','warning','-readrate','1','-readrate_catchup','1','-stream_loop','-1','-i',$InputPath,'-map','0:v:0','-c:v','copy','-an','-f','rtp','-payload_type','96',"rtp://127.0.0.1:$($InputPort)?pkt_size=1200",'-map','0:a:0','-c:a','copy','-vn','-f','rtp','-payload_type','97',"rtp://127.0.0.1:$($InputPort+2)?pkt_size=1200") ($senderLog + '.warmup')
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        while (-not (Test-Path $sdp) -and -not $warmupCli.HasExited -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
        if (-not (Test-Path $sdp)) { throw 'RTP output SDP was not created' }
        Stop-GateProcess $warmupSender
        Stop-GateProcess $warmupCli
        $receiver = Start-GateProcess $ffmpeg @('-hide_banner','-loglevel',$receiverLogLevel,'-protocol_whitelist','file,udp,rtp','-analyzeduration','15000000','-probesize','5000000','-i',$sdp,'-t',"$captureSeconds",'-map','0:v:0','-map','0:a:0','-c','copy','-f','mpegts',$capture) $receiverLog
        Start-Sleep -Milliseconds 750
        $cliProcess = Start-GateProcess $cli $cliArguments $cliLog
        Start-Sleep -Milliseconds 750
        $sender = Start-GateProcess $ffmpeg @('-hide_banner','-loglevel','warning','-readrate','1','-readrate_catchup','1','-stream_loop','-1','-i',$InputPath,'-map','0:v:0','-c:v','copy','-an','-f','rtp','-payload_type','96',"rtp://127.0.0.1:$($InputPort)?pkt_size=1200",'-map','0:a:0','-c:a','copy','-vn','-f','rtp','-payload_type','97',"rtp://127.0.0.1:$($InputPort+2)?pkt_size=1200") $senderLog
    } else {
        $receiver = Start-GateProcess $ffmpeg @('-hide_banner','-loglevel',$receiverLogLevel,'-i',"udp://127.0.0.1:$($OutputPort)?fifo_size=1000000&overrun_nonfatal=1",'-t',"$captureSeconds",'-map','0:v:0','-map','0:a:0','-c','copy',$capture) $receiverLog
        $cliProcess = Start-GateProcess $cli (@('--input-type','mpegts-udp','--input-layout','mpegts','--output-layout','mpegts','--input',"udp://127.0.0.1:$($InputPort)?fifo_size=1000000&overrun_nonfatal=1",'--output',"udp://127.0.0.1:$OutputPort") + $common) $cliLog
        Start-Sleep -Milliseconds 750
        $sender = Start-GateProcess $ffmpeg @('-hide_banner','-loglevel','warning','-readrate','1','-readrate_catchup','1','-stream_loop','-1','-i',$InputPath,'-map','0:v:0','-map','0:a:0','-c','copy','-f','mpegts',"udp://127.0.0.1:$($InputPort)?pkt_size=1316") $senderLog
    }

    $cpuSamples = [Collections.Generic.List[double]]::new()
    $peakWorkingSet = [uint64]0
    $peakThreadCount = 0
    $maximumProgressGapMs = 0.0
    $lastObservedProgress = [uint64]0
    $progressObservedAt = [DateTime]::UtcNow
    $previousCpu = $cliProcess.TotalProcessorTime.TotalSeconds
    $sampleTimer = [Diagnostics.Stopwatch]::StartNew()
    while (-not $cliProcess.HasExited) {
        Start-Sleep -Seconds 1
        $cliProcess.Refresh()
        $elapsed = [Math]::Max($sampleTimer.Elapsed.TotalSeconds, 0.001)
        $currentCpu = $cliProcess.TotalProcessorTime.TotalSeconds
        $cpuSamples.Add(([Math]::Max(0.0, $currentCpu-$previousCpu) / $elapsed / $logicalProcessors) * 100.0)
        $peakWorkingSet = [Math]::Max($peakWorkingSet, [uint64]$cliProcess.WorkingSet64)
        $peakThreadCount = [Math]::Max($peakThreadCount, $cliProcess.Threads.Count)
        if (Test-Path $cliLog) {
            $liveText = Get-Content -LiteralPath $cliLog -Raw -ErrorAction SilentlyContinue
            $progressMatches = [regex]::Matches($liveText, 'encodedPacketsPushed=(\d+)')
            if ($progressMatches.Count -gt 0) {
                $progress = [uint64]$progressMatches[$progressMatches.Count-1].Groups[1].Value
                if ($progress -gt $lastObservedProgress) {
                    if ($lastObservedProgress -gt 0) {
                        $maximumProgressGapMs = [Math]::Max($maximumProgressGapMs, ([DateTime]::UtcNow - $progressObservedAt).TotalMilliseconds)
                    }
                    $lastObservedProgress = $progress
                    $progressObservedAt = [DateTime]::UtcNow
                } elseif ($lastObservedProgress -gt 0) {
                    $maximumProgressGapMs = [Math]::Max($maximumProgressGapMs, ([DateTime]::UtcNow - $progressObservedAt).TotalMilliseconds)
                }
            }
        }
        $previousCpu = $currentCpu
        $sampleTimer.Restart()
    }
    $cliProcess.WaitForExit()
    $cliProcess.Refresh()
    if ($null -ne $cliProcess.ExitCode -and $cliProcess.ExitCode -ne 0) { throw "realtime CLI failed with exit code $($cliProcess.ExitCode)" }
    if (-not (Select-String -LiteralPath $cliLog -SimpleMatch '[CLI] realtime video validation stopped successfully' -Quiet)) { throw 'realtime CLI success marker missing' }
    if (-not $receiver.WaitForExit(30000)) { throw 'receiver did not finish its bounded capture' }

    $streams = & $ffprobe -v error -show_entries stream=codec_type,codec_name,sample_rate,width,height -of json $capture | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw 'capture stream probe failed' }
    $video = $streams.streams | Where-Object codec_type -eq 'video'
    $audio = $streams.streams | Where-Object codec_type -eq 'audio'
    if ($video.codec_name -ne 'h264' -or $video.width -ne 1280 -or $video.height -ne 720) { throw 'capture video contract failed' }
    if ($audio.codec_name -ne 'aac' -or $audio.sample_rate -ne '48000') { throw 'capture audio contract failed' }
    & $ffmpeg -v error -i $capture -f null NUL
    if ($LASTEXITCODE -ne 0) { throw 'capture full decode failed' }

    $cliText = Get-Content -LiteralPath $cliLog -Raw
    if (-not $DisableHw -and ($cliText -notmatch 'selected_chain=cuda-nvenc' -or $cliText -notmatch 'encoder=h264_nvenc')) { throw 'hardware gate did not select CUDA/NVENC' }
    if ($cliText -match 'workerErrors=[1-9]' -or $cliText -match 'errors=[1-9]' -or $cliText -match 'stalledIntervals=[1-9]' -or $cliText -match 'droppedBuffers=[1-9]') { throw 'runtime error, drop, or stall gate failed' }
    $startOffsetMs = [Math]::Abs((PacketPts 'v:0' -First) - (PacketPts 'a:0' -First)) * 1000.0
    $endDriftMs = [Math]::Abs((PacketPts 'v:0') - (PacketPts 'a:0')) * 1000.0
    $driftMs = [Math]::Max($startOffsetMs, $endDriftMs)
    if ($driftMs -gt 100.0) { throw "A/V drift gate failed: start=$startOffsetMs ms end=$endDriftMs ms" }
    $sorted = @($cpuSamples | Sort-Object)
    $average = ($cpuSamples | Measure-Object -Average).Average
    $p95 = Get-Percentile -Values $sorted -Percentile 0.95
    $limit = if ($DisableHw) { 25.0 } else { 5.0 }
    if ($average -gt $limit) { throw "average CLI CPU gate failed: $average > $limit" }

    $queueHighWatermark = [uint64]0
    $droppedBuffers = [uint64]0
    $workerErrors = [uint64]0
    $runtimeErrors = [uint64]0
    $stalledIntervals = [uint64]0
    foreach ($entry in @(
        @{ Pattern='peakQueued=(\d+)'; Name='queue' },
        @{ Pattern='droppedBuffers=(\d+)'; Name='drop' },
        @{ Pattern='workerErrors=(\d+)'; Name='worker' },
        @{ Pattern='errors=(\d+)'; Name='error' },
        @{ Pattern='stalledIntervals=(\d+)'; Name='stall' }
    )) {
        $matches = [regex]::Matches($cliText, $entry.Pattern)
        if ($matches.Count -eq 0) { throw "runtime report metric missing: $($entry.Name)" }
        $maximum = [uint64](($matches | ForEach-Object { [uint64]$_.Groups[1].Value } | Measure-Object -Maximum).Maximum)
        switch ($entry.Name) {
            'queue' { $queueHighWatermark = $maximum }
            'drop' { $droppedBuffers = $maximum }
            'worker' { $workerErrors = $maximum }
            'error' { $runtimeErrors = $maximum }
            'stall' { $stalledIntervals = $maximum }
        }
    }
    $encodingPath = if ($DisableHw) { 'software/libx264' } else { 'cuda-nvenc/h264_nvenc' }
    $report = [pscustomobject][ordered]@{
        status='pass';scenario="${Mode}_$tag";mode=$Mode;hardware=-not $DisableHw
        machine=Get-Priority5MachineInfo;encoding_path=$encodingPath;duration_seconds=$DurationSeconds
        average_cli_cpu_percent=$average;p95_cli_cpu_percent=$p95
        peak_working_set_bytes=$peakWorkingSet;peak_thread_count=$peakThreadCount
        queue_high_watermark=$queueHighWatermark;max_progress_gap_ms=$maximumProgressGapMs
        dropped_buffers=$droppedBuffers;worker_errors=$workerErrors;runtime_errors=$runtimeErrors
        stalled_intervals=$stalledIntervals;av_start_offset_ms=$startOffsetMs
        av_end_drift_ms=$endDriftMs;av_drift_ms=$driftMs;output=$capture
    }
    Test-Priority5RealtimeReport -Report $report -Hardware (-not $DisableHw) | Out-Null
    $report | ConvertTo-Json -Depth 4 -Compress
} finally {
    Stop-GateProcesses
}
