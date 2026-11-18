# r373 Windows Release realtime 准备失败

2026-11-02：FAIL。使用777目标全量Release构建，原规格三源H.264/AAC RTP → HEVC CBR8000kbps 1280×720/30fps、AAC CBR192kbps → MPEG-TS/UDP。

CLI31888 exit1，尚未输出。CUDA source1首个候选失败为exact still-receiving candidate owner；后续QSV缺authoritative packet retention，不能把末条错误替代首因。没有画面或A/V验收证据。

本轮源36468/14564/7268及VLC34448在准备失败后核对身份强制清理；源exit=-1是清理结果，不是自然退出或120秒通过。

## 实际命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r373-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r373-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r373-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r373-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r373-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r373-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r373 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r373-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r373-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r373-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r373-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
```

## 全部非周期CLI诊断与完整stderr

```text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-02 09:40:02.728] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 09:40:02.729] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 09:40:06.605] [info] [graph][runtime.node] rtp_prepared_capture_boundary stage=capacity_stop stream=Video capacity_bytes=1085262428 wire_capacity_bytes=5000000 observed_wire_bytes=4935679 retained_bytes=165800461 stream_buffered_bytes=4768869 maximum_datagram_bytes=65507 queued_datagrams=4170 probe_active=0 last_rtp_ssrc=2645542317 last_rtp_sequence=5832 last_rtp_observed_ns=3065523281100
[2026-11-02 09:40:06.619] [info] [graph][runtime.node] rtp_prepared_capture_boundary stage=capacity_stop stream=Audio capacity_bytes=1085262428 wire_capacity_bytes=5000000 observed_wire_bytes=4936642 retained_bytes=165801592 stream_buffered_bytes=167773 maximum_datagram_bytes=65507 queued_datagrams=197 probe_active=0 last_rtp_ssrc=1642110985 last_rtp_sequence=2133 last_rtp_observed_ns=3065536898300
[2026-11-02 09:40:06.742] [info] [graph][runtime.node] rtp_prepared_capture_boundary stage=capacity_stop stream=Video capacity_bytes=1005131294 wire_capacity_bytes=5000000 observed_wire_bytes=4934780 retained_bytes=85725239 stream_buffered_bytes=4934780 maximum_datagram_bytes=65507 queued_datagrams=4314 probe_active=0 last_rtp_ssrc=3385584020 last_rtp_sequence=5774 last_rtp_observed_ns=3065659986800
[2026-11-02 09:40:07.103] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 09:40:07.153] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 09:40:07.153] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 09:40:07.153] [info] [graph][planner.capability] composition_source_candidate_rejected source=1 candidate=cuda-nvenc error=InvalidArgument: Continuous RTP installation requires the exact still-receiving candidate owner
[2026-11-02 09:40:07.180] [info] [graph][planner.capability] composition_output_candidate_rejected encoder=hevc_nvenc error=InvalidArgument: Continuous RTP installation requires the exact still-receiving candidate owner
[2026-11-02 09:40:08.100] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264_qsv planned=h264_qsv hardware=true hwaccel=qsv hw_pix_fmt=qsv hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 09:40:08.101] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264_qsv hwaccel_flags=1
[2026-11-02 09:40:08.101] [info] [graph][planner.capability] composition_source_candidate_rejected source=0 candidate=qsv error=Unsupported: Composition prepared decoder has no authoritative packet retention
[2026-11-02 09:40:08.114] [info] [graph][planner.capability] composition_output_candidate_rejected encoder=hevc_qsv error=Unsupported: Composition prepared decoder has no authoritative packet retention
[2026-11-02 09:40:08.115] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=2259 completion_waits=2259 completed_datagrams=4591 drained_datagrams=2333 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[in @ 0000022ddd97ee80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022ddd97ee80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 22784.924089
[D3D11VA @ 0000022dd908ae80] Using device 8086:7d55 (Intel(R) Arc(TM) Graphics).
[CLI] realtime video graph preflight failed: Unsupported: Composition prepared decoder has no authoritative packet retention
```

## 本轮文件原始记录

### 
composition-r373-monitor.log

```text
1.0193245,1.25417282809262,140746752,554328064
2.0398427,0,142778368,556212224
3.0629375,0.416516987122174,146780160,560136192
4.0910463,0,150077440,563339264
5.1189677,0.483654772543009,77180928,516837376
```

### 
composition-r373-source0-stdout.log

```text
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


```

### 
composition-r373-source0.log

```text
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
[udp @ 0000019422f0f700] bind failed: Error number -10048 occurred
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
frame=   30 fps=0.0 q=-1.0 size=    1080KiB time=00:00:01.00 bitrate=8849.0kbits/s speed=1.96x elapsed=0:00:00.51    
frame=   46 fps= 45 q=-1.0 size=    2245KiB time=00:00:01.53 bitrate=11992.8kbits/s speed= 1.5x elapsed=0:00:01.02    
frame=   61 fps= 40 q=-1.0 size=    2521KiB time=00:00:02.04 bitrate=10107.3kbits/s speed=1.33x elapsed=0:00:01.53    
frame=   77 fps= 37 q=-1.0 size=    2900KiB time=00:00:02.56 bitrate=9255.1kbits/s speed=1.24x elapsed=0:00:02.06    
frame=   93 fps= 36 q=-1.0 size=    3470KiB time=00:00:03.10 bitrate=9168.7kbits/s speed= 1.2x elapsed=0:00:02.58    
frame=  108 fps= 35 q=-1.0 size=    3880KiB time=00:00:03.63 bitrate=8748.2kbits/s speed=1.17x elapsed=0:00:03.10    
frame=  123 fps= 34 q=-1.0 size=    4388KiB time=00:00:04.10 bitrate=8746.2kbits/s speed=1.14x elapsed=0:00:03.61    
frame=  139 fps= 34 q=-1.0 size=    4747KiB time=00:00:04.63 bitrate=8392.9kbits/s speed=1.12x elapsed=0:00:04.13    
frame=  154 fps= 33 q=-1.0 size=    5443KiB time=00:00:05.13 bitrate=8685.8kbits/s speed= 1.1x elapsed=0:00:04.65    
frame=  170 fps= 33 q=-1.0 size=    5969KiB time=00:00:05.68 bitrate=8594.9kbits/s speed= 1.1x elapsed=0:00:05.17    
frame=  187 fps= 33 q=-1.0 size=    6601KiB time=00:00:06.23 bitrate=8674.9kbits/s speed=1.09x elapsed=0:00:05.70    
frame=  202 fps= 33 q=-1.0 size=    6888KiB time=00:00:06.73 bitrate=8379.8kbits/s speed=1.08x elapsed=0:00:06.21    
frame=  217 fps= 32 q=-1.0 size=    7382KiB time=00:00:07.24 bitrate=8347.7kbits/s speed=1.08x elapsed=0:00:06.72    
frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.73 bitrate=8279.2kbits/s speed=1.07x elapsed=0:00:07.23    
frame=  248 fps= 32 q=-1.0 size=    8407KiB time=00:00:08.26 bitrate=8330.6kbits/s speed=1.07x elapsed=0:00:07.75    
frame=  263 fps= 32 q=-1.0 size=    8814KiB time=00:00:08.76 bitrate=8235.9kbits/s speed=1.06x elapsed=0:00:08.26    
frame=  279 fps= 32 q=-1.0 size=    9867KiB time=00:00:09.30 bitrate=8691.2kbits/s speed=1.06x elapsed=0:00:08.79    
frame=  295 fps= 32 q=-1.0 size=   10503KiB time=00:00:09.83 bitrate=8750.0kbits/s speed=1.06x elapsed=0:00:09.30    
frame=  310 fps= 32 q=-1.0 size=   10808KiB time=00:00:10.33 bitrate=8568.2kbits/s speed=1.05x elapsed=0:00:09.81    
frame=  326 fps= 32 q=-1.0 size=   11079KiB time=00:00:10.86 bitrate=8352.1kbits/s speed=1.05x elapsed=0:00:10.34    
frame=  341 fps= 31 q=-1.0 size=   11682KiB time=00:00:11.36 bitrate=8419.6kbits/s speed=1.05x elapsed=0:00:10.85    
frame=  356 fps= 31 q=-1.0 size=   12076KiB time=00:00:11.86 bitrate=8336.6kbits/s speed=1.04x elapsed=0:00:11.37    
frame=  372 fps= 31 q=-1.0 size=   12551KiB time=00:00:12.40 bitrate=8291.6kbits/s speed=1.04x elapsed=0:00:11.88    
frame=  387 fps= 31 q=-1.0 size=   12909KiB time=00:00:12.91 bitrate=8191.1kbits/s speed=1.04x elapsed=0:00:12.39    
frame=  402 fps= 31 q=-1.0 size=   13671KiB time=00:00:13.40 bitrate=8357.6kbits/s speed=1.04x elapsed=0:00:12.92    
frame=  419 fps= 31 q=-1.0 size=   14199KiB time=00:00:13.96 bitrate=8328.6kbits/s speed=1.04x elapsed=0:00:13.45    
frame=  434 fps= 31 q=-1.0 size=   14790KiB time=00:00:14.46 bitrate=8375.0kbits/s speed=1.04x elapsed=0:00:13.96    
frame=  449 fps= 31 q=-1.0 size=   15066KiB time=00:00:14.97 bitrate=8241.0kbits/s speed=1.03x elapsed=0:00:14.47    
frame=  465 fps= 31 q=-1.0 size=   15598KiB time=00:00:15.50 bitrate=8243.8kbits/s speed=1.03x elapsed=0:00:14.98    
frame=  480 fps= 31 q=-1.0 size=   15981KiB time=00:00:16.00 bitrate=8182.0kbits/s speed=1.03x elapsed=0:00:15.51    
frame=  496 fps= 31 q=-1.0 size=   16592KiB time=00:00:16.53 bitrate=8221.0kbits/s speed=1.03x elapsed=0:00:16.02    
frame=  512 fps= 31 q=-1.0 size=   17340KiB time=00:00:17.06 bitrate=8323.2kbits/s speed=1.03x elapsed=0:00:16.53    
frame=  527 fps= 31 q=-1.0 size=   17859KiB time=00:00:17.56 bitrate=8328.4kbits/s speed=1.03x elapsed=0:00:17.06    
frame=  542 fps= 31 q=-1.0 size=   18667KiB time=00:00:18.06 bitrate=8464.0kbits/s speed=1.03x elapsed=0:00:17.56    
frame=  558 fps= 31 q=-1.0 size=   18945KiB time=00:00:18.60 bitrate=8344.2kbits/s speed=1.03x elapsed=0:00:18.09    
frame=  573 fps= 31 q=-1.0 size=   19308KiB time=00:00:19.13 bitrate=8266.9kbits/s speed=1.03x elapsed=0:00:18.60    
frame=  589 fps= 31 q=-1.0 size=   19773KiB time=00:00:19.63 bitrate=8250.2kbits/s speed=1.03x elapsed=0:00:19.11    
frame=  604 fps= 31 q=-1.0 size=   20344KiB time=00:00:20.13 bitrate=8277.8kbits/s speed=1.03x elapsed=0:00:19.63    
frame=  619 fps= 31 q=-1.0 size=   20706KiB time=00:00:20.64 bitrate=8217.3kbits/s speed=1.02x elapsed=0:00:20.14    
frame=  635 fps= 31 q=-1.0 size=   21262KiB time=00:00:21.17 bitrate=8225.2kbits/s speed=1.02x elapsed=0:00:20.67    
frame=  651 fps= 31 q=-1.0 size=   21916KiB time=00:00:21.70 bitrate=8273.4kbits/s speed=1.02x elapsed=0:00:21.19    
frame=  666 fps= 31 q=-1.0 size=   22456KiB time=00:00:22.22 bitrate=8278.3kbits/s speed=1.02x elapsed=0:00:21.70    
frame=  682 fps= 31 q=-1.0 size=   22949KiB time=00:00:22.73 bitrate=8269.7kbits/s speed=1.02x elapsed=0:00:22.23    
frame=  697 fps= 31 q=-1.0 size=   23303KiB time=00:00:23.24 bitrate=8213.1kbits/s speed=1.02x elapsed=0:00:22.74    
frame=  713 fps= 31 q=-1.0 size=   23719KiB time=00:00:23.77 bitrate=8172.0kbits/s speed=1.02x elapsed=0:00:23.25    
frame=  728 fps= 31 q=-1.0 size=   24293KiB time=00:00:24.30 bitrate=8189.6kbits/s speed=1.02x elapsed=0:00:23.78    
frame=  744 fps= 31 q=-1.0 size=   24736KiB time=00:00:24.80 bitrate=8170.8kbits/s speed=1.02x elapsed=0:00:24.29    
frame=  760 fps= 31 q=-1.0 size=   25225KiB time=00:00:25.33 bitrate=8157.0kbits/s speed=1.02x elapsed=0:00:24.80    
frame=  775 fps= 31 q=-1.0 size=   25725KiB time=00:00:25.83 bitrate=8157.5kbits/s speed=1.02x elapsed=0:00:25.32    
frame=  790 fps= 31 q=-1.0 size=   26684KiB time=00:00:26.33 bitrate=8301.1kbits/s speed=1.02x elapsed=0:00:25.83    
frame=  806 fps= 31 q=-1.0 size=   27000KiB time=00:00:26.86 bitrate=8232.8kbits/s speed=1.02x elapsed=0:00:26.36    
frame=  822 fps= 31 q=-1.0 size=   27388KiB time=00:00:27.40 bitrate=8188.3kbits/s speed=1.02x elapsed=0:00:26.88    
frame=  837 fps= 31 q=-1.0 size=   27919KiB time=00:00:27.93 bitrate=8187.9kbits/s speed=1.02x elapsed=0:00:27.40    
frame=  853 fps= 31 q=-1.0 size=   28473KiB time=00:00:28.43 bitrate=8203.5kbits/s speed=1.02x elapsed=0:00:27.91    
frame=  868 fps= 31 q=-1.0 size=   28835KiB time=00:00:28.93 bitrate=8164.0kbits/s speed=1.02x elapsed=0:00:28.43    
frame=  883 fps= 31 q=-1.0 size=   29319KiB time=00:00:29.44 bitrate=8157.6kbits/s speed=1.02x elapsed=0:00:28.94    
frame=  899 fps= 31 q=-1.0 size=   29962KiB time=00:00:29.97 bitrate=8188.0kbits/s speed=1.02x elapsed=0:00:29.45    
frame=  915 fps= 31 q=-1.0 size=   30572KiB time=00:00:30.50 bitrate=8211.3kbits/s speed=1.02x elapsed=0:00:29.98    
frame=  930 fps= 30 q=-1.0 size=   31097KiB time=00:00:31.00 bitrate=8217.6kbits/s speed=1.02x elapsed=0:00:30.49    
frame=  945 fps= 30 q=-1.0 size=   31432KiB time=00:00:31.50 bitrate=8171.9kbits/s speed=1.02x elapsed=0:00:31.00    
frame=  961 fps= 30 q=-1.0 size=   31850KiB time=00:00:32.03 bitrate=8145.1kbits/s speed=1.02x elapsed=0:00:31.51    
frame=  976 fps= 30 q=-1.0 size=   32381KiB time=00:00:32.55 bitrate=8148.4kbits/s speed=1.02x elapsed=0:00:32.04    
frame=  992 fps= 30 q=-1.0 size=   33024KiB time=00:00:33.06 bitrate=8181.5kbits/s speed=1.01x elapsed=0:00:32.57    
frame= 1008 fps= 30 q=-1.0 size=   33347KiB time=00:00:33.60 bitrate=8130.3kbits/s speed=1.02x elapsed=0:00:33.08    
frame= 1023 fps= 30 q=-1.0 size=   34161KiB time=00:00:34.11 bitrate=8204.2kbits/s speed=1.02x elapsed=0:00:33.59    
frame= 1039 fps= 30 q=-1.0 size=   35003KiB time=00:00:34.63 bitrate=8279.4kbits/s speed=1.01x elapsed=0:00:34.12    
frame= 1054 fps= 30 q=-1.0 size=   35366KiB time=00:00:35.13 bitrate=8246.2kbits/s speed=1.01x elapsed=0:00:34.63    
frame= 1070 fps= 30 q=-1.0 size=   35670KiB time=00:00:35.66 bitrate=8192.7kbits/s speed=1.01x elapsed=0:00:35.15    
frame= 1085 fps= 30 q=-1.0 size=   36221KiB time=00:00:36.16 bitrate=8204.3kbits/s speed=1.01x elapsed=0:00:35.66    
frame= 1100 fps= 30 q=-1.0 size=   36626KiB time=00:00:36.68 bitrate=8178.2kbits/s speed=1.01x elapsed=0:00:36.17    
frame= 1117 fps= 30 q=-1.0 size=   37139KiB time=00:00:37.23 bitrate=8171.3kbits/s speed=1.01x elapsed=0:00:36.70    
frame= 1132 fps= 30 q=-1.0 size=   37478KiB time=00:00:37.73 bitrate=8136.6kbits/s speed=1.01x elapsed=0:00:37.21    
frame= 1147 fps= 30 q=-1.0 size=   38183KiB time=00:00:38.23 bitrate=8181.2kbits/s speed=1.01x elapsed=0:00:37.72    
frame= 1163 fps= 30 q=-1.0 size=   38724KiB time=00:00:38.76 bitrate=8183.0kbits/s speed=1.01x elapsed=0:00:38.25    
frame= 1178 fps= 30 q=-1.0 size=   39327KiB time=00:00:39.26 bitrate=8204.6kbits/s speed=1.01x elapsed=0:00:38.76    
frame= 1194 fps= 30 q=-1.0 size=   39608KiB time=00:00:39.80 bitrate=8152.5kbits/s speed=1.01x elapsed=0:00:39.27    
```

### 
composition-r373-source1-stdout.log

```text
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


```

### 
composition-r373-source1.log

```text
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
frame=   31 fps=0.0 q=-1.0 size=    1174KiB time=00:00:01.03 bitrate=9305.4kbits/s speed=1.98x elapsed=0:00:00.52    
frame=   46 fps= 45 q=-1.0 size=    2245KiB time=00:00:01.53 bitrate=11992.8kbits/s speed=1.48x elapsed=0:00:01.03    
frame=   62 fps= 40 q=-1.0 size=    2629KiB time=00:00:02.06 bitrate=10419.8kbits/s speed=1.33x elapsed=0:00:01.54    
frame=   77 fps= 37 q=-1.0 size=    2900KiB time=00:00:02.56 bitrate=9255.1kbits/s speed=1.24x elapsed=0:00:02.07    
frame=   93 fps= 36 q=-1.0 size=    3470KiB time=00:00:03.10 bitrate=9168.7kbits/s speed=1.19x elapsed=0:00:02.59    
frame=  108 fps= 35 q=-1.0 size=    3880KiB time=00:00:03.60 bitrate=8829.2kbits/s speed=1.16x elapsed=0:00:03.11    
frame=  124 fps= 34 q=-1.0 size=    4406KiB time=00:00:04.13 bitrate=8731.5kbits/s speed=1.14x elapsed=0:00:03.62    
frame=  139 fps= 34 q=-1.0 size=    4747KiB time=00:00:04.63 bitrate=8392.9kbits/s speed=1.12x elapsed=0:00:04.14    
frame=  155 fps= 33 q=-1.0 size=    5467KiB time=00:00:05.16 bitrate=8668.9kbits/s speed=1.11x elapsed=0:00:04.66    
frame=  171 fps= 33 q=-1.0 size=    6003KiB time=00:00:05.70 bitrate=8627.3kbits/s speed= 1.1x elapsed=0:00:05.18    
frame=  186 fps= 33 q=-1.0 size=    6571KiB time=00:00:06.20 bitrate=8682.4kbits/s speed=1.09x elapsed=0:00:05.71    
frame=  202 fps= 32 q=-1.0 size=    6888KiB time=00:00:06.73 bitrate=8380.3kbits/s speed=1.08x elapsed=0:00:06.22    
frame=  217 fps= 32 q=-1.0 size=    7382KiB time=00:00:07.23 bitrate=8360.7kbits/s speed=1.07x elapsed=0:00:06.73    
frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.73 bitrate=8279.2kbits/s speed=1.07x elapsed=0:00:07.24    
frame=  248 fps= 32 q=-1.0 size=    8407KiB time=00:00:08.26 bitrate=8330.6kbits/s speed=1.06x elapsed=0:00:07.76    
frame=  263 fps= 32 q=-1.0 size=    8814KiB time=00:00:08.76 bitrate=8235.9kbits/s speed=1.06x elapsed=0:00:08.27    
frame=  279 fps= 32 q=-1.0 size=    9867KiB time=00:00:09.30 bitrate=8691.2kbits/s speed=1.06x elapsed=0:00:08.80    
frame=  294 fps= 32 q=-1.0 size=   10476KiB time=00:00:09.80 bitrate=8757.4kbits/s speed=1.05x elapsed=0:00:09.31    
frame=  310 fps= 32 q=-1.0 size=   10808KiB time=00:00:10.33 bitrate=8568.2kbits/s speed=1.05x elapsed=0:00:09.82    
frame=  326 fps= 31 q=-1.0 size=   11079KiB time=00:00:10.86 bitrate=8352.3kbits/s speed=1.05x elapsed=0:00:10.35    
frame=  341 fps= 31 q=-1.0 size=   11682KiB time=00:00:11.36 bitrate=8419.6kbits/s speed=1.05x elapsed=0:00:10.86    
frame=  356 fps= 31 q=-1.0 size=   12076KiB time=00:00:11.86 bitrate=8336.6kbits/s speed=1.04x elapsed=0:00:11.38    
frame=  372 fps= 31 q=-1.0 size=   12551KiB time=00:00:12.40 bitrate=8291.6kbits/s speed=1.04x elapsed=0:00:11.89    
frame=  388 fps= 31 q=-1.0 size=   12936KiB time=00:00:12.93 bitrate=8193.5kbits/s speed=1.04x elapsed=0:00:12.40    
frame=  403 fps= 31 q=-1.0 size=   13695KiB time=00:00:13.43 bitrate=8351.4kbits/s speed=1.04x elapsed=0:00:12.93    
frame=  419 fps= 31 q=-1.0 size=   14199KiB time=00:00:13.96 bitrate=8328.6kbits/s speed=1.04x elapsed=0:00:13.46    
frame=  434 fps= 31 q=-1.0 size=   14790KiB time=00:00:14.50 bitrate=8355.7kbits/s speed=1.04x elapsed=0:00:13.97    
frame=  450 fps= 31 q=-1.0 size=   15085KiB time=00:00:15.00 bitrate=8238.4kbits/s speed=1.04x elapsed=0:00:14.48    
frame=  465 fps= 31 q=-1.0 size=   15598KiB time=00:00:15.50 bitrate=8243.8kbits/s speed=1.03x elapsed=0:00:14.99    
frame=  481 fps= 31 q=-1.0 size=   16003KiB time=00:00:16.03 bitrate=8176.7kbits/s speed=1.03x elapsed=0:00:15.52    
frame=  497 fps= 31 q=-1.0 size=   16618KiB time=00:00:16.56 bitrate=8217.3kbits/s speed=1.03x elapsed=0:00:16.03    
frame=  511 fps= 31 q=-1.0 size=   16979KiB time=00:00:17.03 bitrate=8165.7kbits/s speed=1.03x elapsed=0:00:16.54    
frame=  527 fps= 31 q=-1.0 size=   17859KiB time=00:00:17.56 bitrate=8328.4kbits/s speed=1.03x elapsed=0:00:17.07    
frame=  542 fps= 31 q=-1.0 size=   18667KiB time=00:00:18.06 bitrate=8464.0kbits/s speed=1.03x elapsed=0:00:17.57    
frame=  558 fps= 31 q=-1.0 size=   18945KiB time=00:00:18.60 bitrate=8344.2kbits/s speed=1.03x elapsed=0:00:18.10    
frame=  573 fps= 31 q=-1.0 size=   19308KiB time=00:00:19.13 bitrate=8266.9kbits/s speed=1.03x elapsed=0:00:18.61    
frame=  589 fps= 31 q=-1.0 size=   19773KiB time=00:00:19.63 bitrate=8250.2kbits/s speed=1.03x elapsed=0:00:19.12    
frame=  605 fps= 31 q=-1.0 size=   20376KiB time=00:00:20.16 bitrate=8277.0kbits/s speed=1.03x elapsed=0:00:19.64    
frame=  620 fps= 31 q=-1.0 size=   20723KiB time=00:00:20.66 bitrate=8214.4kbits/s speed=1.03x elapsed=0:00:20.15    
frame=  635 fps= 31 q=-1.0 size=   21262KiB time=00:00:21.16 bitrate=8229.0kbits/s speed=1.02x elapsed=0:00:20.68    
frame=  652 fps= 31 q=-1.0 size=   21937KiB time=00:00:21.73 bitrate=8268.9kbits/s speed=1.02x elapsed=0:00:21.20    
frame=  667 fps= 31 q=-1.0 size=   22490KiB time=00:00:22.23 bitrate=8286.4kbits/s speed=1.02x elapsed=0:00:21.71    
frame=  683 fps= 31 q=-1.0 size=   22975KiB time=00:00:22.76 bitrate=8267.0kbits/s speed=1.02x elapsed=0:00:22.24    
frame=  698 fps= 31 q=-1.0 size=   23330KiB time=00:00:23.26 bitrate=8214.2kbits/s speed=1.02x elapsed=0:00:22.75    
frame=  713 fps= 31 q=-1.0 size=   23719KiB time=00:00:23.76 bitrate=8175.7kbits/s speed=1.02x elapsed=0:00:23.26    
frame=  729 fps= 31 q=-1.0 size=   24317KiB time=00:00:24.30 bitrate=8197.7kbits/s speed=1.02x elapsed=0:00:23.79    
frame=  745 fps= 31 q=-1.0 size=   24762KiB time=00:00:24.83 bitrate=8168.5kbits/s speed=1.02x elapsed=0:00:24.30    
frame=  760 fps= 31 q=-1.0 size=   25225KiB time=00:00:25.33 bitrate=8157.0kbits/s speed=1.02x elapsed=0:00:24.81    
frame=  775 fps= 31 q=-1.0 size=   25725KiB time=00:00:25.83 bitrate=8157.5kbits/s speed=1.02x elapsed=0:00:25.33    
frame=  790 fps= 31 q=-1.0 size=   26684KiB time=00:00:26.36 bitrate=8290.6kbits/s speed=1.02x elapsed=0:00:25.84    
frame=  807 fps= 31 q=-1.0 size=   27018KiB time=00:00:26.90 bitrate=8227.8kbits/s speed=1.02x elapsed=0:00:26.37    
frame=  822 fps= 31 q=-1.0 size=   27388KiB time=00:00:27.40 bitrate=8188.3kbits/s speed=1.02x elapsed=0:00:26.89    
frame=  838 fps= 31 q=-1.0 size=   27939KiB time=00:00:27.93 bitrate=8193.5kbits/s speed=1.02x elapsed=0:00:27.41    
frame=  853 fps= 31 q=-1.0 size=   28473KiB time=00:00:28.43 bitrate=8203.5kbits/s speed=1.02x elapsed=0:00:27.92    
frame=  868 fps= 31 q=-1.0 size=   28835KiB time=00:00:28.96 bitrate=8154.6kbits/s speed=1.02x elapsed=0:00:28.44    
frame=  884 fps= 31 q=-1.0 size=   29343KiB time=00:00:29.46 bitrate=8157.7kbits/s speed=1.02x elapsed=0:00:28.95    
frame=  899 fps= 31 q=-1.0 size=   29962KiB time=00:00:30.00 bitrate=8181.8kbits/s speed=1.02x elapsed=0:00:29.46    
frame=  915 fps= 31 q=-1.0 size=   30572KiB time=00:00:30.50 bitrate=8211.3kbits/s speed=1.02x elapsed=0:00:29.99    
frame=  931 fps= 31 q=-1.0 size=   31119KiB time=00:00:31.03 bitrate=8214.7kbits/s speed=1.02x elapsed=0:00:30.52    
frame=  946 fps= 30 q=-1.0 size=   31450KiB time=00:00:31.53 bitrate=8170.5kbits/s speed=1.02x elapsed=0:00:31.03    
frame=  961 fps= 30 q=-1.0 size=   31850KiB time=00:00:32.03 bitrate=8145.1kbits/s speed=1.02x elapsed=0:00:31.54    
frame=  977 fps= 30 q=-1.0 size=   32404KiB time=00:00:32.56 bitrate=8151.0kbits/s speed=1.02x elapsed=0:00:32.07    
frame=  993 fps= 30 q=-1.0 size=   33043KiB time=00:00:33.10 bitrate=8177.8kbits/s speed=1.02x elapsed=0:00:32.58    
frame= 1008 fps= 30 q=-1.0 size=   33347KiB time=00:00:33.60 bitrate=8130.3kbits/s speed=1.02x elapsed=0:00:33.09    
frame= 1023 fps= 30 q=-1.0 size=   34161KiB time=00:00:34.10 bitrate=8206.6kbits/s speed=1.01x elapsed=0:00:33.60    
frame= 1039 fps= 30 q=-1.0 size=   35003KiB time=00:00:34.63 bitrate=8279.4kbits/s speed=1.01x elapsed=0:00:34.13    
frame= 1054 fps= 30 q=-1.0 size=   35366KiB time=00:00:35.13 bitrate=8246.2kbits/s speed=1.01x elapsed=0:00:34.64    
frame= 1070 fps= 30 q=-1.0 size=   35670KiB time=00:00:35.66 bitrate=8192.7kbits/s speed=1.01x elapsed=0:00:35.16    
frame= 1085 fps= 30 q=-1.0 size=   36221KiB time=00:00:36.16 bitrate=8204.3kbits/s speed=1.01x elapsed=0:00:35.67    
frame= 1101 fps= 30 q=-1.0 size=   36654KiB time=00:00:36.70 bitrate=8181.8kbits/s speed=1.01x elapsed=0:00:36.18    
frame= 1116 fps= 30 q=-1.0 size=   37115KiB time=00:00:37.20 bitrate=8173.3kbits/s speed=1.01x elapsed=0:00:36.71    
frame= 1132 fps= 30 q=-1.0 size=   37478KiB time=00:00:37.73 bitrate=8136.6kbits/s speed=1.01x elapsed=0:00:37.22    
frame= 1147 fps= 30 q=-1.0 size=   38183KiB time=00:00:38.23 bitrate=8181.2kbits/s speed=1.01x elapsed=0:00:37.73    
frame= 1163 fps= 30 q=-1.0 size=   38724KiB time=00:00:38.76 bitrate=8183.0kbits/s speed=1.01x elapsed=0:00:38.26    
frame= 1178 fps= 30 q=-1.0 size=   39327KiB time=00:00:39.26 bitrate=8204.6kbits/s speed=1.01x elapsed=0:00:38.77    
frame= 1194 fps= 30 q=-1.0 size=   39608KiB time=00:00:39.80 bitrate=8152.5kbits/s speed=1.01x elapsed=0:00:39.28    
frame= 1209 fps= 30 q=-1.0 size=   40114KiB time=00:00:40.30 bitrate=8154.3kbits/s speed=1.01x elapsed=0:00:39.79    
```

### 
composition-r373-source2-stdout.log

```text
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


```

### 
composition-r373-source2.log

```text
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
frame=   31 fps=0.0 q=-1.0 size=    1174KiB time=00:00:01.03 bitrate=9305.4kbits/s speed=   2x elapsed=0:00:00.51    
frame=   46 fps= 45 q=-1.0 size=    2245KiB time=00:00:01.53 bitrate=11992.8kbits/s speed=1.49x elapsed=0:00:01.02    
frame=   61 fps= 39 q=-1.0 size=    2521KiB time=00:00:02.03 bitrate=10157.2kbits/s speed=1.32x elapsed=0:00:01.54    
frame=   77 fps= 37 q=-1.0 size=    2900KiB time=00:00:02.56 bitrate=9255.1kbits/s speed=1.24x elapsed=0:00:02.06    
frame=   93 fps= 36 q=-1.0 size=    3470KiB time=00:00:03.10 bitrate=9168.7kbits/s speed=1.19x elapsed=0:00:02.59    
frame=  109 fps= 35 q=-1.0 size=    3912KiB time=00:00:03.63 bitrate=8820.2kbits/s speed=1.17x elapsed=0:00:03.10    
frame=  124 fps= 34 q=-1.0 size=    4406KiB time=00:00:04.13 bitrate=8731.5kbits/s speed=1.14x elapsed=0:00:03.61    
frame=  139 fps= 34 q=-1.0 size=    4747KiB time=00:00:04.63 bitrate=8392.9kbits/s speed=1.12x elapsed=0:00:04.13    
frame=  155 fps= 33 q=-1.0 size=    5467KiB time=00:00:05.16 bitrate=8668.9kbits/s speed=1.11x elapsed=0:00:04.66    
frame=  171 fps= 33 q=-1.0 size=    6003KiB time=00:00:05.70 bitrate=8627.3kbits/s speed= 1.1x elapsed=0:00:05.17    
frame=  187 fps= 33 q=-1.0 size=    6601KiB time=00:00:06.23 bitrate=8674.9kbits/s speed=1.09x elapsed=0:00:05.70    
frame=  202 fps= 32 q=-1.0 size=    6888KiB time=00:00:06.73 bitrate=8380.3kbits/s speed=1.08x elapsed=0:00:06.22    
frame=  218 fps= 32 q=-1.0 size=    7430KiB time=00:00:07.26 bitrate=8375.9kbits/s speed=1.08x elapsed=0:00:06.73    
frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.73 bitrate=8279.2kbits/s speed=1.07x elapsed=0:00:07.24    
frame=  248 fps= 32 q=-1.0 size=    8407KiB time=00:00:08.26 bitrate=8330.6kbits/s speed=1.07x elapsed=0:00:07.75    
frame=  264 fps= 32 q=-1.0 size=    8849KiB time=00:00:08.80 bitrate=8238.1kbits/s speed=1.06x elapsed=0:00:08.27    
frame=  279 fps= 32 q=-1.0 size=    9867KiB time=00:00:09.30 bitrate=8691.2kbits/s speed=1.06x elapsed=0:00:08.80    
frame=  294 fps= 32 q=-1.0 size=   10476KiB time=00:00:09.80 bitrate=8757.4kbits/s speed=1.05x elapsed=0:00:09.30    
frame=  310 fps= 32 q=-1.0 size=   10808KiB time=00:00:10.33 bitrate=8568.2kbits/s speed=1.05x elapsed=0:00:09.82    
frame=  325 fps= 31 q=-1.0 size=   11067KiB time=00:00:10.86 bitrate=8343.1kbits/s speed=1.05x elapsed=0:00:10.34    
frame=  341 fps= 31 q=-1.0 size=   11682KiB time=00:00:11.36 bitrate=8419.6kbits/s speed=1.05x elapsed=0:00:10.86    
frame=  356 fps= 31 q=-1.0 size=   12076KiB time=00:00:11.86 bitrate=8336.6kbits/s speed=1.04x elapsed=0:00:11.37    
frame=  372 fps= 31 q=-1.0 size=   12551KiB time=00:00:12.40 bitrate=8291.6kbits/s speed=1.04x elapsed=0:00:11.88    
frame=  388 fps= 31 q=-1.0 size=   12936KiB time=00:00:12.93 bitrate=8193.5kbits/s speed=1.04x elapsed=0:00:12.40    
frame=  402 fps= 31 q=-1.0 size=   13671KiB time=00:00:13.40 bitrate=8357.6kbits/s speed=1.04x elapsed=0:00:12.92    
frame=  419 fps= 31 q=-1.0 size=   14199KiB time=00:00:13.96 bitrate=8328.6kbits/s speed=1.04x elapsed=0:00:13.45    
frame=  435 fps= 31 q=-1.0 size=   14816KiB time=00:00:14.50 bitrate=8370.3kbits/s speed=1.04x elapsed=0:00:13.96    
frame=  449 fps= 31 q=-1.0 size=   15066KiB time=00:00:14.96 bitrate=8246.6kbits/s speed=1.03x elapsed=0:00:14.47    
frame=  465 fps= 31 q=-1.0 size=   15598KiB time=00:00:15.50 bitrate=8243.8kbits/s speed=1.03x elapsed=0:00:14.99    
frame=  481 fps= 31 q=-1.0 size=   16003KiB time=00:00:16.03 bitrate=8176.7kbits/s speed=1.03x elapsed=0:00:15.51    
frame=  496 fps= 31 q=-1.0 size=   16592KiB time=00:00:16.53 bitrate=8221.0kbits/s speed=1.03x elapsed=0:00:16.02    
frame=  512 fps= 31 q=-1.0 size=   17340KiB time=00:00:17.06 bitrate=8323.2kbits/s speed=1.03x elapsed=0:00:16.54    
frame=  527 fps= 31 q=-1.0 size=   17859KiB time=00:00:17.60 bitrate=8312.6kbits/s speed=1.03x elapsed=0:00:17.06    
frame=  543 fps= 31 q=-1.0 size=   18688KiB time=00:00:18.10 bitrate=8457.9kbits/s speed=1.03x elapsed=0:00:17.58    
frame=  558 fps= 31 q=-1.0 size=   18945KiB time=00:00:18.60 bitrate=8344.2kbits/s speed=1.03x elapsed=0:00:18.11    
frame=  574 fps= 31 q=-1.0 size=   19319KiB time=00:00:19.13 bitrate=8271.7kbits/s speed=1.03x elapsed=0:00:18.62    
frame=  589 fps= 31 q=-1.0 size=   19773KiB time=00:00:19.63 bitrate=8250.2kbits/s speed=1.03x elapsed=0:00:19.13    
frame=  605 fps= 31 q=-1.0 size=   20376KiB time=00:00:20.16 bitrate=8277.0kbits/s speed=1.03x elapsed=0:00:19.65    
frame=  620 fps= 31 q=-1.0 size=   20723KiB time=00:00:20.66 bitrate=8214.4kbits/s speed=1.02x elapsed=0:00:20.16    
frame=  636 fps= 31 q=-1.0 size=   21288KiB time=00:00:21.20 bitrate=8226.1kbits/s speed=1.02x elapsed=0:00:20.69    
frame=  651 fps= 31 q=-1.0 size=   21916KiB time=00:00:21.70 bitrate=8273.4kbits/s speed=1.02x elapsed=0:00:21.20    
frame=  666 fps= 31 q=-1.0 size=   22456KiB time=00:00:22.20 bitrate=8286.4kbits/s speed=1.02x elapsed=0:00:21.71    
frame=  682 fps= 31 q=-1.0 size=   22949KiB time=00:00:22.73 bitrate=8269.7kbits/s speed=1.02x elapsed=0:00:22.23    
frame=  697 fps= 31 q=-1.0 size=   23303KiB time=00:00:23.23 bitrate=8216.6kbits/s speed=1.02x elapsed=0:00:22.74    
frame=  713 fps= 31 q=-1.0 size=   23719KiB time=00:00:23.76 bitrate=8175.7kbits/s speed=1.02x elapsed=0:00:23.26    
frame=  729 fps= 31 q=-1.0 size=   24317KiB time=00:00:24.30 bitrate=8197.7kbits/s speed=1.02x elapsed=0:00:23.78    
frame=  744 fps= 31 q=-1.0 size=   24736KiB time=00:00:24.80 bitrate=8170.8kbits/s speed=1.02x elapsed=0:00:24.30    
frame=  759 fps= 31 q=-1.0 size=   25193KiB time=00:00:25.30 bitrate=8157.5kbits/s speed=1.02x elapsed=0:00:24.81    
frame=  775 fps= 31 q=-1.0 size=   25725KiB time=00:00:25.83 bitrate=8157.5kbits/s speed=1.02x elapsed=0:00:25.32    
frame=  791 fps= 31 q=-1.0 size=   26721KiB time=00:00:26.33 bitrate=8312.6kbits/s speed=1.02x elapsed=0:00:25.83    
frame=  806 fps= 31 q=-1.0 size=   27000KiB time=00:00:26.86 bitrate=8232.8kbits/s speed=1.02x elapsed=0:00:26.36    
frame=  822 fps= 31 q=-1.0 size=   27388KiB time=00:00:27.40 bitrate=8188.3kbits/s speed=1.02x elapsed=0:00:26.89    
frame=  837 fps= 31 q=-1.0 size=   27919KiB time=00:00:27.90 bitrate=8197.6kbits/s speed=1.02x elapsed=0:00:27.40    
frame=  853 fps= 31 q=-1.0 size=   28473KiB time=00:00:28.43 bitrate=8203.5kbits/s speed=1.02x elapsed=0:00:27.92    
frame=  869 fps= 31 q=-1.0 size=   28855KiB time=00:00:28.93 bitrate=8169.9kbits/s speed=1.02x elapsed=0:00:28.43    
frame=  883 fps= 31 q=-1.0 size=   29319KiB time=00:00:29.43 bitrate=8160.3kbits/s speed=1.02x elapsed=0:00:28.94    
frame=  899 fps= 31 q=-1.0 size=   29962KiB time=00:00:29.96 bitrate=8190.9kbits/s speed=1.02x elapsed=0:00:29.46    
frame=  915 fps= 31 q=-1.0 size=   30572KiB time=00:00:30.50 bitrate=8211.3kbits/s speed=1.02x elapsed=0:00:29.99    
frame=  931 fps= 31 q=-1.0 size=   31119KiB time=00:00:31.03 bitrate=8214.7kbits/s speed=1.02x elapsed=0:00:30.51    
frame=  946 fps= 30 q=-1.0 size=   31450KiB time=00:00:31.53 bitrate=8170.5kbits/s speed=1.02x elapsed=0:00:31.02    
frame=  961 fps= 30 q=-1.0 size=   31850KiB time=00:00:32.03 bitrate=8145.1kbits/s speed=1.02x elapsed=0:00:31.53    
frame=  977 fps= 30 q=-1.0 size=   32404KiB time=00:00:32.56 bitrate=8151.0kbits/s speed=1.02x elapsed=0:00:32.06    
frame=  993 fps= 30 q=-1.0 size=   33043KiB time=00:00:33.10 bitrate=8177.8kbits/s speed=1.02x elapsed=0:00:32.58    
frame= 1008 fps= 30 q=-1.0 size=   33347KiB time=00:00:33.60 bitrate=8130.3kbits/s speed=1.02x elapsed=0:00:33.09    
frame= 1024 fps= 30 q=-1.0 size=   34323KiB time=00:00:34.10 bitrate=8245.5kbits/s speed=1.01x elapsed=0:00:33.60    
frame= 1039 fps= 30 q=-1.0 size=   35003KiB time=00:00:34.63 bitrate=8279.4kbits/s speed=1.01x elapsed=0:00:34.13    
frame= 1054 fps= 30 q=-1.0 size=   35366KiB time=00:00:35.13 bitrate=8246.2kbits/s speed=1.01x elapsed=0:00:34.64    
frame= 1070 fps= 30 q=-1.0 size=   35670KiB time=00:00:35.66 bitrate=8192.7kbits/s speed=1.01x elapsed=0:00:35.15    
frame= 1086 fps= 30 q=-1.0 size=   36238KiB time=00:00:36.20 bitrate=8200.7kbits/s speed=1.01x elapsed=0:00:35.66    
frame= 1100 fps= 30 q=-1.0 size=   36626KiB time=00:00:36.70 bitrate=8175.5kbits/s speed=1.01x elapsed=0:00:36.18    
frame= 1117 fps= 30 q=-1.0 size=   37139KiB time=00:00:37.23 bitrate=8171.3kbits/s speed=1.01x elapsed=0:00:36.70    
frame= 1132 fps= 30 q=-1.0 size=   37478KiB time=00:00:37.73 bitrate=8136.6kbits/s speed=1.01x elapsed=0:00:37.22    
frame= 1147 fps= 30 q=-1.0 size=   38183KiB time=00:00:38.23 bitrate=8181.2kbits/s speed=1.01x elapsed=0:00:37.72    
frame= 1163 fps= 30 q=-1.0 size=   38724KiB time=00:00:38.76 bitrate=8183.0kbits/s speed=1.01x elapsed=0:00:38.25    
frame= 1178 fps= 30 q=-1.0 size=   39327KiB time=00:00:39.26 bitrate=8204.6kbits/s speed=1.01x elapsed=0:00:38.76    
frame= 1193 fps= 30 q=-1.0 size=   39589KiB time=00:00:39.76 bitrate=8155.5kbits/s speed=1.01x elapsed=0:00:39.27    
frame= 1209 fps= 30 q=-1.0 size=   40114KiB time=00:00:40.30 bitrate=8154.3kbits/s speed=1.01x elapsed=0:00:39.78    
```

### 
composition-r373-vlc.log

```text
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
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r373-vlc.log'
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
cache_read debug: pre-buffering done 300 bytes in 0s - 292968 KiB/s
main debug: using stream_filter module "cache_read"
main debug: looking for stream_filter module matching "any": 24 candidates
playlist debug: using XSPF playlist reader
main debug: using stream_filter module "playlist"
main debug: stream filter added to 03f2bb88
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
mmdevice debug: display name changed: VLC media player (LibVLC 3.0.23)
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
main debug: resyncing on udp://127.0.0.1:62024
main debug: udp://127.0.0.1:62024 is at 0
main debug: creating new input thread
main debug: Creating an input for 'udp://127.0.0.1:62024'
main debug: requesting art for new input thread
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `udp://@127.0.0.1:62024' gives access `udp' demux `any' path `@127.0.0.1:62024'
qt debug: IM: Setting an input
main debug: creating demux: access='udp' demux='any' location='@127.0.0.1:62024' file='\\@127.0.0.1:62024'
main debug: looking for access_demux module matching "udp": 15 candidates
main debug: no access_demux modules matched
main debug: creating access: udp://@127.0.0.1:62024
main debug: looking for meta fetcher module matching "any": 1 candidates
main debug:  (path: \\@127.0.0.1:62024)
main debug: looking for access module matching "udp": 27 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\fetcher
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\fetcher
main debug: no meta fetcher modules matched
main debug: looking for art finder module matching "any": 2 candidates
udp debug: opening server=:0 local=127.0.0.1:62024
main debug: net: opening 127.0.0.1 datagram port
```

## 清理清单

- composition-r373-cli-error.log (529 bytes)
- composition-r373-cli.log (3737 bytes)
- composition-r373-monitor.log (211 bytes)
- composition-r373-source0-stdout.log (540 bytes)
- composition-r373-source0.log (10903 bytes)
- composition-r373-source1-stdout.log (335 bytes)
- composition-r373-source1.log (10495 bytes)
- composition-r373-source2-stdout.log (335 bytes)
- composition-r373-source2.log (10495 bytes)
- composition-r373-vlc.log (8309 bytes)

逐项归档后删除。无快照/抓包/远程产物，指定120秒源保留。

## 首因定位

原始诊断显示09:40:06.742 capture容量停止：observed4934780、wireCapacity5000000，剩余65220小于maximumDatagram65507。captureFinished随后使09:40:07.153 CUDA source1安装失败；统一InvalidArgument触发后续候选尝试，最终QSV Unsupported覆盖了首个失败。需继续核查捕获容量停止的失败传播及连续会话交接时机；当前未修改生产代码，不通过增加probeSize或跳过still-receiving检查消除失败。
