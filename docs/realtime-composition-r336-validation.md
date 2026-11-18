# r336：同规格 CDB 崩溃诊断

状态：诊断已结束，复现致命异常并取得根因栈；媒体验收 FAIL。保持 r335 Release 构建及原 120 秒三源规格，HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz 双声道，TS/RTP。使用现有 CDB 标准堆捕获 0xc0000409 的异常上下文、线程栈和转储，未修改注册表或外部依赖。

## 执行命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$debugger=Start-Process -FilePath 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -ArgumentList ('-G -hd -logo D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-cdb.log -y D:/Code/MyCode/MediaTranscode/out/build/x64-release -c "sxe -c2 \".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-crash.dmp;qd\" 0xc0000409;g" D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe ' + '--media-id composition-r336 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2') -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-cli-error.log' -PassThru
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
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r336-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r336-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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


本轮 PID：源39996、32804、2004；CDB40928；CLI35256；VLC26904。CLI/PDB均为2026-10-26 11:14:42的同一Release构建。第一张VLC原生快照已查看，三格有画面、右下黑。最终取得fatal上下文如下。

## 结果与根因

判定：诊断复现成功、媒体验收 FAIL。CLI35256退出-1073740791（0xc0000409），不是自然结束或caller_stop；源39996/32804/2004均3600帧/120秒自然exit0。CDB40928捕获thread85（0x1aa0）second chance，FAST_FAIL_FATAL_APP_EXIT子码7，成功写入1622330392字节转储。VLC26904已RC正常退出。无正常终态发送对账或payload释放证据。

崩溃前runtime CPU平均3.019305%、峰8.955224%；WS初始210550784、末275726336、峰276729856。最后内部phase1ns、frequency/stretch0，不是内容A/V证明。worker/errors及pressureFailures=0仅为失败前快照；payloadCurrent15834411、objects888、reserve/release130886/129998。转储期间外部WS升至1621954560，不能当作正常运行内存增长。

真实栈与源码闭合：FFmpegNodeRuntime::transferOrDefer保留output reservation；scheduler.reserveOutputCommit→transition.reserveOutputCommit持m_mutex；commitReservedOutput晚提交诊断→mapCanonicalToMaster→playbackEpoch→snapshot再次lock同mutex；snapshot为noexcept，因此该lock抛异常直接terminate，外层诊断catch不能处理。r335/r326没有各自栈，不能追溯认定都是同因。

