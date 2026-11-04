# r266：生命周期迁移复验（证据不完整）

结论：不能判定通过。实际观测间隔拉长，未及时取得三格画面快照，未执行重入；源流结束后CLI连续黑屏输出约一小时。最后快照未人工检查，不作为画面验收证据。所有三个源退出0，CLI caller_stop退出0，非EOF自然退出。

Release SHA256：9345922784CA5E2BEAA4CE36AD9D0E2C406C552A4901009044C5F9BD0E31150A。VLC12016，源45076/29960/28300，CLI44100，监控会话33094。

最终错误/丢帧0、payload bytes/objects0，reservations/releases898313；peakWS268312576，最后WS243687424。平均CPU1.013463%，峰值6.172840%。长黑屏段占主导，不与完整有媒体运行平均值直接比较。sender457147，最大提交迟到25.319142ms，deadline_misses0，delivery_evidence=not_proven。预算闭合及最新生命周期完整实流门禁仍未完成。

## 实际命令

~~~powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r266-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r266-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r266-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r266-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r266-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r266-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r266-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r266-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r266 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r266-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r266-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r266-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r266-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

$target=Get-CimInstance Win32_Process -Filter "ProcessId=44100"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r266*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R266Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R266Console]::FreeConsole() | Out-Null
if(-not [R266Console]::AttachConsole(44100)){throw 'AttachConsole failed'}
try {
[R266Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R266Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 44100 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R266Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R266Console]::FreeConsole() | Out-Null}
~~~

## 必要原始诊断

### composition-r266-cli-error.log (1964 bytes)

~~~text
[in @ 0000019436ffd880] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000019436ffd880] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 11260.0639
[in @ 0000019436ffdb80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000019436ffdb80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28577.713889
[in @ 000001943c323640] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001943c323640] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 27719.591556
[in @ 000001943c1e8580] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001943c1e8580] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28577.713889
[in @ 000001943c0c5000] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001943c0c5000] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 27719.591556
[in @ 000001943c1fcc80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001943c1fcc80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 11260.0639
[aac @ 000001943c3849c0] Qavg: 63469.754

~~~

### composition-r266-cli.log (11502736 bytes)

~~~text
[2026-10-19 15:08:14.596] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=387182 committed_datagrams=457147 committed_payload_bytes=174433668 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=457147 pacing_cancelled=0 pacing_submitted=457147 pacing_maximum_submit_lateness_ns=25319142 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=49450200 last_materialized_sequence=457147 last_scheduled_sequence=457147 last_submitted_sequence=457147 last_committed_sequence=457147 shared_scope_charged_datagrams=457147 shared_scope_charged_wire_bytes=187233784 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=456264 endpoint_1_committed_payload_bytes=174373616 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=883 endpoint_2_committed_payload_bytes=60052
[2026-10-19 15:08:14.814] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r266.output audio_commits=154292 real_samples=5027840 silence_samples=152967168
[2026-10-19 15:08:14.815] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=0 software_frame=0
[2026-10-19 15:08:14.816] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r266.output output_generation=1 video_commits=107479 source[0].group=composition-r266.source.0 source[0].real_frames=3420 source[0].black_frames=104059 source[0].contribution_generation=0 source[1].group=composition-r266.source.1 source[1].real_frames=3420 source[1].black_frames=104059 source[1].contribution_generation=0 source[2].group=composition-r266.source.2 source[2].real_frames=3421 source[2].black_frames=104058 source[2].contribution_generation=0
[2026-10-19 15:08:14.819] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-10-19 15:08:14.823] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-10-19 15:08:14.828] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-10-19 15:08:14.835] [info] [graph][runtime.node] hardware_transfer.summary forwarded=878 download=0 upload=0
[2026-10-19 15:08:14.835] [info] [graph][runtime.node] hardware_transfer.summary forwarded=824 download=0 upload=0
[2026-10-19 15:08:14.835] [info] [graph][runtime.node] hardware_transfer.summary forwarded=827 download=0 upload=0
[2026-10-19 15:08:14.836] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-10-19 15:08:14.850] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-10-19 15:08:14.867] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=935, threads=82, processThreads=130, workers=0, workerProgress=2869229, workerProcessCalls=8659864, workerWaits=5790617, workerWakeups=5518342, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=11564, logicalProcessors=22, averageSystemMachineCpuPercent=7.500499, averageProcessMachineCpuPercent=1.013463, peakProcessMachineCpuPercent=6.172840, averageProcessSingleCoreCpuPercent=22.296194, peakProcessSingleCoreCpuPercent=135.802469, initialWorkingSetBytes=206770176, workingSetBytes=243687424, peakWorkingSetBytes=268312576, totalPushed=2456141, totalPopped=2456117, droppedBuffers=0, encodedPacketsPushed=1201382, encodedPacketsPopped=1201382, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16840944, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=737, graphPayloadReservations=898313, graphPayloadReleases=898313, graphPayloadPressureFailures=0, lastInputReceivedAtNs=19639820516100
[2026-10-19 14:08:31.898] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5633372921 raw_phase_ns=1 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-19 14:08:31.898] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5679812830 raw_phase_ns=1 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-19 14:10:24.585] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4874 source_master_ns=118784211922 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-10-19 14:10:25.630] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119829109882 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
~~~

### composition-r266-monitor.log (260546 bytes)

~~~text
seconds=3584.2421244 cpu=0.962419648738151 ws=243691520 private=1139281920
seconds=3585.2802475 cpu=1.23146194406954 ws=243687424 private=1139281920
seconds=3586.3220929 cpu=0.954381697916147 ws=243687424 private=1139281920
seconds=3587.3592165 cpu=1.09568776215635 ws=243687424 private=1139281920
seconds=3588.3959851 cpu=1.23307080375422 ws=243318784 private=1138720768
~~~

### composition-r266-native-2026-10-19-15h07m55s352.png (12152 bytes)

### composition-r266-output.sdp (247 bytes)

~~~text
v=0
o=composition-r266 17185791564821223068 1 IN IP4 127.0.0.1
s=composition-r266
c=IN IP4 127.0.0.1
t=0 0
m=video 62020 RTP/AVP 33
a=rtcp:62021 IN IP4 127.0.0.1
a=rtpmap:33 MP2T/90000
a=ssrc:469959784 cname:mt-3081271716@media-transcode

~~~

### composition-r266-source0-stdout.log (540 bytes)

~~~text
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
t=0 0
a=tool:libavformat 62.19.101
m=video 61020 RTP/AVP 96
c=IN IP4 127.0.0.1
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F
m=audio 61022 RTP/AVP 97
c=IN IP4 127.0.0.1
b=AS:274
a=rtpmap:97 MPEG4-GENERIC/44100/2
a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3; config=121056E500


~~~

### composition-r266-source0.log (29521 bytes)

~~~text
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Duration: 00:02:00.00, start: 0.000000, bitrate: 8295 kb/s
  Stream #0:0[0x1](eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], 8013 kb/s, 30 fps, 30 tbr, 15360 tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
  Stream #0:1[0x2](eng): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 274 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
  Stream #0:1 -> #1:0 (copy)
