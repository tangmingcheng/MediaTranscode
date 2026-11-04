# r347：音频直通交接回归

状态：实流尝试FAIL，preflight拒绝，未进入scheduler。沿用r346构建和120秒源，视频HEVC CBR 1280x720/30fps/8000kbps及原期限不变；移除音频编码目标，由源匹配选择CopyPacket，专门覆盖源startup原子批次到scheduler交接。本项不能替代r346音频转码或完整合屏验收。

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r347-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r347-source0.log' -PassThru
Write-Output "SOURCE_PID=$($source0.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r347 --input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r347-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r347-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r347-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r347-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r347-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r347-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
exit $cli.ExitCode
~~~~

## 实测结果

源38412、CLI9124；CLI自然exit1：NotInitialized: wire planning requires authoritative opened audio emission readback。未进入生产DAG，不构成CopyPacket交接运行通过；无VLC/快照。原r346转码失败仍未解决。归档后精确停止源。
~~~~text
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r347-cli-error.log
[CLI] realtime video graph preflight failed: NotInitialized: wire planning requires authoritative opened audio emission readback

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r347-cli.log
[CLI] input_type=2 input=rtp://127.0.0.1:61020 audio=on max_duration=source_driven hw=planner-highest-score
[2026-10-26 13:00:35.475] [info] [graph][planner.input] input=rtp://127.0.0.1:61020 branch_mode=transcode_frame source_stream=0 input_codec=h264 output_codec=hevc
[2026-10-26 13:00:35.477] [info] [graph][planner.score] candidate=cuda-nvenc score=3280 status=available decoder=h264 filter=scale_cuda=1280:720 encoder=hevc_nvenc zero_copy=true
[2026-10-26 13:00:35.477] [info] [graph][planner.score] candidate=qsv score=3268 status=available decoder=h264_qsv filter=scale_qsv=w=1280:h=720 encoder=hevc_qsv zero_copy=true
[2026-10-26 13:00:35.477] [info] [graph][planner.score] candidate=d3d11va-mediafoundation score=0 status=unavailable reason="unavailable chain; score=0; encoder=hevc_mf unavailable: encoder not found: hevc_mf"
[2026-10-26 13:00:35.477] [info] [graph][planner.score] candidate=rkmpp score=0 status=unavailable reason="unavailable chain; incomplete or inconsistent planner frame/transfer contracts"
[2026-10-26 13:00:35.477] [info] [graph][planner.score] candidate=vaapi score=0 status=unavailable reason="unavailable chain; score=0; filter=scale_vaapi=w=1280:h=720 unavailable: filter not found: scale_vaapi=w=1280:h=720; encoder=hevc_vaapi unavailable: encoder not found: hevc_vaapi"
[2026-10-26 13:00:35.478] [info] [graph][planner.score] candidate=videotoolbox score=0 status=unavailable reason="unavailable chain; score=0; filter=scale_videotoolbox=1280:720 unavailable: filter not found: scale_videotoolbox=1280:720; encoder=hevc_videotoolbox unavailable: encoder not found: hevc_videotoolbox"
[2026-10-26 13:00:35.623] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-26 13:00:35.655] [info] [graph][planner.capability] backend=cuda status=found probe=decoder_filter_encoder_open note=decoder/filter/encoder chain opened and negotiated
[2026-10-26 13:00:35.655] [info] [graph][planner.select] branch_mode=transcode_frame selected_chain=cuda-nvenc score=3280 source_stream=0 decoder=h264 filter=scale_cuda=1280:720 encoder=hevc_nvenc encoder_pix_fmt=cuda encoder_surface_fmt=nv12 backend=cuda zero_copy=true
[2026-10-26 13:00:35.655] [info] [graph][runtime.node] rtp_prepared_capture_boundary stage=sealed stream=Video capacity_bytes=1085262308 wire_capacity_bytes=5000000 observed_wire_bytes=1168943 retained_bytes=161470301 stream_buffered_bytes=1144525 maximum_datagram_bytes=65507 queued_datagrams=984 probe_active=0 last_rtp_ssrc=1039057716 last_rtp_sequence=4767 last_rtp_observed_ns=15300539645900
[2026-10-26 13:00:35.656] [info] [graph][runtime.node] rtp_prepared_capture_boundary stage=sealed stream=Audio capacity_bytes=1085262308 wire_capacity_bytes=5000000 observed_wire_bytes=1168943 retained_bytes=161470301 stream_buffered_bytes=24418 maximum_datagram_bytes=65507 queued_datagrams=29 probe_active=0 last_rtp_ssrc=3786969232 last_rtp_sequence=3649 last_rtp_observed_ns=15300539485400

~~~~

源日志首次读取因活动句柄拒绝，停止精确源PID后补录：
~~~~text
composition-r347-source0-stdout.log
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


