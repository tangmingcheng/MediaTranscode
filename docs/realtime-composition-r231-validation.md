# r231：Release realtime 三源验证失败记录

## 规格与判定

2026-10-15，使用r229产物：realtime CLI 6043648字节，SHA256 A977887C6155A0660C67E3967E123EDF1263617E47441C6B89924AB8448B4562。三源均来自指定120秒连续源（124427809字节），H264 1280×720/30fps约8Mbps；输出HEVC 1280×720/30fps、8Mbps CBR/GOP60＋AAC192kbps/44100Hz双声道，经MP2T/RTP62020进入VLC。输入等待100/150/200ms，准备存储2GiB、出口50Mbps、wire residence100ms。无disable-hw、loop、max-duration或local CLI。

**FAIL**：CLI于16:37:53.740自行以1退出，运行约60秒，非caller_stop或源EOF。三源各自完整3600帧/120秒并退出0；本轮未执行重入。未抓包、录制或进行离线解码，因此不提供线端/全量解码证据。未降低失败标准，不能作为成功验收提交。

## 画面和运行数据

16:37:31.728的VLC原生RC snapshot返回0，主智能体及独立审查员实际查看：左上、右上、左下视频，右下黑。只证明此时布局，不代表全时段或失源/重入通过。未使用系统截图。

- 实际decoder日志为native h264、hardware=true、CUDA；replay记录same_device=1。未单独输出r229 allocation产品内容，不能据此声称全部能力分支覆盖。
- CPU外部58样本均值1.816166%、峰4.433473%；内部均值1.937696%、峰6.517312%。WS初始208150528、终值265179136、峰265248768字节。
- preparation graph_bound retained845157361、high_water871207090、capacity2147483648。
- generation1漂移55样本，raw_phase绝对值最大1ns；这不是内容A/V对齐证据。
- 最终queued=0、workers=0、workerErrors=1、errors=1；payload bytes/objects=0，reservations=releases=52146、pressureFailures=0。
- FFmpeg保留unknown→tv range变化警告；AAC异常关闭时2帧仍在队列。VLC有零音频sample、D3D纹理尺寸/切片、playback too late、picture late及converter创建失败记录，不能忽略。

## 失败关联证据

原始错误：

```text
InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=60162745660 materialized_ns=60243223000 protocol_deadline_ns=60229412328 deadline_before_release=0 materialized_after_deadline=1
```

节点80 MediaMpegTsDatagramMaterializerNode，generation1；同batch attempts=7、consumed_datagrams=0、remaining_datagrams=1。mux_dequeued=60156795100，first_dequeued=60191736100，last_wire_published=60191731300。物化超期13810672ns；首次出队至失败尝试51486900ns，首次出队时余37676228ns；mux出队至物化节点首次出队34941000ns。last_wire_published早于当前首次出队4800ns，不能当当前batch进展。

sender最终committed_datagrams=39046、socket would_block/writable_waits/deadline_misses=0；ordered_queue_max_batches=1、max_datagrams=59、backlog_max_residence=78705300ns、pacing_max_submit_lateness=21151340ns。后两者是全会话峰值，不能归因到当前失败batch。canvas峰值在video_cursor262，不能当本次错误根因。

两名独立只读审查均确认当前证据不能区分前六次重试的阻塞、许可等待、唤醒或线程调度。attempts=7不等于连续睡眠51ms；没有固定50ms timer或GPU根因证据。应继续核对RTP reservation backlog、global sequence准入及唤醒路径，并补同batch尝试返回原因和时间关联，不能放宽deadline掩盖失败。

## 进程及清理

VLC30784，sources28696/31468/31948，CLI18076。CLI自然失败退出，三源自然结束；发起清理时源已不在进程表，未强制杀源。VLC RC shutdown后仍在，核对命令身份后按PID强制清理；不能记作VLC正常退出。

本轮12件临时产物已在记录完成后按下面精确清单逐项删除，前缀文件和五个本轮PID残留均为0；保留指定120秒源。无远程测试或产物。完整合屏保持75/FAIL，非cadence容量、完整Windows门禁及RKMPP未完成。

## 实际命令

### 启动与监控

```powershell
$ErrorActionPreference='Stop'
$r231Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r231-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($r231Vlc.Id)"
$r231Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-source0.log' -PassThru
Write-Output "SOURCE0_PID=$($r231Source0.Id)"
$r231Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-source1.log' -PassThru
Write-Output "SOURCE1_PID=$($r231Source1.Id)"
$r231Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-source2.log' -PassThru
Write-Output "SOURCE2_PID=$($r231Source2.Id)"
$r231Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r231 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-cli-error.log' -PassThru
Write-Output "CLI_PID=$($r231Cli.Id)"
$r231Clock=[Diagnostics.Stopwatch]::StartNew()
$r231LastTime=0.0
$r231LastCpu=0.0
$r231Cores=[Environment]::ProcessorCount
while(-not $r231Cli.WaitForExit(1000)) {
$r231Cli.Refresh()
$r231Time=$r231Clock.Elapsed.TotalSeconds
$r231Cpu=$r231Cli.TotalProcessorTime.TotalSeconds
$r231Usage=100*($r231Cpu-$r231LastCpu)/(($r231Time-$r231LastTime)*$r231Cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-monitor.log' -Value "seconds=$r231Time cpu=$r231Usage ws=$($r231Cli.WorkingSet64) private=$($r231Cli.PrivateMemorySize64)"
$r231LastTime=$r231Time
$r231LastCpu=$r231Cpu
}
Write-Output "CLI_EXIT=$($r231Cli.ExitCode)"
$r231Source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($r231Source0.ExitCode)"
$r231Source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($r231Source1.ExitCode)"
$r231Source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($r231Source2.ExitCode)"
exit $r231Cli.ExitCode
```