Output #0, rtp, to 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Stream #0:0(eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], q=2-31, 8013 kb/s, 30 fps, 30 tbr, 90k tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
Output #1, rtp, to 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Stream #1:0(eng): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 274 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
frame=   31 fps=0.0 q=-1.0 size=    1174KiB time=00:00:01.03 bitrate=9305.4kbits/s speed=   2x elapsed=0:00:00.51    frame=   46 fps= 45 q=-1.0 size=    2245KiB time=00:00:01.53 bitrate=11992.8kbits/s speed=1.49x elapsed=0:00:01.02    frame=   61 fps= 40 q=-1.0 size=    2521KiB time=00:00:02.04 bitrate=10107.3kbits/s speed=1.33x elapsed=0:00:01.53    frame=   77 fps= 37 q=-1.0 size=    2900KiB time=00:00:02.56 bitrate=9255.1kbits/s speed=1.24x elapsed=0:00:02.06    frame=   92 fps= 36 q=-1.0 size=    3450KiB time=00:00:03.06 bitrate=9216.5kbits/s speed=1.19x elapsed=0:00:02.57    frame=  108 fps= 35 q=-1.0 size=    3880KiB time=00:00:03.60 bitrate=8829.2kbits/s speed=1.17x elapsed=0:00:03.08    frame=  123 fps= 34 q=-1.0 size=    4388KiB time=00:00:04.10 bitrate=8767.4kbits/s speed=1.14x elapsed=0:00:03.60    frame=  139 fps= 34 q=-1.0 size=    4747KiB time=00:00:04.63 bitrate=8392.9kbits/s speed=1.12x elapsed=0:00:04.12    frame=  155 fps= 33 q=-1.0 size=    5467KiB time=00:00:05.16 bitrate=8668.9kbits/s speed=1.11x elapsed=0:00:04.63    frame=  170 fps= 33 q=-1.0 size=    5969KiB time=00:00:05.66 bitrate=8628.6kbits/s speed= 1.1x elapsed=0:00:05.16    frame=  186 fps= 33 q=-1.0 size=    6571KiB time=00:00:06.20 bitrate=8682.4kbits/s speed=1.09x elapsed=0:00:05.67    frame=  200 fps= 32 q=-1.0 size=    6845KiB time=00:00:06.68 bitrate=8385.0kbits/s speed=1.08x elapsed=0:00:06.18    frame=  216 fps= 32 q=-1.0 size=    7357KiB time=00:00:07.22 bitrate=8345.7kbits/s speed=1.08x elapsed=0:00:06.70    frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.73 bitrate=8279.2kbits/s speed=1.07x elapsed=0:00:07.21    frame=  247 fps= 32 q=-1.0 size=    8354KiB time=00:00:08.23 bitrate=8312.2kbits/s speed=1.07x elapsed=0:00:07.72    frame=  263 fps= 32 q=-1.0 size=    8814KiB time=00:00:08.76 bitrate=8235.9kbits/s speed=1.06x elapsed=0:00:08.25    frame=  279 fps= 32 q=-1.0 size=    9867KiB time=00:00:09.30 bitrate=8691.2kbits/s speed=1.06x elapsed=0:00:08.76    frame=  294 fps= 32 q=-1.0 size=   10441KiB time=00:00:09.77 bitrate=8749.6kbits/s speed=1.05x elapsed=0:00:09.28    frame=  309 fps= 32 q=-1.0 size=   10765KiB time=00:00:10.30 bitrate=8561.7kbits/s speed=1.05x elapsed=0:00:09.79    frame=  324 fps= 31 q=-1.0 size=   11046KiB time=00:00:10.80 bitrate=8378.7kbits/s speed=1.05x elapsed=0:00:10.30    frame=  340 fps= 31 q=-1.0 size=   11663KiB time=00:00:11.33 bitrate=8430.4kbits/s speed=1.05x elapsed=0:00:10.82    frame=  356 fps= 31 q=-1.0 size=   12076KiB time=00:00:11.86 bitrate=8336.6kbits/s speed=1.05x elapsed=0:00:11.35    frame=  371 fps= 31 q=-1.0 size=   12531KiB time=00:00:12.36 bitrate=8300.5kbits/s speed=1.04x elapsed=0:00:11.86    frame=  387 fps= 31 q=-1.0 size=   12909KiB time=00:00:12.90 bitrate=8197.6kbits/s speed=1.04x elapsed=0:00:12.38    frame=  402 fps= 31 q=-1.0 size=   13671KiB time=00:00:13.40 bitrate=8357.6kbits/s speed=1.04x elapsed=0:00:12.88    frame=  417 fps= 31 q=-1.0 size=   14130KiB time=00:00:13.90 bitrate=8322.1kbits/s speed=1.04x elapsed=0:00:13.41    frame=  433 fps= 31 q=-1.0 size=   14763KiB time=00:00:14.44 bitrate=8373.4kbits/s speed=1.04x elapsed=0:00:13.92    frame=  448 fps= 31 q=-1.0 size=   15051KiB time=00:00:14.95 bitrate=8245.3kbits/s speed=1.04x elapsed=0:00:14.44    frame=  464 fps= 31 q=-1.0 size=   15574KiB time=00:00:15.46 bitrate=8248.9kbits/s speed=1.03x elapsed=0:00:14.95    frame=  480 fps= 31 q=-1.0 size=   15981KiB time=00:00:16.00 bitrate=8182.0kbits/s speed=1.03x elapsed=0:00:15.47    frame=  494 fps= 31 q=-1.0 size=   16533KiB time=00:00:16.48 bitrate=8215.5kbits/s speed=1.03x elapsed=0:00:15.97    frame=  510 fps= 31 q=-1.0 size=   16954KiB time=00:00:17.00 bitrate=8169.8kbits/s speed=1.03x elapsed=0:00:16.50    frame=  525 fps= 31 q=-1.0 size=   17698KiB time=00:00:17.50 bitrate=8281.1kbits/s speed=1.03x elapsed=0:00:17.01    frame=  541 fps= 31 q=-1.0 size=   18599KiB time=00:00:18.03 bitrate=8449.0kbits/s speed=1.03x elapsed=0:00:17.52    frame=  557 fps= 31 q=-1.0 size=   18902KiB time=00:00:18.56 bitrate=8340.0kbits/s speed=1.03x elapsed=0:00:18.04    frame=  572 fps= 31 q=-1.0 size=   19295KiB time=00:00:19.06 bitrate=8290.2kbits/s speed=1.03x elapsed=0:00:18.55    frame=  587 fps= 31 q=-1.0 size=   19694KiB time=00:00:19.60 bitrate=8231.3kbits/s speed=1.03x elapsed=0:00:19.07    frame=  603 fps= 31 q=-1.0 size=   20303KiB time=00:00:20.10 bitrate=8271.4kbits/s speed=1.03x elapsed=0:00:19.58    frame=  619 fps= 31 q=-1.0 size=   20706KiB time=00:00:20.63 bitrate=8221.0kbits/s speed=1.03x elapsed=0:00:20.11    frame=  634 fps= 31 q=-1.0 size=   21241KiB time=00:00:21.13 bitrate=8233.7kbits/s speed=1.02x elapsed=0:00:20.61    frame=  649 fps= 31 q=-1.0 size=   21867KiB time=00:00:21.64 bitrate=8277.4kbits/s speed=1.02x elapsed=0:00:21.14    frame=  665 fps= 31 q=-1.0 size=   22421KiB time=00:00:22.16 bitrate=8286.0kbits/s speed=1.02x elapsed=0:00:21.64    frame=  680 fps= 31 q=-1.0 size=   22892KiB time=00:00:22.66 bitrate=8273.5kbits/s speed=1.02x elapsed=0:00:22.17    frame=  696 fps= 31 q=-1.0 size=   23287KiB time=00:00:23.20 bitrate=8222.8kbits/s speed=1.02x elapsed=0:00:22.68    frame=  712 fps= 31 q=-1.0 size=   23695KiB time=00:00:23.73 bitrate=8178.6kbits/s speed=1.02x elapsed=0:00:23.21    frame=  727 fps= 31 q=-1.0 size=   24269KiB time=00:00:24.24 bitrate=8201.4kbits/s speed=1.02x elapsed=0:00:23.71    frame=  742 fps= 31 q=-1.0 size=   24677KiB time=00:00:24.73 bitrate=8173.4kbits/s speed=1.02x elapsed=0:00:24.22    frame=  758 fps= 31 q=-1.0 size=   25175KiB time=00:00:25.26 bitrate=8162.2kbits/s speed=1.02x elapsed=0:00:24.75    frame=  773 fps= 31 q=-1.0 size=   25507KiB time=00:00:25.77 bitrate=8107.1kbits/s speed=1.02x elapsed=0:00:25.26    frame=  789 fps= 31 q=-1.0 size=   26638KiB time=00:00:26.30 bitrate=8297.1kbits/s speed=1.02x elapsed=0:00:25.78    frame=  804 fps= 31 q=-1.0 size=   26943KiB time=00:00:26.80 bitrate=8235.6kbits/s speed=1.02x elapsed=0:00:26.29    frame=  820 fps= 31 q=-1.0 size=   27352KiB time=00:00:27.33 bitrate=8197.6kbits/s speed=1.02x elapsed=0:00:26.81    frame=  835 fps= 31 q=-1.0 size=   27840KiB time=00:00:27.84 bitrate=8191.9kbits/s speed=1.02x elapsed=0:00:27.32    frame=  850 fps= 31 q=-1.0 size=   28356KiB time=00:00:28.35 bitrate=8193.2kbits/s speed=1.02x elapsed=0:00:27.84    frame=  866 fps= 31 q=-1.0 size=   28799KiB time=00:00:28.86 bitrate=8172.7kbits/s speed=1.02x elapsed=0:00:28.35    frame=  881 fps= 31 q=-1.0 size=   29276KiB time=00:00:29.37 bitrate=8164.9kbits/s speed=1.02x elapsed=0:00:28.87    frame=  897 fps= 31 q=-1.0 size=   29912KiB time=00:00:29.90 bitrate=8193.2kbits/s speed=1.02x elapsed=0:00:29.38    frame=  912 fps= 30 q=-1.0 size=   30463KiB time=00:00:30.41 bitrate=8204.1kbits/s speed=1.02x elapsed=0:00:29.91    frame=  928 fps= 31 q=-1.0 size=   31041KiB time=00:00:30.93 bitrate=8220.6kbits/s speed=1.02x elapsed=0:00:30.41    frame=  944 fps= 31 q=-1.0 size=   31417KiB time=00:00:31.46 bitrate=8179.2kbits/s speed=1.02x elapsed=0:00:30.94    frame=  959 fps= 30 q=-1.0 size=   31798KiB time=00:00:31.96 bitrate=8148.8kbits/s speed=1.02x elapsed=0:00:31.44    frame=  974 fps= 30 q=-1.0 size=   32339KiB time=00:00:32.46 bitrate=8159.8kbits/s speed=1.02x elapsed=0:00:31.96    frame=  990 fps= 30 q=-1.0 size=   32767KiB time=00:00:33.00 bitrate=8134.2kbits/s speed=1.02x elapsed=0:00:32.48    frame= 1005 fps= 30 q=-1.0 size=   33285KiB time=00:00:33.50 bitrate=8137.8kbits/s speed=1.02x elapsed=0:00:32.99    frame= 1020 fps= 30 q=-1.0 size=   33575KiB time=00:00:34.00 bitrate=8089.6kbits/s speed=1.01x elapsed=0:00:33.50    frame= 1036 fps= 30 q=-1.0 size=   34879KiB time=00:00:34.53 bitrate=8273.9kbits/s speed=1.01x elapsed=0:00:34.03    frame= 1051 fps= 30 q=-1.0 size=   35211KiB time=00:00:35.03 bitrate=8232.3kbits/s speed=1.01x elapsed=0:00:34.54    frame= 1067 fps= 30 q=-1.0 size=   35620KiB time=00:00:35.57 bitrate=8202.8kbits/s speed=1.01x elapsed=0:00:35.06    frame= 1083 fps= 30 q=-1.0 size=   36150KiB time=00:00:36.10 bitrate=8203.4kbits/s speed=1.01x elapsed=0:00:35.57    frame= 1098 fps= 30 q=-1.0 size=   36552KiB time=00:00:36.60 bitrate=8181.2kbits/s speed=1.01x elapsed=0:00:36.09    frame= 1113 fps= 30 q=-1.0 size=   37036KiB time=00:00:37.13 bitrate=8170.5kbits/s speed=1.01x elapsed=0:00:36.60    frame= 1129 fps= 30 q=-1.0 size=   37399KiB time=00:00:37.63 bitrate=8139.6kbits/s speed=1.01x elapsed=0:00:37.12    frame= 1144 fps= 30 q=-1.0 size=   38094KiB time=00:00:38.13 bitrate=8183.5kbits/s speed=1.01x elapsed=0:00:37.63    frame= 1160 fps= 30 q=-1.0 size=   38619KiB time=00:00:38.66 bitrate=8182.0kbits/s speed=1.01x elapsed=0:00:38.14    frame= 1176 fps= 30 q=-1.0 size=   39272KiB time=00:00:39.20 bitrate=8206.9kbits/s speed=1.01x elapsed=0:00:38.67    frame= 1191 fps= 30 q=-1.0 size=   39557KiB time=00:00:39.70 bitrate=8162.6kbits/s speed=1.01x elapsed=0:00:39.18    frame= 1206 fps= 30 q=-1.0 size=   40043KiB time=00:00:40.23 bitrate=8153.2kbits/s speed=1.01x elapsed=0:00:39.70    frame= 1221 fps= 30 q=-1.0 size=   40434KiB time=00:00:40.70 bitrate=8137.6kbits/s speed=1.01x elapsed=0:00:40.21    frame= 1238 fps= 30 q=-1.0 size=   41034KiB time=00:00:41.26 bitrate=8145.8kbits/s speed=1.01x elapsed=0:00:40.73    frame= 1252 fps= 30 q=-1.0 size=   41411KiB time=00:00:41.74 bitrate=8125.7kbits/s speed=1.01x elapsed=0:00:41.24    frame= 1268 fps= 30 q=-1.0 size=   42055KiB time=00:00:42.26 bitrate=8150.9kbits/s speed=1.01x elapsed=0:00:41.75    frame= 1284 fps= 30 q=-1.0 size=   43072KiB time=00:00:42.80 bitrate=8244.1kbits/s speed=1.01x elapsed=0:00:42.28    frame= 1299 fps= 30 q=-1.0 size=   43413KiB time=00:00:43.30 bitrate=8213.4kbits/s speed=1.01x elapsed=0:00:42.79    frame= 1315 fps= 30 q=-1.0 size=   43713KiB time=00:00:43.83 bitrate=8168.5kbits/s speed=1.01x elapsed=0:00:43.31    frame= 1330 fps= 30 q=-1.0 size=   44226KiB time=00:00:44.33 bitrate=8172.2kbits/s speed=1.01x elapsed=0:00:43.82    frame= 1345 fps= 30 q=-1.0 size=   44622KiB time=00:00:44.83 bitrate=8152.5kbits/s speed=1.01x elapsed=0:00:44.35    frame= 1361 fps= 30 q=-1.0 size=   45122KiB time=00:00:45.37 bitrate=8146.9kbits/s speed=1.01x elapsed=0:00:44.87    frame= 1377 fps= 30 q=-1.0 size=   45524KiB time=00:00:45.90 bitrate=8123.9kbits/s speed=1.01x elapsed=0:00:45.39    frame= 1392 fps= 30 q=-1.0 size=   46316KiB time=00:00:46.40 bitrate=8177.1kbits/s speed=1.01x elapsed=0:00:45.90    frame= 1408 fps= 30 q=-1.0 size=   46790KiB time=00:00:46.93 bitrate=8167.0kbits/s speed=1.01x elapsed=0:00:46.41    frame= 1423 fps= 30 q=-1.0 size=   47371KiB time=00:00:47.43 bitrate=8180.3kbits/s speed=1.01x elapsed=0:00:46.93    frame= 1439 fps= 30 q=-1.0 size=   47703KiB time=00:00:47.97 bitrate=8146.0kbits/s speed=1.01x elapsed=0:00:47.44    frame= 1455 fps= 30 q=-1.0 size=   48199KiB time=00:00:48.50 bitrate=8141.2kbits/s speed=1.01x elapsed=0:00:47.97    frame= 1470 fps= 30 q=-1.0 size=   48596KiB time=00:00:49.00 bitrate=8124.5kbits/s speed=1.01x elapsed=0:00:48.50    frame= 1485 fps= 30 q=-1.0 size=   49144KiB time=00:00:49.50 bitrate=8132.2kbits/s speed=1.01x elapsed=0:00:49.00    frame= 1502 fps= 30 q=-1.0 size=   49778KiB time=00:00:50.06 bitrate=8144.8kbits/s speed=1.01x elapsed=0:00:49.53    frame= 1516 fps= 30 q=-1.0 size=   49969KiB time=00:00:50.54 bitrate=8097.9kbits/s speed=1.01x elapsed=0:00:50.04    frame= 1532 fps= 30 q=-1.0 size=   51031KiB time=00:00:51.06 bitrate=8186.3kbits/s speed=1.01x elapsed=0:00:50.55    frame= 1548 fps= 30 q=-1.0 size=   51404KiB time=00:00:51.60 bitrate=8160.8kbits/s speed=1.01x elapsed=0:00:51.08    frame= 1563 fps= 30 q=-1.0 size=   51840KiB time=00:00:52.10 bitrate=8151.1kbits/s speed=1.01x elapsed=0:00:51.59    frame= 1579 fps= 30 q=-1.0 size=   52311KiB time=00:00:52.63 bitrate=8141.8kbits/s speed=1.01x elapsed=0:00:52.11    frame= 1595 fps= 30 q=-1.0 size=   52920KiB time=00:00:53.16 bitrate=8154.0kbits/s speed=1.01x elapsed=0:00:52.64    frame= 1610 fps= 30 q=-1.0 size=   53374KiB time=00:00:53.66 bitrate=8147.3kbits/s speed=1.01x elapsed=0:00:53.14    frame= 1625 fps= 30 q=-1.0 size=   53896KiB time=00:00:54.17 bitrate=8150.3kbits/s speed=1.01x elapsed=0:00:53.67    frame= 1640 fps= 30 q=-1.0 size=   54483KiB time=00:00:54.70 bitrate=8159.5kbits/s speed=1.01x elapsed=0:00:54.18    frame= 1657 fps= 30 q=-1.0 size=   55065KiB time=00:00:55.23 bitrate=8167.1kbits/s speed=1.01x elapsed=0:00:54.70    frame= 1672 fps= 30 q=-1.0 size=   55589KiB time=00:00:55.73 bitrate=8170.8kbits/s speed=1.01x elapsed=0:00:55.21    frame= 1687 fps= 30 q=-1.0 size=   55947KiB time=00:00:56.23 bitrate=8150.3kbits/s speed=1.01x elapsed=0:00:55.72    frame= 1703 fps= 30 q=-1.0 size=   56340KiB time=00:00:56.76 bitrate=8130.4kbits/s speed=1.01x elapsed=0:00:56.24    frame= 1718 fps= 30 q=-1.0 size=   56895KiB time=00:00:57.26 bitrate=8138.9kbits/s speed=1.01x elapsed=0:00:56.75    frame= 1734 fps= 30 q=-1.0 size=   57329KiB time=00:00:57.80 bitrate=8125.3kbits/s speed=1.01x elapsed=0:00:57.28    frame= 1749 fps= 30 q=-1.0 size=   57935KiB time=00:00:58.30 bitrate=8140.6kbits/s speed=1.01x elapsed=0:00:57.79    frame= 1765 fps= 30 q=-1.0 size=   58230KiB time=00:00:58.83 bitrate=8108.0kbits/s speed=1.01x elapsed=0:00:58.31    frame= 1780 fps= 30 q=-1.0 size=   59271KiB time=00:00:59.33 bitrate=8183.4kbits/s speed=1.01x elapsed=0:00:58.82    frame= 1796 fps= 30 q=-1.0 size=   59632KiB time=00:00:59.86 bitrate=8159.9kbits/s speed=1.01x elapsed=0:00:59.34    frame= 1811 fps= 30 q=-1.0 size=   60029KiB time=00:01:00.36 bitrate=8146.2kbits/s speed=1.01x elapsed=0:00:59.85    frame= 1826 fps= 30 q=-1.0 size=   60494KiB time=00:01:00.88 bitrate=8139.8kbits/s speed=1.01x elapsed=0:01:00.37    frame= 1842 fps= 30 q=-1.0 size=   61017KiB time=00:01:01.40 bitrate=8140.9kbits/s speed=1.01x elapsed=0:01:00.89    frame= 1858 fps= 30 q=-1.0 size=   61462KiB time=00:01:01.93 bitrate=8129.6kbits/s speed=1.01x elapsed=0:01:01.40    frame= 1873 fps= 30 q=-1.0 size=   61924KiB time=00:01:02.43 bitrate=8124.5kbits/s speed=1.01x elapsed=0:01:01.92    frame= 1888 fps= 30 q=-1.0 size=   62546KiB time=00:01:02.93 bitrate=8141.6kbits/s speed=1.01x elapsed=0:01:02.43    frame= 1904 fps= 30 q=-1.0 size=   63085KiB time=00:01:03.46 bitrate=8142.7kbits/s speed=1.01x elapsed=0:01:02.95    frame= 1919 fps= 30 q=-1.0 size=   63631KiB time=00:01:03.96 bitrate=8149.1kbits/s speed=1.01x elapsed=0:01:03.46    frame= 1935 fps= 30 q=-1.0 size=   64011KiB time=00:01:04.50 bitrate=8129.9kbits/s speed=1.01x elapsed=0:01:03.98    frame= 1950 fps= 30 q=-1.0 size=   64415KiB time=00:01:05.00 bitrate=8118.3kbits/s speed=1.01x elapsed=0:01:04.49    frame= 1966 fps= 30 q=-1.0 size=   64995KiB time=00:01:05.53 bitrate=8124.7kbits/s speed=1.01x elapsed=0:01:05.01    frame= 1981 fps= 30 q=-1.0 size=   65416KiB time=00:01:06.03 bitrate=8115.4kbits/s speed=1.01x elapsed=0:01:05.52    frame= 1997 fps= 30 q=-1.0 size=   65991KiB time=00:01:06.56 bitrate=8121.2kbits/s speed=1.01x elapsed=0:01:06.05    frame= 2012 fps= 30 q=-1.0 size=   66649KiB time=00:01:07.06 bitrate=8141.0kbits/s speed=1.01x elapsed=0:01:06.56    frame= 2028 fps= 30 q=-1.0 size=   67594KiB time=00:01:07.60 bitrate=8191.3kbits/s speed=1.01x elapsed=0:01:07.07    frame= 2043 fps= 30 q=-1.0 size=   67949KiB time=00:01:08.10 bitrate=8173.3kbits/s speed=1.01x elapsed=0:01:07.59    frame= 2058 fps= 30 q=-1.0 size=   68234KiB time=00:01:08.63 bitrate=8144.3kbits/s speed=1.01x elapsed=0:01:08.10    frame= 2074 fps= 30 q=-1.0 size=   68767KiB time=00:01:09.13 bitrate=8148.6kbits/s speed=1.01x elapsed=0:01:08.61    frame= 2090 fps= 30 q=-1.0 size=   69200KiB time=00:01:09.66 bitrate=8137.1kbits/s speed=1.01x elapsed=0:01:09.14    frame= 2105 fps= 30 q=-1.0 size=   69675KiB time=00:01:10.17 bitrate=8134.1kbits/s speed=1.01x elapsed=0:01:09.65    frame= 2120 fps= 30 q=-1.0 size=   70025KiB time=00:01:10.68 bitrate=8115.9kbits/s speed=1.01x elapsed=0:01:10.17    frame= 2136 fps= 30 q=-1.0 size=   70766KiB time=00:01:11.20 bitrate=8142.1kbits/s speed=1.01x elapsed=0:01:10.68    frame= 2151 fps= 30 q=-1.0 size=   71244KiB time=00:01:11.70 bitrate=8139.5kbits/s speed=1.01x elapsed=0:01:11.20    frame= 2166 fps= 30 q=-1.0 size=   71867KiB time=00:01:12.21 bitrate=8152.6kbits/s speed=1.01x elapsed=0:01:11.71    frame= 2182 fps= 30 q=-1.0 size=   72233KiB time=00:01:12.73 bitrate=8135.6kbits/s speed=1.01x elapsed=0:01:12.23    frame= 2198 fps= 30 q=-1.0 size=   72751KiB time=00:01:13.26 bitrate=8134.3kbits/s speed=1.01x elapsed=0:01:12.74    frame= 2213 fps= 30 q=-1.0 size=   73134KiB time=00:01:13.76 bitrate=8121.4kbits/s speed=1.01x elapsed=0:01:13.26    frame= 2229 fps= 30 q=-1.0 size=   73697KiB time=00:01:14.30 bitrate=8125.5kbits/s speed=1.01x elapsed=0:01:13.78    frame= 2244 fps= 30 q=-1.0 size=   74010KiB time=00:01:14.81 bitrate=8103.9kbits/s speed=1.01x elapsed=0:01:14.29    frame= 2260 fps= 30 q=-1.0 size=   74535KiB time=00:01:15.33 bitrate=8105.2kbits/s speed=1.01x elapsed=0:01:14.81    frame= 2275 fps= 30 q=-1.0 size=   75458KiB time=00:01:15.83 bitrate=8151.2kbits/s speed=1.01x elapsed=0:01:15.32    frame= 2291 fps= 30 q=-1.0 size=   75915KiB time=00:01:16.37 bitrate=8143.2kbits/s speed=1.01x elapsed=0:01:15.85    frame= 2306 fps= 30 q=-1.0 size=   76263KiB time=00:01:16.86 bitrate=8127.7kbits/s speed=1.01x elapsed=0:01:16.35    frame= 2321 fps= 30 q=-1.0 size=   76795KiB time=00:01:17.36 bitrate=8131.2kbits/s speed=1.01x elapsed=0:01:16.88    frame= 2337 fps= 30 q=-1.0 size=   77262KiB time=00:01:17.90 bitrate=8124.9kbits/s speed=1.01x elapsed=0:01:17.38    frame= 2352 fps= 30 q=-1.0 size=   77774KiB time=00:01:18.40 bitrate=8126.6kbits/s speed=1.01x elapsed=0:01:17.89    frame= 2368 fps= 30 q=-1.0 size=   78168KiB time=00:01:18.93 bitrate=8112.5kbits/s speed=1.01x elapsed=0:01:18.42    frame= 2383 fps= 30 q=-1.0 size=   78948KiB time=00:01:19.43 bitrate=8141.8kbits/s speed=1.01x elapsed=0:01:18.93    frame= 2399 fps= 30 q=-1.0 size=   79470KiB time=00:01:19.96 bitrate=8140.9kbits/s speed=1.01x elapsed=0:01:19.45    frame= 2415 fps= 30 q=-1.0 size=   80042KiB time=00:01:20.50 bitrate=8145.3kbits/s speed=1.01x elapsed=0:01:19.96    frame= 2430 fps= 30 q=-1.0 size=   80345KiB time=00:01:21.00 bitrate=8125.8kbits/s speed=1.01x elapsed=0:01:20.49    frame= 2446 fps= 30 q=-1.0 size=   80814KiB time=00:01:21.53 bitrate=8119.7kbits/s speed=1.01x elapsed=0:01:21.01    frame= 2461 fps= 30 q=-1.0 size=   81214KiB time=00:01:22.03 bitrate=8109.9kbits/s speed=1.01x elapsed=0:01:21.52    frame= 2477 fps= 30 q=-1.0 size=   81781KiB time=00:01:22.57 bitrate=8113.7kbits/s speed=1.01x elapsed=0:01:22.05    frame= 2492 fps= 30 q=-1.0 size=   82547KiB time=00:01:23.08 bitrate=8139.4kbits/s speed=1.01x elapsed=0:01:22.56    frame= 2508 fps= 30 q=-1.0 size=   82910KiB time=00:01:23.60 bitrate=8124.4kbits/s speed=1.01x elapsed=0:01:23.08    frame= 2524 fps= 30 q=-1.0 size=   83888KiB time=00:01:24.13 bitrate=8168.1kbits/s speed=1.01x elapsed=0:01:23.61    frame= 2539 fps= 30 q=-1.0 size=   84212KiB time=00:01:24.63 bitrate=8150.9kbits/s speed=1.01x elapsed=0:01:24.11    frame= 2555 fps= 30 q=-1.0 size=   84594KiB time=00:01:25.16 bitrate=8136.9kbits/s speed=1.01x elapsed=0:01:24.64    frame= 2570 fps= 30 q=-1.0 size=   84997KiB time=00:01:25.66 bitrate=8128.0kbits/s speed=1.01x elapsed=0:01:25.15    frame= 2585 fps= 30 q=-1.0 size=   85548KiB time=00:01:26.20 bitrate=8130.0kbits/s speed=1.01x elapsed=0:01:25.67    frame= 2601 fps= 30 q=-1.0 size=   85923KiB time=00:01:26.70 bitrate=8118.6kbits/s speed=1.01x elapsed=0:01:26.18    frame= 2616 fps= 30 q=-1.0 size=   86468KiB time=00:01:27.21 bitrate=8122.0kbits/s speed=1.01x elapsed=0:01:26.69    frame= 2632 fps= 30 q=-1.0 size=   87089KiB time=00:01:27.73 bitrate=8131.9kbits/s speed=1.01x elapsed=0:01:27.22    frame= 2647 fps= 30 q=-1.0 size=   87616KiB time=00:01:28.23 bitrate=8134.5kbits/s speed=1.01x elapsed=0:01:27.72    frame= 2663 fps= 30 q=-1.0 size=   88186KiB time=00:01:28.76 bitrate=8138.1kbits/s speed=1.01x elapsed=0:01:28.25    frame= 2678 fps= 30 q=-1.0 size=   88562KiB time=00:01:29.28 bitrate=8126.1kbits/s speed=1.01x elapsed=0:01:28.76    frame= 2693 fps= 30 q=-1.0 size=   88938KiB time=00:01:29.76 bitrate=8116.2kbits/s speed=1.01x elapsed=0:01:29.28    frame= 2709 fps= 30 q=-1.0 size=   89500KiB time=00:01:30.30 bitrate=8119.2kbits/s speed=1.01x elapsed=0:01:29.78    frame= 2725 fps= 30 q=-1.0 size=   89930KiB time=00:01:30.83 bitrate=8110.5kbits/s speed=1.01x elapsed=0:01:30.31    frame= 2740 fps= 30 q=-1.0 size=   90550KiB time=00:01:31.34 bitrate=8120.5kbits/s speed=1.01x elapsed=0:01:30.82    frame= 2755 fps= 30 q=-1.0 size=   90763KiB time=00:01:31.83 bitrate=8096.4kbits/s speed=1.01x elapsed=0:01:31.34    frame= 2771 fps= 30 q=-1.0 size=   91892KiB time=00:01:32.36 bitrate=8149.7kbits/s speed=1.01x elapsed=0:01:31.85    frame= 2786 fps= 30 q=-1.0 size=   92288KiB time=00:01:32.90 bitrate=8138.0kbits/s speed=1.01x elapsed=0:01:32.37    frame= 2802 fps= 30 q=-1.0 size=   92688KiB time=00:01:33.40 bitrate=8129.5kbits/s speed=1.01x elapsed=0:01:32.88    frame= 2817 fps= 30 q=-1.0 size=   93107KiB time=00:01:33.90 bitrate=8122.7kbits/s speed=1.01x elapsed=0:01:33.40    frame= 2833 fps= 30 q=-1.0 size=   93631KiB time=00:01:34.43 bitrate=8122.4kbits/s speed=1.01x elapsed=0:01:33.92    frame= 2848 fps= 30 q=-1.0 size=   94015KiB time=00:01:34.94 bitrate=8111.6kbits/s speed=1.01x elapsed=0:01:34.44    frame= 2864 fps= 30 q=-1.0 size=   94544KiB time=00:01:35.46 bitrate=8112.9kbits/s speed=1.01x elapsed=0:01:34.94    frame= 2880 fps= 30 q=-1.0 size=   95196KiB time=00:01:36.00 bitrate=8123.4kbits/s speed=1.01x elapsed=0:01:35.47    frame= 2895 fps= 30 q=-1.0 size=   95699KiB time=00:01:36.50 bitrate=8123.8kbits/s speed=1.01x elapsed=0:01:35.98    frame= 2910 fps= 30 q=-1.0 size=   96231KiB time=00:01:37.01 bitrate=8125.9kbits/s speed=1.01x elapsed=0:01:36.50    frame= 2926 fps= 30 q=-1.0 size=   96647KiB time=00:01:37.53 bitrate=8117.5kbits/s speed=1.01x elapsed=0:01:37.01    frame= 2941 fps= 30 q=-1.0 size=   97038KiB time=00:01:38.03 bitrate=8108.8kbits/s speed=1.01x elapsed=0:01:37.52    frame= 2957 fps= 30 q=-1.0 size=   97617KiB time=00:01:38.56 bitrate=8112.9kbits/s speed=1.01x elapsed=0:01:38.04    frame= 2972 fps= 30 q=-1.0 size=   98230KiB time=00:01:39.06 bitrate=8122.8kbits/s speed=1.01x elapsed=0:01:38.56    frame= 2987 fps= 30 q=-1.0 size=   98646KiB time=00:01:39.56 bitrate=8116.2kbits/s speed=   1x elapsed=0:01:39.07    frame= 3003 fps= 30 q=-1.0 size=   99345KiB time=00:01:40.10 bitrate=8130.2kbits/s speed=1.01x elapsed=0:01:39.58    frame= 3019 fps= 30 q=-1.0 size=  100213KiB time=00:01:40.63 bitrate=8157.6kbits/s speed=1.01x elapsed=0:01:40.11    frame= 3034 fps= 30 q=-1.0 size=  100588KiB time=00:01:41.14 bitrate=8146.8kbits/s speed=1.01x elapsed=0:01:40.62    frame= 3049 fps= 30 q=-1.0 size=  100880KiB time=00:01:41.66 bitrate=8128.6kbits/s speed=1.01x elapsed=0:01:41.13    frame= 3065 fps= 30 q=-1.0 size=  101399KiB time=00:01:42.16 bitrate=8130.4kbits/s speed=   1x elapsed=0:01:41.66    frame= 3080 fps= 30 q=-1.0 size=  101769KiB time=00:01:42.70 bitrate=8117.7kbits/s speed=1.01x elapsed=0:01:42.17    frame= 3095 fps= 30 q=-1.0 size=  102305KiB time=00:01:43.16 bitrate=8123.6kbits/s speed=   1x elapsed=0:01:42.67    frame= 3111 fps= 30 q=-1.0 size=  102664KiB time=00:01:43.70 bitrate=8110.1kbits/s speed=   1x elapsed=0:01:43.20    frame= 3127 fps= 30 q=-1.0 size=  103417KiB time=00:01:44.23 bitrate=8127.7kbits/s speed=   1x elapsed=0:01:43.72    frame= 3143 fps= 30 q=-1.0 size=  103959KiB time=00:01:44.76 bitrate=8128.8kbits/s speed=1.01x elapsed=0:01:44.23    frame= 3157 fps= 30 q=-1.0 size=  104496KiB time=00:01:45.23 bitrate=8134.6kbits/s speed=   1x elapsed=0:01:44.74    frame= 3173 fps= 30 q=-1.0 size=  104891KiB time=00:01:45.76 bitrate=8124.2kbits/s speed=   1x elapsed=0:01:45.27    frame= 3189 fps= 30 q=-1.0 size=  105390KiB time=00:01:46.30 bitrate=8121.8kbits/s speed=   1x elapsed=0:01:45.78    frame= 3204 fps= 30 q=-1.0 size=  105771KiB time=00:01:46.81 bitrate=8112.2kbits/s speed=   1x elapsed=0:01:46.29    frame= 3220 fps= 30 q=-1.0 size=  106347KiB time=00:01:47.33 bitrate=8116.7kbits/s speed=   1x elapsed=0:01:46.81    frame= 3235 fps= 30 q=-1.0 size=  106763KiB time=00:01:47.83 bitrate=8110.7kbits/s speed=   1x elapsed=0:01:47.32    frame= 3251 fps= 30 q=-1.0 size=  107500KiB time=00:01:48.36 bitrate=8126.4kbits/s speed=   1x elapsed=0:01:47.85    frame= 3266 fps= 30 q=-1.0 size=  108260KiB time=00:01:48.87 bitrate=8145.5kbits/s speed=   1x elapsed=0:01:48.35    frame= 3281 fps= 30 q=-1.0 size=  108708KiB time=00:01:49.38 bitrate=8141.0kbits/s speed=   1x elapsed=0:01:48.86    frame= 3297 fps= 30 q=-1.0 size=  109014KiB time=00:01:49.90 bitrate=8125.9kbits/s speed=   1x elapsed=0:01:49.39    frame= 3313 fps= 30 q=-1.0 size=  109517KiB time=00:01:50.43 bitrate=8124.1kbits/s speed=   1x elapsed=0:01:49.90    frame= 3328 fps= 30 q=-1.0 size=  109891KiB time=00:01:50.94 bitrate=8114.2kbits/s speed=   1x elapsed=0:01:50.42    frame= 3343 fps= 30 q=-1.0 size=  110425KiB time=00:01:51.43 bitrate=8117.8kbits/s speed=   1x elapsed=0:01:50.93    frame= 3359 fps= 30 q=-1.0 size=  110807KiB time=00:01:51.96 bitrate=8107.1kbits/s speed=   1x elapsed=0:01:51.45    frame= 3374 fps= 30 q=-1.0 size=  111575KiB time=00:01:52.46 bitrate=8127.0kbits/s speed=   1x elapsed=0:01:51.96    frame= 3389 fps= 30 q=-1.0 size=  112056KiB time=00:01:52.96 bitrate=8125.9kbits/s speed=   1x elapsed=0:01:52.49    frame= 3405 fps= 30 q=-1.0 size=  112622KiB time=00:01:53.50 bitrate=8128.6kbits/s speed=   1x elapsed=0:01:52.99    frame= 3421 fps= 30 q=-1.0 size=  113013KiB time=00:01:54.03 bitrate=8118.7kbits/s speed=   1x elapsed=0:01:53.52    frame= 3436 fps= 30 q=-1.0 size=  113461KiB time=00:01:54.53 bitrate=8115.3kbits/s speed=   1x elapsed=0:01:54.02    frame= 3452 fps= 30 q=-1.0 size=  114074KiB time=00:01:55.06 bitrate=8121.3kbits/s speed=   1x elapsed=0:01:54.54    frame= 3467 fps= 30 q=-1.0 size=  114447KiB time=00:01:55.56 bitrate=8112.6kbits/s speed=   1x elapsed=0:01:55.06    frame= 3482 fps= 30 q=-1.0 size=  115008KiB time=00:01:56.07 bitrate=8116.6kbits/s speed=   1x elapsed=0:01:55.57    frame= 3498 fps= 30 q=-1.0 size=  115226KiB time=00:01:56.61 bitrate=8094.8kbits/s speed=   1x elapsed=0:01:56.09    frame= 3513 fps= 30 q=-1.0 size=  116145KiB time=00:01:57.10 bitrate=8125.2kbits/s speed=   1x elapsed=0:01:56.60    frame= 3529 fps= 30 q=-1.0 size=  116696KiB time=00:01:57.63 bitrate=8126.7kbits/s speed=   1x elapsed=0:01:57.12    frame= 3544 fps= 30 q=-1.0 size=  117123KiB time=00:01:58.13 bitrate=8121.9kbits/s speed=   1x elapsed=0:01:57.64    frame= 3560 fps= 30 q=-1.0 size=  117571KiB time=00:01:58.67 bitrate=8115.6kbits/s speed=   1x elapsed=0:01:58.17    frame= 3576 fps= 30 q=-1.0 size=  118130KiB time=00:01:59.20 bitrate=8118.5kbits/s speed=   1x elapsed=0:01:58.69    frame= 3592 fps= 30 q=-1.0 size=  118576KiB time=00:01:59.73 bitrate=8112.8kbits/s speed=   1x elapsed=0:01:59.20    [out#0/rtp @ 0000015bfde19740] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 0000015bffd1f100] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    

~~~

### composition-r266-source1-stdout.log (335 bytes)

~~~text
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
c=IN IP4 127.0.0.1
t=0 0
a=tool:libavformat 62.19.101
m=video 61030 RTP/AVP 96
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F


~~~

### composition-r266-source1.log (28926 bytes)

~~~text
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Duration: 00:02:00.00, start: 0.000000, bitrate: 8295 kb/s
  Stream #0:0[0x1](eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], 8013 kb/s, 30 fps, 30 tbr, 15360 tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
  Stream #0:1[0x2](eng): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 274 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
Output #0, rtp, to 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Stream #0:0(eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], q=2-31, 8013 kb/s, 30 fps, 30 tbr, 90k tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
frame=   30 fps=0.0 q=-1.0 size=    1080KiB time=00:00:01.00 bitrate=8849.0kbits/s speed=1.96x elapsed=0:00:00.50    frame=   46 fps= 45 q=-1.0 size=    2245KiB time=00:00:01.53 bitrate=11992.8kbits/s speed= 1.5x elapsed=0:00:01.02    frame=   61 fps= 40 q=-1.0 size=    2521KiB time=00:00:02.03 bitrate=10157.2kbits/s speed=1.33x elapsed=0:00:01.53    frame=   77 fps= 37 q=-1.0 size=    2900KiB time=00:00:02.56 bitrate=9255.1kbits/s speed=1.25x elapsed=0:00:02.06    frame=   92 fps= 36 q=-1.0 size=    3450KiB time=00:00:03.06 bitrate=9216.5kbits/s speed=1.19x elapsed=0:00:02.57    frame=  108 fps= 35 q=-1.0 size=    3880KiB time=00:00:03.60 bitrate=8829.2kbits/s speed=1.17x elapsed=0:00:03.08    frame=  124 fps= 34 q=-1.0 size=    4406KiB time=00:00:04.13 bitrate=8731.5kbits/s speed=1.14x elapsed=0:00:03.61    frame=  139 fps= 34 q=-1.0 size=    4747KiB time=00:00:04.63 bitrate=8392.9kbits/s speed=1.12x elapsed=0:00:04.13    frame=  155 fps= 33 q=-1.0 size=    5467KiB time=00:00:05.16 bitrate=8668.9kbits/s speed=1.11x elapsed=0:00:04.64    frame=  170 fps= 33 q=-1.0 size=    5969KiB time=00:00:05.66 bitrate=8628.6kbits/s speed= 1.1x elapsed=0:00:05.17    frame=  185 fps= 33 q=-1.0 size=    6540KiB time=00:00:06.20 bitrate=8641.4kbits/s speed=1.09x elapsed=0:00:05.68    frame=  201 fps= 32 q=-1.0 size=    6860KiB time=00:00:06.70 bitrate=8387.9kbits/s speed=1.08x elapsed=0:00:06.18    frame=  217 fps= 32 q=-1.0 size=    7382KiB time=00:00:07.23 bitrate=8360.7kbits/s speed=1.08x elapsed=0:00:06.71    frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.73 bitrate=8279.2kbits/s speed=1.07x elapsed=0:00:07.22    frame=  248 fps= 32 q=-1.0 size=    8407KiB time=00:00:08.26 bitrate=8330.6kbits/s speed=1.07x elapsed=0:00:07.73    frame=  263 fps= 32 q=-1.0 size=    8814KiB time=00:00:08.76 bitrate=8235.9kbits/s speed=1.06x elapsed=0:00:08.26    frame=  278 fps= 32 q=-1.0 size=    9819KiB time=00:00:09.26 bitrate=8680.4kbits/s speed=1.06x elapsed=0:00:08.77    frame=  294 fps= 32 q=-1.0 size=   10476KiB time=00:00:09.80 bitrate=8757.4kbits/s speed=1.05x elapsed=0:00:09.29    frame=  309 fps= 32 q=-1.0 size=   10765KiB time=00:00:10.33 bitrate=8534.1kbits/s speed=1.05x elapsed=0:00:09.80    frame=  325 fps= 32 q=-1.0 size=   11067KiB time=00:00:10.83 bitrate=8368.8kbits/s speed=1.05x elapsed=0:00:10.31    frame=  340 fps= 31 q=-1.0 size=   11663KiB time=00:00:11.33 bitrate=8430.4kbits/s speed=1.05x elapsed=0:00:10.83    frame=  356 fps= 31 q=-1.0 size=   12076KiB time=00:00:11.86 bitrate=8336.6kbits/s speed=1.04x elapsed=0:00:11.35    frame=  372 fps= 31 q=-1.0 size=   12551KiB time=00:00:12.40 bitrate=8291.6kbits/s speed=1.04x elapsed=0:00:11.86    frame=  386 fps= 31 q=-1.0 size=   12887KiB time=00:00:12.86 bitrate=8204.7kbits/s speed=1.04x elapsed=0:00:12.39    frame=  402 fps= 31 q=-1.0 size=   13671KiB time=00:00:13.40 bitrate=8357.6kbits/s speed=1.04x elapsed=0:00:12.89    frame=  418 fps= 31 q=-1.0 size=   14165KiB time=00:00:13.93 bitrate=8328.3kbits/s speed=1.04x elapsed=0:00:13.42    frame=  433 fps= 31 q=-1.0 size=   14763KiB time=00:00:14.43 bitrate=8378.9kbits/s speed=1.04x elapsed=0:00:13.93    frame=  448 fps= 31 q=-1.0 size=   15051KiB time=00:00:14.93 bitrate=8256.5kbits/s speed=1.03x elapsed=0:00:14.45    frame=  464 fps= 31 q=-1.0 size=   15574KiB time=00:00:15.46 bitrate=8248.9kbits/s speed=1.03x elapsed=0:00:14.96    frame=  479 fps= 31 q=-1.0 size=   15958KiB time=00:00:15.96 bitrate=8187.6kbits/s speed=1.03x elapsed=0:00:15.47    frame=  495 fps= 31 q=-1.0 size=   16563KiB time=00:00:16.50 bitrate=8223.3kbits/s speed=1.03x elapsed=0:00:15.98    frame=  510 fps= 31 q=-1.0 size=   16954KiB time=00:00:17.00 bitrate=8169.8kbits/s speed=1.03x elapsed=0:00:16.51    frame=  526 fps= 31 q=-1.0 size=   17773KiB time=00:00:17.53 bitrate=8303.8kbits/s speed=1.03x elapsed=0:00:17.02    frame=  541 fps= 31 q=-1.0 size=   18599KiB time=00:00:18.03 bitrate=8449.0kbits/s speed=1.03x elapsed=0:00:17.53    frame=  557 fps= 31 q=-1.0 size=   18902KiB time=00:00:18.56 bitrate=8340.0kbits/s speed=1.03x elapsed=0:00:18.05    frame=  572 fps= 31 q=-1.0 size=   19295KiB time=00:00:19.06 bitrate=8290.2kbits/s speed=1.03x elapsed=0:00:18.56    frame=  588 fps= 31 q=-1.0 size=   19754KiB time=00:00:19.60 bitrate=8256.4kbits/s speed=1.03x elapsed=0:00:19.08    frame=  603 fps= 31 q=-1.0 size=   20303KiB time=00:00:20.10 bitrate=8274.9kbits/s speed=1.03x elapsed=0:00:19.59    frame=  619 fps= 31 q=-1.0 size=   20706KiB time=00:00:20.63 bitrate=8221.0kbits/s speed=1.03x elapsed=0:00:20.12    frame=  634 fps= 31 q=-1.0 size=   21241KiB time=00:00:21.13 bitrate=8233.7kbits/s speed=1.02x elapsed=0:00:20.62    frame=  650 fps= 31 q=-1.0 size=   21892KiB time=00:00:21.66 bitrate=8277.3kbits/s speed=1.02x elapsed=0:00:21.15    frame=  665 fps= 31 q=-1.0 size=   22421KiB time=00:00:22.16 bitrate=8286.0kbits/s speed=1.02x elapsed=0:00:21.65    frame=  681 fps= 31 q=-1.0 size=   22921KiB time=00:00:22.70 bitrate=8271.8kbits/s speed=1.02x elapsed=0:00:22.18    frame=  696 fps= 31 q=-1.0 size=   23287KiB time=00:00:23.20 bitrate=8222.8kbits/s speed=1.02x elapsed=0:00:22.69    frame=  712 fps= 31 q=-1.0 size=   23695KiB time=00:00:23.73 bitrate=8178.6kbits/s speed=1.02x elapsed=0:00:23.21    frame=  727 fps= 31 q=-1.0 size=   24269KiB time=00:00:24.23 bitrate=8204.2kbits/s speed=1.02x elapsed=0:00:23.72    frame=  742 fps= 31 q=-1.0 size=   24677KiB time=00:00:24.73 bitrate=8173.4kbits/s speed=1.02x elapsed=0:00:24.23    frame=  758 fps= 31 q=-1.0 size=   25175KiB time=00:00:25.26 bitrate=8162.2kbits/s speed=1.02x elapsed=0:00:24.76    frame=  773 fps= 31 q=-1.0 size=   25507KiB time=00:00:25.76 bitrate=8109.4kbits/s speed=1.02x elapsed=0:00:25.27    frame=  789 fps= 31 q=-1.0 size=   26638KiB time=00:00:26.30 bitrate=8297.1kbits/s speed=1.02x elapsed=0:00:25.79    frame=  805 fps= 31 q=-1.0 size=   26959KiB time=00:00:26.83 bitrate=8230.3kbits/s speed=1.02x elapsed=0:00:26.30    frame=  820 fps= 31 q=-1.0 size=   27352KiB time=00:00:27.33 bitrate=8197.6kbits/s speed=1.02x elapsed=0:00:26.82    frame=  835 fps= 31 q=-1.0 size=   27840KiB time=00:00:27.83 bitrate=8194.0kbits/s speed=1.02x elapsed=0:00:27.33    frame=  851 fps= 31 q=-1.0 size=   28400KiB time=00:00:28.36 bitrate=8201.6kbits/s speed=1.02x elapsed=0:00:27.85    frame=  866 fps= 31 q=-1.0 size=   28799KiB time=00:00:28.86 bitrate=8172.7kbits/s speed=1.02x elapsed=0:00:28.36    frame=  882 fps= 31 q=-1.0 size=   29299KiB time=00:00:29.40 bitrate=8164.0kbits/s speed=1.02x elapsed=0:00:28.88    frame=  897 fps= 31 q=-1.0 size=   29912KiB time=00:00:29.90 bitrate=8195.2kbits/s speed=1.02x elapsed=0:00:29.39    frame=  913 fps= 31 q=-1.0 size=   30503KiB time=00:00:30.43 bitrate=8210.8kbits/s speed=1.02x elapsed=0:00:29.91    frame=  928 fps= 31 q=-1.0 size=   31041KiB time=00:00:30.93 bitrate=8220.6kbits/s speed=1.02x elapsed=0:00:30.42    frame=  944 fps= 31 q=-1.0 size=   31417KiB time=00:00:31.46 bitrate=8179.2kbits/s speed=1.02x elapsed=0:00:30.94    frame=  959 fps= 30 q=-1.0 size=   31798KiB time=00:00:31.96 bitrate=8148.8kbits/s speed=1.02x elapsed=0:00:31.45    frame=  974 fps= 30 q=-1.0 size=   32339KiB time=00:00:32.50 bitrate=8151.5kbits/s speed=1.02x elapsed=0:00:31.97    frame=  990 fps= 30 q=-1.0 size=   32767KiB time=00:00:33.00 bitrate=8134.2kbits/s speed=1.02x elapsed=0:00:32.49    frame= 1005 fps= 30 q=-1.0 size=   33285KiB time=00:00:33.50 bitrate=8139.3kbits/s speed=1.01x elapsed=0:00:33.00    frame= 1021 fps= 30 q=-1.0 size=   33723KiB time=00:00:34.03 bitrate=8117.3kbits/s speed=1.02x elapsed=0:00:33.51    frame= 1036 fps= 30 q=-1.0 size=   34879KiB time=00:00:34.53 bitrate=8273.9kbits/s speed=1.01x elapsed=0:00:34.04    frame= 1052 fps= 30 q=-1.0 size=   35300KiB time=00:00:35.06 bitrate=8246.5kbits/s speed=1.01x elapsed=0:00:34.55    frame= 1067 fps= 30 q=-1.0 size=   35620KiB time=00:00:35.56 bitrate=8204.3kbits/s speed=1.01x elapsed=0:00:35.07    frame= 1083 fps= 30 q=-1.0 size=   36150KiB time=00:00:36.10 bitrate=8203.4kbits/s speed=1.01x elapsed=0:00:35.58    frame= 1098 fps= 30 q=-1.0 size=   36552KiB time=00:00:36.60 bitrate=8181.2kbits/s speed=1.01x elapsed=0:00:36.10    frame= 1114 fps= 30 q=-1.0 size=   37084KiB time=00:00:37.13 bitrate=8181.0kbits/s speed=1.01x elapsed=0:00:36.61    frame= 1129 fps= 30 q=-1.0 size=   37399KiB time=00:00:37.63 bitrate=8140.9kbits/s speed=1.01x elapsed=0:00:37.13    frame= 1144 fps= 30 q=-1.0 size=   38094KiB time=00:00:38.13 bitrate=8183.5kbits/s speed=1.01x elapsed=0:00:37.64    frame= 1160 fps= 30 q=-1.0 size=   38619KiB time=00:00:38.66 bitrate=8182.0kbits/s speed=1.01x elapsed=0:00:38.15    frame= 1176 fps= 30 q=-1.0 size=   39272KiB time=00:00:39.20 bitrate=8206.9kbits/s speed=1.01x elapsed=0:00:38.68    frame= 1191 fps= 30 q=-1.0 size=   39557KiB time=00:00:39.70 bitrate=8162.6kbits/s speed=1.01x elapsed=0:00:39.19    frame= 1206 fps= 30 q=-1.0 size=   40043KiB time=00:00:40.20 bitrate=8159.9kbits/s speed=1.01x elapsed=0:00:39.71    frame= 1222 fps= 30 q=-1.0 size=   40456KiB time=00:00:40.73 bitrate=8136.2kbits/s speed=1.01x elapsed=0:00:40.22    frame= 1237 fps= 30 q=-1.0 size=   41012KiB time=00:00:41.26 bitrate=8141.5kbits/s speed=1.01x elapsed=0:00:40.74    frame= 1253 fps= 30 q=-1.0 size=   41427KiB time=00:00:41.76 bitrate=8125.5kbits/s speed=1.01x elapsed=0:00:41.25    frame= 1268 fps= 30 q=-1.0 size=   42055KiB time=00:00:42.26 bitrate=8150.9kbits/s speed=1.01x elapsed=0:00:41.76    frame= 1284 fps= 30 q=-1.0 size=   43072KiB time=00:00:42.80 bitrate=8244.1kbits/s speed=1.01x elapsed=0:00:42.29    frame= 1299 fps= 30 q=-1.0 size=   43413KiB time=00:00:43.30 bitrate=8213.4kbits/s speed=1.01x elapsed=0:00:42.80    frame= 1314 fps= 30 q=-1.0 size=   43703KiB time=00:00:43.80 bitrate=8173.9kbits/s speed=1.01x elapsed=0:00:43.32    frame= 1331 fps= 30 q=-1.0 size=   44248KiB time=00:00:44.36 bitrate=8170.2kbits/s speed=1.01x elapsed=0:00:43.83    frame= 1346 fps= 30 q=-1.0 size=   44649KiB time=00:00:44.86 bitrate=8152.2kbits/s speed=1.01x elapsed=0:00:44.36    frame= 1361 fps= 30 q=-1.0 size=   45122KiB time=00:00:45.36 bitrate=8147.8kbits/s speed=1.01x elapsed=0:00:44.88    frame= 1377 fps= 30 q=-1.0 size=   45524KiB time=00:00:45.90 bitrate=8125.0kbits/s speed=1.01x elapsed=0:00:45.40    frame= 1392 fps= 30 q=-1.0 size=   46316KiB time=00:00:46.40 bitrate=8177.1kbits/s speed=1.01x elapsed=0:00:45.91    frame= 1408 fps= 30 q=-1.0 size=   46790KiB time=00:00:46.93 bitrate=8167.0kbits/s speed=1.01x elapsed=0:00:46.42    frame= 1424 fps= 30 q=-1.0 size=   47407KiB time=00:00:47.46 bitrate=8181.7kbits/s speed=1.01x elapsed=0:00:46.94    frame= 1439 fps= 30 q=-1.0 size=   47703KiB time=00:00:47.96 bitrate=8146.9kbits/s speed=1.01x elapsed=0:00:47.45    frame= 1455 fps= 30 q=-1.0 size=   48199KiB time=00:00:48.50 bitrate=8141.2kbits/s speed=1.01x elapsed=0:00:47.98    frame= 1470 fps= 30 q=-1.0 size=   48596KiB time=00:00:49.00 bitrate=8124.5kbits/s speed=1.01x elapsed=0:00:48.51    frame= 1486 fps= 30 q=-1.0 size=   49168KiB time=00:00:49.53 bitrate=8131.5kbits/s speed=1.01x elapsed=0:00:49.01    frame= 1501 fps= 30 q=-1.0 size=   49509KiB time=00:00:50.03 bitrate=8106.1kbits/s speed=1.01x elapsed=0:00:49.54    frame= 1517 fps= 30 q=-1.0 size=   50042KiB time=00:00:50.56 bitrate=8107.0kbits/s speed=1.01x elapsed=0:00:50.05    frame= 1532 fps= 30 q=-1.0 size=   51031KiB time=00:00:51.06 bitrate=8186.3kbits/s speed=1.01x elapsed=0:00:50.56    frame= 1548 fps= 30 q=-1.0 size=   51404KiB time=00:00:51.60 bitrate=8160.8kbits/s speed=1.01x elapsed=0:00:51.09    frame= 1563 fps= 30 q=-1.0 size=   51840KiB time=00:00:52.10 bitrate=8151.1kbits/s speed=1.01x elapsed=0:00:51.60    frame= 1579 fps= 30 q=-1.0 size=   52311KiB time=00:00:52.63 bitrate=8141.8kbits/s speed=1.01x elapsed=0:00:52.12    frame= 1595 fps= 30 q=-1.0 size=   52920KiB time=00:00:53.16 bitrate=8154.0kbits/s speed=1.01x elapsed=0:00:52.65    frame= 1610 fps= 30 q=-1.0 size=   53374KiB time=00:00:53.66 bitrate=8147.3kbits/s speed=1.01x elapsed=0:00:53.15    frame= 1625 fps= 30 q=-1.0 size=   53896KiB time=00:00:54.16 bitrate=8151.1kbits/s speed=1.01x elapsed=0:00:53.68    frame= 1641 fps= 30 q=-1.0 size=   54508KiB time=00:00:54.70 bitrate=8163.2kbits/s speed=1.01x elapsed=0:00:54.18    frame= 1656 fps= 30 q=-1.0 size=   55029KiB time=00:00:55.20 bitrate=8166.6kbits/s speed=1.01x elapsed=0:00:54.71    frame= 1672 fps= 30 q=-1.0 size=   55589KiB time=00:00:55.73 bitrate=8170.8kbits/s speed=1.01x elapsed=0:00:55.22    frame= 1687 fps= 30 q=-1.0 size=   55947KiB time=00:00:56.23 bitrate=8150.3kbits/s speed=1.01x elapsed=0:00:55.73    frame= 1703 fps= 30 q=-1.0 size=   56340KiB time=00:00:56.76 bitrate=8130.4kbits/s speed=1.01x elapsed=0:00:56.25    frame= 1719 fps= 30 q=-1.0 size=   56917KiB time=00:00:57.30 bitrate=8137.2kbits/s speed=1.01x elapsed=0:00:56.76    frame= 1734 fps= 30 q=-1.0 size=   57329KiB time=00:00:57.80 bitrate=8125.3kbits/s speed=1.01x elapsed=0:00:57.29    frame= 1749 fps= 30 q=-1.0 size=   57935KiB time=00:00:58.30 bitrate=8140.6kbits/s speed=1.01x elapsed=0:00:57.80    frame= 1764 fps= 30 q=-1.0 size=   58156KiB time=00:00:58.80 bitrate=8102.2kbits/s speed=1.01x elapsed=0:00:58.32    frame= 1780 fps= 30 q=-1.0 size=   59271KiB time=00:00:59.33 bitrate=8183.4kbits/s speed=1.01x elapsed=0:00:58.83    frame= 1796 fps= 30 q=-1.0 size=   59632KiB time=00:00:59.86 bitrate=8159.9kbits/s speed=1.01x elapsed=0:00:59.35    frame= 1811 fps= 30 q=-1.0 size=   60029KiB time=00:01:00.36 bitrate=8146.2kbits/s speed=1.01x elapsed=0:00:59.86    frame= 1827 fps= 30 q=-1.0 size=   60522KiB time=00:01:00.90 bitrate=8141.2kbits/s speed=1.01x elapsed=0:01:00.38    frame= 1842 fps= 30 q=-1.0 size=   61017KiB time=00:01:01.40 bitrate=8140.9kbits/s speed=1.01x elapsed=0:01:00.90    frame= 1857 fps= 30 q=-1.0 size=   61398KiB time=00:01:01.90 bitrate=8125.6kbits/s speed=1.01x elapsed=0:01:01.40    frame= 1873 fps= 30 q=-1.0 size=   61924KiB time=00:01:02.43 bitrate=8125.1kbits/s speed=1.01x elapsed=0:01:01.93    frame= 1888 fps= 30 q=-1.0 size=   62546KiB time=00:01:02.93 bitrate=8141.6kbits/s speed=1.01x elapsed=0:01:02.44    frame= 1904 fps= 30 q=-1.0 size=   63085KiB time=00:01:03.46 bitrate=8142.7kbits/s speed=1.01x elapsed=0:01:02.96    frame= 1919 fps= 30 q=-1.0 size=   63631KiB time=00:01:03.96 bitrate=8149.1kbits/s speed=1.01x elapsed=0:01:03.47    frame= 1935 fps= 30 q=-1.0 size=   64011KiB time=00:01:04.50 bitrate=8129.9kbits/s speed=1.01x elapsed=0:01:03.99    frame= 1950 fps= 30 q=-1.0 size=   64415KiB time=00:01:05.00 bitrate=8118.3kbits/s speed=1.01x elapsed=0:01:04.50    frame= 1966 fps= 30 q=-1.0 size=   64995KiB time=00:01:05.53 bitrate=8124.7kbits/s speed=1.01x elapsed=0:01:05.02    frame= 1982 fps= 30 q=-1.0 size=   65604KiB time=00:01:06.06 bitrate=8134.6kbits/s speed=1.01x elapsed=0:01:05.53    frame= 1997 fps= 30 q=-1.0 size=   65991KiB time=00:01:06.56 bitrate=8121.2kbits/s speed=1.01x elapsed=0:01:06.06    frame= 2012 fps= 30 q=-1.0 size=   66649KiB time=00:01:07.06 bitrate=8141.0kbits/s speed=1.01x elapsed=0:01:06.57    frame= 2027 fps= 30 q=-1.0 size=   67550KiB time=00:01:07.56 bitrate=8189.9kbits/s speed=1.01x elapsed=0:01:07.08    frame= 2043 fps= 30 q=-1.0 size=   67949KiB time=00:01:08.10 bitrate=8173.8kbits/s speed=1.01x elapsed=0:01:07.60    frame= 2059 fps= 30 q=-1.0 size=   68244KiB time=00:01:08.63 bitrate=8145.5kbits/s speed=1.01x elapsed=0:01:08.11    frame= 2074 fps= 30 q=-1.0 size=   68767KiB time=00:01:09.13 bitrate=8148.6kbits/s speed=1.01x elapsed=0:01:08.62    frame= 2090 fps= 30 q=-1.0 size=   69200KiB time=00:01:09.66 bitrate=8137.1kbits/s speed=1.01x elapsed=0:01:09.15    frame= 2105 fps= 30 q=-1.0 size=   69675KiB time=00:01:10.16 bitrate=8134.6kbits/s speed=1.01x elapsed=0:01:09.66    frame= 2121 fps= 30 q=-1.0 size=   70054KiB time=00:01:10.70 bitrate=8117.2kbits/s speed=1.01x elapsed=0:01:10.18    frame= 2136 fps= 30 q=-1.0 size=   70766KiB time=00:01:11.20 bitrate=8142.1kbits/s speed=1.01x elapsed=0:01:10.69    frame= 2151 fps= 30 q=-1.0 size=   71244KiB time=00:01:11.70 bitrate=8139.9kbits/s speed=1.01x elapsed=0:01:11.21    frame= 2167 fps= 30 q=-1.0 size=   71904KiB time=00:01:12.23 bitrate=8154.6kbits/s speed=1.01x elapsed=0:01:11.72    frame= 2183 fps= 30 q=-1.0 size=   72249KiB time=00:01:12.76 bitrate=8133.8kbits/s speed=1.01x elapsed=0:01:12.24    frame= 2198 fps= 30 q=-1.0 size=   72751KiB time=00:01:13.26 bitrate=8134.3kbits/s speed=1.01x elapsed=0:01:12.75    frame= 2213 fps= 30 q=-1.0 size=   73134KiB time=00:01:13.76 bitrate=8121.8kbits/s speed=1.01x elapsed=0:01:13.27    frame= 2229 fps= 30 q=-1.0 size=   73697KiB time=00:01:14.30 bitrate=8125.5kbits/s speed=1.01x elapsed=0:01:13.79    frame= 2244 fps= 30 q=-1.0 size=   74010KiB time=00:01:14.80 bitrate=8105.5kbits/s speed=1.01x elapsed=0:01:14.30    frame= 2259 fps= 30 q=-1.0 size=   74514KiB time=00:01:15.30 bitrate=8106.5kbits/s speed=1.01x elapsed=0:01:14.82    frame= 2275 fps= 30 q=-1.0 size=   75458KiB time=00:01:15.86 bitrate=8147.9kbits/s speed=1.01x elapsed=0:01:15.33    frame= 2290 fps= 30 q=-1.0 size=   75898KiB time=00:01:16.33 bitrate=8145.2kbits/s speed=1.01x elapsed=0:01:15.85    frame= 2306 fps= 30 q=-1.0 size=   76263KiB time=00:01:16.86 bitrate=8127.7kbits/s speed=1.01x elapsed=0:01:16.36    frame= 2322 fps= 30 q=-1.0 size=   76827KiB time=00:01:17.40 bitrate=8131.3kbits/s speed=1.01x elapsed=0:01:16.89    frame= 2337 fps= 30 q=-1.0 size=   77262KiB time=00:01:17.90 bitrate=8124.9kbits/s speed=1.01x elapsed=0:01:17.39    frame= 2352 fps= 30 q=-1.0 size=   77774KiB time=00:01:18.40 bitrate=8126.6kbits/s speed=1.01x elapsed=0:01:17.90    frame= 2368 fps= 30 q=-1.0 size=   78168KiB time=00:01:18.93 bitrate=8112.5kbits/s speed=1.01x elapsed=0:01:18.43    frame= 2383 fps= 30 q=-1.0 size=   78948KiB time=00:01:19.43 bitrate=8142.0kbits/s speed=1.01x elapsed=0:01:18.94    frame= 2399 fps= 30 q=-1.0 size=   79470KiB time=00:01:19.96 bitrate=8141.2kbits/s speed=1.01x elapsed=0:01:19.46    frame= 2414 fps= 30 q=-1.0 size=   80009KiB time=00:01:20.46 bitrate=8145.4kbits/s speed=1.01x elapsed=0:01:19.97    frame= 2430 fps= 30 q=-1.0 size=   80345KiB time=00:01:21.00 bitrate=8125.8kbits/s speed=1.01x elapsed=0:01:20.50    frame= 2446 fps= 30 q=-1.0 size=   80814KiB time=00:01:21.53 bitrate=8119.7kbits/s speed=1.01x elapsed=0:01:21.02    frame= 2462 fps= 30 q=-1.0 size=   81401KiB time=00:01:22.06 bitrate=8125.6kbits/s speed=1.01x elapsed=0:01:21.53    frame= 2477 fps= 30 q=-1.0 size=   81781KiB time=00:01:22.56 bitrate=8114.0kbits/s speed=1.01x elapsed=0:01:22.06    frame= 2492 fps= 30 q=-1.0 size=   82547KiB time=00:01:23.06 bitrate=8140.8kbits/s speed=1.01x elapsed=0:01:22.56    frame= 2508 fps= 30 q=-1.0 size=   82910KiB time=00:01:23.60 bitrate=8124.4kbits/s speed=1.01x elapsed=0:01:23.09    frame= 2523 fps= 30 q=-1.0 size=   83827KiB time=00:01:24.13 bitrate=8162.2kbits/s speed=1.01x elapsed=0:01:23.60    frame= 2539 fps= 30 q=-1.0 size=   84212KiB time=00:01:24.63 bitrate=8151.2kbits/s speed=1.01x elapsed=0:01:24.11    frame= 2554 fps= 30 q=-1.0 size=   84583KiB time=00:01:25.13 bitrate=8139.0kbits/s speed=1.01x elapsed=0:01:24.63    frame= 2570 fps= 30 q=-1.0 size=   84997KiB time=00:01:25.66 bitrate=8128.0kbits/s speed=1.01x elapsed=0:01:25.14    frame= 2585 fps= 30 q=-1.0 size=   85548KiB time=00:01:26.16 bitrate=8133.2kbits/s speed=1.01x elapsed=0:01:25.67    frame= 2600 fps= 30 q=-1.0 size=   85902KiB time=00:01:26.66 bitrate=8119.7kbits/s speed=1.01x elapsed=0:01:26.18    frame= 2616 fps= 30 q=-1.0 size=   86468KiB time=00:01:27.20 bitrate=8123.3kbits/s speed=1.01x elapsed=0:01:26.68    frame= 2632 fps= 30 q=-1.0 size=   87089KiB time=00:01:27.73 bitrate=8131.9kbits/s speed=1.01x elapsed=0:01:27.21    frame= 2647 fps= 30 q=-1.0 size=   87616KiB time=00:01:28.23 bitrate=8134.7kbits/s speed=1.01x elapsed=0:01:27.72    frame= 2662 fps= 30 q=-1.0 size=   88149KiB time=00:01:28.73 bitrate=8138.0kbits/s speed=1.01x elapsed=0:01:28.24    frame= 2678 fps= 30 q=-1.0 size=   88562KiB time=00:01:29.26 bitrate=8127.3kbits/s speed=1.01x elapsed=0:01:28.75    frame= 2693 fps= 30 q=-1.0 size=   88938KiB time=00:01:29.76 bitrate=8116.4kbits/s speed=1.01x elapsed=0:01:29.27    frame= 2709 fps= 30 q=-1.0 size=   89500KiB time=00:01:30.30 bitrate=8119.4kbits/s speed=1.01x elapsed=0:01:29.78    frame= 2725 fps= 30 q=-1.0 size=   89930KiB time=00:01:30.83 bitrate=8110.5kbits/s speed=1.01x elapsed=0:01:30.30    frame= 2740 fps= 30 q=-1.0 size=   90550KiB time=00:01:31.33 bitrate=8121.7kbits/s speed=1.01x elapsed=0:01:30.81    frame= 2755 fps= 30 q=-1.0 size=   90763KiB time=00:01:31.86 bitrate=8093.6kbits/s speed=1.01x elapsed=0:01:31.34    frame= 2770 fps= 30 q=-1.0 size=   91824KiB time=00:01:32.36 bitrate=8143.9kbits/s speed=1.01x elapsed=0:01:31.84    frame= 2786 fps= 30 q=-1.0 size=   92288KiB time=00:01:32.86 bitrate=8140.9kbits/s speed=1.01x elapsed=0:01:32.37    frame= 2802 fps= 30 q=-1.0 size=   92688KiB time=00:01:33.40 bitrate=8129.5kbits/s speed=1.01x elapsed=0:01:32.88    frame= 2817 fps= 30 q=-1.0 size=   93107KiB time=00:01:33.90 bitrate=8122.8kbits/s speed=1.01x elapsed=0:01:33.39    frame= 2833 fps= 30 q=-1.0 size=   93631KiB time=00:01:34.43 bitrate=8122.4kbits/s speed=1.01x elapsed=0:01:33.91    frame= 2848 fps= 30 q=-1.0 size=   94015KiB time=00:01:34.93 bitrate=8112.7kbits/s speed=1.01x elapsed=0:01:34.43    frame= 2863 fps= 30 q=-1.0 size=   94522KiB time=00:01:35.43 bitrate=8113.8kbits/s speed=1.01x elapsed=0:01:34.94    frame= 2879 fps= 30 q=-1.0 size=   95140KiB time=00:01:35.96 bitrate=8121.4kbits/s speed=1.01x elapsed=0:01:35.46    frame= 2894 fps= 30 q=-1.0 size=   95664KiB time=00:01:36.46 bitrate=8123.8kbits/s speed=1.01x elapsed=0:01:35.97    frame= 2910 fps= 30 q=-1.0 size=   96231KiB time=00:01:37.00 bitrate=8127.0kbits/s speed=1.01x elapsed=0:01:36.49    frame= 2925 fps= 30 q=-1.0 size=   96631KiB time=00:01:37.50 bitrate=8119.0kbits/s speed=1.01x elapsed=0:01:37.01    frame= 2941 fps= 30 q=-1.0 size=   97038KiB time=00:01:38.03 bitrate=8108.8kbits/s speed=1.01x elapsed=0:01:37.51    frame= 2957 fps= 30 q=-1.0 size=   97617KiB time=00:01:38.56 bitrate=8113.0kbits/s speed=1.01x elapsed=0:01:38.04    frame= 2972 fps= 30 q=-1.0 size=   98230KiB time=00:01:39.06 bitrate=8122.8kbits/s speed=1.01x elapsed=0:01:38.56    frame= 2988 fps= 30 q=-1.0 size=   98663KiB time=00:01:39.60 bitrate=8114.9kbits/s speed=1.01x elapsed=0:01:39.07    frame= 3002 fps= 30 q=-1.0 size=   99296KiB time=00:01:40.06 bitrate=8128.9kbits/s speed=   1x elapsed=0:01:39.58    frame= 3018 fps= 30 q=-1.0 size=  100152KiB time=00:01:40.60 bitrate=8155.5kbits/s speed=   1x elapsed=0:01:40.10    frame= 3034 fps= 30 q=-1.0 size=  100588KiB time=00:01:41.13 bitrate=8147.9kbits/s speed=1.01x elapsed=0:01:40.61    frame= 3049 fps= 30 q=-1.0 size=  100880KiB time=00:01:41.63 bitrate=8131.3kbits/s speed=   1x elapsed=0:01:41.13    frame= 3065 fps= 30 q=-1.0 size=  101399KiB time=00:01:42.16 bitrate=8130.5kbits/s speed=1.01x elapsed=0:01:41.65    frame= 3080 fps= 30 q=-1.0 size=  101769KiB time=00:01:42.66 bitrate=8120.4kbits/s speed=   1x elapsed=0:01:42.16    frame= 3095 fps= 30 q=-1.0 size=  102305KiB time=00:01:43.16 bitrate=8123.6kbits/s speed=   1x elapsed=0:01:42.67    frame= 3111 fps= 30 q=-1.0 size=  102664KiB time=00:01:43.70 bitrate=8110.2kbits/s speed=   1x elapsed=0:01:43.19    frame= 3127 fps= 30 q=-1.0 size=  103417KiB time=00:01:44.23 bitrate=8127.8kbits/s speed=   1x elapsed=0:01:43.71    frame= 3142 fps= 30 q=-1.0 size=  103921KiB time=00:01:44.73 bitrate=8128.5kbits/s speed=   1x elapsed=0:01:44.22    frame= 3158 fps= 30 q=-1.0 size=  104560KiB time=00:01:45.26 bitrate=8137.0kbits/s speed=1.01x elapsed=0:01:44.73    frame= 3173 fps= 30 q=-1.0 size=  104891KiB time=00:01:45.76 bitrate=8124.2kbits/s speed=   1x elapsed=0:01:45.26    frame= 3188 fps= 30 q=-1.0 size=  105364KiB time=00:01:46.26 bitrate=8122.4kbits/s speed=   1x elapsed=0:01:45.77    frame= 3204 fps= 30 q=-1.0 size=  105771KiB time=00:01:46.80 bitrate=8113.1kbits/s speed=   1x elapsed=0:01:46.28    frame= 3219 fps= 30 q=-1.0 size=  106326KiB time=00:01:47.30 bitrate=8117.6kbits/s speed=   1x elapsed=0:01:46.80    frame= 3235 fps= 30 q=-1.0 size=  106763KiB time=00:01:47.83 bitrate=8110.7kbits/s speed=   1x elapsed=0:01:47.31    frame= 3250 fps= 30 q=-1.0 size=  107492KiB time=00:01:48.33 bitrate=8128.4kbits/s speed=   1x elapsed=0:01:47.84    frame= 3266 fps= 30 q=-1.0 size=  108260KiB time=00:01:48.86 bitrate=8146.3kbits/s speed=   1x elapsed=0:01:48.35    frame= 3281 fps= 30 q=-1.0 size=  108708KiB time=00:01:49.36 bitrate=8142.7kbits/s speed=   1x elapsed=0:01:48.86    frame= 3297 fps= 30 q=-1.0 size=  109014KiB time=00:01:49.90 bitrate=8125.9kbits/s speed=   1x elapsed=0:01:49.38    frame= 3312 fps= 30 q=-1.0 size=  109488KiB time=00:01:50.40 bitrate=8124.3kbits/s speed=   1x elapsed=0:01:49.89    frame= 3328 fps= 30 q=-1.0 size=  109891KiB time=00:01:50.93 bitrate=8115.0kbits/s speed=   1x elapsed=0:01:50.41    frame= 3343 fps= 30 q=-1.0 size=  110425KiB time=00:01:51.43 bitrate=8117.8kbits/s speed=   1x elapsed=0:01:50.92    frame= 3359 fps= 30 q=-1.0 size=  110807KiB time=00:01:51.96 bitrate=8107.1kbits/s speed=   1x elapsed=0:01:51.44    frame= 3374 fps= 30 q=-1.0 size=  111575KiB time=00:01:52.46 bitrate=8127.0kbits/s speed=   1x elapsed=0:01:51.95    frame= 3390 fps= 30 q=-1.0 size=  112096KiB time=00:01:53.00 bitrate=8126.4kbits/s speed=   1x elapsed=0:01:52.48    frame= 3405 fps= 30 q=-1.0 size=  112622KiB time=00:01:53.50 bitrate=8128.6kbits/s speed=   1x elapsed=0:01:52.99    frame= 3421 fps= 30 q=-1.0 size=  113013KiB time=00:01:54.03 bitrate=8118.7kbits/s speed=   1x elapsed=0:01:53.51    frame= 3436 fps= 30 q=-1.0 size=  113461KiB time=00:01:54.53 bitrate=8115.3kbits/s speed=   1x elapsed=0:01:54.02    frame= 3451 fps= 30 q=-1.0 size=  113850KiB time=00:01:55.03 bitrate=8107.7kbits/s speed=   1x elapsed=0:01:54.53    frame= 3467 fps= 30 q=-1.0 size=  114447KiB time=00:01:55.56 bitrate=8112.6kbits/s speed=   1x elapsed=0:01:55.06    frame= 3482 fps= 30 q=-1.0 size=  115008KiB time=00:01:56.06 bitrate=8117.3kbits/s speed=   1x elapsed=0:01:55.56    frame= 3498 fps= 30 q=-1.0 size=  115226KiB time=00:01:56.60 bitrate=8095.5kbits/s speed=   1x elapsed=0:01:56.09    frame= 3513 fps= 30 q=-1.0 size=  116145KiB time=00:01:57.10 bitrate=8125.2kbits/s speed=   1x elapsed=0:01:56.59    frame= 3529 fps= 30 q=-1.0 size=  116696KiB time=00:01:57.63 bitrate=8126.7kbits/s speed=   1x elapsed=0:01:57.11    frame= 3544 fps= 30 q=-1.0 size=  117123KiB time=00:01:58.13 bitrate=8121.9kbits/s speed=   1x elapsed=0:01:57.64    frame= 3560 fps= 30 q=-1.0 size=  117571KiB time=00:01:58.66 bitrate=8116.4kbits/s speed=   1x elapsed=0:01:58.14    frame= 3575 fps= 30 q=-1.0 size=  118060KiB time=00:01:59.16 bitrate=8115.9kbits/s speed=   1x elapsed=0:01:58.67    frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.17    [out#0/rtp @ 000001f7ec497300] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    

~~~

### composition-r266-source2-stdout.log (335 bytes)

~~~text
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
c=IN IP4 127.0.0.1
t=0 0
a=tool:libavformat 62.19.101
m=video 61040 RTP/AVP 96
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F


~~~

### composition-r266-source2.log (28926 bytes)

~~~text
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Duration: 00:02:00.00, start: 0.000000, bitrate: 8295 kb/s
  Stream #0:0[0x1](eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], 8013 kb/s, 30 fps, 30 tbr, 15360 tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
  Stream #0:1[0x2](eng): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 274 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
Output #0, rtp, to 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Stream #0:0(eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], q=2-31, 8013 kb/s, 30 fps, 30 tbr, 90k tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
frame=   31 fps=0.0 q=-1.0 size=    1174KiB time=00:00:01.03 bitrate=9305.4kbits/s speed=1.99x elapsed=0:00:00.51    frame=   46 fps= 45 q=-1.0 size=    2245KiB time=00:00:01.53 bitrate=11992.8kbits/s speed=1.49x elapsed=0:00:01.03    frame=   61 fps= 40 q=-1.0 size=    2521KiB time=00:00:02.03 bitrate=10157.2kbits/s speed=1.32x elapsed=0:00:01.54    frame=   77 fps= 37 q=-1.0 size=    2900KiB time=00:00:02.60 bitrate=9136.4kbits/s speed=1.26x elapsed=0:00:02.06    frame=   92 fps= 36 q=-1.0 size=    3450KiB time=00:00:03.10 bitrate=9117.4kbits/s speed= 1.2x elapsed=0:00:02.57    frame=  108 fps= 35 q=-1.0 size=    3880KiB time=00:00:03.60 bitrate=8829.2kbits/s speed=1.16x elapsed=0:00:03.09    frame=  124 fps= 34 q=-1.0 size=    4406KiB time=00:00:04.13 bitrate=8731.5kbits/s speed=1.15x elapsed=0:00:03.60    frame=  139 fps= 34 q=-1.0 size=    4747KiB time=00:00:04.63 bitrate=8392.9kbits/s speed=1.12x elapsed=0:00:04.12    frame=  155 fps= 33 q=-1.0 size=    5467KiB time=00:00:05.16 bitrate=8668.9kbits/s speed=1.11x elapsed=0:00:04.63    frame=  170 fps= 33 q=-1.0 size=    5969KiB time=00:00:05.66 bitrate=8628.6kbits/s speed= 1.1x elapsed=0:00:05.16    frame=  186 fps= 33 q=-1.0 size=    6571KiB time=00:00:06.20 bitrate=8682.4kbits/s speed=1.09x elapsed=0:00:05.67    frame=  201 fps= 33 q=-1.0 size=    6860KiB time=00:00:06.70 bitrate=8387.9kbits/s speed=1.08x elapsed=0:00:06.18    frame=  216 fps= 32 q=-1.0 size=    7357KiB time=00:00:07.20 bitrate=8370.5kbits/s speed=1.07x elapsed=0:00:06.71    frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.73 bitrate=8279.2kbits/s speed=1.07x elapsed=0:00:07.22    frame=  247 fps= 32 q=-1.0 size=    8354KiB time=00:00:08.23 bitrate=8312.2kbits/s speed=1.07x elapsed=0:00:07.73    frame=  263 fps= 32 q=-1.0 size=    8814KiB time=00:00:08.76 bitrate=8235.9kbits/s speed=1.06x elapsed=0:00:08.25    frame=  279 fps= 32 q=-1.0 size=    9867KiB time=00:00:09.30 bitrate=8691.2kbits/s speed=1.06x elapsed=0:00:08.76    frame=  293 fps= 32 q=-1.0 size=   10441KiB time=00:00:09.76 bitrate=8757.6kbits/s speed=1.05x elapsed=0:00:09.29    frame=  309 fps= 32 q=-1.0 size=   10765KiB time=00:00:10.30 bitrate=8561.7kbits/s speed=1.05x elapsed=0:00:09.79    frame=  325 fps= 32 q=-1.0 size=   11067KiB time=00:00:10.83 bitrate=8368.8kbits/s speed=1.05x elapsed=0:00:10.30    frame=  340 fps= 31 q=-1.0 size=   11663KiB time=00:00:11.33 bitrate=8430.4kbits/s speed=1.05x elapsed=0:00:10.83    frame=  355 fps= 31 q=-1.0 size=   12043KiB time=00:00:11.83 bitrate=8337.0kbits/s speed=1.04x elapsed=0:00:11.35    frame=  371 fps= 31 q=-1.0 size=   12531KiB time=00:00:12.36 bitrate=8300.5kbits/s speed=1.04x elapsed=0:00:11.86    frame=  387 fps= 31 q=-1.0 size=   12909KiB time=00:00:12.90 bitrate=8197.6kbits/s speed=1.04x elapsed=0:00:12.38    frame=  402 fps= 31 q=-1.0 size=   13671KiB time=00:00:13.40 bitrate=8357.6kbits/s speed=1.04x elapsed=0:00:12.89    frame=  418 fps= 31 q=-1.0 size=   14165KiB time=00:00:13.93 bitrate=8328.3kbits/s speed=1.04x elapsed=0:00:13.41    frame=  433 fps= 31 q=-1.0 size=   14763KiB time=00:00:14.43 bitrate=8378.9kbits/s speed=1.04x elapsed=0:00:13.92    frame=  449 fps= 31 q=-1.0 size=   15066KiB time=00:00:14.96 bitrate=8246.6kbits/s speed=1.04x elapsed=0:00:14.44    frame=  464 fps= 31 q=-1.0 size=   15574KiB time=00:00:15.46 bitrate=8248.9kbits/s speed=1.03x elapsed=0:00:14.95    frame=  480 fps= 31 q=-1.0 size=   15981KiB time=00:00:16.00 bitrate=8182.0kbits/s speed=1.03x elapsed=0:00:15.47    frame=  494 fps= 31 q=-1.0 size=   16533KiB time=00:00:16.46 bitrate=8225.2kbits/s speed=1.03x elapsed=0:00:15.98    frame=  510 fps= 31 q=-1.0 size=   16954KiB time=00:00:17.00 bitrate=8169.8kbits/s speed=1.03x elapsed=0:00:16.50    frame=  526 fps= 31 q=-1.0 size=   17773KiB time=00:00:17.53 bitrate=8303.8kbits/s speed=1.03x elapsed=0:00:17.01    frame=  541 fps= 31 q=-1.0 size=   18599KiB time=00:00:18.03 bitrate=8449.0kbits/s speed=1.03x elapsed=0:00:17.52    frame=  557 fps= 31 q=-1.0 size=   18902KiB time=00:00:18.56 bitrate=8340.0kbits/s speed=1.03x elapsed=0:00:18.05    frame=  572 fps= 31 q=-1.0 size=   19295KiB time=00:00:19.06 bitrate=8290.2kbits/s speed=1.03x elapsed=0:00:18.55    frame=  588 fps= 31 q=-1.0 size=   19754KiB time=00:00:19.60 bitrate=8256.4kbits/s speed=1.03x elapsed=0:00:19.08    frame=  603 fps= 31 q=-1.0 size=   20303KiB time=00:00:20.10 bitrate=8274.9kbits/s speed=1.03x elapsed=0:00:19.59    frame=  619 fps= 31 q=-1.0 size=   20706KiB time=00:00:20.63 bitrate=8221.0kbits/s speed=1.03x elapsed=0:00:20.11    frame=  634 fps= 31 q=-1.0 size=   21241KiB time=00:00:21.13 bitrate=8233.7kbits/s speed=1.02x elapsed=0:00:20.62    frame=  649 fps= 31 q=-1.0 size=   21867KiB time=00:00:21.63 bitrate=8280.3kbits/s speed=1.02x elapsed=0:00:21.14    frame=  665 fps= 31 q=-1.0 size=   22421KiB time=00:00:22.16 bitrate=8286.0kbits/s speed=1.02x elapsed=0:00:21.65    frame=  681 fps= 31 q=-1.0 size=   22921KiB time=00:00:22.70 bitrate=8271.8kbits/s speed=1.02x elapsed=0:00:22.17    frame=  696 fps= 31 q=-1.0 size=   23287KiB time=00:00:23.20 bitrate=8222.8kbits/s speed=1.02x elapsed=0:00:22.68    frame=  712 fps= 31 q=-1.0 size=   23695KiB time=00:00:23.73 bitrate=8178.6kbits/s speed=1.02x elapsed=0:00:23.21    frame=  727 fps= 31 q=-1.0 size=   24269KiB time=00:00:24.23 bitrate=8204.2kbits/s speed=1.02x elapsed=0:00:23.72    frame=  742 fps= 31 q=-1.0 size=   24677KiB time=00:00:24.73 bitrate=8173.4kbits/s speed=1.02x elapsed=0:00:24.23    frame=  758 fps= 31 q=-1.0 size=   25175KiB time=00:00:25.26 bitrate=8162.2kbits/s speed=1.02x elapsed=0:00:24.75    frame=  773 fps= 31 q=-1.0 size=   25507KiB time=00:00:25.80 bitrate=8098.9kbits/s speed=1.02x elapsed=0:00:25.26    frame=  789 fps= 31 q=-1.0 size=   26638KiB time=00:00:26.30 bitrate=8297.1kbits/s speed=1.02x elapsed=0:00:25.78    frame=  804 fps= 31 q=-1.0 size=   26943KiB time=00:00:26.80 bitrate=8235.6kbits/s speed=1.02x elapsed=0:00:26.29    frame=  820 fps= 31 q=-1.0 size=   27352KiB time=00:00:27.33 bitrate=8197.6kbits/s speed=1.02x elapsed=0:00:26.82    frame=  835 fps= 31 q=-1.0 size=   27840KiB time=00:00:27.83 bitrate=8194.0kbits/s speed=1.02x elapsed=0:00:27.32    frame=  851 fps= 31 q=-1.0 size=   28400KiB time=00:00:28.36 bitrate=8201.6kbits/s speed=1.02x elapsed=0:00:27.85    frame=  866 fps= 31 q=-1.0 size=   28799KiB time=00:00:28.86 bitrate=8172.7kbits/s speed=1.02x elapsed=0:00:28.35    frame=  881 fps= 31 q=-1.0 size=   29276KiB time=00:00:29.36 bitrate=8166.7kbits/s speed=1.02x elapsed=0:00:28.88    frame=  897 fps= 31 q=-1.0 size=   29912KiB time=00:00:29.90 bitrate=8195.2kbits/s speed=1.02x elapsed=0:00:29.38    frame=  912 fps= 30 q=-1.0 size=   30463KiB time=00:00:30.40 bitrate=8209.0kbits/s speed=1.02x elapsed=0:00:29.91    frame=  928 fps= 31 q=-1.0 size=   31041KiB time=00:00:30.93 bitrate=8220.6kbits/s speed=1.02x elapsed=0:00:30.41    frame=  943 fps= 30 q=-1.0 size=   31403KiB time=00:00:31.46 bitrate=8175.4kbits/s speed=1.02x elapsed=0:00:30.94    frame=  959 fps= 30 q=-1.0 size=   31798KiB time=00:00:31.96 bitrate=8148.8kbits/s speed=1.02x elapsed=0:00:31.45    frame=  974 fps= 30 q=-1.0 size=   32339KiB time=00:00:32.46 bitrate=8159.8kbits/s speed=1.02x elapsed=0:00:31.96    frame=  990 fps= 30 q=-1.0 size=   32767KiB time=00:00:33.00 bitrate=8134.2kbits/s speed=1.02x elapsed=0:00:32.49    frame= 1005 fps= 30 q=-1.0 size=   33285KiB time=00:00:33.50 bitrate=8139.3kbits/s speed=1.02x elapsed=0:00:33.00    frame= 1020 fps= 30 q=-1.0 size=   33575KiB time=00:00:34.00 bitrate=8089.6kbits/s speed=1.01x elapsed=0:00:33.51    frame= 1036 fps= 30 q=-1.0 size=   34879KiB time=00:00:34.53 bitrate=8273.9kbits/s speed=1.01x elapsed=0:00:34.03    frame= 1051 fps= 30 q=-1.0 size=   35211KiB time=00:00:35.03 bitrate=8233.7kbits/s speed=1.01x elapsed=0:00:34.54    frame= 1067 fps= 30 q=-1.0 size=   35620KiB time=00:00:35.56 bitrate=8204.3kbits/s speed=1.01x elapsed=0:00:35.06    frame= 1082 fps= 30 q=-1.0 size=   36127KiB time=00:00:36.06 bitrate=8205.7kbits/s speed=1.01x elapsed=0:00:35.57    frame= 1098 fps= 30 q=-1.0 size=   36552KiB time=00:00:36.60 bitrate=8181.2kbits/s speed=1.01x elapsed=0:00:36.10    frame= 1114 fps= 30 q=-1.0 size=   37084KiB time=00:00:37.13 bitrate=8181.0kbits/s speed=1.01x elapsed=0:00:36.60    frame= 1129 fps= 30 q=-1.0 size=   37399KiB time=00:00:37.63 bitrate=8140.9kbits/s speed=1.01x elapsed=0:00:37.13    frame= 1145 fps= 30 q=-1.0 size=   38125KiB time=00:00:38.16 bitrate=8183.1kbits/s speed=1.01x elapsed=0:00:37.64    frame= 1160 fps= 30 q=-1.0 size=   38619KiB time=00:00:38.66 bitrate=8182.0kbits/s speed=1.01x elapsed=0:00:38.15    frame= 1176 fps= 30 q=-1.0 size=   39272KiB time=00:00:39.20 bitrate=8206.9kbits/s speed=1.01x elapsed=0:00:38.67    frame= 1191 fps= 30 q=-1.0 size=   39557KiB time=00:00:39.70 bitrate=8162.6kbits/s speed=1.01x elapsed=0:00:39.18    frame= 1207 fps= 30 q=-1.0 size=   40064KiB time=00:00:40.23 bitrate=8157.4kbits/s speed=1.01x elapsed=0:00:39.70    frame= 1222 fps= 30 q=-1.0 size=   40456KiB time=00:00:40.73 bitrate=8136.2kbits/s speed=1.01x elapsed=0:00:40.21    frame= 1237 fps= 30 q=-1.0 size=   41012KiB time=00:00:41.26 bitrate=8141.5kbits/s speed=1.01x elapsed=0:00:40.74    frame= 1253 fps= 30 q=-1.0 size=   41427KiB time=00:00:41.76 bitrate=8125.5kbits/s speed=1.01x elapsed=0:00:41.24    frame= 1268 fps= 30 q=-1.0 size=   42055KiB time=00:00:42.26 bitrate=8150.9kbits/s speed=1.01x elapsed=0:00:41.76    frame= 1284 fps= 30 q=-1.0 size=   43072KiB time=00:00:42.80 bitrate=8244.1kbits/s speed=1.01x elapsed=0:00:42.28    frame= 1299 fps= 30 q=-1.0 size=   43413KiB time=00:00:43.30 bitrate=8213.4kbits/s speed=1.01x elapsed=0:00:42.79    frame= 1315 fps= 30 q=-1.0 size=   43713KiB time=00:00:43.83 bitrate=8169.6kbits/s speed=1.01x elapsed=0:00:43.32    frame= 1330 fps= 30 q=-1.0 size=   44226KiB time=00:00:44.33 bitrate=8172.2kbits/s speed=1.01x elapsed=0:00:43.83    frame= 1345 fps= 30 q=-1.0 size=   44622KiB time=00:00:44.83 bitrate=8153.3kbits/s speed=1.01x elapsed=0:00:44.35    frame= 1361 fps= 30 q=-1.0 size=   45122KiB time=00:00:45.36 bitrate=8147.8kbits/s speed=1.01x elapsed=0:00:44.87    frame= 1377 fps= 30 q=-1.0 size=   45524KiB time=00:00:45.90 bitrate=8125.0kbits/s speed=1.01x elapsed=0:00:45.39    frame= 1392 fps= 30 q=-1.0 size=   46316KiB time=00:00:46.40 bitrate=8177.1kbits/s speed=1.01x elapsed=0:00:45.90    frame= 1408 fps= 30 q=-1.0 size=   46790KiB time=00:00:46.93 bitrate=8167.0kbits/s speed=1.01x elapsed=0:00:46.41    frame= 1423 fps= 30 q=-1.0 size=   47371KiB time=00:00:47.43 bitrate=8181.2kbits/s speed=1.01x elapsed=0:00:46.94    frame= 1439 fps= 30 q=-1.0 size=   47703KiB time=00:00:47.96 bitrate=8146.9kbits/s speed=1.01x elapsed=0:00:47.45    frame= 1455 fps= 30 q=-1.0 size=   48199KiB time=00:00:48.50 bitrate=8141.2kbits/s speed=1.01x elapsed=0:00:47.97    frame= 1470 fps= 30 q=-1.0 size=   48596KiB time=00:00:49.00 bitrate=8124.5kbits/s speed=1.01x elapsed=0:00:48.50    frame= 1485 fps= 30 q=-1.0 size=   49144KiB time=00:00:49.50 bitrate=8133.0kbits/s speed=1.01x elapsed=0:00:49.01    frame= 1501 fps= 30 q=-1.0 size=   49509KiB time=00:00:50.03 bitrate=8106.1kbits/s speed=1.01x elapsed=0:00:49.53    frame= 1516 fps= 30 q=-1.0 size=   49969KiB time=00:00:50.53 bitrate=8100.6kbits/s speed=1.01x elapsed=0:00:50.04    frame= 1532 fps= 30 q=-1.0 size=   51031KiB time=00:00:51.06 bitrate=8186.3kbits/s speed=1.01x elapsed=0:00:50.56    frame= 1548 fps= 30 q=-1.0 size=   51404KiB time=00:00:51.60 bitrate=8160.8kbits/s speed=1.01x elapsed=0:00:51.08    frame= 1563 fps= 30 q=-1.0 size=   51840KiB time=00:00:52.10 bitrate=8151.1kbits/s speed=1.01x elapsed=0:00:51.59    frame= 1579 fps= 30 q=-1.0 size=   52311KiB time=00:00:52.63 bitrate=8141.8kbits/s speed=1.01x elapsed=0:00:52.12    frame= 1594 fps= 30 q=-1.0 size=   52872KiB time=00:00:53.16 bitrate=8146.6kbits/s speed=1.01x elapsed=0:00:52.64    frame= 1610 fps= 30 q=-1.0 size=   53374KiB time=00:00:53.66 bitrate=8147.3kbits/s speed=1.01x elapsed=0:00:53.15    frame= 1625 fps= 30 q=-1.0 size=   53896KiB time=00:00:54.16 bitrate=8151.1kbits/s speed=1.01x elapsed=0:00:53.67    frame= 1641 fps= 30 q=-1.0 size=   54508KiB time=00:00:54.70 bitrate=8163.2kbits/s speed=1.01x elapsed=0:00:54.18    frame= 1657 fps= 30 q=-1.0 size=   55065KiB time=00:00:55.23 bitrate=8167.1kbits/s speed=1.01x elapsed=0:00:54.70    frame= 1672 fps= 30 q=-1.0 size=   55589KiB time=00:00:55.73 bitrate=8170.8kbits/s speed=1.01x elapsed=0:00:55.22    frame= 1687 fps= 30 q=-1.0 size=   55947KiB time=00:00:56.23 bitrate=8150.3kbits/s speed=1.01x elapsed=0:00:55.73    frame= 1703 fps= 30 q=-1.0 size=   56340KiB time=00:00:56.76 bitrate=8130.4kbits/s speed=1.01x elapsed=0:00:56.25    frame= 1718 fps= 30 q=-1.0 size=   56895KiB time=00:00:57.26 bitrate=8138.9kbits/s speed=1.01x elapsed=0:00:56.76    frame= 1734 fps= 30 q=-1.0 size=   57329KiB time=00:00:57.80 bitrate=8125.3kbits/s speed=1.01x elapsed=0:00:57.28    frame= 1749 fps= 30 q=-1.0 size=   57935KiB time=00:00:58.30 bitrate=8140.6kbits/s speed=1.01x elapsed=0:00:57.79    frame= 1764 fps= 30 q=-1.0 size=   58156KiB time=00:00:58.83 bitrate=8097.7kbits/s speed=1.01x elapsed=0:00:58.32    frame= 1780 fps= 30 q=-1.0 size=   59271KiB time=00:00:59.33 bitrate=8183.4kbits/s speed=1.01x elapsed=0:00:58.82    frame= 1796 fps= 30 q=-1.0 size=   59632KiB time=00:00:59.86 bitrate=8159.9kbits/s speed=1.01x elapsed=0:00:59.35    frame= 1811 fps= 30 q=-1.0 size=   60029KiB time=00:01:00.36 bitrate=8146.2kbits/s speed=1.01x elapsed=0:00:59.86    frame= 1827 fps= 30 q=-1.0 size=   60522KiB time=00:01:00.90 bitrate=8141.2kbits/s speed=1.01x elapsed=0:01:00.38    frame= 1842 fps= 30 q=-1.0 size=   61017KiB time=00:01:01.40 bitrate=8140.9kbits/s speed=1.01x elapsed=0:01:00.89    frame= 1858 fps= 30 q=-1.0 size=   61462KiB time=00:01:01.93 bitrate=8129.6kbits/s speed=1.01x elapsed=0:01:01.40    frame= 1873 fps= 30 q=-1.0 size=   61924KiB time=00:01:02.43 bitrate=8125.1kbits/s speed=1.01x elapsed=0:01:01.92    frame= 1888 fps= 30 q=-1.0 size=   62546KiB time=00:01:02.93 bitrate=8141.6kbits/s speed=1.01x elapsed=0:01:02.43    frame= 1904 fps= 30 q=-1.0 size=   63085KiB time=00:01:03.46 bitrate=8142.7kbits/s speed=1.01x elapsed=0:01:02.95    frame= 1919 fps= 30 q=-1.0 size=   63631KiB time=00:01:03.96 bitrate=8149.1kbits/s speed=1.01x elapsed=0:01:03.46    frame= 1935 fps= 30 q=-1.0 size=   64011KiB time=00:01:04.50 bitrate=8129.9kbits/s speed=1.01x elapsed=0:01:03.98    frame= 1950 fps= 30 q=-1.0 size=   64415KiB time=00:01:05.00 bitrate=8118.3kbits/s speed=1.01x elapsed=0:01:04.49    frame= 1966 fps= 30 q=-1.0 size=   64995KiB time=00:01:05.53 bitrate=8124.7kbits/s speed=1.01x elapsed=0:01:05.02    frame= 1981 fps= 30 q=-1.0 size=   65416KiB time=00:01:06.03 bitrate=8115.4kbits/s speed=1.01x elapsed=0:01:05.53    frame= 1997 fps= 30 q=-1.0 size=   65991KiB time=00:01:06.56 bitrate=8121.2kbits/s speed=1.01x elapsed=0:01:06.05    frame= 2012 fps= 30 q=-1.0 size=   66649KiB time=00:01:07.06 bitrate=8141.0kbits/s speed=1.01x elapsed=0:01:06.56    frame= 2027 fps= 30 q=-1.0 size=   67550KiB time=00:01:07.56 bitrate=8189.9kbits/s speed=1.01x elapsed=0:01:07.07    frame= 2043 fps= 30 q=-1.0 size=   67949KiB time=00:01:08.10 bitrate=8173.8kbits/s speed=1.01x elapsed=0:01:07.60    frame= 2059 fps= 30 q=-1.0 size=   68244KiB time=00:01:08.63 bitrate=8145.5kbits/s speed=1.01x elapsed=0:01:08.11    frame= 2074 fps= 30 q=-1.0 size=   68767KiB time=00:01:09.13 bitrate=8148.6kbits/s speed=1.01x elapsed=0:01:08.62    frame= 2089 fps= 30 q=-1.0 size=   69170KiB time=00:01:09.66 bitrate=8133.6kbits/s speed=1.01x elapsed=0:01:09.14    frame= 2105 fps= 30 q=-1.0 size=   69675KiB time=00:01:10.16 bitrate=8134.6kbits/s speed=1.01x elapsed=0:01:09.65    frame= 2120 fps= 30 q=-1.0 size=   70025KiB time=00:01:10.66 bitrate=8117.7kbits/s speed=1.01x elapsed=0:01:10.17    frame= 2136 fps= 30 q=-1.0 size=   70766KiB time=00:01:11.20 bitrate=8142.1kbits/s speed=1.01x elapsed=0:01:10.68    frame= 2151 fps= 30 q=-1.0 size=   71244KiB time=00:01:11.70 bitrate=8139.9kbits/s speed=1.01x elapsed=0:01:11.20    frame= 2167 fps= 30 q=-1.0 size=   71904KiB time=00:01:12.23 bitrate=8154.6kbits/s speed=1.01x elapsed=0:01:11.71    frame= 2182 fps= 30 q=-1.0 size=   72233KiB time=00:01:12.73 bitrate=8135.6kbits/s speed=1.01x elapsed=0:01:12.23    frame= 2197 fps= 30 q=-1.0 size=   72730KiB time=00:01:13.26 bitrate=8131.9kbits/s speed=1.01x elapsed=0:01:12.74    frame= 2214 fps= 30 q=-1.0 size=   73158KiB time=00:01:13.80 bitrate=8120.8kbits/s speed=1.01x elapsed=0:01:13.26    frame= 2229 fps= 30 q=-1.0 size=   73697KiB time=00:01:14.30 bitrate=8125.5kbits/s speed=1.01x elapsed=0:01:13.79    frame= 2244 fps= 30 q=-1.0 size=   74010KiB time=00:01:14.80 bitrate=8105.5kbits/s speed=1.01x elapsed=0:01:14.29    frame= 2260 fps= 30 q=-1.0 size=   74535KiB time=00:01:15.33 bitrate=8105.2kbits/s speed=1.01x elapsed=0:01:14.82    frame= 2275 fps= 30 q=-1.0 size=   75458KiB time=00:01:15.83 bitrate=8151.5kbits/s speed=1.01x elapsed=0:01:15.32    frame= 2291 fps= 30 q=-1.0 size=   75915KiB time=00:01:16.36 bitrate=8143.6kbits/s speed=1.01x elapsed=0:01:15.85    frame= 2306 fps= 30 q=-1.0 size=   76263KiB time=00:01:16.86 bitrate=8127.7kbits/s speed=1.01x elapsed=0:01:16.35    frame= 2322 fps= 30 q=-1.0 size=   76827KiB time=00:01:17.40 bitrate=8131.3kbits/s speed=1.01x elapsed=0:01:16.88    frame= 2337 fps= 30 q=-1.0 size=   77262KiB time=00:01:17.90 bitrate=8124.9kbits/s speed=1.01x elapsed=0:01:17.39    frame= 2352 fps= 30 q=-1.0 size=   77774KiB time=00:01:18.40 bitrate=8126.6kbits/s speed=1.01x elapsed=0:01:17.90    frame= 2368 fps= 30 q=-1.0 size=   78168KiB time=00:01:18.93 bitrate=8112.5kbits/s speed=1.01x elapsed=0:01:18.42    frame= 2383 fps= 30 q=-1.0 size=   78948KiB time=00:01:19.46 bitrate=8138.6kbits/s speed=1.01x elapsed=0:01:18.93    frame= 2399 fps= 30 q=-1.0 size=   79470KiB time=00:01:19.96 bitrate=8141.2kbits/s speed=1.01x elapsed=0:01:19.46    frame= 2415 fps= 30 q=-1.0 size=   80042KiB time=00:01:20.50 bitrate=8145.3kbits/s speed=1.01x elapsed=0:01:19.97    frame= 2430 fps= 30 q=-1.0 size=   80345KiB time=00:01:21.00 bitrate=8125.8kbits/s speed=1.01x elapsed=0:01:20.49    frame= 2446 fps= 30 q=-1.0 size=   80814KiB time=00:01:21.53 bitrate=8119.7kbits/s speed=1.01x elapsed=0:01:21.02    frame= 2461 fps= 30 q=-1.0 size=   81214KiB time=00:01:22.03 bitrate=8110.2kbits/s speed=1.01x elapsed=0:01:21.53    frame= 2477 fps= 30 q=-1.0 size=   81781KiB time=00:01:22.56 bitrate=8114.0kbits/s speed=1.01x elapsed=0:01:22.05    frame= 2492 fps= 30 q=-1.0 size=   82547KiB time=00:01:23.06 bitrate=8140.8kbits/s speed=1.01x elapsed=0:01:22.56    frame= 2508 fps= 30 q=-1.0 size=   82910KiB time=00:01:23.60 bitrate=8124.4kbits/s speed=1.01x elapsed=0:01:23.08    frame= 2523 fps= 30 q=-1.0 size=   83827KiB time=00:01:24.10 bitrate=8165.5kbits/s speed=1.01x elapsed=0:01:23.61    frame= 2539 fps= 30 q=-1.0 size=   84212KiB time=00:01:24.63 bitrate=8151.2kbits/s speed=1.01x elapsed=0:01:24.12    frame= 2554 fps= 30 q=-1.0 size=   84583KiB time=00:01:25.16 bitrate=8135.9kbits/s speed=1.01x elapsed=0:01:24.64    frame= 2570 fps= 30 q=-1.0 size=   84997KiB time=00:01:25.66 bitrate=8128.0kbits/s speed=1.01x elapsed=0:01:25.15    frame= 2586 fps= 30 q=-1.0 size=   85575KiB time=00:01:26.20 bitrate=8132.6kbits/s speed=1.01x elapsed=0:01:25.68    frame= 2601 fps= 30 q=-1.0 size=   85923KiB time=00:01:26.70 bitrate=8118.6kbits/s speed=1.01x elapsed=0:01:26.18    frame= 2616 fps= 30 q=-1.0 size=   86468KiB time=00:01:27.20 bitrate=8123.3kbits/s speed=1.01x elapsed=0:01:26.69    frame= 2632 fps= 30 q=-1.0 size=   87089KiB time=00:01:27.73 bitrate=8131.9kbits/s speed=1.01x elapsed=0:01:27.22    frame= 2647 fps= 30 q=-1.0 size=   87616KiB time=00:01:28.23 bitrate=8134.7kbits/s speed=1.01x elapsed=0:01:27.73    frame= 2663 fps= 30 q=-1.0 size=   88186KiB time=00:01:28.76 bitrate=8138.4kbits/s speed=1.01x elapsed=0:01:28.25    frame= 2678 fps= 30 q=-1.0 size=   88562KiB time=00:01:29.26 bitrate=8127.3kbits/s speed=1.01x elapsed=0:01:28.76    frame= 2694 fps= 30 q=-1.0 size=   88961KiB time=00:01:29.80 bitrate=8115.4kbits/s speed=1.01x elapsed=0:01:29.28    frame= 2709 fps= 30 q=-1.0 size=   89500KiB time=00:01:30.30 bitrate=8119.4kbits/s speed=1.01x elapsed=0:01:29.79    frame= 2724 fps= 30 q=-1.0 size=   89903KiB time=00:01:30.80 bitrate=8111.1kbits/s speed=1.01x elapsed=0:01:30.31    frame= 2740 fps= 30 q=-1.0 size=   90550KiB time=00:01:31.33 bitrate=8121.7kbits/s speed=1.01x elapsed=0:01:30.82    frame= 2756 fps= 30 q=-1.0 size=   90788KiB time=00:01:31.86 bitrate=8095.8kbits/s speed=1.01x elapsed=0:01:31.35    frame= 2771 fps= 30 q=-1.0 size=   91892KiB time=00:01:32.36 bitrate=8149.9kbits/s speed=1.01x elapsed=0:01:31.85    frame= 2787 fps= 30 q=-1.0 size=   92304KiB time=00:01:32.90 bitrate=8139.5kbits/s speed=1.01x elapsed=0:01:32.38    frame= 2802 fps= 30 q=-1.0 size=   92688KiB time=00:01:33.40 bitrate=8129.5kbits/s speed=1.01x elapsed=0:01:32.89    frame= 2817 fps= 30 q=-1.0 size=   93107KiB time=00:01:33.90 bitrate=8122.8kbits/s speed=1.01x elapsed=0:01:33.40    frame= 2833 fps= 30 q=-1.0 size=   93631KiB time=00:01:34.43 bitrate=8122.4kbits/s speed=1.01x elapsed=0:01:33.92    frame= 2849 fps= 30 q=-1.0 size=   94039KiB time=00:01:34.96 bitrate=8112.0kbits/s speed=1.01x elapsed=0:01:34.44    frame= 2864 fps= 30 q=-1.0 size=   94544KiB time=00:01:35.46 bitrate=8112.9kbits/s speed=1.01x elapsed=0:01:34.95    frame= 2880 fps= 30 q=-1.0 size=   95196KiB time=00:01:36.00 bitrate=8123.4kbits/s speed=1.01x elapsed=0:01:35.47    frame= 2895 fps= 30 q=-1.0 size=   95699KiB time=00:01:36.50 bitrate=8124.0kbits/s speed=1.01x elapsed=0:01:35.98    frame= 2910 fps= 30 q=-1.0 size=   96231KiB time=00:01:37.00 bitrate=8127.0kbits/s speed=1.01x elapsed=0:01:36.50    frame= 2926 fps= 30 q=-1.0 size=   96647KiB time=00:01:37.53 bitrate=8117.5kbits/s speed=1.01x elapsed=0:01:37.01    frame= 2941 fps= 30 q=-1.0 size=   97038KiB time=00:01:38.03 bitrate=8108.8kbits/s speed=1.01x elapsed=0:01:37.52    frame= 2957 fps= 30 q=-1.0 size=   97617KiB time=00:01:38.56 bitrate=8113.0kbits/s speed=1.01x elapsed=0:01:38.05    frame= 2972 fps= 30 q=-1.0 size=   98230KiB time=00:01:39.06 bitrate=8122.8kbits/s speed=1.01x elapsed=0:01:38.57    frame= 2988 fps= 30 q=-1.0 size=   98663KiB time=00:01:39.60 bitrate=8114.9kbits/s speed=1.01x elapsed=0:01:39.08    frame= 3003 fps= 30 q=-1.0 size=   99345KiB time=00:01:40.10 bitrate=8130.2kbits/s speed=1.01x elapsed=0:01:39.59    frame= 3019 fps= 30 q=-1.0 size=  100213KiB time=00:01:40.63 bitrate=8157.8kbits/s speed=1.01x elapsed=0:01:40.11    frame= 3034 fps= 30 q=-1.0 size=  100588KiB time=00:01:41.13 bitrate=8147.9kbits/s speed=1.01x elapsed=0:01:40.62    frame= 3049 fps= 30 q=-1.0 size=  100880KiB time=00:01:41.63 bitrate=8131.3kbits/s speed=   1x elapsed=0:01:41.14    frame= 3065 fps= 30 q=-1.0 size=  101399KiB time=00:01:42.16 bitrate=8130.5kbits/s speed=   1x elapsed=0:01:41.66    frame= 3080 fps= 30 q=-1.0 size=  101769KiB time=00:01:42.66 bitrate=8120.4kbits/s speed=   1x elapsed=0:01:42.17    frame= 3096 fps= 30 q=-1.0 size=  102319KiB time=00:01:43.20 bitrate=8122.1kbits/s speed=1.01x elapsed=0:01:42.68    frame= 3111 fps= 30 q=-1.0 size=  102664KiB time=00:01:43.70 bitrate=8110.2kbits/s speed=   1x elapsed=0:01:43.20    frame= 3127 fps= 30 q=-1.0 size=  103417KiB time=00:01:44.23 bitrate=8127.8kbits/s speed=   1x elapsed=0:01:43.72    frame= 3143 fps= 30 q=-1.0 size=  103959KiB time=00:01:44.76 bitrate=8128.8kbits/s speed=1.01x elapsed=0:01:44.23    frame= 3157 fps= 30 q=-1.0 size=  104496KiB time=00:01:45.23 bitrate=8134.6kbits/s speed=   1x elapsed=0:01:44.74    frame= 3173 fps= 30 q=-1.0 size=  104891KiB time=00:01:45.76 bitrate=8124.2kbits/s speed=   1x elapsed=0:01:45.27    frame= 3189 fps= 30 q=-1.0 size=  105390KiB time=00:01:46.30 bitrate=8121.9kbits/s speed=   1x elapsed=0:01:45.78    frame= 3204 fps= 30 q=-1.0 size=  105771KiB time=00:01:46.80 bitrate=8113.1kbits/s speed=   1x elapsed=0:01:46.29    frame= 3220 fps= 30 q=-1.0 size=  106347KiB time=00:01:47.33 bitrate=8116.7kbits/s speed=   1x elapsed=0:01:46.81    frame= 3235 fps= 30 q=-1.0 size=  106763KiB time=00:01:47.83 bitrate=8110.7kbits/s speed=   1x elapsed=0:01:47.32    frame= 3251 fps= 30 q=-1.0 size=  107500KiB time=00:01:48.36 bitrate=8126.5kbits/s speed=   1x elapsed=0:01:47.85    frame= 3266 fps= 30 q=-1.0 size=  108260KiB time=00:01:48.86 bitrate=8146.3kbits/s speed=   1x elapsed=0:01:48.36    frame= 3281 fps= 30 q=-1.0 size=  108708KiB time=00:01:49.36 bitrate=8142.7kbits/s speed=   1x elapsed=0:01:48.87    frame= 3297 fps= 30 q=-1.0 size=  109014KiB time=00:01:49.90 bitrate=8125.9kbits/s speed=   1x elapsed=0:01:49.39    frame= 3313 fps= 30 q=-1.0 size=  109517KiB time=00:01:50.43 bitrate=8124.1kbits/s speed=   1x elapsed=0:01:49.90    frame= 3328 fps= 30 q=-1.0 size=  109891KiB time=00:01:50.93 bitrate=8115.0kbits/s speed=   1x elapsed=0:01:50.42    frame= 3343 fps= 30 q=-1.0 size=  110425KiB time=00:01:51.43 bitrate=8117.8kbits/s speed=   1x elapsed=0:01:50.93    frame= 3359 fps= 30 q=-1.0 size=  110807KiB time=00:01:51.96 bitrate=8107.1kbits/s speed=   1x elapsed=0:01:51.45    frame= 3374 fps= 30 q=-1.0 size=  111575KiB time=00:01:52.46 bitrate=8127.0kbits/s speed=   1x elapsed=0:01:51.96    frame= 3390 fps= 30 q=-1.0 size=  112096KiB time=00:01:53.00 bitrate=8126.4kbits/s speed=   1x elapsed=0:01:52.49    frame= 3406 fps= 30 q=-1.0 size=  112676KiB time=00:01:53.50 bitrate=8132.6kbits/s speed=   1x elapsed=0:01:52.99    frame= 3421 fps= 30 q=-1.0 size=  113013KiB time=00:01:54.03 bitrate=8118.7kbits/s speed=   1x elapsed=0:01:53.52    frame= 3436 fps= 30 q=-1.0 size=  113461KiB time=00:01:54.53 bitrate=8115.3kbits/s speed=   1x elapsed=0:01:54.03    frame= 3451 fps= 30 q=-1.0 size=  113850KiB time=00:01:55.03 bitrate=8107.7kbits/s speed=   1x elapsed=0:01:54.54    frame= 3467 fps= 30 q=-1.0 size=  114447KiB time=00:01:55.56 bitrate=8112.6kbits/s speed=   1x elapsed=0:01:55.07    frame= 3482 fps= 30 q=-1.0 size=  115008KiB time=00:01:56.06 bitrate=8117.3kbits/s speed=   1x elapsed=0:01:55.57    frame= 3498 fps= 30 q=-1.0 size=  115226KiB time=00:01:56.60 bitrate=8095.5kbits/s speed=   1x elapsed=0:01:56.10    frame= 3513 fps= 30 q=-1.0 size=  116145KiB time=00:01:57.10 bitrate=8125.2kbits/s speed=   1x elapsed=0:01:56.60    frame= 3529 fps= 30 q=-1.0 size=  116696KiB time=00:01:57.63 bitrate=8126.7kbits/s speed=   1x elapsed=0:01:57.12    frame= 3545 fps= 30 q=-1.0 size=  117136KiB time=00:01:58.16 bitrate=8120.5kbits/s speed=   1x elapsed=0:01:57.65    frame= 3560 fps= 30 q=-1.0 size=  117571KiB time=00:01:58.66 bitrate=8116.4kbits/s speed=   1x elapsed=0:01:58.17    frame= 3576 fps= 30 q=-1.0 size=  118130KiB time=00:01:59.20 bitrate=8118.5kbits/s speed=   1x elapsed=0:01:58.69    frame= 3592 fps= 30 q=-1.0 size=  118576KiB time=00:01:59.73 bitrate=8112.8kbits/s speed=   1x elapsed=0:01:59.20    [out#0/rtp @ 000001b230749280] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.46    

~~~

### composition-r266-vlc.log (28568 bytes)

~~~text
-- logger module started --
main debug: VLC media player - 3.0.23 Vetinari
main debug: Copyright © 1996-2025 the VideoLAN team
main debug: revision 3.0.23-2-0-g79128878dd
main debug: configured with /builds/videolan/vlc/extras/package/win32/../../../configure  '--enable-update-check' '--enable-lua' '--enable-faad' '--enable-flac' '--enable-theora' '--enable-avcodec' '--enable-merge-ffmpeg' '--enable-mpc' '--enable-libass' '--enable-schroedinger' '--enable-realrtsp' '--enable-live555' '--enable-shout' '--enable-goom' '--enable-sse' '--enable-mmx' '--enable-libcddb' '--enable-zvbi' '--disable-telx' '--enable-nls' '--host=i686-w64-mingw32' '--with-contrib=../contrib/i686-w64-mingw32' '--with-breakpad=https://win.crashes.videolan.org' '--enable-qt' '--enable-skins2' '--enable-dvdread' '--enable-caca' 'host_alias=i686-w64-mingw32' 'CFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 ' 'CPPFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502' 'CXXFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 ' 'PKG_CONFIG=pkg-config' 'PKG_CONFIG_LIBDIR=/usr/i686-w64-mingw32/lib/pkgconfig:/usr/lib/i686-w64-mingw32/pkgconfig'
main debug: using multimedia timers as clock source
main debug:  min period: 1 ms, max period: 1000000 ms
main debug: searching plug-in modules
main debug: loading plugins cache file D:\VideoLAN\VLC\plugins\plugins.dat
main debug: recursively browsing `D:\VideoLAN\VLC\plugins'
main debug: plug-ins loaded: 495 modules
main debug: opening config file (C:\Users\ROG\AppData\Roaming\vlc\vlcrc)
main debug: looking for logger module matching "any": 2 candidates
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r266-vlc.log'
main debug: using logger module "file"
main debug: translation test: code is "C"
main debug: looking for keystore module matching "memory": 3 candidates
main debug: using keystore module "memory"
main debug: CPU has capabilities MMX MMXEXT SSE SSE2 SSE3 SSSE3 SSE4.1 SSE4.2 FPU 
main debug: Creating an input for 'Media Library'
main debug: Input is a meta file: disabling unneeded options
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `file/directory:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' gives access `file' demux `directory' path `/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf'
main debug: creating demux: access='file' demux='directory' location='/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' file='C:\Users\ROG\AppData\Roaming\vlc\ml.xspf'
main debug: looking for access_demux module matching "file": 15 candidates
main debug: no access_demux modules matched
main debug: creating access: file:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf
main debug:  (path: C:\Users\ROG\AppData\Roaming\vlc\ml.xspf)
main debug: looking for access module matching "file": 27 candidates
main debug: using access module "filesystem"
main debug: looking for stream_filter module matching "prefetch,cache_read": 24 candidates
cache_read debug: Using stream method for AStream*
cache_read debug: starting pre-buffering
cache_read debug: received first data after 0 ms
cache_read debug: pre-buffering done 304 bytes in 0s - 296875 KiB/s
main debug: using stream_filter module "cache_read"
main debug: looking for stream_filter module matching "any": 24 candidates
playlist debug: using XSPF playlist reader
main debug: using stream_filter module "playlist"
main debug: stream filter added to 039323d8
main debug: looking for stream_filter module matching "any": 24 candidates
main debug: no stream_filter modules matched
main debug: looking for stream_directory module matching "any": 1 candidates
main debug: no stream_directory modules matched
main debug: attachment of directory-extractor failed for file:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf
main debug: looking for stream_filter module matching "record": 24 candidates
main debug: using stream_filter module "record"
main debug: creating demux: access='file' demux='directory' location='/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' file='C:\Users\ROG\AppData\Roaming\vlc\ml.xspf'
main debug: looking for demux module matching "directory": 56 candidates
main debug: using demux module "directory"
main debug: looking for meta reader module matching "any": 2 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\reader
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\reader
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\reader\filename.luac
main debug: no meta reader modules matched
main debug: `file/directory:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' successfully opened
main debug: looking for xml reader module matching "any": 1 candidates
main debug: using xml reader module "xml"
main debug: EOF reached
main debug: removing module "directory"
main debug: removing module "record"
main debug: removing module "playlist"
main debug: removing module "cache_read"
main debug: removing module "filesystem"
main debug: creating audio output
main debug: looking for audio output module matching "any": 6 candidates
mmdevice debug: using default device
mmdevice debug: version 2 session control unavailable
mmdevice debug: volume from -65.250000 dB to +0.000000 dB with 0.031250 dB increments
main debug: using audio output module "mmdevice"
main debug: keeping audio output
main debug: looking for interface module matching "rc,none": 16 candidates
oldrc debug: base: 127.0.0.1, port: 63020
main debug: net: listening to 127.0.0.1 port 63020
main debug: using interface module "oldrc"
main debug: looking for interface module matching "hotkeys,none": 16 candidates
main debug: using interface module "hotkeys"
main debug: looking for interface module matching "globalhotkeys,none": 16 candidates
main debug: using interface module "win32"
main: Running vlc with the default interface. Use 'cvlc' to use vlc without interface.
main debug: looking for interface module matching "any": 16 candidates
main debug: looking for extension module matching "any": 1 candidates
lua debug: Opening Lua Extension module
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\extensions
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\extensions
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\extensions\VLSub.luac
lua debug: Scanning Lua script D:\VideoLAN\VLC\lua\extensions\VLSub.luac
lua debug: Script D:\VideoLAN\VLC\lua\extensions\VLSub.luac has the following capability flags: 0x5
main debug: using extension module "lua"
main debug: using interface module "qt"
main debug: processing request item: null, node: Playlist, skip: 0
main debug: rebuilding array of current - root Playlist
main debug: rebuild done - 1 items, index -1
main debug: starting playback of new item
main debug: resyncing on rtp://127.0.0.1:62020
main debug: rtp://127.0.0.1:62020 is at 0
main debug: creating new input thread
main debug: Creating an input for 'rtp://127.0.0.1:62020'
main debug: requesting art for new input thread
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `rtp://@127.0.0.1:62020' gives access `rtp' demux `any' path `@127.0.0.1:62020'
qt debug: IM: Setting an input
main debug: creating demux: access='rtp' demux='any' location='@127.0.0.1:62020' file='\\@127.0.0.1:62020'
main debug: looking for access_demux module matching "rtp": 15 candidates
main debug: looking for meta fetcher module matching "any": 1 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\fetcher
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\fetcher
main debug: no meta fetcher modules matched
main debug: looking for art finder module matching "any": 2 candidates
main debug: net: opening 127.0.0.1 datagram port 62020
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\art
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\art
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
main debug: using access_demux module "rtp"
main debug: looking for meta reader module matching "any": 2 candidates
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\reader
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\reader
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\reader\filename.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
main debug: no meta reader modules matched
main debug: `rtp://@127.0.0.1:62020' successfully opened
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
main debug: no art finder modules matched
rtp debug: detected MPEG2 TS
rtp debug: added payload type 33 (f = 90000 Hz)
rtp debug: added RTP source (1c030468)
main debug: creating demux: access='' demux='ts' location='' file='(null)'
main debug: looking for demux module matching "ts": 56 candidates
ts debug: Standard set to Auto
main debug: using demux module "ts"
ts debug: DEMUX_SET_GROUP -1 00000000
ts debug: PATCallBack called
ts debug: new PAT ts_id=1 version=0 current_next=1
ts debug:   * number=1 pid=256
ts debug: temporary receiving program 1
ts debug: PMTCallBack called for program 1
ts debug: new PMT program number=1 version=0 pid_pcr=257
ts debug:   * pid=257 type=0x24 ISO/IEC Reserved
ts debug:    => pid 257 has now es fcc=hevc
main debug: selecting program id=1
main debug: looking for packetizer module matching "any": 25 candidates
main debug: using packetizer module "hevc"
main debug: looking for video decoder module matching "any": 19 candidates
avcodec debug: using ffmpeg Lavc58.134.100
avcodec debug: CPU flags: 0x000fd3db
avcodec debug: allowing 10 thread(s) for decoding
avcodec debug: codec (hevc) started
avcodec debug: using frame thread mode with 10 threads
main debug: using video decoder module "avcodec"
ts debug: Default program is 1
ts debug:   * pid=258 type=0xf ISO/IEC 13818-7 Audio with ADTS transport
ts debug:      - SL/FMC descriptor not found/matched
ts debug:    => pid 258 has now es fcc=mp4a
main debug: looking for packetizer module matching "any": 25 candidates
mpeg4audio debug: running MPEG4 audio packetizer
mpeg4audio debug: ADTS Mode
main debug: using packetizer module "mpeg4audio"
main debug: looking for audio decoder module matching "any": 21 candidates
main debug: using audio decoder module "faad"
ts debug:   * pid=17 listening for SDT
ts debug: enabling pid 257 from program 1
ts debug: enabling pid 258 from program 1
ts debug: enabling pcr pid 257 from program 1
main debug: Buffering 0%
main debug: Buffering 6%
mpeg4audio: AAC channels: 2 samplerate: 44100
main debug: restarting module due to input format change
main debug: removing module "faad"
main debug: looking for audio decoder module matching "any": 21 candidates
main debug: using audio decoder module "faad"
faad warning: decoded zero sample
main debug: reusing audio output
main debug: looking for aout stream module matching "any": 2 candidates
main debug: restarting module due to input format change
main debug: removing module "avcodec"
main debug: looking for video decoder module matching "any": 19 candidates
avcodec debug: using ffmpeg Lavc58.134.100
avcodec debug: CPU flags: 0x000fd3db
avcodec debug: allowing 10 thread(s) for decoding
avcodec debug: codec (hevc) started
avcodec debug: using frame thread mode with 10 threads
main debug: using video decoder module "avcodec"
avcodec debug: available hardware decoder output format 53 (dxva2_vld)
avcodec debug: available hardware decoder output format 118 (d3d11va_vld)
avcodec debug: available hardware decoder output format 174 (d3d11)
avcodec debug: available software decoder output format 0 (yuv420p)
main debug: Buffering 13%
avcodec debug: trying format d3d11va_vld
main debug: looking for text renderer module matching "any": 3 candidates
freetype debug: Using DWrite backend
freetype debug: DWrite_GetFamily(): family name: Arial
freetype debug: DWrite_ParseFamily(): using font at index 0 with weight 400 for bold: 0, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 1 with weight 700 for bold: 1, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 2 with weight 400 for bold: 0, italic: 1
freetype debug: DWrite_ParseFamily(): using font at index 3 with weight 700 for bold: 1, italic: 1
main debug: using text renderer module "freetype"
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 32x32 (32x32) chroma: YUVA -> 16x16 (16x16) chroma: RGBA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: looking for video converter module matching "any": 24 candidates
yuvp debug: YUVP to YUVA converter
main debug: using video converter module "yuvp"
main debug: Deinterlacing available
main debug: deinterlace -1, mode auto, is_needed 0
main debug: looking for vout window module matching "qt,any": 3 candidates
qt debug: requesting video window...
main debug: resized to 80330752x59965912
main debug: resized to 80330752x59965912
main debug: resized to 80330496x59965912
main debug: resized to 80330496x59965912
main debug: resized to 80329984x59965912
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 2560x1382
main debug: resized to 2560x1382
main debug: resized to 2560x1382
main debug: resized to 2560x1382
main debug: resized to 2560x1382
main debug: resized to 2560x1382
main debug: resized to 2560x1382
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: using vout window module "qt"
main debug: looking for inhibit module matching "any": 1 candidates
main debug: using inhibit module "inhibit"
main debug: Opening vout display wrapper
main debug: looking for vout display module matching "any": 12 candidates
direct3d11 debug: creating Vout EventThread
direct3d11 debug: Win32VoutCreateWindow
main debug: VoutDisplayEvent 'resize' 1280x720
qt debug: Logical video size: 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: Buffering 19%
direct3d11 debug: inclinometer not found. (hr=0x80070490)
direct3d11 debug: created video sub-window
direct3d11 debug: Vout EventThread running
main debug: Buffering 26%
wasapi debug: modified format
main debug: Buffering 33%
direct3d11 debug: Created the D3D11 device type 1 level b100.
direct3d11 debug: supports colorspace RGB Rec.709 gamma:22 range:FULL
direct3d11 debug: supports colorspace RGB Rec.709 gamma:22 range:STUDIO
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:22 range:STUDIO
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:22 range:FULL
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:2084 range:FULL
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:2084 range:STUDIO
direct3d11 debug: Output max luminance: 400.0, colorspace RGB Rec.709 gamma:22 range:FULL, bits per pixel 10
direct3d11 debug: using colorspace RGB Rec.709 gamma:22 range:FULL
direct3d11 debug: Using pixel format VA_NV12 for chroma DX11
main debug: VoutDisplayEvent 'resize' 1302x776
main debug: Buffering 39%
direct3d11 debug: using GPU render fence
direct3d11 debug: Direct3D11 resources created
direct3d11 debug: Direct3D11 device adapter successfully initialized
direct3d11 debug: Direct3D11 Open Succeeded
main debug: using vout display module "direct3d11"
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x0a1700e0
main debug: original format sz 1280x736, of (0,0), vsz 1280x720, 4cc DX11, sar 1:1, msk r0x0 g0x0 b0x0
main debug: removing module "freetype"
main debug: looking for text renderer module matching "any": 3 candidates
main debug: VoutDisplayEvent 'resize' 1280x720
direct3d11 debug: Detected size change 1280x720
freetype debug: Using DWrite backend
freetype debug: DWrite_GetFamily(): family name: Arial
freetype debug: DWrite_ParseFamily(): using font at index 0 with weight 400 for bold: 0, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 1 with weight 700 for bold: 1, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 2 with weight 400 for bold: 0, italic: 1
freetype debug: DWrite_ParseFamily(): using font at index 3 with weight 700 for bold: 1, italic: 1
main debug: using text renderer module "freetype"
main debug: looking for hw decoder module matching "any": 2 candidates
d3d11va debug: Reusing Direct3D11 device
d3d11va debug: CreateDevice succeed
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
qt debug: Logical video size: 1280x720
d3d11va debug: - 'MPEG-2 & MPEG-1 variable-length decoder' is supported
main debug: resized to 1280x720
d3d11va debug: - 'MPEG-2 variable-length decoder' is supported
main debug: VoutDisplayEvent 'resize' 1280x720
d3d11va debug: - 'MPEG-1 variable-length decoder, no D pictures' is supported
d3d11va debug: - 'VC-1 variable-length decoder' is supported
d3d11va debug: - 'VC-1 variable-length decoder' is supported
d3d11va debug: - 'Unknown decoder 0x32fcfe3f-0xde46-0x4a49-0x861bac71110649d5' is supported
d3d11va debug: - 'H.264 stereo high profile, mbs flag set' is supported
d3d11va debug: - 'H.264 stereo high profile' is supported
d3d11va debug: - 'H.264 variable-length decoder, no film grain technology' is supported
d3d11va debug: - 'HEVC Main profile' is supported
d3d11va debug: - 'HEVC Main 10 profile' is supported
d3d11va debug: - 'Unknown decoder 0x1a72925f-0x0c2c-0x4f15-0x96fbb17d1473603f' is supported
d3d11va debug: - 'Unknown decoder 0x15df9b21-0x06c4-0x47f1-0x841ea67c97d7f312' is supported
d3d11va debug: - 'Unknown decoder 0x4008018f-0xf537-0x4b36-0x98cf61af8a2c1a33' is supported
d3d11va debug: - 'Unknown decoder 0x0dabeffa-0x4458-0x4602-0xbc030795659d617c' is supported
d3d11va debug: - 'Unknown decoder 0x9798634d-0xfe9d-0x48e5-0xb4dadbec45b3df01' is supported
d3d11va debug: - 'MPEG-4 Part 2 variable-length decoder, Simple Profile' is supported
d3d11va debug: - 'MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, no GMC' is supported
d3d11va debug: - 'MPEG-4 Part 2 nVidia bitstream decoder' is supported
d3d11va debug: - 'Unknown decoder 0x33fcfe41-0xde46-0x4a49-0x861bac71110649d5' is supported
d3d11va debug: - 'VP9 profile 0' is supported
d3d11va debug: - 'VP9 profile 2' is supported
d3d11va debug: - 'Unknown decoder 0xdda19dc7-0x93b5-0x49f5-0xa9b32bda28a2ce6e' is supported
d3d11va debug: - 'AV1 Main profile 8' is supported
d3d11va debug: - 'Unknown decoder 0x6affd11e-0x1d96-0x42b1-0xa21593a31f09a53d' is supported
d3d11va debug: - 'Unknown decoder 0x914c84a3-0x4078-0x4fa9-0x984ce2f262cb5c9c' is supported
d3d11va debug: - 'Unknown decoder 0x8a1a1031-0x29bc-0x46d0-0xa007e9b092ca6767' is supported
d3d11va debug: Trying to use 'HEVC Main profile' as input
d3d11va debug: NV12 output is supported for decoder HEVC Main profile.
d3d11va debug: Using output format NV12 for decoder HEVC Main profile
d3d11va debug: va_pool_SetupDecoder id 173 1280x736 count: 28
d3d11va warning: not enough decoding slices in the texture (6/28)
d3d11va debug: ID3D11VideoDecoderOutputView succeed with 28 surfaces (1280x768)
d3d11va debug: we got 2 decoder configurations
d3d11va debug: configuration[0] ConfigBitstreamRaw 1
d3d11va debug: configuration[1] ConfigBitstreamRaw 1
d3d11va debug: DxCreateDecoderSurfaces succeed
main debug: using hw decoder module "d3d11va"
avcodec: Using D3D11VA (NVIDIA GeForce RTX 4060 Laptop GPU, vendor 10de(NVIDIA), device 28e0, revision a1) for hardware decoding
main debug: Buffering 46%
main debug: Received first picture
main debug: Buffering 53%
main debug: Buffering 59%
main debug: Buffering 66%
wasapi debug: buffer size    : 96000 frames
wasapi debug: maximum latency: 000 ns
wasapi debug: default period : 10000000 ns
wasapi debug: minimum period : 3000000 ns
main debug: using aout stream module "wasapi"
main debug: output 'f32l' 48000 Hz Stereo frame=1 samples/8 bytes
main debug: looking for audio volume module matching "any": 2 candidates
main debug: using audio volume module "float_mixer"
main debug: input 'f32l' 44100 Hz Stereo frame=1 samples/8 bytes
main debug: looking for audio filter module matching "scaletempo": 16 candidates
scaletempo debug: format: 44100 rate, 2 nch, 4 bps, fl32
scaletempo debug: params: 30 stride, 0.200 overlap, 14 search
scaletempo debug: 1.000 scale, 1323.000 stride_in, 1323 stride_out, 1059 standing, 264 overlap, 617 search, 2204 queue, fl32 mode
main debug: using audio filter module "scaletempo"
main debug: conversion: 'f32l'->'f32l' 44100 Hz->44100 Hz Stereo->Stereo
main debug: conversion pipeline complete
main debug: conversion: 'f32l'->'f32l' 44100 Hz->44100 Hz Stereo->Stereo
main debug: conversion pipeline complete
main debug: looking for audio resampler module matching "any": 3 candidates
main debug: using audio resampler module "samplerate"
main debug: Buffering 73%
main debug: Buffering 79%
main debug: Buffering 86%
main debug: Buffering 93%
main debug: Buffering 99%
main debug: Stream buffering done (1066 ms in 961 ms)
main debug: Decoder wait done in 0 ms
main debug: inserting 9594 zeroes
mmdevice debug: state changed: 1
main warning: playback too late (78326): up-sampling
main debug: auto hiding mouse cursor
main warning: picture is too late to be displayed (missing 31 ms)
main debug: resampling stopped (drift: -11274 us)
main debug: accepted socket 3944 (from socket 1176)
main debug: looking for encoder module matching "any": 24 candidates
main debug: using encoder module "png"
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to build resize+chroma
main debug: looking for video converter module matching "any": 24 candidates
main debug: no video converter modules matched
main error: Failed to create video converter
chain debug: Trying to build chroma+resize
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1a10d158) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1a597e90) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (050eaac0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (19c76d20) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1a10d158 removed from chain
main debug: removing module "swscale"
main debug: Filter 1a597e90 removed from chain
main debug: Filter 050eaac0 removed from chain
main debug: removing module "swscale"
main debug: Filter 19c76d20 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r266-native-2026-10-19-15h07m55s352.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (050eaac0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1a10d158) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 050eaac0 removed from chain
main debug: removing module "swscale"
main debug: Filter 1a10d158 removed from chain
main error: write error: No error
main error: read error: No error
main debug: accepted socket 3944 (from socket 1176)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
main debug: killing decoder fourcc `hevc'
main debug: removing module "avcodec"
main error: read error: No error
main debug: saving a free vout
main debug: reusing provided vout
main debug: removing module "hevc"
main debug: incoming request - stopping current input
main debug: killing decoder fourcc `mp4a'
main debug: removing module "faad"
mmdevice debug: state changed: 0
wasapi debug: reset
main debug: removing module "samplerate"
main debug: removing module "scaletempo"
main debug: removing module "float_mixer"
main debug: keeping audio output
main debug: removing module "mpeg4audio"
main debug: removing module "rtp"
rtp debug: removing RTP source (1c030468)
ts debug: Can't read TS packet at 168898448
main debug: removing module "ts"
main debug: Program doesn't contain anymore ES
main debug: dead input
qt debug: IM: Deleting the input
main debug: nothing to play
main debug: destroying useless vout
main debug: removing module "direct3d11"
direct3d11 debug: Direct3D11 resources destroyed
direct3d11 debug: Direct3D11 device adapter closed
direct3d11 error: SetThumbNailClip failed: 0x800706f4
direct3d11 debug: Win32 Vout EventThread terminating
direct3d11 debug: Win32VoutCloseWindow
direct3d11 debug: WinProc WM_DESTROY
main debug: removing module "inhibit"
qt debug: releasing video...
qt debug: video widget is released
main debug: removing module "freetype"
main debug: removing module "yuvp"
main debug: removing module "swscale"
main debug: removing module "mmdevice"
qt debug: requesting exit...
qt debug: waiting for UI thread...
qt debug: video widget is orphaned
main debug: exiting
main debug: no exit handler
qt debug: QApp exec() finished
qt debug: video widget is orphaned
qt debug: Killing extension dialog provider
qt debug: ExtensionsDialogProvider is quitting...
main debug: removing module "lua"
main debug: removing module "win32"
main debug: removing module "hotkeys"
main debug: removing module "oldrc"
main debug: destroying
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp12016
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `rtp://127.0.0.1:62020'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --

~~~

重复逐周期telemetry不整段嵌入，保留最终汇总、错误警告、首尾漂移和监控末段。上述12个产物归档后逐项删除，5个本轮PID均不存在；指定源保留。
