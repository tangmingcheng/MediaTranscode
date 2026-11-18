# r339：晚提交映射分支的单变量诊断

状态：目标分支运行证据通过，整链验收不适用。保持r337/r338的Release二进制及PDB、原120秒三源HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz双声道、TS/RTP。仅在scheduler已进入媒体诊断但尚未读取now的源码784行设一次断点，暂停250ms；记录命中栈和前后时间，未修改源码/阈值/公共参数。VLC提前至启动后1秒打开，便于观察早期诊断。

依据：[Microsoft一次性断点](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/bp--bu--bm--set-breakpoint-)、[源码行表达式](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/source-line-syntax)、[调试器暂停](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/-sleep--pause-debugger-)。采用-lines启用源码行，.sleep 0n250明确十进制毫秒。暂停并不保证other同代head存在；必须由实际late_output_commit字段、映射算式及后续进展证明。暂停可能导致原期限失败，必须原样保留并明确为调试干预，不能据此标记整链通过。

## 实际执行命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$debugger=Start-Process -FilePath 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -ArgumentList ('-G -hd -lines -logo D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-cdb.log -y D:/Code/MyCode/MediaTranscode/out/build/x64-release -c "bp /1 `media_transcode_realtime_video_cli!MediaAvOutputSchedulerNode.cpp:784` \".echo R339_DELAY_BEGIN;.time;~.;kb 8;.sleep 0n250;.time;.echo R339_DELAY_END;gc\";sxe -c2 \".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-crash.dmp;qd\" 0xc0000409;g" D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe ' + '--media-id composition-r339 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2') -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-cli-error.log' -PassThru
Write-Output "CDB_PID=$($debugger.Id)"
$child=$null
$discovery=[Diagnostics.Stopwatch]::StartNew()
while(-not $child -and -not $debugger.HasExited -and $discovery.Elapsed.TotalSeconds -lt 15){
$child=Get-CimInstance Win32_Process -Filter "ParentProcessId=$($debugger.Id)" | Where-Object {$_.Name -eq 'media_transcode_realtime_video_cli.exe'}
if(-not $child){Start-Sleep -Milliseconds 100}
}
if(-not $child){throw 'Debugger did not create realtime CLI'}
$cli=[Diagnostics.Process]::GetProcessById($child.ProcessId)
$cliHandle=$cli.Handle
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 1) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r339-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r339-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
$source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($source1.ExitCode)"
$source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($source2.ExitCode)"
exit $cli.ExitCode
~~~~

## 结果：目标分支验证通过，整链验收不适用

PID：三源36240/37500/39196、CDB32368、CLI32392、VLC36280。三个源均完成3600帧/120秒自然exit0；CLI约7秒时按原期限检查退出1，父监控会话最终exit1。没有0xc0000409或转储。本轮仅分支诊断，VLC已启动但未形成可验收持续画面，不获取或宣称画面通过。

一次断点命中线程74（0x79e4），commitReservedOutput+0xb1，源码784行。命令请求250ms，含栈记录在内实际窗口11:41:44.651→11:41:44.966，约315ms。恢复后晚提交诊断同条记录audio/other均generation1，other master=6742544868+(0-0)=6742544868。该原持permit映射分支完成并继续执行，未再进入r336的snapshot重锁终止路径。独立审查crash_release_audit实读日志明确PASS，范围仅此目标分支。

随后原错误保持：`MPEG-TS access unit expired before prefix publication prefix_release_ns=6642544868 au_deadline_ns=6709211536 observed_ns=6947214900`。workerErrors/errors=1，payloadPressureFailures=1，均未隐藏；进入正常失败清理，payloadBytes/objects=0，reservation/release1487。sender只提交3数据报/600字节，不能作完整媒体对账或交付证明。

外部7样本CPU平均1.117552%、峰2.971028%，WS峰234127360、末215330816，最后private686120960；此短诊断不支持稳定性能结论。仅一条有效内部phase2268ns/frequency/stretch0，不构成持续漂移或内容A/V验收。完整未干预链路证据另见r337/r338；其他历史崩溃不能一概认定同根因。

### 原始断点记录

