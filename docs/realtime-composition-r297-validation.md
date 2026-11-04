# r297：启动时调试与Release符号缺口

原规格三源H.264→HEVC/AAC MPEG-TS/UDP，1280×72030、8000kbps CBR/GOP60、AAC192kbps CBR。诊断FAIL。源11364/47176/47492，cdb44484，CLI15508；CLI早期协议截止失败，未启动VLC/重入。源中止退出-1，五PID已核验消失。

启动前已配置eh首次异常及fast-fail第二次异常命令；抓到若干C++首次异常但没有fast-fail/dump。首次异常可能被正常处理，不能据此认定崩溃原因。配置C++异常栈记录也会暂停线程，故本轮截止失败不作为未调试生产时序证据。

发现现有CLI PDB修改时间2026-07-08 19:30:32，Release缓存/O2 /Ob2 /DNDEBUG，无/Z7或/Zi；app只在Debug/RelWithDebInfo链接/DEBUG。无法证明栈函数名与当前二进制匹配，下面原始地址保留但不据显示名称归责。CLI GetProcessById句柄ExitCode为空，wrapper最终0不是CLI成功；CLI日志明确报失败。

后续为MSVC Release添加/Z7、app链接/DEBUG:FULL /OPT:REF /OPT:ICF，保留优化；必须验证生成PDB及加载匹配后重做定位。

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$debugger=Start-Process -FilePath 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -ArgumentList ('-G -logo D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-cdb.log -y D:/Code/MyCode/MediaTranscode/out/build/x64-release -c "sxe -c \".exr -1;kb;gn\" eh;sxe -c2 \".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-crash.dmp;qd\" 0xc0000409;g" D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe ' + '--media-id composition-r297 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2') -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-cli-error.log' -PassThru
Write-Output "CDB_PID=$($debugger.Id)"
$child=$null
$discovery=[Diagnostics.Stopwatch]::StartNew()
while(-not $child -and -not $debugger.HasExited -and $discovery.Elapsed.TotalSeconds -lt 15){
$child=Get-CimInstance Win32_Process -Filter "ParentProcessId=$($debugger.Id)" | Where-Object {$_.Name -eq 'media_transcode_realtime_video_cli.exe'}
if(-not $child){Start-Sleep -Milliseconds 100}
}
if(-not $child){throw 'Debugger did not create realtime CLI'}
$cli=[Diagnostics.Process]::GetProcessById($child.ProcessId)
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r297-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

