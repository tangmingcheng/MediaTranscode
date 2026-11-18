# rk391：RKMPP 所选音视频源失源与重入

代码：6d983c7c；文档基线 8efaf674。2026-11-04。
原规格三路 RTP H.264，源 2 带 AAC 且为所选音频；输出 MPEG-TS/UDP HEVC 1280×720、30fps、8000kbps CBR 目标，AAC 192kbps、44100Hz 双声道。指定 120 秒连续源，不循环、不降规格；无 local CLI 测试。

## 过程与取证边界

源 0/1/2 PID 1866374/1866375/1866377，CLI 1866378，监控 1866392，VLC 26664。
16:42:49.659 核对 exe 后 SIGINT 源 2；源 0/1 保持运行。
16:43:23.955 同源同端口新发送进程 1912011 启动；日志恢复 generation 2，内部 A/V phase 约 2268ns。
VLC 使用既有 sout duplicate，同时显示并录制 TS。该模式 RC stats 的 decoded/displayed 均为 0，snapshot 返回成功但没有生成文件；不能把该统计解释成引擎零输出，也不能声称已有实时快照。后续使用录制离线回放与音频检测补充证据。

## 实际命令

远程：/bin/bash /home/tang/MediaTranscode-composition-09093919/acceptance/rk391/run.sh

~~~bash
#!/usr/bin/env bash
set -u
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk391
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk391 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
printf 'SOURCE0=%s SOURCE1=%s SOURCE2=%s CLI=%s\n' "$s0" "$s1" "$s2" "$cli" | tee pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" | tee -a pids.txt
wait "$cli"; code=$?
printf 'CLI_EXIT=%s\n' "$code" | tee result.txt
wait "$mon"
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" | tee -a result.txt
wait "$s1"; printf 'SOURCE1_EXIT=%s\n' "$?" | tee -a result.txt
wait "$s2"; printf 'SOURCE2_EXIT=%s\n' "$?" | tee -a result.txt
printf 'RK391_RUN_FINISHED=1\n'

~~~

停止所选源并重入（本轮 acceptance/rk391 目录）：

~~~bash
test "$(readlink /proc/1866377/exe)" = /home/tang/ffmpeg-ab1e61a/bin/ffmpeg && kill -INT 1866377
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2-reentry.out 2>source2-reentry.log &
r2=$!; printf 'REENTRY2=%s\n' "$r2" | tee reentry-pid.txt
~~~

Windows VLC：

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk391-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk391-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk391-received.ts}}" --sout-keep udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

状态：实流与离线取证完成；本轮匹配 BYE 的所选音视频源恢复功能有证据，完整合屏门禁未通过。


## 最终结果

原源 0/1 自然结束 exit0，原源2主动SIGINT exit255；重入FFmpeg发送端完整120秒/3600帧 exit0，CLI generation2解码发布3569、准备发布3568。CLI观测结束后核对exe并SIGINT，caller_stop exit0。解码/滤镜/编码均DRM PRIME、software_frame=0。

队列最终0/峰值354；工作集33,710,080→42,459,136 bytes，后段维持末值；errors/workerErrors/droppedBuffers=0。payload预约/释放253099一致且归零，硬边界背压计数1；编码包75015/75015。不能将有限时段或计数平衡解释成长期稳定或零内容丢失。

VLC录制211427244 bytes，起点0.400000秒、时长223.468133秒，HEVC1280×720/30及AAC44100Hz双声道。离线FFmpeg退出0；silencedetect阈值-80dB、持续0.5秒，检出47.813107→82.779252（34.966145秒）及201.207460→223.468141（22.260680秒）近静音。两区间之间约118.428208秒没有达到该阈值/时长的静音事件，支持重入后非静音恢复，不证明逐样本零值、音频内容逐样本一致或端到端A/V。

离线音频分析保留DTS out of order和non monotonically increasing DTS警告，不能声称时序门禁通过；VLC进行了重封装，尚不能据此确定引擎线上输出是根因。

第一次回放PID7320使用D:/路径及start-time20，RC状态为stop且没有快照；退出后以file:/// URI重新启动PID42288，成功获得VLC原生回放快照。实际get_time分别34、71、105、168、223秒，画面分别三格、源2黑格/另两格继续、恢复三格、仅源2、全黑。均已逐张查看；这是离线回放取证，不冒充实时快照。

## 离线实际命令