```
R339_DELAY_BEGIN
Debug session time: Mon Oct 26 11:41:44.651 2026 (UTC + 8:00)
System Uptime: 0 days 2:56:09.549
Process Uptime: 0 days 0:00:06.783
  Kernel time: 0 days 0:00:00.921
  User time: 0 days 0:00:00.734
. 74  Id: 7e88.79e4 Suspend: 1 Teb: 0000000e`f6498000 Unfrozen
      Start: ucrtbase!wcsrchr+0x120 (00007fff`be6ccd00)
      Priority: 0  Priority class: 32  Affinity: 3fffff
RetAddr               : Args to Child                                                           : Call Site
00007ff6`9b5939eb     : 00000198`9cfa1a50 00000000`00000000 0000000e`fbdfe640 00000198`9bc4a5a8 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvOutputSchedulerNode::commitReservedOutput+0xb1 [D:\Code\MyCode\MediaTranscode\src\internal\graph\nodes\sync\MediaAvOutputSchedulerNode.cpp @ 784]
00007ff6`9b5910f7     : 00000000`000000d9 00000198`00000000 00000000`000000d9 0000000e`fbdfe941 : media_transcode_realtime_video_cli!media::ffmpeg::graph::FFmpegNodeRuntime::transferOrDefer+0x6cb [D:\Code\MyCode\MediaTranscode\src\internal\graph\nodes\FFmpegNodeRuntime.cpp @ 675]
00007ff6`9b6ee27d     : 00000000`00000001 6e616863`20343231 0000000e`f66efcc0 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::FFmpegNodeRuntime::emitOutput+0x247 [D:\Code\MyCode\MediaTranscode\src\internal\graph\nodes\FFmpegNodeRuntime.cpp @ 510]
00007ff6`9b6f222d     : 00000000`00000000 00000001`91e311e4 00000198`9bad06e0 00000000`0000ac44 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvOutputSchedulerNode::emitWithCommit+0x11d [D:\Code\MyCode\MediaTranscode\src\internal\graph\nodes\sync\MediaAvOutputSchedulerNode.cpp @ 728]
00007ff6`9b6f22fc     : 3432313d`736c6500 00000198`9cfa1a40 0000000e`fbdff330 0000000e`fbdff330 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvOutputSchedulerNode::processMedia+0x4bd [D:\Code\MyCode\MediaTranscode\src\internal\graph\nodes\sync\MediaAvOutputSchedulerNode.cpp @ 653]
00007ff6`9b6f1320     : 00000198`9cfa1a40 00000198`9cfa1a40 00007ff6`9b902ce0 00007ff6`9b4a69eb : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvOutputSchedulerNode::processSelected+0x3c [D:\Code\MyCode\MediaTranscode\src\internal\graph\nodes\sync\MediaAvOutputSchedulerNode.cpp @ 624]
00007ff6`9b58eab2     : 00000198`9cfa1a40 0000000e`fbdff810 00007ff6`9b902ce0 00000001`91e311e4 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvOutputSchedulerNode::onProcess+0x510 [D:\Code\MyCode\MediaTranscode\src\internal\graph\nodes\sync\MediaAvOutputSchedulerNode.cpp @ 273]
00007ff6`9b591d22     : 00000000`00000000 00000000`00000000 0000000e`fbdff810 0000000e`fbdff810 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaNodeRuntime::process+0x332 [D:\Code\MyCode\MediaTranscode\src\internal\graph\nodes\MediaNodeRuntime.cpp @ 100]
Debug session time: Mon Oct 26 11:41:44.966 2026 (UTC + 8:00)
System Uptime: 0 days 2:56:09.863
Process Uptime: 0 days 0:00:07.099
  Kernel time: 0 days 0:00:00.921
  User time: 0 days 0:00:00.734
R339_DELAY_END
```

### 原始目标分支记录

```
[2026-10-26 11:41:44.970] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=0 emit_ns=6642544868 dispatch_ns=6742544868 committed_at_ns=6945770300 canonical_sequence=1 head_missing_valid=1 head_missing_intervals=0 head_missing_completed_ns=0 head_missing_active=0 head_missing_last_begin_ns=unavailable head_missing_last_end_ns=unavailable head_resumed_generation=unavailable head_resumed_sequence=unavailable other_generation=1 other_sequence=1 other_canonical_dispatch_ns=0 other_arrived_ns=6612766700 other_dispatch_master_ns=6742544868 head_arrived_ns=6624928300 scheduled_at_ns=6625202700
```

## VLC正常退出命令

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 36280){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("quit`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose()
~~~~

## 本轮清理清单

必要证据已归档后逐项删除12项，保留复用源及正式构建。

```
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r339-cdb.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r339-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r339-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r339-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r339-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r339-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r339-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r339-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r339-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r339-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r339-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r339-vlc.log
```



清理核验：12项产物已逐项删除，六个精确PID均不存在。