~~~~text
[2026-10-21 16:49:47.493] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=1 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=6537453000 mux_dequeued_master_ns=6537410100 first_dequeued_master_ns=6537451900 last_wire_published_master_ns=unavailable previous_attempt_started_master_ns=unavailable previous_attempt_completed_master_ns=unavailable last_attempt_completed_master_ns=6537476000 last_blocked_started_master_ns=unavailable last_blocked_completed_master_ns=unavailable blocked_attempts=0 remaining_front_release_master_ns=6406896600 remaining_front_deadline_master_ns=6473563268 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=6406896600 materialized_ns=6537453000 protocol_deadline_ns=6473563268 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-21 16:49:47.493] [info] [graph][runtime.node] process.failed node=79 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=2] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=6406896600 materialized_ns=6537453000 protocol_deadline_ns=6473563268 deadline_before_release=0 materialized_after_deadline=1
[2026-10-21 16:49:47.494] [info] [graph][runtime.node] worker.failed node=79 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=6406896600 materialized_ns=6537453000 protocol_deadline_ns=6473563268 deadline_before_release=0 materialized_after_deadline=1
[2026-10-21 16:49:47.495] [info] [graph][runtime.node] process.failed node=41 name=VideoDecodeNode kind=VideoDecode inputs=[channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=59] outputs=[channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:49:47.494] [info] [graph][runtime.node] process.failed node=30 name=MediaRtpPacketClockBinderNode kind=RtpPacketClockBinder inputs=[channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=1; channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=10] outputs=[channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=10] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:49:47.494] [info] [graph][runtime.node] process.failed node=45 name=VideoFilterNode kind=VideoFilter inputs=[channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=6] outputs=[channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:49:47.494] [info] [graph][runtime.node] process.failed node=70 name=AudioDecodeNode kind=AudioDecode inputs=[channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=229] outputs=[channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:49:47.494] [info] [graph][runtime.node] process.failed node=4 name=AudioEncodeNode kind=AudioEncode inputs=[channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0] outputs=[channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0; channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:49:47.494] [info] [graph][runtime.node] process.failed node=11 name=MediaRtpPacketClockBinderNode kind=RtpPacketClockBinder inputs=[channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=1; channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=10] outputs=[channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=9] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:49:47.495] [info] [graph][runtime.node] process.failed node=26 name=VideoFilterNode kind=VideoFilter inputs=[channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=6] outputs=[channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:49:47.495] [info] [graph][runtime.node] process.failed node=68 name=VideoFilterNode kind=VideoFilter inputs=[channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=6] outputs=[channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=1] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:49:47.495] [info] [graph][runtime.node] process.failed node=22 name=VideoDecodeNode kind=VideoDecode inputs=[channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=61] outputs=[channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:49:47.496] [info] [graph][runtime.node] process.failed node=64 name=VideoDecodeNode kind=VideoDecode inputs=[channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=20] outputs=[channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[CLI] final runtime report: queued=0, peakQueued=624, threads=81, processThreads=86, workers=0, workerProgress=3012, workerProcessCalls=7733, workerWaits=4704, workerWakeups=4646, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=0, logicalProcessors=22, averageSystemMachineCpuPercent=0.000000, averageProcessMachineCpuPercent=0.000000, peakProcessMachineCpuPercent=0.000000, averageProcessSingleCoreCpuPercent=0.000000, peakProcessSingleCoreCpuPercent=0.000000, initialWorkingSetBytes=209219584, workingSetBytes=209219584, peakWorkingSetBytes=209219584, totalPushed=2928, totalPopped=2405, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=122, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=7534953, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=593, graphPayloadReservations=828, graphPayloadReleases=828, graphPayloadPressureFailures=0, lastInputReceivedAtNs=29171410780500
Opened log file 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-cdb.log'

************* Path validation summary **************
Response                         Time (ms)     Location
OK                                             D:/Code/MyCode/MediaTranscode/out/build/x64-release

************* Preparing the environment for Debugger Extensions Gallery repositories **************
   ExtensionRepository : Implicit
   UseExperimentalFeatureForNugetShare : true
   AllowNugetExeUpdate : true
   NonInteractiveNuget : true
   AllowNugetMSCredentialProviderInstall : true
   AllowParallelInitializationOfLocalRepositories : true

   EnableRedirectToV8JsProvider : false

   -- Configuring repositories
      ----> Repository : LocalInstalled, Enabled: true
      ----> Repository : UserExtensions, Enabled: true

>>>>>>>>>>>>> Preparing the environment for Debugger Extensions Gallery repositories completed, duration 0.015 seconds

************* Waiting for Debugger Extensions Gallery to Initialize **************

>>>>>>>>>>>>> Waiting for Debugger Extensions Gallery to Initialize completed, duration 0.016 seconds
   ----> Repository : UserExtensions, Enabled: true, Packages count: 0
   ----> Repository : LocalInstalled, Enabled: true, Packages count: 30

Microsoft (R) Windows Debugger Version 10.0.26100.7175 AMD64
Copyright (c) Microsoft Corporation. All rights reserved.

CommandLine: D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-r297 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2
NatVis script successfully loaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\atlmfc.natvis'
NatVis script successfully loaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\ObjectiveC.natvis'
NatVis script successfully loaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\concurrency.natvis'
NatVis script successfully loaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\cpp_rest.natvis'
NatVis script successfully loaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\stl.natvis'
NatVis script successfully loaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\Windows.Data.Json.natvis'
NatVis script successfully loaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\Windows.Devices.Geolocation.natvis'
NatVis script successfully loaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\Windows.Devices.Sensors.natvis'
NatVis script successfully loaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\Windows.Media.natvis'
NatVis script successfully loaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\windows.natvis'
NatVis script successfully loaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\winrt.natvis'

************* Path validation summary **************
Response                         Time (ms)     Location
OK                                             D:/Code/MyCode/MediaTranscode/out/build/x64-release
Symbol search path is: D:/Code/MyCode/MediaTranscode/out/build/x64-release
Executable search path is: 

+------------------------------------------------------------------------+
| This target supports Hardware-enforced Stack Protection. A HW based    |
| "Shadow Stack" may be available to assist in debugging and analysis.   |
| See aka.ms/userhsp for more info.                                      |
|                                                                        |
| dps @ssp                                                               |
|                                                                        |
+------------------------------------------------------------------------+

ModLoad: 00007ff7`eb770000 00007ff7`ebd74000   image00007ff7`eb770000
ModLoad: 00007ff9`02f60000 00007ff9`031c8000   ntdll.dll
ModLoad: 00007ff9`02340000 00007ff9`0240b000   C:\WINDOWS\System32\KERNEL32.DLL
ModLoad: 00007ff9`00370000 00007ff9`00771000   C:\WINDOWS\System32\KERNELBASE.dll
ModLoad: 00007ff9`01ac0000 00007ff9`01b41000   C:\WINDOWS\System32\WS2_32.dll
ModLoad: 00007ff9`00210000 00007ff9`0035c000   C:\WINDOWS\System32\ucrtbase.dll
ModLoad: 00007ff9`011f0000 00007ff9`01308000   C:\WINDOWS\System32\RPCRT4.dll
ModLoad: 00007fff`97470000 00007fff`9cf66000   D:\Code\MyCode\MediaTranscode\out\build\x64-release\avcodec-62.dll
ModLoad: 00007ff9`02c30000 00007ff9`02cec000   C:\WINDOWS\System32\ADVAPI32.dll
ModLoad: 00007fff`9e5b0000 00007fff`9ee00000   D:\Code\MyCode\MediaTranscode\out\build\x64-release\avutil-60.dll
ModLoad: 00007ff9`01d50000 00007ff9`01df9000   C:\WINDOWS\System32\msvcrt.dll
ModLoad: 00007ff9`00c70000 00007ff9`00d18000   C:\WINDOWS\System32\sechost.dll
ModLoad: 00007ff9`00900000 00007ff9`00a7f000   C:\WINDOWS\System32\CRYPT32.dll
ModLoad: 00007ff9`02160000 00007ff9`022fa000   C:\WINDOWS\System32\ole32.dll
ModLoad: 000001e0`51d90000 000001e0`51f2a000   C:\WINDOWS\System32\ole32.dll
ModLoad: 00007fff`894f0000 00007fff`8aca5000   D:\Code\MyCode\MediaTranscode\out\build\x64-release\avformat-62.dll
ModLoad: 00007ff9`02450000 00007ff9`02627000   C:\WINDOWS\System32\USER32.dll
ModLoad: 00007ff9`00a90000 00007ff9`00b33000   C:\WINDOWS\System32\msvcp_win.dll
ModLoad: 00007ff9`00840000 00007ff9`00867000   C:\WINDOWS\System32\win32u.dll
ModLoad: 00007ff9`01310000 00007ff9`01aac000   C:\WINDOWS\System32\SHELL32.dll
ModLoad: 00007ff9`02300000 00007ff9`0232b000   C:\WINDOWS\System32\GDI32.dll
ModLoad: 000001e0`51a80000 000001e0`51aab000   C:\WINDOWS\System32\GDI32.dll
ModLoad: 00007ff9`00b40000 00007ff9`00c6a000   C:\WINDOWS\System32\gdi32full.dll
ModLoad: 00007ff9`00e60000 00007ff9`011eb000   C:\WINDOWS\System32\combase.dll
ModLoad: 00007fff`9ee00000 00007fff`a255d000   D:\Code\MyCode\MediaTranscode\out\build\x64-release\avfilter-11.dll
ModLoad: 00007ff9`02d30000 00007ff9`02d96000   C:\WINDOWS\System32\SHLWAPI.dll
ModLoad: 00007fff`adb10000 00007fff`adbcb000   D:\Code\MyCode\MediaTranscode\out\build\x64-release\swresample-6.dll
ModLoad: 00007ff8`b7fd0000 00007ff8`b7fe6000   C:\WINDOWS\SYSTEM32\MSVCP140_ATOMIC_WAIT.dll
ModLoad: 00007ff8`e04f0000 00007ff8`e058d000   C:\WINDOWS\SYSTEM32\MSVCP140.dll
ModLoad: 00007ff8`e0090000 00007ff8`e00bc000   C:\WINDOWS\SYSTEM32\VCRUNTIME140.dll
ModLoad: 00007ff8`de540000 00007ff8`de549000   C:\WINDOWS\SYSTEM32\MSVCP140_1.dll
ModLoad: 00007ff8`e00c0000 00007ff8`e00cc000   C:\WINDOWS\SYSTEM32\VCRUNTIME140_1.dll
ModLoad: 00007ff9`00030000 00007ff9`0005a000   C:\WINDOWS\SYSTEM32\bcrypt.dll
ModLoad: 00007fff`9e260000 00007fff`9e5a4000   D:\Code\MyCode\MediaTranscode\out\build\x64-release\libx264-165.dll
ModLoad: 00007ff8`ffa20000 00007ff8`ffa50000   C:\WINDOWS\SYSTEM32\ncrypt.dll
ModLoad: 00007ff8`fe940000 00007ff8`fe973000   C:\WINDOWS\SYSTEM32\IPHLPAPI.DLL
ModLoad: 00007ff8`ef170000 00007ff8`ef1a7000   C:\WINDOWS\SYSTEM32\WINMM.dll
ModLoad: 00007ff8`ffdc0000 00007ff8`ffe12000   C:\WINDOWS\SYSTEM32\CFGMGR32.dll
ModLoad: 00007fff`9e170000 00007fff`9e25f000   D:\Code\MyCode\MediaTranscode\out\build\x64-release\libopenh264-7.dll
ModLoad: 00007fff`967e0000 00007fff`9746b000   D:\Code\MyCode\MediaTranscode\out\build\x64-release\swscale-9.dll
ModLoad: 00007ff8`ff730000 00007ff8`ff73c000   C:\WINDOWS\SYSTEM32\CRYPTBASE.DLL
(3c94.aaf4): Break instruction exception - code 80000003 (first chance)
ntdll!RtlGetReturnAddressHijackTarget+0x749:
00007ff9`030826c9 cc              int     3
0:000> sxe -c ".exr -1;kb;gn" eh;sxe -c2 ".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r297-crash.dmp;qd" 0xc0000409;g
ModLoad: 00007ff9`02410000 00007ff9`02441000   C:\WINDOWS\System32\IMM32.DLL
ModLoad: 00007ff9`00780000 00007ff9`0082c000   C:\WINDOWS\System32\bcryptPrimitives.dll
ModLoad: 00007ff8`ff9d0000 00007ff8`ffa0f000   C:\WINDOWS\SYSTEM32\NTASN1.dll
ModLoad: 00007ff8`fee60000 00007ff8`fee7b000   C:\WINDOWS\SYSTEM32\kernel.appcore.dll
ModLoad: 00007ff8`ff420000 00007ff8`ff48c000   C:\WINDOWS\system32\mswsock.dll
ModLoad: 00007fff`95f10000 00007fff`967d4000   D:\Code\MyCode\MediaTranscode\out\build\x64-release\libx265_main12.dll
ModLoad: 00007ff8`cfa50000 00007ff8`cfecf000   C:\WINDOWS\SYSTEM32\nvcuda.dll
ModLoad: 00007ff8`fc890000 00007ff8`fc8d4000   C:\WINDOWS\SYSTEM32\dxcore.dll
ModLoad: 00007ff8`cd170000 00007ff8`cf04f000   C:\WINDOWS\system32\DriverStore\FileRepository\nvami.inf_amd64_97bcbdbdb2ab507c\nvcuda64.dll
ModLoad: 00007ff8`f77f0000 00007ff8`f77fb000   C:\WINDOWS\SYSTEM32\VERSION.dll
ModLoad: 00007ff8`ff790000 00007ff8`ff7a3000   C:\WINDOWS\SYSTEM32\msasn1.dll
ModLoad: 00007ff8`f7150000 00007ff8`f718b000   C:\WINDOWS\SYSTEM32\cryptnet.dll
ModLoad: 00007ff8`ff8c0000 00007ff8`ff941000   C:\WINDOWS\SYSTEM32\wldp.dll
ModLoad: 00007ff8`f6fd0000 00007ff8`f714a000   C:\WINDOWS\SYSTEM32\drvstore.dll
ModLoad: 00007ff8`ffd80000 00007ff8`ffdaf000   C:\WINDOWS\SYSTEM32\devobj.dll
ModLoad: 00007ff8`e4290000 00007ff8`e4319000   C:\WINDOWS\SYSTEM32\nvapi64.dll
ModLoad: 00007ff8`fdb60000 00007ff8`fe40e000   C:\WINDOWS\SYSTEM32\windows.storage.dll
ModLoad: 00007ff9`01f70000 00007ff9`02069000   C:\WINDOWS\System32\SHCORE.dll
ModLoad: 00007ff8`e3970000 00007ff8`e3ee7000   C:\WINDOWS\system32\DriverStore\FileRepository\nvami.inf_amd64_97bcbdbdb2ab507c\nvapi64_impl.dll
ModLoad: 00007ff9`027a0000 00007ff9`02c27000   C:\WINDOWS\System32\SETUPAPI.dll
ModLoad: 00007ff9`01e00000 00007ff9`01ed9000   C:\WINDOWS\System32\OLEAUT32.dll
ModLoad: 00007ff8`ccf20000 00007ff8`ccfcf000   C:\WINDOWS\System32\DriverStore\FileRepository\nvami.inf_amd64_97bcbdbdb2ab507c\nvdxgdmal64.dll
ModLoad: 00007ff8`cbed0000 00007ff8`cccd2000   C:\WINDOWS\System32\DriverStore\FileRepository\nvami.inf_amd64_97bcbdbdb2ab507c\nvobjectloader64.dll
ModLoad: 00007fff`84280000 00007fff`86133000   C:\WINDOWS\SYSTEM32\nvcuvid.dll
ModLoad: 00007fff`9dfc0000 00007fff`9e0c5000   C:\WINDOWS\SYSTEM32\nvEncodeAPI64.dll
ModLoad: 00007ff9`00870000 00007ff9`008f2000   C:\WINDOWS\System32\WINTRUST.dll
ModLoad: 00007ff9`02150000 00007ff9`0215a000   C:\WINDOWS\System32\NSI.dll
(3c94.aaf4): C++ EH exception - code e06d7363 (first chance)
ExceptionAddress: 00007ff90043408a (KERNELBASE!RaiseException+0x000000000000008a)
   ExceptionCode: e06d7363 (C++ EH exception)
  ExceptionFlags: 00000081
NumberParameters: 4
   Parameter[0]: 0000000019930520
   Parameter[1]: 0000005ac78bee70
   Parameter[2]: 00007ff7ebd33e00
   Parameter[3]: 00007ff7eb770000
!cppexr ffffffffffffffff
*************************************************************************
***                                                                   ***
***                                                                   ***
***    Either you specified an unqualified symbol, or your debugger   ***
***    doesn't have full symbol information.  Unqualified symbol      ***
***    resolution is turned off by default. Please either specify a   ***
***    fully qualified symbol module!symbolname, or enable resolution ***
***    of unqualified symbols by typing ".symopt- 100". Note that     ***
***    enabling unqualified symbol resolution with network symbol     ***
***    server shares in the symbol path may cause the debugger to     ***
***    appear to hang for long periods of time when an incorrect      ***
***    symbol name is typed or the network symbol server is down.     ***
***                                                                   ***
***    For some commands to work properly, your symbol path           ***
***    must point to .pdb files that have full type information.      ***
***                                                                   ***
***    Certain .pdb files (such as the public OS symbols) do not      ***
***    contain the required information.  Contact the group that      ***
***    provided you with these symbols if you need this command to    ***
***    work.                                                          ***
***                                                                   ***
***    Type referenced: msvcrt!EHExceptionRecord                      ***
***                                                                   ***
*************************************************************************
*************************************************************************
***                                                                   ***
***                                                                   ***
***    Either you specified an unqualified symbol, or your debugger   ***
***    doesn't have full symbol information.  Unqualified symbol      ***
***    resolution is turned off by default. Please either specify a   ***
***    fully qualified symbol module!symbolname, or enable resolution ***
***    of unqualified symbols by typing ".symopt- 100". Note that     ***
***    enabling unqualified symbol resolution with network symbol     ***
***    server shares in the symbol path may cause the debugger to     ***
***    appear to hang for long periods of time when an incorrect      ***
***    symbol name is typed or the network symbol server is down.     ***
***                                                                   ***
***    For some commands to work properly, your symbol path           ***
***    must point to .pdb files that have full type information.      ***
***                                                                   ***
***    Certain .pdb files (such as the public OS symbols) do not      ***
***    contain the required information.  Contact the group that      ***
***    provided you with these symbols if you need this command to    ***
***    work.                                                          ***
***                                                                   ***
***    Type referenced: ucrtbase!EHExceptionRecord                    ***
***                                                                   ***
*************************************************************************
*************************************************************************
***                                                                   ***
***                                                                   ***
***    Either you specified an unqualified symbol, or your debugger   ***
***    doesn't have full symbol information.  Unqualified symbol      ***
***    resolution is turned off by default. Please either specify a   ***
***    fully qualified symbol module!symbolname, or enable resolution ***
***    of unqualified symbols by typing ".symopt- 100". Note that     ***
***    enabling unqualified symbol resolution with network symbol     ***
***    server shares in the symbol path may cause the debugger to     ***
***    appear to hang for long periods of time when an incorrect      ***
***    symbol name is typed or the network symbol server is down.     ***
***                                                                   ***
***    For some commands to work properly, your symbol path           ***
***    must point to .pdb files that have full type information.      ***
***                                                                   ***
***    Certain .pdb files (such as the public OS symbols) do not      ***
***    contain the required information.  Contact the group that      ***
***    provided you with these symbols if you need this command to    ***
***    work.                                                          ***
***                                                                   ***
***    Type referenced: EHExceptionRecord                             ***
***                                                                   ***
*************************************************************************
unable to find C-Runtime symbols, even with unqualified search
RetAddr               : Args to Child                                                           : Call Site
00007ff8`e00955a9     : 000001e0`7aa87d70 0000005a`c78c16c0 00007ff7`ebc6dc00 000001e0`51be0f00 : KERNELBASE!RaiseException+0x8a
*** WARNING: Unable to verify checksum for D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe
00007ff7`ebb24df6     : 00000000`0000086a 0000005a`c78c8fa8 000001e0`791d5660 0000005a`c78bf0a8 : VCRUNTIME140!CxxThrowException+0x99
00007ff7`eb947514     : 00007ff9`003498b0 0000005a`c78bf200 000001e0`791d5660 00007ff7`ebc2c50e : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x106bd6
00007ff7`eb946650     : 00000000`0000000f 000001e0`66811a01 00000000`00000004 00007ff7`ebb24d74 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x2af4
00007ff7`eb946948     : 0000005a`c78bf200 0000005a`c78bf200 00000000`00000050 000001e0`51ae0000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x1c30
00007ff7`eb9477de     : 0000005a`c78bf200 00000000`00000000 0000005a`c78bf200 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x1f28
00007ff7`eb9c6842     : 0000005a`c78c8ee8 00000000`00000004 00000000`000003d9 00000000`0000000f : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x2dbe
00007ff7`eb9c75aa     : 0000005a`c78c8f78 0000005a`c78c8e90 00000000`0000001c 00007ff7`eb896d09 : media_transcode_realtime_video_cli!`anonymous namespace'::_Reversing::_Reverse_tail<unsigned __int64 *>+0x72
00007ff7`eb894001     : 00000000`00000001 00000000`00000000 00000000`00000000 0000005a`c78c1e50 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_cmpeq<`anonymous namespace'::_Find_seq::_Find_seq_traits_avx_4,unsigned int>+0x41a
00007ff7`eb7c2e10     : 000001e0`51bd4be0 00000001`0000001e 000001e0`51bd4be0 000001e0`51bd4be0 : media_transcode_realtime_video_cli!std::_Verify_range+0x11
00007ff7`eb796bba     : 0000005a`c78f0040 0000005a`c78fe7f0 00000000`00000000 0000005a`c78fe7f0 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaVideoTranscodeParameters::MediaVideoTranscodeParameters+0xc0
00007ff7`eb79c788     : 00000000`00000002 000001e0`51ae0100 0000005a`c78fef20 000001e0`51bcda00 : media_transcode_realtime_video_cli!std::operator<<<std::char_traits<char> >+0x3fa
00007ff7`eb7820fc     : 0000005a`c78fef20 ffffffff`a10e52c2 000001e0`51afc430 000001e0`51bcda00 : media_transcode_realtime_video_cli!std::_Traits_copy_batch<std::char_traits<char>,char>+0x18
00007ff7`eb7832c9     : 00007ff7`ebc5a050 00007ff7`ebc2ccfe 000001e0`51bcda00 00000000`00000000 : media_transcode_realtime_video_cli!thunk@7ff7eb77d42c+0x4cd0
00007ff7`ebc2ca2f     : 00007ff7`ebcc5240 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!thunk@7ff7eb77d42c+0x5e9d
00007ff9`0236dda7     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x20e80f
00007ff9`02fdee4c     : 00000000`00000000 00000000`00000000 000004f0`fffffb30 000004d0`fffffb30 : KERNEL32!BaseThreadInitThunk+0x17
00000000`00000000     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ntdll!RtlUserThreadStart+0x2c
(3c94.aaf4): C++ EH exception - code e06d7363 (first chance)
ExceptionAddress: 00007ff90043408a (KERNELBASE!RaiseException+0x000000000000008a)
   ExceptionCode: e06d7363 (C++ EH exception)
  ExceptionFlags: 00000081
NumberParameters: 4
   Parameter[0]: 0000000019930520
   Parameter[1]: 0000005ac78bee80
   Parameter[2]: 00007ff7ebd33e00
   Parameter[3]: 00007ff7eb770000
!cppexr ffffffffffffffff
*************************************************************************
***                                                                   ***
***                                                                   ***
***    Either you specified an unqualified symbol, or your debugger   ***
***    doesn't have full symbol information.  Unqualified symbol      ***
***    resolution is turned off by default. Please either specify a   ***
***    fully qualified symbol module!symbolname, or enable resolution ***
***    of unqualified symbols by typing ".symopt- 100". Note that     ***
***    enabling unqualified symbol resolution with network symbol     ***
***    server shares in the symbol path may cause the debugger to     ***
***    appear to hang for long periods of time when an incorrect      ***
***    symbol name is typed or the network symbol server is down.     ***
***                                                                   ***
***    For some commands to work properly, your symbol path           ***
***    must point to .pdb files that have full type information.      ***
***                                                                   ***
***    Certain .pdb files (such as the public OS symbols) do not      ***
***    contain the required information.  Contact the group that      ***
***    provided you with these symbols if you need this command to    ***
***    work.                                                          ***
***                                                                   ***
***    Type referenced: msvcrt!EHExceptionRecord                      ***
***                                                                   ***
*************************************************************************
*************************************************************************
***                                                                   ***
***                                                                   ***
***    Either you specified an unqualified symbol, or your debugger   ***
***    doesn't have full symbol information.  Unqualified symbol      ***
***    resolution is turned off by default. Please either specify a   ***
***    fully qualified symbol module!symbolname, or enable resolution ***
***    of unqualified symbols by typing ".symopt- 100". Note that     ***
***    enabling unqualified symbol resolution with network symbol     ***
***    server shares in the symbol path may cause the debugger to     ***
***    appear to hang for long periods of time when an incorrect      ***
***    symbol name is typed or the network symbol server is down.     ***
***                                                                   ***
***    For some commands to work properly, your symbol path           ***
***    must point to .pdb files that have full type information.      ***
***                                                                   ***
***    Certain .pdb files (such as the public OS symbols) do not      ***
***    contain the required information.  Contact the group that      ***
***    provided you with these symbols if you need this command to    ***
***    work.                                                          ***
***                                                                   ***
***    Type referenced: ucrtbase!EHExceptionRecord                    ***
***                                                                   ***
*************************************************************************
RetAddr               : Args to Child                                                           : Call Site
00007ff8`e00955a9     : 00007ff7`ebc6dc00 000001e0`51be0f00 00007ff7`eb770000 00000000`19930520 : KERNELBASE!RaiseException+0x8a
00007ff7`ebb24df6     : 00000000`00000018 00007ff9`002150d9 000001e0`791d5660 0000005a`c78bf058 : VCRUNTIME140!CxxThrowException+0x99
00007ff7`ebb6bc8a     : 000001e0`791d5660 00007ff7`ebc2c50e 0000005a`c78bef28 00007ff7`ebb24c2d : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x106bd6
00007ff7`ebb6c138     : 0000005a`c78bf1b0 00000000`00000004 00000000`00000004 00007ff7`eb9468b6 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x14da6a
00007ff7`ebb700fe     : 0000005a`c78bf1b0 00000000`00000000 0000005a`c78bf1b0 00000000`00000000 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x14df18
00007ff7`eb9c6900     : 0000005a`c78c8ee8 00000000`00000004 00000000`000003d9 00000000`0000000f : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x151ede
00007ff7`eb9c75aa     : 0000005a`c78c8f78 0000005a`c78c8e90 00000000`0000001c 00007ff7`eb896d09 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_cmpeq<`anonymous namespace'::_Find_seq::_Find_seq_traits_avx_2,unsigned short>+0xa0
00007ff7`eb894001     : 00000000`00000001 00000000`00000000 00000000`00000000 0000005a`c78c1e50 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_cmpeq<`anonymous namespace'::_Find_seq::_Find_seq_traits_avx_4,unsigned int>+0x41a
00007ff7`eb7c2e10     : 000001e0`51bd4be0 00000001`0000001e 000001e0`51bd4be0 000001e0`51bd4be0 : media_transcode_realtime_video_cli!std::_Verify_range+0x11
00007ff7`eb796bba     : 0000005a`c78f0040 0000005a`c78fe7f0 00000000`00000000 0000005a`c78fe7f0 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaVideoTranscodeParameters::MediaVideoTranscodeParameters+0xc0
00007ff7`eb79c788     : 00000000`00000002 000001e0`51ae0100 0000005a`c78fef20 000001e0`51bcda00 : media_transcode_realtime_video_cli!std::operator<<<std::char_traits<char> >+0x3fa
00007ff7`eb7820fc     : 0000005a`c78fef20 ffffffff`a10e52c2 000001e0`51afc430 000001e0`51bcda00 : media_transcode_realtime_video_cli!std::_Traits_copy_batch<std::char_traits<char>,char>+0x18
00007ff7`eb7832c9     : 00007ff7`ebc5a050 00007ff7`ebc2ccfe 000001e0`51bcda00 00000000`00000000 : media_transcode_realtime_video_cli!thunk@7ff7eb77d42c+0x4cd0
00007ff7`ebc2ca2f     : 00007ff7`ebcc5240 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!thunk@7ff7eb77d42c+0x5e9d
00007ff9`0236dda7     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x20e80f
00007ff9`02fdee4c     : 00000000`00000000 00000000`00000000 000004f0`fffffb30 000004d0`fffffb30 : KERNEL32!BaseThreadInitThunk+0x17
00000000`00000000     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ntdll!RtlUserThreadStart+0x2c
(3c94.aaf4): C++ EH exception - code e06d7363 (first chance)
ExceptionAddress: 00007ff90043408a (KERNELBASE!RaiseException+0x000000000000008a)
   ExceptionCode: e06d7363 (C++ EH exception)
  ExceptionFlags: 00000081
NumberParameters: 4
   Parameter[0]: 0000000019930520
   Parameter[1]: 0000005ac78be960
   Parameter[2]: 00007ff7ebd33e00
   Parameter[3]: 00007ff7eb770000
!cppexr ffffffffffffffff
RetAddr               : Args to Child                                                           : Call Site
00007ff8`e00955a9     : 0000005a`c78bf6a0 00007ff7`eb796037 0000005a`c78bf6a0 00007ff7`eb796037 : KERNELBASE!RaiseException+0x8a
00007ff7`ebb24df6     : 00000000`0000086a 0000005a`c78c00c0 000001e0`791d5660 0000005a`c78beb98 : VCRUNTIME140!CxxThrowException+0x99
00007ff7`eb947514     : 00007ff9`003498b0 0000005a`c78becf0 000001e0`791d5660 00007ff7`ebc2c50e : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x106bd6
00007ff7`eb946650     : 00000000`00000138 00000000`00000000 00000000`00000004 00007ff7`ebb24d74 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x2af4
00007ff7`eb946948     : 0000005a`c78becf0 0000005a`c78becf0 00000170`00000000 0000005a`c78bec80 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x1c30
00007ff7`eb9477de     : 0000005a`c78becf0 00007ff7`eb9c952e 0000005a`c78becf0 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x1f28
00007ff7`eb9c6842     : 00000000`00000000 000059ce`a10a5f52 00000000`000005c0 00000000`00000004 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x2dbe
00007ff7`eb9c9aa7     : 00000000`00000000 0000005a`c78bfc00 00000000`00000040 00000000`00000000 : media_transcode_realtime_video_cli!`anonymous namespace'::_Reversing::_Reverse_tail<unsigned __int64 *>+0x72
00007ff7`eb9c88f0     : 0000005a`c78c0180 00000000`00000000 00000000`00000000 00007ff7`eb794010 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_impl<`anonymous namespace'::_Finding::_Find_traits_1,void,void,unsigned char>+0x4f7
00007ff7`eb894001     : 00000000`00000001 00000000`00000000 00000000`00000000 0000005a`c78c1e50 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_cmpeq<`anonymous namespace'::_Find_seq::_Find_seq_traits_sse_4,unsigned int>+0x620
00007ff7`eb7c2e10     : 000001e0`51bd4be0 00000001`0000001e 000001e0`51bd4be0 000001e0`51bd4be0 : media_transcode_realtime_video_cli!std::_Verify_range+0x11
00007ff7`eb796bba     : 0000005a`c78f0040 0000005a`c78fe7f0 00000000`00000000 0000005a`c78fe7f0 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaVideoTranscodeParameters::MediaVideoTranscodeParameters+0xc0
00007ff7`eb79c788     : 00000000`00000002 000001e0`51ae0100 0000005a`c78fef20 000001e0`51bcda00 : media_transcode_realtime_video_cli!std::operator<<<std::char_traits<char> >+0x3fa
00007ff7`eb7820fc     : 0000005a`c78fef20 ffffffff`a10e52c2 000001e0`51afc430 000001e0`51bcda00 : media_transcode_realtime_video_cli!std::_Traits_copy_batch<std::char_traits<char>,char>+0x18
00007ff7`eb7832c9     : 00007ff7`ebc5a050 00007ff7`ebc2ccfe 000001e0`51bcda00 00000000`00000000 : media_transcode_realtime_video_cli!thunk@7ff7eb77d42c+0x4cd0
00007ff7`ebc2ca2f     : 00007ff7`ebcc5240 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!thunk@7ff7eb77d42c+0x5e9d
00007ff9`0236dda7     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x20e80f
00007ff9`02fdee4c     : 00000000`00000000 00000000`00000000 000004f0`fffffb30 000004d0`fffffb30 : KERNEL32!BaseThreadInitThunk+0x17
00000000`00000000     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ntdll!RtlUserThreadStart+0x2c
(3c94.aaf4): C++ EH exception - code e06d7363 (first chance)
ExceptionAddress: 00007ff90043408a (KERNELBASE!RaiseException+0x000000000000008a)
   ExceptionCode: e06d7363 (C++ EH exception)
  ExceptionFlags: 00000081
NumberParameters: 4
   Parameter[0]: 0000000019930520
   Parameter[1]: 0000005ac78be970
   Parameter[2]: 00007ff7ebd33e00
   Parameter[3]: 00007ff7eb770000
!cppexr ffffffffffffffff
RetAddr               : Args to Child                                                           : Call Site
00007ff8`e00955a9     : 0000005a`c78bf6a0 00007ff7`eb796037 00007ff7`eb770000 00000000`19930520 : KERNELBASE!RaiseException+0x8a
00007ff7`ebb24df6     : 00000000`00000018 00007ff9`002150d9 000001e0`791d5e10 0000005a`c78beb48 : VCRUNTIME140!CxxThrowException+0x99
00007ff7`ebb6bc8a     : 000001e0`791d5660 00007ff7`ebc2c50e 0000005a`c78bea18 00007ff7`ebb24c2d : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x106bd6
00007ff7`ebb6c138     : 0000005a`c78beca0 00000000`00000004 00000000`00000004 00007ff7`eb9468b6 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x14da6a
00007ff7`ebb700fe     : 0000005a`c78beca0 00007ff7`eb9c952e 0000005a`c78beca0 00000000`00000000 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x14df18
00007ff7`eb9c6900     : 00000000`00000000 000059ce`a10a5f52 00000000`000005c0 00000000`00000004 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x151ede
00007ff7`eb9c9aa7     : 00000000`00000000 0000005a`c78bfc00 00000000`00000040 00000000`00000000 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_cmpeq<`anonymous namespace'::_Find_seq::_Find_seq_traits_avx_2,unsigned short>+0xa0
00007ff7`eb9c88f0     : 0000005a`c78c0180 00000000`00000000 00000000`00000000 00007ff7`eb794010 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_impl<`anonymous namespace'::_Finding::_Find_traits_1,void,void,unsigned char>+0x4f7
00007ff7`eb894001     : 00000000`00000001 00000000`00000000 00000000`00000000 0000005a`c78c1e50 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_cmpeq<`anonymous namespace'::_Find_seq::_Find_seq_traits_sse_4,unsigned int>+0x620
00007ff7`eb7c2e10     : 000001e0`51bd4be0 00000001`0000001e 000001e0`51bd4be0 000001e0`51bd4be0 : media_transcode_realtime_video_cli!std::_Verify_range+0x11
00007ff7`eb796bba     : 0000005a`c78f0040 0000005a`c78fe7f0 00000000`00000000 0000005a`c78fe7f0 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaVideoTranscodeParameters::MediaVideoTranscodeParameters+0xc0
00007ff7`eb79c788     : 00000000`00000002 000001e0`51ae0100 0000005a`c78fef20 000001e0`51bcda00 : media_transcode_realtime_video_cli!std::operator<<<std::char_traits<char> >+0x3fa
00007ff7`eb7820fc     : 0000005a`c78fef20 ffffffff`a10e52c2 000001e0`51afc430 000001e0`51bcda00 : media_transcode_realtime_video_cli!std::_Traits_copy_batch<std::char_traits<char>,char>+0x18
00007ff7`eb7832c9     : 00007ff7`ebc5a050 00007ff7`ebc2ccfe 000001e0`51bcda00 00000000`00000000 : media_transcode_realtime_video_cli!thunk@7ff7eb77d42c+0x4cd0
00007ff7`ebc2ca2f     : 00007ff7`ebcc5240 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!thunk@7ff7eb77d42c+0x5e9d
00007ff9`0236dda7     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x20e80f
00007ff9`02fdee4c     : 00000000`00000000 00000000`00000000 000004f0`fffffb30 000004d0`fffffb30 : KERNEL32!BaseThreadInitThunk+0x17
00000000`00000000     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ntdll!RtlUserThreadStart+0x2c
ModLoad: 00007fff`9de00000 00007fff`9df05000   C:\WINDOWS\SYSTEM32\nvEncodeAPI64.dll
(3c94.b898): C++ EH exception - code e06d7363 (first chance)
ExceptionAddress: 00007ff90043408a (KERNELBASE!RaiseException+0x000000000000008a)
   ExceptionCode: e06d7363 (C++ EH exception)
  ExceptionFlags: 00000081
NumberParameters: 4
   Parameter[0]: 0000000019930520
   Parameter[1]: 0000005accafcbd0
   Parameter[2]: 00007ff7ebd33e00
   Parameter[3]: 00007ff7eb770000
!cppexr ffffffffffffffff
RetAddr               : Args to Child                                                           : Call Site
00007ff8`e00955a9     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : KERNELBASE!RaiseException+0x8a
00007ff7`ebb24df6     : 00000000`0000086a 000001e0`794a5f08 000001e0`0f3c5230 0000005a`ccafce08 : VCRUNTIME140!CxxThrowException+0x99
00007ff7`eb947514     : 00007ff9`003498b0 0000005a`ccafcf60 000001e0`0f3c5230 00007ff7`ebc2c50e : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x106bd6
00007ff7`eb946650     : 00000000`00000000 00000000`00000000 00000000`00000004 00007ff7`ebb24d74 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x2af4
00007ff7`eb946948     : 0000005a`ccafcf60 0000005a`ccafcf60 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x1c30
00007ff7`eb9477de     : 0000005a`ccafcf60 00000000`00000000 0000005a`ccafcf60 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x1f28
00007ff7`eb9c6842     : 000001e0`794a5e30 00000000`00000004 00000000`0000038d 00000000`0000000f : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x2dbe
00007ff7`eb9c75aa     : 000001e0`794a5ed8 000001e0`794a5dd8 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!`anonymous namespace'::_Reversing::_Reverse_tail<unsigned __int64 *>+0x72
00007ff7`ebb721b6     : 00000000`00000000 00000000`00000000 000001e0`794a5c60 0000005a`ccafee50 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_cmpeq<`anonymous namespace'::_Find_seq::_Find_seq_traits_avx_4,unsigned int>+0x41a
00007ff7`ebb9baaa     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x153f96
00007ff7`eba3f827     : 0000005a`ccaff430 000001e0`794a5c60 00000000`00000001 0000005a`ccaff5c8 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x17d88a
00007ff7`eb92ab42     : 00000000`00000000 00000000`00000000 00000000`00000000 0000005a`ccaffac0 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x21607
00007ff7`eb92ddb2     : 00000000`00000000 00000000`00000000 0000005a`ccaffac0 0000005a`ccaffac0 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaVideoPlanOptionApplier::applySelectedPlan+0x4b2
00007ff7`eb92dace     : 000001e0`7aac2300 00007ff7`eb86098e 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::`anonymous namespace'::setOption+0x132
00007ff7`eb80165c     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaVideoTranscodeOptionApplier::applyUserOptions+0xce
00007ff7`eb7ff72f     : 00000000`00000000 00000000`00000000 ffffffff`fffffffe 00000000`00000000 : media_transcode_realtime_video_cli!fmt::v11::detail::copy<unsigned int,unsigned int const *,unsigned int *,0>+0x7c
00007ff9`0023cd30     : 000001e0`00ca31c0 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!fmt::v11::detail::basic_fp<fmt::v11::detail::uint128_fallback>::assign<float,0>+0x1f
00007ff9`0236dda7     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ucrtbase!wcsrchr+0x150
00007ff9`02fdee4c     : 00000000`00000000 00000000`00000000 000004f0`fffffb30 000004d0`fffffb30 : KERNEL32!BaseThreadInitThunk+0x17
00000000`00000000     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ntdll!RtlUserThreadStart+0x2c
(3c94.b898): C++ EH exception - code e06d7363 (first chance)
ExceptionAddress: 00007ff90043408a (KERNELBASE!RaiseException+0x000000000000008a)
   ExceptionCode: e06d7363 (C++ EH exception)
  ExceptionFlags: 00000081
NumberParameters: 4
   Parameter[0]: 0000000019930520
   Parameter[1]: 0000005accafcbe0
   Parameter[2]: 00007ff7ebd33e00
   Parameter[3]: 00007ff7eb770000
!cppexr ffffffffffffffff
RetAddr               : Args to Child                                                           : Call Site
00007ff8`e00955a9     : 00000000`00000000 00000000`00000000 00007ff7`eb770000 00000000`19930520 : KERNELBASE!RaiseException+0x8a
00007ff7`ebb24df6     : 00000000`00000018 00007ff9`002150d9 000001e0`79487260 0000005a`ccafcdb8 : VCRUNTIME140!CxxThrowException+0x99
00007ff7`ebb6bc8a     : 000001e0`0f3c5230 00007ff7`ebc2c50e 0000005a`ccafcc88 00007ff7`ebb24c2d : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x106bd6
00007ff7`ebb6c138     : 0000005a`ccafcf10 00000000`00000004 00000000`00000004 00007ff7`eb9468b6 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x14da6a
00007ff7`ebb700fe     : 0000005a`ccafcf10 00000000`00000000 0000005a`ccafcf10 00000000`00000000 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x14df18
00007ff7`eb9c6900     : 000001e0`794a5e30 00000000`00000004 00000000`0000038d 00000000`0000000f : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x151ede
00007ff7`eb9c75aa     : 000001e0`794a5ed8 000001e0`794a5dd8 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_cmpeq<`anonymous namespace'::_Find_seq::_Find_seq_traits_avx_2,unsigned short>+0xa0
00007ff7`ebb721b6     : 00000000`00000000 00000000`00000000 000001e0`794a5c60 0000005a`ccafee50 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_cmpeq<`anonymous namespace'::_Find_seq::_Find_seq_traits_avx_4,unsigned int>+0x41a
00007ff7`ebb9baaa     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x153f96
00007ff7`eba3f827     : 0000005a`ccaff430 000001e0`794a5c60 00000000`00000001 0000005a`ccaff5c8 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x17d88a
00007ff7`eb92ab42     : 00000000`00000000 00000000`00000000 00000000`00000000 0000005a`ccaffac0 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x21607
00007ff7`eb92ddb2     : 00000000`00000000 00000000`00000000 0000005a`ccaffac0 0000005a`ccaffac0 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaVideoPlanOptionApplier::applySelectedPlan+0x4b2
00007ff7`eb92dace     : 000001e0`7aac2300 00007ff7`eb86098e 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::`anonymous namespace'::setOption+0x132
00007ff7`eb80165c     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaVideoTranscodeOptionApplier::applyUserOptions+0xce
00007ff7`eb7ff72f     : 00000000`00000000 00000000`00000000 ffffffff`fffffffe 00000000`00000000 : media_transcode_realtime_video_cli!fmt::v11::detail::copy<unsigned int,unsigned int const *,unsigned int *,0>+0x7c
00007ff9`0023cd30     : 000001e0`00ca31c0 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!fmt::v11::detail::basic_fp<fmt::v11::detail::uint128_fallback>::assign<float,0>+0x1f
00007ff9`0236dda7     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ucrtbase!wcsrchr+0x150
00007ff9`02fdee4c     : 00000000`00000000 00000000`00000000 000004f0`fffffb30 000004d0`fffffb30 : KERNEL32!BaseThreadInitThunk+0x17
00000000`00000000     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ntdll!RtlUserThreadStart+0x2c
(3c94.b898): C++ EH exception - code e06d7363 (first chance)
ExceptionAddress: 00007ff90043408a (KERNELBASE!RaiseException+0x000000000000008a)
   ExceptionCode: e06d7363 (C++ EH exception)
  ExceptionFlags: 00000081
NumberParameters: 4
   Parameter[0]: 0000000019930520
   Parameter[1]: 0000005accafc6c0
   Parameter[2]: 00007ff7ebd33e00
   Parameter[3]: 00007ff7eb770000
!cppexr ffffffffffffffff
RetAddr               : Args to Child                                                           : Call Site
00007ff8`e00955a9     : 0000005a`ccafd400 00007ff7`eb796037 0000005a`ccafd400 00007ff7`eb796037 : KERNELBASE!RaiseException+0x8a
00007ff7`ebb24df6     : 00000000`0000086a 0000005a`ccafe740 000001e0`79487470 0000005a`ccafc8f8 : VCRUNTIME140!CxxThrowException+0x99
00007ff7`eb947514     : 00007ff9`003498b0 0000005a`ccafca50 000001e0`79487470 00007ff7`ebc2c50e : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x106bd6
00007ff7`eb946650     : 00000000`00000138 00000000`00000000 00000000`00000004 00007ff7`ebb24d74 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x2af4
00007ff7`eb946948     : 0000005a`ccafca50 0000005a`ccafca50 000001e0`00000000 0000005a`ccafc9e0 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x1c30
00007ff7`eb9477de     : 0000005a`ccafca50 00007ff7`eb9c952e 0000005a`ccafca50 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x1f28
00007ff7`eb9c6842     : 00000000`00000000 000059ce`aa2e7c32 00000000`00000d24 00000000`00000004 : media_transcode_realtime_video_cli!media::ffmpeg::graph::CodecResolverEncoderContextBuilder::build+0x2dbe
00007ff7`eb9c9aa7     : 00000000`00000000 0000005a`ccafd960 00000000`00000040 00000000`00000000 : media_transcode_realtime_video_cli!`anonymous namespace'::_Reversing::_Reverse_tail<unsigned __int64 *>+0x72
00007ff7`eb9c88f0     : 0000005a`ccaff410 00000000`00000000 00000000`00000000 00007ff7`eb794010 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_impl<`anonymous namespace'::_Finding::_Find_traits_1,void,void,unsigned char>+0x4f7
00007ff7`ebb721b6     : 00000000`00000000 00000000`00000000 000001e0`794a5c60 0000005a`ccafee50 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_cmpeq<`anonymous namespace'::_Find_seq::_Find_seq_traits_sse_4,unsigned int>+0x620
00007ff7`ebb9baaa     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x153f96
00007ff7`eba3f827     : 0000005a`ccaff430 000001e0`794a5c60 00000000`00000001 0000005a`ccaff5c8 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x17d88a
00007ff7`eb92ab42     : 00000000`00000000 00000000`00000000 00000000`00000000 0000005a`ccaffac0 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x21607
00007ff7`eb92ddb2     : 00000000`00000000 00000000`00000000 0000005a`ccaffac0 0000005a`ccaffac0 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaVideoPlanOptionApplier::applySelectedPlan+0x4b2
00007ff7`eb92dace     : 000001e0`7aac2300 00007ff7`eb86098e 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::`anonymous namespace'::setOption+0x132
00007ff7`eb80165c     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaVideoTranscodeOptionApplier::applyUserOptions+0xce
00007ff7`eb7ff72f     : 00000000`00000000 00000000`00000000 ffffffff`fffffffe 00000000`00000000 : media_transcode_realtime_video_cli!fmt::v11::detail::copy<unsigned int,unsigned int const *,unsigned int *,0>+0x7c
00007ff9`0023cd30     : 000001e0`00ca31c0 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!fmt::v11::detail::basic_fp<fmt::v11::detail::uint128_fallback>::assign<float,0>+0x1f
00007ff9`0236dda7     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ucrtbase!wcsrchr+0x150
00007ff9`02fdee4c     : 00000000`00000000 00000000`00000000 000004f0`fffffb30 000004d0`fffffb30 : KERNEL32!BaseThreadInitThunk+0x17
00000000`00000000     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ntdll!RtlUserThreadStart+0x2c
(3c94.b898): C++ EH exception - code e06d7363 (first chance)
ExceptionAddress: 00007ff90043408a (KERNELBASE!RaiseException+0x000000000000008a)
   ExceptionCode: e06d7363 (C++ EH exception)
  ExceptionFlags: 00000081
NumberParameters: 4
   Parameter[0]: 0000000019930520
   Parameter[1]: 0000005accafc6d0
   Parameter[2]: 00007ff7ebd33e00
   Parameter[3]: 00007ff7eb770000
!cppexr ffffffffffffffff
RetAddr               : Args to Child                                                           : Call Site
00007ff8`e00955a9     : 0000005a`ccafd400 00007ff7`eb796037 00007ff7`eb770000 00000000`19930520 : KERNELBASE!RaiseException+0x8a
00007ff7`ebb24df6     : 00000000`00000018 00007ff9`002150d9 000001e0`794872c0 0000005a`ccafc8a8 : VCRUNTIME140!CxxThrowException+0x99
00007ff7`ebb6bc8a     : 000001e0`79487470 00007ff7`ebc2c50e 0000005a`ccafc778 00007ff7`ebb24c2d : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x106bd6
00007ff7`ebb6c138     : 0000005a`ccafca00 00000000`00000004 00000000`00000004 00007ff7`eb9468b6 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x14da6a
00007ff7`ebb700fe     : 0000005a`ccafca00 00007ff7`eb9c952e 0000005a`ccafca00 00000000`00000000 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x14df18
00007ff7`eb9c6900     : 00000000`00000000 000059ce`aa2e7c32 00000000`00000d24 00000000`00000004 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x151ede
00007ff7`eb9c9aa7     : 00000000`00000000 0000005a`ccafd960 00000000`00000040 00000000`00000000 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_cmpeq<`anonymous namespace'::_Find_seq::_Find_seq_traits_avx_2,unsigned short>+0xa0
00007ff7`eb9c88f0     : 0000005a`ccaff410 00000000`00000000 00000000`00000000 00007ff7`eb794010 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_impl<`anonymous namespace'::_Finding::_Find_traits_1,void,void,unsigned char>+0x4f7
00007ff7`ebb721b6     : 00000000`00000000 00000000`00000000 000001e0`794a5c60 0000005a`ccafee50 : media_transcode_realtime_video_cli!`anonymous namespace'::_Find_seq::_Search_cmpeq<`anonymous namespace'::_Find_seq::_Find_seq_traits_sse_4,unsigned int>+0x620
00007ff7`ebb9baaa     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x153f96
00007ff7`eba3f827     : 0000005a`ccaff430 000001e0`794a5c60 00000000`00000001 0000005a`ccaff5c8 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x17d88a
00007ff7`eb92ab42     : 00000000`00000000 00000000`00000000 00000000`00000000 0000005a`ccaffac0 : media_transcode_realtime_video_cli!std::`dynamic atexit destructor for '_Fac_tidy_reg''+0x21607
00007ff7`eb92ddb2     : 00000000`00000000 00000000`00000000 0000005a`ccaffac0 0000005a`ccaffac0 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaVideoPlanOptionApplier::applySelectedPlan+0x4b2
00007ff7`eb92dace     : 000001e0`7aac2300 00007ff7`eb86098e 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::`anonymous namespace'::setOption+0x132
00007ff7`eb80165c     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!media::ffmpeg::graph::MediaVideoTranscodeOptionApplier::applyUserOptions+0xce
00007ff7`eb7ff72f     : 00000000`00000000 00000000`00000000 ffffffff`fffffffe 00000000`00000000 : media_transcode_realtime_video_cli!fmt::v11::detail::copy<unsigned int,unsigned int const *,unsigned int *,0>+0x7c
00007ff9`0023cd30     : 000001e0`00ca31c0 00000000`00000000 00000000`00000000 00000000`00000000 : media_transcode_realtime_video_cli!fmt::v11::detail::basic_fp<fmt::v11::detail::uint128_fallback>::assign<float,0>+0x1f
00007ff9`0236dda7     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ucrtbase!wcsrchr+0x150
00007ff9`02fdee4c     : 00000000`00000000 00000000`00000000 000004f0`fffffb30 000004d0`fffffb30 : KERNEL32!BaseThreadInitThunk+0x17
00000000`00000000     : 00000000`00000000 00000000`00000000 00000000`00000000 00000000`00000000 : ntdll!RtlUserThreadStart+0x2c
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\atlmfc.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\ObjectiveC.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\concurrency.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\cpp_rest.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\stl.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\Windows.Data.Json.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\Windows.Devices.Geolocation.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\Windows.Devices.Sensors.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\Windows.Media.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\windows.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\winrt.natvis'
[in @ 000001e07aa83540] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e07aa83540] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 4520.196622
[in @ 000001e07adaf440] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e07adaf440] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 14329.524133
[in @ 000001e07b0ba4c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e07b0ba4c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 29353.121233
[in @ 000001e07a6ba480] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e0003efa40] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e0003efa40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 4520.196622
[in @ 000001e07a6ba480] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 14329.524133
[in @ 000001e07a6bb240] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e07a6bb240] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 29353.121233
[aac @ 000001e07aa66040] Qavg: nan
[aac @ 000001e07aa66040] 1 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=6406896600 materialized_ns=6537453000 protocol_deadline_ns=6473563268 deadline_before_release=0 materialized_after_deadline=1
~~~~

## 清理清单

composition-r297-cdb.log
composition-r297-cli-error.log
composition-r297-cli.log
composition-r297-monitor.log
composition-r297-source0-stdout.log
composition-r297-source0.log
composition-r297-source1-stdout.log
composition-r297-source1.log
composition-r297-source2-stdout.log
composition-r297-source2.log
