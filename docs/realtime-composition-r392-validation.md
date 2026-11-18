# r392：Windows 四路 H.264/HEVC 异构合屏准备失败

状态：FAIL。代码 6d983c7c，Release 产物沿用已成功构建版本；本轮未修改源码或执行 local CLI。
四路均使用指定 120 秒源；前三路 H.264 copy，源2带所选 AAC；第四路 NVENC 实时编码 HEVC CBR8000kbps、720p30/GOP60。输出 HEVC CBR8000kbps、720p30 与 AAC192kbps、44100Hz双声道，MPEG-TS/UDP。

CLI PID43564 在准备阶段 exit1：源3报 Prepared RTP AU prefix admission exhausted。未进入合屏运行，未取得画面或完整120秒验收；不能记四路功能通过。发送器仍工作，失败判定后核对精确PID/命令并清理30392/36064/37900/33340，均exit -1；这些是中止清理，不是自然结束。VLC40048经所属RC端口quit。

错误来自共享 MediaPreparedRtpSourceSession::drain 对冻结前 admittedUnits/retention.maximumUnits 的检查，RKMPP同样潜在受影响；尚未证明根因是容量计算、时钟等待还是HEVC事实探测。不得扩大经验容量或降低源规格。下一步核对HEVC前缀上界推导与准备状态机，再同规格复验。

## 实际命令

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r392-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r392-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r392-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r392-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r392-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r392-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r392-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r392-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r392 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r392-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r392-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r392-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r392-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~


## 诊断原文
~~~text
[CLI] realtime video graph preflight failed: InvalidArgument: Prepared RTP AU prefix admission exhausted
[CLI] composition_inputs=4 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-04 16:55:03.289] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-04 16:55:03.296] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-04 16:55:03.380] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-04 16:55:04.340] [info] [graph][planner.capability] composition_protocol_installed source=3
[2026-11-04 16:55:04.434] [info] [graph][planner.capability] composition_preparation_input_terminal source=3 error=InvalidArgument: Prepared RTP AU prefix admission exhausted
[2026-11-04 16:55:04.453] [info] [graph][planner.capability] composition_output_candidate_rejected encoder=hevc_nvenc error=InvalidArgument: Prepared RTP AU prefix admission exhausted
[2026-11-04 16:55:04.454] [info] [graph][planner.capability] composition_preparation_input_terminal source=3 error=InvalidArgument: Prepared RTP AU prefix admission exhausted
[2026-11-04 16:55:04.454] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=751 completion_waits=751 completed_datagrams=955 drained_datagrams=205 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 16:55:04.455] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=814 completion_waits=814 completed_datagrams=936 drained_datagrams=123 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 16:55:04.455] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=45 completion_waits=45 completed_datagrams=45 drained_datagrams=1 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 16:55:04.455] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=647 completion_waits=647 completed_datagrams=900 drained_datagrams=254 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 16:55:04.456] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=46 completion_waits=46 completed_datagrams=51 drained_datagrams=5 would_block=0 cancellations=0 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
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
  Stream #0:0 -> #0:0 (h264 (native) -> hevc (hevc_nvenc))
Output #0, rtp, to 'rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Stream #0:0(eng): Video: hevc (Main), yuv420p(tv, progressive), 1280x720 [SAR 1:1 DAR 16:9], q=2-31, 8000 kb/s, 30 fps, 90k tbn (default)
    Metadata:
      encoder         : Lavc62.36.101 hevc_nvenc
      handler_name    : ?Mainconcept Video Media Handler
    Side data:
      CPB properties: bitrate max/min/avg: 0/0/8000000 buffer size: 16000000 vbv_delay: N/A
frame=    1 fps=0.0 q=9.0 size=       0KiB time=-00:00:00.06 bitrate=  -0.0kbits/s speed=N/A elapsed=0:00:00.50    
frame=   17 fps= 16 q=12.0 size=     565KiB time=00:00:00.46 bitrate=9921.5kbits/s speed=0.452x elapsed=0:00:01.03    
frame=   33 fps= 21 q=14.0 size=    1512KiB time=00:00:01.00 bitrate=12383.6kbits/s speed=0.645x elapsed=0:00:01.55    
frame=   48 fps= 23 q=24.0 size=    2370KiB time=00:00:01.50 bitrate=12945.7kbits/s speed=0.725x elapsed=0:00:02.07    
frame=   64 fps= 25 q=22.0 size=    2755KiB time=00:00:02.03 bitrate=11101.0kbits/s speed=0.786x elapsed=0:00:02.58    
~~~

清理复核：上述11项CLI/四个源/VLC日志及stdout逐项删除，r392无文件残留；六个精确PID均不存在。没有抓包、录制或快照；指定源与正式构建产物保留。

下一诊断切片：仅在既有失败分支附加admitted/maximum/prefix/acquiring/canonical_retention/remaining_ns，复用锁内实际状态，不调整容量或错误码，不改变接纳行为。待Release重建与同规格复现。

诊断切片最终包含planner的media_id/rate/window_ns/replay_units/maximum_units与运行时失败detail；两名独立审查均明确PASS，尚不代表根因确认。候选：MediaRtpVideoFrameRateObserver将相邻marker时间戳无符号差值当帧间隔，可能误解呈现重排；RFC3550第5.1节允许呈现顺序与发送顺序不同：https://www.rfc-editor.org/rfc/rfc3550.html#section-5.1。须以现场marker证据核验，不先扩容或关闭B帧。