~~~powershell
& 'D:/mabs/local64/bin-video/ffprobe.exe' -v error -show_entries format=duration,start_time,size:stream=codec_name,width,height,r_frame_rate,sample_rate,channels -of json 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk391-received.ts'
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk391-received.ts' -map 0:a:0 -vn -af 'silencedetect=noise=-80dB:d=0.5' -f null - 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/rk391/audio-analysis.log'
D:/VideoLAN/VLC/vlc.exe --no-one-instance --start-time=20 --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk391-replay- --snapshot-format=png D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk391-received.ts
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk391-replay.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk391-replay- --snapshot-format=png file:///D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk391-received.ts
~~~

两个回放均经Start-Process Hidden启动；成功回放经RC依次seek20/60/90/150/210，随后snapshot/get_time，末段pause，最终quit。

## 必要诊断原文

~~~text
[in#0/mpegts @ 00000290e4280640] DTS 120031 < 122999 out of order
Input #0, mpegts, from 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk391-received.ts':
  Duration: 00:03:43.47, start: 0.400000, bitrate: 7568 kb/s
  Program 1 
    Metadata:
      service_name    : 
      service_provider: 
  Stream #0:0[0xc8]: Audio: aac (LC) ([15][0][0][0] / 0x000F), 44100 Hz, stereo, fltp, 197 kb/s, start 0.400000
  Stream #0:1[0x64]: Video: hevc (Main) ([36][0][0][0] / 0x0024), yuv420p(tv), 1280x720, 30 fps, 30 tbr, 90k tbn, start 0.400000
Stream mapping:
  Stream #0:0 -> #0:0 (aac (native) -> pcm_s16le (native))
Output #0, null, to 'pipe:':
  Metadata:
    encoder         : Lavf62.19.101
  Stream #0:0: Audio: pcm_s16le, 44100 Hz, stereo, s16, 1411 kb/s
    Metadata:
      encoder         : Lavc62.36.101 pcm_s16le
[null @ 00000290e62be280] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 45056 >= 43155
[null @ 00000290e62be280] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 45056 >= 44179
[null @ 00000290e62be280] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 64554 >= 64384
[Parsed_silencedetect_0 @ 00000290e64d3f40] silence_start: 47.813107
[Parsed_silencedetect_0 @ 00000290e64d3f40] silence_end: 82.779252 | silence_duration: 34.966145
[Parsed_silencedetect_0 @ 00000290e64d3f40] silence_start: 201.20746
[Parsed_silencedetect_0 @ 00000290e64d3f40] silence_end: 223.468141 | silence_duration: 22.26068
[out#0/null @ 00000290e42c4940] video:0KiB audio:38508KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: unknown
size=N/A time=00:03:43.46 bitrate=N/A speed=1.64e+03x elapsed=0:00:00.13    
mpp[1866378]: mpp_platform: client 12 driver is not ready!
mpp[1866378]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x35010330] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x35010330] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 8341.369544
mpp[1866378]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x3505b4b0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x3505b4b0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 45462.488989
mpp[1866378]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x3505b750] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x3505b750] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 4195.877556
mpp[1866378]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[1866378]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[1866378]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[1866378]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[1866378]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[1866378]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[1866378]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[1866378]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[1866378]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[1866378]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[1866378]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x7efc001b90] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7efc001b90] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 8341.369544
[in @ 0x7e80002910] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7e80002910] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 45462.488989
[in @ 0x7e54002620] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7e54002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 4195.877556
mpp[1866378]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:720] fmt 0 rotate 0 mirror 0
[aac @ 0x7f28000b80] Qavg: 17267.225
mpp[1866378]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group
[2026-11-04 16:43:25.006] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=generation_change steady_ns=198511384981064 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1438 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:43:25.006] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=generation_change steady_ns=198511385169776 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=18 release_count=1457 release_committed_count=1438 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:43:25.006] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=generation_change steady_ns=198511385378030 input_count=1455 accepted_count=1455 would_block_count=0 resolved_count=1455 output_attempt_count=0 published_count=1455 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:43:25.011] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=generation_change steady_ns=198511389997830 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1455 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:43:25.013] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=generation_change steady_ns=198511391991119 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1455 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:43:25.013] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=generation_change steady_ns=198511392092038 input_count=1455 accepted_count=1455 would_block_count=0 resolved_count=1455 output_attempt_count=0 published_count=1455 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:43:25.015] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=generation_change steady_ns=198511393920241 input_count=1451 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1451 published_count=1450 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=4 incomplete=0
[2026-11-04 16:43:25.081] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-rk391.source.2.input.video generation=2 steady_ns=198511459723297 source_sequence=5 presentation_ns=1132333333 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 16:45:46.095] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=6711 software_frame=0
[2026-11-04 16:45:46.095] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=2 reason=stop steady_ns=198652474585767 input_count=3568 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3568 published_count=3568 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=5 incomplete=0
[2026-11-04 16:45:46.096] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=stop steady_ns=198652474616684 input_count=3597 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3597 published_count=3593 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=1 incomplete=0
[2026-11-04 16:45:46.096] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=stop steady_ns=198652474643809 input_count=3597 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3597 published_count=3593 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=1 incomplete=0
[2026-11-04 16:45:46.096] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=5024 drm_prime_output=5024 rga=5024 software_frame=0
[2026-11-04 16:45:46.096] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=2 reason=stop steady_ns=198652474788770 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.097] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=3598 drm_prime_output=3598 rga=3598 software_frame=0
[2026-11-04 16:45:46.097] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=stop steady_ns=198652476500304 input_count=3598 accepted_count=3598 would_block_count=0 resolved_count=3598 output_attempt_count=0 published_count=3598 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.099] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=3598 drm_prime_output=3598 rga=3598 software_frame=0
[2026-11-04 16:45:46.099] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=stop steady_ns=198652478543468 input_count=3598 accepted_count=3598 would_block_count=0 resolved_count=3598 output_attempt_count=0 published_count=3598 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.101] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=2 reason=stop steady_ns=198652480285919 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.101] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=stop steady_ns=198652480427963 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3598 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.101] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=stop steady_ns=198652480450422 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3598 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.101] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=2 reason=stop steady_ns=198652480485423 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.101] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=stop steady_ns=198652480513423 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3598 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.101] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=stop steady_ns=198652480541132 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3598 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.102] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=5024 software_frame=0 lineage_admission_receive_retries=7
[2026-11-04 16:45:46.102] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=2 reason=stop steady_ns=198652480690177 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.105] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=3598 software_frame=0 lineage_admission_receive_retries=0
[2026-11-04 16:45:46.105] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=stop steady_ns=198652484355331 input_count=3598 accepted_count=3598 would_block_count=0 resolved_count=3598 output_attempt_count=0 published_count=3598 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.109] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=3598 software_frame=0 lineage_admission_receive_retries=2
[2026-11-04 16:45:46.109] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=stop steady_ns=198652488118196 input_count=3598 accepted_count=3598 would_block_count=0 resolved_count=3598 output_attempt_count=0 published_count=3598 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.113] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=2 reason=stop steady_ns=198652491841977 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=3554 release_committed_count=3554 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.113] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=198652491921312 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=3600 release_committed_count=3583 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.113] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=198652491940854 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=3600 release_committed_count=3583 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.113] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=2 reason=stop steady_ns=198652491959229 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3554 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.113] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=198652491984896 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3583 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:45:46.113] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=198652492006480 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3583 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=354, threads=81, processThreads=112, workers=0, workerProgress=436326, workerProcessCalls=1298028, workerWaits=861685, workerWakeups=834268, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=890, logicalProcessors=8, averageSystemMachineCpuPercent=46.987984, averageProcessMachineCpuPercent=10.449172, peakProcessMachineCpuPercent=20.855615, averageProcessSingleCoreCpuPercent=83.593374, peakProcessSingleCoreCpuPercent=166.844920, initialWorkingSetBytes=33710080, workingSetBytes=42459136, peakWorkingSetBytes=42459136, totalPushed=386776, totalPopped=386740, droppedBuffers=0, encodedPacketsPushed=75015, encodedPacketsPopped=75015, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=2832671, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=130, graphPayloadReservations=253099, graphPayloadReleases=253099, graphPayloadPressureFailures=1, lastInputReceivedAtNs=198629817498435
~~~

## 清理复核
双端本轮16项cli/source/monitor/result/PID/run.sh文件逐项删除；本地另删audio-analysis.log、VLC实时/回放日志、received.ts及16:48:25、16:48:48、16:49:17、16:49:55、16:50:09五张原生回放快照。双端rk391目录仅留空目录。远程1866374/1866375/1866377/1866378/1866392/1912011与本地VLC26664/7320/42288均复核不存在。原指定源与正式产物保留。

独立证据边界复核：progress_frame_review_a、progress_frame_review_b 均明确 PASS；不代表完整合屏或时序验收通过。