工业依据：[Microsoft STL mutex实现](https://github.com/microsoft/STL/blob/main/stl/inc/mutex)的lock错误路径与本轮MSVC栈相符；[GStreamer同步模型](https://gstreamer.freedesktop.org/documentation/additional/design/synchronisation.html)基于时间段内确定的timestamp/running-time转换。当前固定同代映射可直接复用scheduled已有canonicalDispatch与dispatchOnMaster，以checked算术推导另一head的master时间，不需再次读取transition状态。保留generation相等检查、全部原诊断字段与失败时unavailable语义；不使用递归锁、提前释放permit或隐藏异常。共享Windows/Linux/RKMPP路径均受影响，先Windows同规格验证实际late_output_commit，再按原顺序推进RKMPP。

## 原始致命线程栈

```
# 85  Id: 89b8.1aa0 Suspend: 1 Teb: 00000094`d7652000 Unfrozen
RetAddr               : Args to Child                                                           : Call Site
00007fff`be76218e     : 00000094`00000003 00000000`00000003 0000026f`f1b4f8a0 0000026f`f1b4f870 : ucrtbase!abort+0x4e
00007fff`9ff7186b     : 00000094`dcefd940 00000000`00000000 00000000`00000000 00000000`00000000 : ucrtbase!terminate+0x1e
00007fff`9ff721a5     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : VCRUNTIME140_1+0x186b
00007fff`9ff721f6     : 00000094`dcefd3e0 00000094`dcefd240 00000000`00000000 00000094`dcefd240 : VCRUNTIME140_1+0x21a5
00007fff`9ff743f5     : 00007ff6`fd630000 00007ff6`fd9cc2d1 00000094`dcefe220 00007ff6`fdc536d4 : VCRUNTIME140_1+0x21f6
00007fff`c0d864af     : 00000094`dcefe220 00000094`dcefde30 00000094`dcefd740 00007ff6`fdc536d4 : VCRUNTIME140_1!_CxxFrameHandler4+0xa5
00007fff`c0ce4065     : 00000094`dcefde30 00000000`00000081 00007ff6`fd9cc2d1 00007ff6`fdc536d4 : ntdll!_chkstk+0x9f
00007fff`c0d85dee     : 00007fff`c0df7110 00007fff`c0c413cb 00000094`dcefe188 00007fff`c0c39e9b : ntdll!RtlWow64GetCurrentMachine+0x655
00007fff`be10408a     : 00000000`0000001d 00000094`dcefe188 00000000`00000001 00000000`e06d7363 : ntdll!KiUserExceptionDispatcher+0x2e
00007fff`9d3d55a9     : 00000000`00000400 00007fff`c0c52144 00000094`00000024 00007fff`9d5b22f0 : KERNELBASE!RaiseException+0x8a
00007fff`9d538906     : 00000000`0000000c 00000000`00000005 00000000`00000005 00000000`00000000 : VCRUNTIME140!CxxThrowException+0x99
00007ff6`fd9cc2d1     : 0000026f`f1ba03c0 0000026f`f1ba03c0 0000026f`dcc7a2e0 00007fff`9d53372d : MSVCP140!std::_Throw_Cpp_error+0x46
(Inline Function)     : --------`-------- --------`-------- --------`-------- --------`-------- : media_transcode_realtime_video_cli!std::_Mutex_base::lock+0x121
(Inline Function)     : --------`-------- --------`-------- --------`-------- --------`-------- : media_transcode_realtime_video_cli!std::lock_guard<std::mutex>::{ctor}+0x121
00007ff6`fd7b7f23     : 00000019`3b281baa 0000026f`dcd70000 00000000`00120000 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvEpochTransitionService::snapshot+0x131
00007ff6`fd7b7766     : 00000000`00000400 00007ff6`fd6390f2 00000019`3b281baa 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvSyncGroupRuntime::playbackEpoch+0x43
00007ff6`fd94cec8     : 00000094`dcefe610 00000094`dcefe610 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvSyncGroupRuntime::mapCanonicalToMaster+0x26
00007ff6`fd7f39eb     : 0000026f`fc88c790 00000000`00000000 00000094`dcefe910 0000026f`f1c6e188 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvOutputSchedulerNode::commitReservedOutput+0x758
00007ff6`fd7f10f7     : 00000000`000000d9 0000026f`00000000 00000000`000000d9 00000094`dcefec11 : media_transcode_realtime_video_cli!media::ffmpeg::graph::FFmpegNodeRuntime::transferOrDefer+0x6cb
00007ff6`fd94e23d     : 00000000`0000123b 00000000`00000000 00000094`d7580060 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::FFmpegNodeRuntime::emitOutput+0x247
00007ff6`fd9521ed     : 00000000`00000000 00000001`7c6e77a4 0000026f`86386320 00000000`0000ac44 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvOutputSchedulerNode::emitWithCommit+0x11d
00007ff6`fd9522bc     : 00000000`00000000 0000026f`fc88c780 00000094`dceff600 00000094`dceff600 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvOutputSchedulerNode::processMedia+0x4bd
00007ff6`fd9512e0     : 0000026f`fc88c780 0000026f`fc88c780 00007ff6`fdb62ce0 00007ff6`fd7069eb : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvOutputSchedulerNode::processSelected+0x3c
00007ff6`fd7eeab2     : 0000026f`fc88c780 00000094`dceffae0 00007ff6`fdb62ce0 00000001`7c6e77a4 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvOutputSchedulerNode::onProcess+0x510
00007ff6`fd7f1d22     : 00000000`00000000 00000000`00001aa0 00000094`dceffae0 00000094`dceffae0 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaNodeRuntime::process+0x332
00007ff6`fd7f1a3e     : 00000000`00000100 00000000`00000001 00000000`00000001 0000026f`fc88c780 : media_transcode_realtime_video_cli!media::ffmpeg::graph::FFmpegNodeRuntime::processImpl+0x192
00007ff6`fd951b34     : 00000094`dceffae0 00000094`d7580060 00000000`00000000 00000094`dceffae0 : media_transcode_realtime_video_cli!media::ffmpeg::graph::FFmpegNodeRuntime::process+0xe
00007ff6`fd6c1b5c     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaAvOutputSchedulerNode::process+0x214
00007ff6`fd6bfc2f     : 00000000`00000000 00000000`00000000 ffffffff`fffffffe 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaGraphWorker::run+0xfc
(Inline Function)     : --------`-------- --------`-------- --------`-------- --------`-------- : media_transcode_realtime_video_cli!std::invoke+0x6
00007fff`be6ccd30     : 0000026f`845629e0 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!std::thread::_Invoke<std::tuple<void (__cdecl media::ffmpeg::graph::MediaGraphWorker::*)(void),media::ffmpeg::graph::MediaGraphWorker *>,0,1>+0xf
00007fff`bf3bdda7     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ucrtbase!wcsrchr+0x150
00007fff`c0c9ee4c     : 00000000`00000000 00000000`00000000 000004f0`fffffb30 000004d0`fffffb30 : KERNEL32!BaseThreadInitThunk+0x17
00000000`00000000     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ntdll!RtlUserThreadStart+0x2c
```

## 原生快照与退出命令

快照已实际查看三格输出；没有将崩溃后冻结画面当作黑屏证据。

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 26904){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose()
~~~~

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 26904){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("quit`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose()
~~~~

## 本轮清理清单

独立调查确认该栈及源码已足够确定本次自锁，无需继续读取转储；保留上述必要诊断后逐项删除14项，保留复用源和正式构建产物。

```
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-cdb.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-crash.dmp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-native-2026-10-26-11h19m48s865.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r336-vlc.log
```


清理核验完成：14项本轮文件逐项删除，六个精确PID均不存在。实际命令、异常栈、关键指标与根因已保存在本记录。

修复已冻结：仅MediaAvOutputSchedulerNode.cpp晚提交诊断，使用otherCanonical-scheduledCanonical的checked差值加scheduledMaster；不再调用会获取transition锁的mapCanonicalToMaster。原generation判断、字段、unavailable及commit permit不变，无公共参数或平台特例。正在双独立复审；修复效果尚未运行验证。

两名独立审查crash_release_audit与publication_identity_review_b均明确静态PASS；未提前关闭运行门禁。修复后Release全量重建已启动，复验见r337记录。
