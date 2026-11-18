# r369 Windows Release realtime 回归

2026-11-02：FAIL。非沙箱同规格复验，源PID2520/34116/4200，CLI14468 exit1，VLC27216。已收到RTP并打开三个CUDA解码器；在建图时以 Composition source branch differs from its planned connection product 失败。该错误为本轮真实回归发现，双独立源码局部PASS与Release构建成功不能代替实流验证。

CLI不足一秒退出，监控没有取得有效周期样本，无输出画面、快照、A/V或播放证明；源未完成120秒，不记成功。确认失败后按PID、程序名和命令行核对并清理三源及VLC，CLI已退出。

## 实际命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r369-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r369-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r369-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r369-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r369-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r369-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r369 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r369-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r369-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r369-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r369-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
```

三个源随后CLI/VLC同一宿主调用启动；每秒CPU/WS/private监控尚未取得样本CLI即退出。

## 原始失败与末尾诊断

```
[in @ 000001d53f487ac0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d53f487ac0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 9086.447622
[in @ 000001d53f4ebf80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d53f4ebf80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 23050.175222
[in @ 000001d531466d40] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d531466d40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 21514.240689
[CLI] realtime video graph preflight failed: InvalidArgument: Composition source branch differs from its planned connection product
[2026-11-02 09:12:07.462] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 09:12:07.463] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 09:12:07.464] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 09:12:07.534] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 09:12:07.534] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 09:12:07.534] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 09:12:07.574] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 09:12:07.574] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 09:12:07.575] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 09:12:07.655] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 09:12:07.669] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=222 completion_waits=222 completed_datagrams=509 drained_datagrams=288 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 09:12:07.669] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=200 completion_waits=200 completed_datagrams=477 drained_datagrams=278 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 09:12:07.670] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=4 completion_waits=4 completed_datagrams=3 drained_datagrams=0 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 09:12:07.670] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=203 completion_waits=203 completed_datagrams=349 drained_datagrams=147 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
```

## 清理清单

- composition-r369-cli-error.log (1095 bytes)
- composition-r369-cli.log (3363 bytes)
- composition-r369-source0-stdout.log (540 bytes)
- composition-r369-source0.log (8711 bytes)
- composition-r369-source1-stdout.log (335 bytes)
- composition-r369-source1.log (8135 bytes)
- composition-r369-source2-stdout.log (335 bytes)
- composition-r369-source2.log (8253 bytes)
- composition-r369-vlc.log (8309 bytes)

以上文件归档后逐项删除；保留指定120秒复用源。未运行local、未改外部FFmpeg。下一步定位规划产品与实际分支的不一致，保留严格校验后原规格复验。