### VLC原生快照

```powershell
$r231Client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try {$r231Stream=$r231Client.GetStream(); $r231Stream.ReadTimeout=2000; $r231Bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`r`n"); $r231Stream.Write($r231Bytes,0,$r231Bytes.Length); $r231Buffer=[byte[]]::new(4096); try {while(($r231Read=$r231Stream.Read($r231Buffer,0,4096)) -gt 0){[Text.Encoding]::UTF8.GetString($r231Buffer,0,$r231Read)}}catch [IO.IOException] {}} finally {$r231Client.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r231-native-*.png' | Select-Object -ExpandProperty FullName
```

### 结束后清理进程

```powershell
$ErrorActionPreference='Stop'
foreach($r231Id in @(28696,31468,31948)){
$r231Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r231Id"
if($r231Process){if($r231Process.Name -ne 'ffmpeg.exe' -or $r231Process.CommandLine -notlike '*test-continuous-120s.mp4*' -or $r231Process.CommandLine -notmatch '61020|61030|61040'){throw 'Source identity mismatch'}; Stop-Process -Id $r231Id -Force; Write-Output "SOURCE_CLEANUP_PID=$r231Id"}
}
$r231Client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try {$r231Bytes=[Text.Encoding]::ASCII.GetBytes("shutdown`r`n");$r231Client.GetStream().Write($r231Bytes,0,$r231Bytes.Length)}finally{$r231Client.Dispose()}
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(28696,31468,31948,18076,30784)} | Select-Object ProcessId,Name
$r231Process=Get-CimInstance Win32_Process -Filter 'ProcessId=30784'
if($r231Process){if($r231Process.Name -ne 'vlc.exe' -or $r231Process.CommandLine -notlike '*composition-r231*'){throw 'VLC identity mismatch'};Stop-Process -Id 30784 -Force;Write-Output 'VLC cleanup after RC shutdown did not exit; PID=30784'}
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(28696,31468,31948,18076,30784)} | Select-Object ProcessId,Name
```

### 指标提取

```powershell
$r231Log=Get-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-cli.log'
$r231Monitor=Get-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-monitor.log'
$r231Cpu=@($r231Monitor|ForEach-Object{if($_ -match 'cpu=([\d.]+)'){[double]$Matches[1]}})
$r231Drift=@($r231Log|ForEach-Object{if($_ -match 'av_drift_trace.*raw_phase_ns=(-?\d+)'){[math]::Abs([long]$Matches[1])}})
[pscustomobject]@{CpuSamples=$r231Cpu.Count;CpuMean=($r231Cpu|Measure-Object -Average).Average;CpuPeak=($r231Cpu|Measure-Object -Maximum).Maximum;DriftSamples=$r231Drift.Count;DriftMaxAbsNs=($r231Drift|Measure-Object -Maximum).Maximum;Final=($r231Log|Select-String 'final runtime report').Line;Preparation=@(($r231Log|Select-String 'preparation.*retained|preparation.*high_water').Line);Failure=@(($r231Log|Select-String 'protocol_batch_trace stage=materialization_failed').Line);VlcErrors=@((Get-Content 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-vlc.log'|Select-String 'error:|warning:').Line)}|ConvertTo-Json -Depth 3
foreach($r231Source in @('source0','source1','source2')){Get-Content -LiteralPath "D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r231-$r231Source.log" -Tail 3}
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(28696,31468,31948,18076,30784)} | Select-Object ProcessId,Name
```

### 临时产物清单与清理命令

```powershell
$r231Root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')+[IO.Path]::DirectorySeparatorChar
$r231Names=@('composition-r231-cli-error.log','composition-r231-cli.log','composition-r231-monitor.log','composition-r231-native-2026-10-15-16h37m31s728.png','composition-r231-output.sdp','composition-r231-source0-stdout.log','composition-r231-source0.log','composition-r231-source1-stdout.log','composition-r231-source1.log','composition-r231-source2-stdout.log','composition-r231-source2.log','composition-r231-vlc.log')
foreach($r231Name in $r231Names){$r231Path=[IO.Path]::GetFullPath((Join-Path $r231Root $r231Name));if(-not $r231Path.StartsWith($r231Root,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup path escaped acceptance directory'};if(Test-Path -LiteralPath $r231Path){Remove-Item -LiteralPath $r231Path -Force;Write-Output "REMOVED $r231Name"}}
Get-ChildItem -LiteralPath $r231Root -Filter 'composition-r231-*' | Select-Object Name
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(28696,31468,31948,18076,30784)} | Select-Object ProcessId,Name
Get-Item -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' | Select-Object Length
```

独立源码交叉核验进一步确认：RTP本地outstanding上限、global reservation均可返回WouldBlock；本地RTP在整笔reservation完成时释放并通知；global sequence通知还取决于commitBeforeNextReservation，可在部分提交后通知。FFmpegNodeRuntime将WouldBlock转为无deadline等待，Windows采用事件；没有固定50ms timer。仍缺逐次返回原因与通知时间，不能把这些候选当根因。

两名独立审查对失败记录与原始证据一致性均给出有限PASS，实流结论仍为FAIL。未修改生产代码或重建。
