param(
    [Parameter(Mandatory = $true)][string]$ExecutableDirectory,
    [Parameter(Mandatory = $true)][string]$InputPath,
    [int]$DurationSeconds = 60,
    [switch]$DisableHw
)

$ErrorActionPreference = 'Stop'
$cli = Join-Path $ExecutableDirectory 'media_transcode_local_video_cli.exe'
$ffmpeg = Join-Path $ExecutableDirectory 'ffmpeg.exe'
$ffprobe = Join-Path $ExecutableDirectory 'ffprobe.exe'
$tag = if ($DisableHw) { 'sw' } else { 'hw' }
$output = Join-Path $ExecutableDirectory "priority4_local_$tag.mp4"
$log = Join-Path $ExecutableDirectory "priority4_local_$tag.log"
$timer = [Diagnostics.Stopwatch]::StartNew()
$loops = 0

do {
    $arguments = @(
        '--input',$InputPath,'--output',$output,
        '--metadata-queue','1','--packet-queue','256','--frame-queue','128','--mux-queue','256',
        '--video-codec','h264','--width','1280','--height','720','--bitrate','2000',
        '--audio-codec','aac','--audio-bitrate','128','--sample-rate','48000','--channels','2','--quiet-graph'
    )
    if ($DisableHw) { $arguments += '--disable-hw' }
    $ErrorActionPreference = 'Continue'
    & $cli @arguments *> $log
    $cliExitCode = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($cliExitCode -ne 0) { throw "local CLI failed with exit code $cliExitCode" }
    $loops++
} while ($timer.Elapsed.TotalSeconds -lt $DurationSeconds)

$streams = & $ffprobe -v error -show_entries stream=codec_type,codec_name,sample_rate,width,height -of json $output | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { throw 'local stream probe failed' }
$video = $streams.streams | Where-Object codec_type -eq 'video'
$audio = $streams.streams | Where-Object codec_type -eq 'audio'
if ($video.codec_name -ne 'h264' -or $video.width -ne 1280 -or $video.height -ne 720) { throw 'local video contract failed' }
if ($audio.codec_name -ne 'aac' -or $audio.sample_rate -ne '48000') { throw 'local audio contract failed' }
& $ffmpeg -v error -i $output -f null NUL
if ($LASTEXITCODE -ne 0) { throw 'local full decode failed' }

[ordered]@{
    status = 'pass'
    mode = 'local'
    hardware = -not $DisableHw
    elapsed_seconds = $timer.Elapsed.TotalSeconds
    loops = $loops
    output = $output
} | ConvertTo-Json -Compress
