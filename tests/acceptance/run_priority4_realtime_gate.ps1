param(
    [Parameter(Mandatory = $true)][ValidateSet('rtp','mpegts')][string]$Mode,
    [Parameter(Mandatory = $true)][string]$ExecutableDirectory,
    [Parameter(Mandatory = $true)][string]$InputPath,
    [Parameter(Mandatory = $true)][int]$InputPort,
    [Parameter(Mandatory = $true)][int]$OutputPort,
    [int]$DurationSeconds = 60,
    [switch]$DisableHw
)

$ErrorActionPreference = 'Stop'
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
$capture = Join-Path $ExecutableDirectory "$name.capture.mkv"
$sdp = Join-Path $ExecutableDirectory "$name.sdp"
$captureSeconds = [Math]::Max(1, $DurationSeconds - 10)
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

function Last-PacketPts([string]$Selector) {
    $values = & $ffprobe -v error -select_streams $Selector -show_entries packet=pts_time -of csv=p=0 $capture
    if ($LASTEXITCODE -ne 0) { throw "packet timestamp probe failed for $Selector" }
    $last = $values | Where-Object { $_ -match '^-?\d+(\.\d+)?$' } | Select-Object -Last 1
    if ($null -eq $last) { throw "packet timestamp missing for $Selector" }
    return [double]::Parse($last, [Globalization.CultureInfo]::InvariantCulture)
}

try {
    Remove-Item $cliLog,($cliLog+'.err'),$senderLog,($senderLog+'.err'),$receiverLog,($receiverLog+'.err'),$capture,$sdp -Force -ErrorAction SilentlyContinue
    $common = @(
        '--metadata-queue','1','--packet-queue','256','--frame-queue','128','--mux-queue','256',
        '--open-timeout-ms','5000','--read-timeout-ms','5000','--analyze-duration-us','1000000','--probe-size','1048576',
        '--video-codec','h264','--width','1280','--height','720','--bitrate','2000',
        '--audio-codec','aac','--sample-rate','48000','--audio-bitrate','128',
        '--max-duration',"$DurationSeconds",'--progress-timeout-ms','5000','--poll-interval-ms','250','--quiet-graph'
    )
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
        $cliProcess = Start-GateProcess $cli $cliArguments $cliLog
        Start-Sleep -Milliseconds 750
        $sender = Start-GateProcess $ffmpeg @('-hide_banner','-loglevel','warning','-re','-stream_loop','-1','-i',$InputPath,'-map','0:v:0','-c:v','copy','-an','-f','rtp','-payload_type','96',"rtp://127.0.0.1:$($InputPort)?pkt_size=1200",'-map','0:a:0','-c:a','copy','-vn','-f','rtp','-payload_type','97',"rtp://127.0.0.1:$($InputPort+2)?pkt_size=1200") $senderLog
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        while (-not (Test-Path $sdp) -and -not $cliProcess.HasExited -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
        if (-not (Test-Path $sdp)) { throw 'RTP output SDP was not created' }
        $receiver = Start-GateProcess $ffmpeg @('-hide_banner','-loglevel','warning','-protocol_whitelist','file,udp,rtp','-i',$sdp,'-t',"$captureSeconds",'-map','0:v:0','-map','0:a:0','-c','copy',$capture) $receiverLog
    } else {
        $receiver = Start-GateProcess $ffmpeg @('-hide_banner','-loglevel','warning','-i',"udp://127.0.0.1:$($OutputPort)?fifo_size=1000000&overrun_nonfatal=1",'-t',"$captureSeconds",'-map','0:v:0','-map','0:a:0','-c','copy',$capture) $receiverLog
        $cliProcess = Start-GateProcess $cli (@('--input-type','mpegts-udp','--input-layout','mpegts','--output-layout','mpegts','--input',"udp://127.0.0.1:$($InputPort)?fifo_size=1000000&overrun_nonfatal=1",'--output',"udp://127.0.0.1:$($OutputPort)?pkt_size=1316") + $common) $cliLog
        Start-Sleep -Milliseconds 750
        $sender = Start-GateProcess $ffmpeg @('-hide_banner','-loglevel','warning','-re','-stream_loop','-1','-i',$InputPath,'-map','0:v:0','-map','0:a:0','-c','copy','-f','mpegts',"udp://127.0.0.1:$($InputPort)?pkt_size=1316") $senderLog
    }

    $cpuSamples = [Collections.Generic.List[double]]::new()
    $previousCpu = $cliProcess.TotalProcessorTime.TotalSeconds
    $sampleTimer = [Diagnostics.Stopwatch]::StartNew()
    while (-not $cliProcess.HasExited) {
        Start-Sleep -Seconds 1
        $cliProcess.Refresh()
        $elapsed = [Math]::Max($sampleTimer.Elapsed.TotalSeconds, 0.001)
        $currentCpu = $cliProcess.TotalProcessorTime.TotalSeconds
        $cpuSamples.Add(([Math]::Max(0.0, $currentCpu-$previousCpu) / $elapsed / $logicalProcessors) * 100.0)
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
    if ($cliText -match 'workerErrors=[1-9]' -or $cliText -match 'errors=[1-9]' -or $cliText -match 'stalledIntervals=[1-9]' -or $cliText -match '(?i)drop.?frame') { throw 'runtime error, drop, or stall gate failed' }
    $driftMs = [Math]::Abs((Last-PacketPts 'v:0') - (Last-PacketPts 'a:0')) * 1000.0
    if ($driftMs -gt 100.0) { throw "A/V drift gate failed: $driftMs ms" }
    $sorted = @($cpuSamples | Sort-Object)
    $average = ($cpuSamples | Measure-Object -Average).Average
    $p95 = $sorted[[Math]::Min($sorted.Count-1, [Math]::Ceiling($sorted.Count*0.95)-1)]
    $limit = if ($DisableHw) { 25.0 } else { 5.0 }
    if ($average -gt $limit) { throw "average CLI CPU gate failed: $average > $limit" }

    [ordered]@{status='pass';mode=$Mode;hardware=-not $DisableHw;duration_seconds=$DurationSeconds;average_cli_cpu_percent=$average;p95_cli_cpu_percent=$p95;av_drift_ms=$driftMs;output=$capture} | ConvertTo-Json -Compress
} finally {
    Stop-GateProcesses
}