composition-r347-source0.log
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
frame=   30 fps=0.0 q=-1.0 size=    1080KiB time=00:00:01.00 bitrate=8849.0kbits/s speed=1.94x elapsed=0:00:00.51    frame=   46 fps= 45 q=-1.0 size=    2245KiB time=00:00:01.53 bitrate=11992.8kbits/s speed= 1.5x elapsed=0:00:01.02    frame=   62 fps= 40 q=-1.0 size=    2629KiB time=00:00:02.06 bitrate=10419.8kbits/s speed=1.34x elapsed=0:00:01.54    frame=   77 fps= 37 q=-1.0 size=    2900KiB time=00:00:02.56 bitrate=9255.1kbits/s speed=1.25x elapsed=0:00:02.06    frame=   92 fps= 36 q=-1.0 size=    3450KiB time=00:00:03.06 bitrate=9216.5kbits/s speed=1.19x elapsed=0:00:02.57    frame=  108 fps= 35 q=-1.0 size=    3880KiB time=00:00:03.60 bitrate=8829.2kbits/s speed=1.17x elapsed=0:00:03.08    frame=  123 fps= 34 q=-1.0 size=    4388KiB time=00:00:04.10 bitrate=8746.2kbits/s speed=1.14x elapsed=0:00:03.61    frame=  139 fps= 34 q=-1.0 size=    4747KiB time=00:00:04.63 bitrate=8392.9kbits/s speed=1.12x elapsed=0:00:04.12    frame=  154 fps= 33 q=-1.0 size=    5443KiB time=00:00:05.15 bitrate=8649.5kbits/s speed=1.11x elapsed=0:00:04.63    frame=  170 fps= 33 q=-1.0 size=    5969KiB time=00:00:05.66 bitrate=8628.6kbits/s speed= 1.1x elapsed=0:00:05.15    frame=  185 fps= 33 q=-1.0 size=    6540KiB time=00:00:06.16 bitrate=8688.1kbits/s speed=1.09x elapsed=0:00:05.67    frame=  200 fps= 32 q=-1.0 size=    6845KiB time=00:00:06.68 bitrate=8385.0kbits/s speed=1.08x elapsed=0:00:06.18    frame=  216 fps= 32 q=-1.0 size=    7357KiB time=00:00:07.20 bitrate=8370.5kbits/s speed=1.07x elapsed=0:00:06.69    frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.73 bitrate=8279.2kbits/s speed=1.07x elapsed=0:00:07.22    frame=  247 fps= 32 q=-1.0 size=    8354KiB time=00:00:08.23 bitrate=8312.2kbits/s speed=1.06x elapsed=0:00:07.73    frame=  263 fps= 32 q=-1.0 size=    8814KiB time=00:00:08.76 bitrate=8235.9kbits/s speed=1.06x elapsed=0:00:08.24    frame=  278 fps= 32 q=-1.0 size=    9819KiB time=00:00:09.26 bitrate=8680.4kbits/s speed=1.06x elapsed=0:00:08.77    frame=  293 fps= 32 q=-1.0 size=   10441KiB time=00:00:09.80 bitrate=8727.8kbits/s speed=1.06x elapsed=0:00:09.28    frame=  309 fps= 32 q=-1.0 size=   10765KiB time=00:00:10.30 bitrate=8561.7kbits/s speed=1.05x elapsed=0:00:09.79    frame=  324 fps= 31 q=-1.0 size=   11046KiB time=00:00:10.80 bitrate=8378.7kbits/s speed=1.05x elapsed=0:00:10.30    frame=  340 fps= 31 q=-1.0 size=   11663KiB time=00:00:11.33 bitrate=8430.4kbits/s speed=1.05x elapsed=0:00:10.82    frame=  355 fps= 31 q=-1.0 size=   12043KiB time=00:00:11.86 bitrate=8313.6kbits/s speed=1.05x elapsed=0:00:11.33    frame=  371 fps= 31 q=-1.0 size=   12531KiB time=00:00:12.36 bitrate=8300.5kbits/s speed=1.04x elapsed=0:00:11.85    frame=  386 fps= 31 q=-1.0 size=   12887KiB time=00:00:12.86 bitrate=8204.7kbits/s speed=1.04x elapsed=0:00:12.37    frame=  402 fps= 31 q=-1.0 size=   13671KiB time=00:00:13.40 bitrate=8357.6kbits/s speed=1.04x elapsed=0:00:12.88    frame=  417 fps= 31 q=-1.0 size=   14130KiB time=00:00:13.90 bitrate=8327.3kbits/s speed=1.04x elapsed=0:00:13.39    frame=  433 fps= 31 q=-1.0 size=   14763KiB time=00:00:14.43 bitrate=8378.9kbits/s speed=1.04x elapsed=0:00:13.91    frame=  448 fps= 31 q=-1.0 size=   15051KiB time=00:00:14.93 bitrate=8256.5kbits/s speed=1.03x elapsed=0:00:14.43    frame=  464 fps= 31 q=-1.0 size=   15574KiB time=00:00:15.46 bitrate=8248.9kbits/s speed=1.03x elapsed=0:00:14.95    frame=  479 fps= 31 q=-1.0 size=   15958KiB time=00:00:15.97 bitrate=8183.1kbits/s speed=1.03x elapsed=0:00:15.46    frame=  494 fps= 31 q=-1.0 size=   16533KiB time=00:00:16.48 bitrate=8215.5kbits/s speed=1.03x elapsed=0:00:15.98    frame=  510 fps= 31 q=-1.0 size=   16954KiB time=00:00:17.00 bitrate=8169.8kbits/s speed=1.03x elapsed=0:00:16.49    frame=  525 fps= 31 q=-1.0 size=   17698KiB time=00:00:17.50 bitrate=8281.1kbits/s speed=1.03x elapsed=0:00:17.01    frame=  541 fps= 31 q=-1.0 size=   18599KiB time=00:00:18.03 bitrate=8449.0kbits/s speed=1.03x elapsed=0:00:17.52    frame=  556 fps= 31 q=-1.0 size=   18883KiB time=00:00:18.53 bitrate=8346.6kbits/s speed=1.03x elapsed=0:00:18.03    frame=  572 fps= 31 q=-1.0 size=   19295KiB time=00:00:19.06 bitrate=8290.2kbits/s speed=1.03x elapsed=0:00:18.55    frame=  587 fps= 31 q=-1.0 size=   19694KiB time=00:00:19.57 bitrate=8242.0kbits/s speed=1.03x elapsed=0:00:19.07    frame=  603 fps= 31 q=-1.0 size=   20303KiB time=00:00:20.10 bitrate=8271.4kbits/s speed=1.03x elapsed=0:00:19.58    frame=  618 fps= 31 q=-1.0 size=   20684KiB time=00:00:20.60 bitrate=8225.3kbits/s speed=1.02x elapsed=0:00:20.10    frame=  634 fps= 31 q=-1.0 size=   21241KiB time=00:00:21.13 bitrate=8233.7kbits/s speed=1.02x elapsed=0:00:20.62    frame=  649 fps= 31 q=-1.0 size=   21867KiB time=00:00:21.64 bitrate=8277.4kbits/s speed=1.02x elapsed=0:00:21.13    frame=  665 fps= 31 q=-1.0 size=   22421KiB time=00:00:22.16 bitrate=8286.0kbits/s speed=1.02x elapsed=0:00:21.64    frame=  680 fps= 31 q=-1.0 size=   22892KiB time=00:00:22.66 bitrate=8273.5kbits/s speed=1.02x elapsed=0:00:22.17    frame=  696 fps= 31 q=-1.0 size=   23287KiB time=00:00:23.20 bitrate=8222.8kbits/s speed=1.02x elapsed=0:00:22.68    frame=  711 fps= 31 q=-1.0 size=   23668KiB time=00:00:23.70 bitrate=8181.0kbits/s speed=1.02x elapsed=0:00:23.19    frame=  726 fps= 31 q=-1.0 size=   24248KiB time=00:00:24.21 bitrate=8202.2kbits/s speed=1.02x elapsed=0:00:23.72    frame=  742 fps= 31 q=-1.0 size=   24677KiB time=00:00:24.75 bitrate=8167.1kbits/s speed=1.02x elapsed=0:00:24.23    frame=  758 fps= 31 q=-1.0 size=   25175KiB time=00:00:25.26 bitrate=8162.2kbits/s speed=1.02x elapsed=0:00:24.74    frame=  773 fps= 31 q=-1.0 size=   25507KiB time=00:00:25.77 bitrate=8107.1kbits/s speed=1.02x elapsed=0:00:25.26    frame=  789 fps= 31 q=-1.0 size=   26638KiB time=00:00:26.30 bitrate=8297.1kbits/s speed=1.02x elapsed=0:00:25.78    frame=  804 fps= 31 q=-1.0 size=   26943KiB time=00:00:26.80 bitrate=8235.6kbits/s speed=1.02x elapsed=0:00:26.30    frame=  820 fps= 31 q=-1.0 size=   27352KiB time=00:00:27.33 bitrate=8197.6kbits/s speed=1.02x elapsed=0:00:26.82    frame=  835 fps= 31 q=-1.0 size=   27840KiB time=00:00:27.84 bitrate=8191.9kbits/s speed=1.02x elapsed=0:00:27.34    frame=  851 fps= 31 q=-1.0 size=   28400KiB time=00:00:28.36 bitrate=8201.6kbits/s speed=1.02x elapsed=0:00:27.85    
~~~~
源清理exit-1，监控会话77474 exit1，两个PID无残留。
四个本轮产物按清单清理完成。

## 独立源码诊断

publication_identity_review_b对照HEAD确认此次拒绝原已存在：MediaPreparedEmissionResolver要求TranscodeFrame；AudioPipelinePlanner虽可选CopyPacket，但仅转码填preparedEmission，后续resolveAudioFrames/GraphResourceLedger仍需decoder/resampler/encoder帧产品。因此是既有支持链缺口，不是本轮输出edges改动引入；本项仍未获运行证明。正确补齐应以已封闭输入AU envelope及权威时钟/规范化输出事实形成独立复制emission和packet-only保留，不能伪造encoder readback或把样本平均码率当未来峰率。当前未实施该扩展，继续r346主线。
