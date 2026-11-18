# 双源 realtime 合屏首次运行记录

## r61：启动探测失败

Release 第四轮全量配置/构建 exit0；源码冻结双独立审查A/B PASS。双H.264/AAC RTP/RTCP输入，HEVC 1280×720 30fps 8000kbps CBR + AAC 192kbps，MPEG-TS over RTP输出，原120秒源。

CLI session85624自然exit1：`raw RTP video signaling probe reached open timeout before complete parameter sets; matching_rtp_packets=0 matching_datagram_bytes=0 parameter_sets=none`。未进入DAG、没有输出画面/A-V漂移数据，未及时取得CLI PID和CPU/内存趋势；证据不完整，FAIL。CLI启动后分步展示和启动源可能耗尽30秒窗口，不据此判断合屏算法。源0 PID30816/session6322、源1 PID31388/session59244，VLC PID30664/启动exit0；准备按已结束测试清理，不记自然源结束或通过。无截图、SDP或抓包。下一轮紧接启动CLI/源，保持30秒与原规格。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r61 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r61.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r61-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r61-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r61-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r61-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r61- --snapshot-format=png rtp://@127.0.0.1:62020
```


时间证据：CLI日志14:33:39.392建立、14:34:09.608结束；两源日志14:34:09.932/934建立，确认源启动晚于CLI探测期限。该失败属于本轮启动编排。已按身份核对清理PID30816/31388/30664，非自然结束。

## r62：实际源事实未满足几何合同

CLI PID28176自然exit1（2.24秒）；采样14:36:12.854，CPU0.3125s、WS170373120、private268845056。两个源紧接CLI启动，已收到参数集；日志出现D3D11VA Intel Arc设备，随后`InvalidArgument: Fixed canvas source requires authoritative dimensions and sample aspect ratio`，未进入runtime，无输出画面/A-V漂移。不能当作完整测试通过，FAIL。源0 PID36076/session17131、源1 PID32188/session83907，VLC PID16708/启动0；诊断后清理属于中止，不记源自然结束。没有SDP/截图/抓包。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r62 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r62.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r62-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r62-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r62-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r62-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r62- --snapshot-format=png rtp://@127.0.0.1:62020
```

清理核对：r61/r62各4个日志均已按文件清单删除；r62两个源session结束码-1（精确PID强制清理），CLI自然exit1，VLC已清理。无本轮抓包/截图/SDP残留；未触及指定120秒源。

## r63：SAR修复后进入解码器保留合同校验

2026-10-04 Release全量重建成功（配置/构建exit0）。完整SPS/VUI产出SAR，已越过r62几何拒绝。CLI日志19:43:33建立、19:43:38结束，自然exit1：`Unsupported: Composition prepared decoder has no authoritative packet retention`。日志显示CUDA解码和QSV候选准备，尚未运行输出DAG，无画面/A-V漂移。监控采样时CLI已退出，未捕获CLI PID/CPU/内存，证据不完整，FAIL。源0 PID8456/session31304、源1 PID14788/session36260、VLC PID9984；按中止测试清理，不记自然源结束或验收通过。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r63 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r63.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r63-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r63-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r63-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r63-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r63- --snapshot-format=png rtp://@127.0.0.1:62020
```


清理核对：源session31304/36260均以-1结束（按身份核对强制清理），VLC9984已清理；r63文件清单逐项删除，无前缀残留，指定120秒源保留。QSV qsvdec.c 的packet_fifo采用AUTO_GROW且保留AVPacket引用，当前adapter未声明该后端权威硬上界，不能添加经验容量绕过。两层候选错误覆盖使此前CUDA失败不可见，补充既有diagnostics记录后复验。

## r64：候选诊断暴露prepared捕获容量失败

2026-10-04 第二轮Release全量重建配置/构建exit0；SAR与候选诊断经双独立审查A/B明确PASS（源码范围）。CLI PID21204自然exit1。采样CPU累计0.015625→0.4375s，WS13488128→295534592，private19808256→386662400，短暂预flight分配，不能证明稳态内存趋势；尚无runtime A/V漂移或画面，FAIL。

CUDA/NVENC明确拒绝于源0：`AllocationFailed: raw RTP prepared A/V capture exceeded aggregate byte capacity: stream=probe retained_bytes=3002285 capacity=5000000`。随后QSV仍拒绝未知packet retention。r64没有证据支持CUDA filter失效，实际优先阻塞是prepared捕获总预算。不得通过提高调用方probe-size或经验容量规避。源0 PID7080/session57992、源1 PID2608/session90573、VLC17656；诊断记录后中止清理。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r64 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r64.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r64-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r64-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r64-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r64-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r64- --snapshot-format=png rtp://@127.0.0.1:62020
```

### CLI采样

```text
CLI_PID=21204
{"At":"2026-10-04T19:48:43.2821871+08:00","PID":21204,"CPU":0.015625,"WS":13488128,"Private":19808256}
{"At":"2026-10-04T19:48:43.8186572+08:00","PID":21204,"CPU":0.015625,"WS":13508608,"Private":19808256}
{"At":"2026-10-04T19:48:44.3486022+08:00","PID":21204,"CPU":0.015625,"WS":13508608,"Private":19808256}
{"At":"2026-10-04T19:48:44.8697875+08:00","PID":21204,"CPU":0.046875,"WS":64774144,"Private":71168000}
{"At":"2026-10-04T19:48:45.3780182+08:00","PID":21204,"CPU":0.046875,"WS":112476160,"Private":119111680}
{"At":"2026-10-04T19:48:45.8792516+08:00","PID":21204,"CPU":0.09375,"WS":153796608,"Private":160559104}
{"At":"2026-10-04T19:48:46.3888746+08:00","PID":21204,"CPU":0.109375,"WS":175505408,"Private":182333440}
{"At":"2026-10-04T19:48:46.9030308+08:00","PID":21204,"CPU":0.4375,"WS":295534592,"Private":386662400}
CLI_EXIT_OBSERVED
```

清理核对：源session57992/90573均-1，VLC17656已按身份清理；r64前缀文件无残留，源保留。

## r65：共享快照越过CUDA双源准备，源同步合同仍未闭合

第三轮Release全量配置/构建exit0；SAR、诊断及共享快照经双独立源码复审PASS。CLI PID30576自然exit1。CUDA双源解码与缩放已完成，并进入两源startup retention规划，越过r64快照重复payload预算失败。新拒绝：`InvalidArgument: Composition source requires its own synchronized A/V source plan`。未启动输出DAG，无画面/A-V漂移，FAIL；不能以预flight通过片段替代完整120秒验收。

CPU累计0.0625→0.25s、WS13533184→391557120、private19824640→513236992，均为短暂准备采样，非稳态趋势。startup retention两源video_bytes=3377500000/2922500000，需检查权威AU容量与聚合内存门禁，不据此分配或放宽预算。源0 PID24536/session49608、源1 PID12308/session67385、VLC30380，记录后中止清理。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r65 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r65.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r65-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r65-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r65-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r65-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r65- --snapshot-format=png rtp://@127.0.0.1:62020
```

### CLI采样

```text
CLI_PID=30576
{"At":"2026-10-04T19:55:43.1577077+08:00","PID":30576,"CPU":0.0625,"WS":13533184,"Private":19824640}
{"At":"2026-10-04T19:55:43.6929940+08:00","PID":30576,"CPU":0.0625,"WS":13545472,"Private":19824640}
{"At":"2026-10-04T19:55:44.2020095+08:00","PID":30576,"CPU":0.0625,"WS":13545472,"Private":19824640}
{"At":"2026-10-04T19:55:44.7170710+08:00","PID":30576,"CPU":0.078125,"WS":65466368,"Private":71802880}
{"At":"2026-10-04T19:55:45.2260609+08:00","PID":30576,"CPU":0.09375,"WS":115044352,"Private":121626624}
{"At":"2026-10-04T19:55:45.7453828+08:00","PID":30576,"CPU":0.109375,"WS":155205632,"Private":161824768}
{"At":"2026-10-04T19:55:46.2554918+08:00","PID":30576,"CPU":0.109375,"WS":177090560,"Private":183803904}
{"At":"2026-10-04T19:55:46.7635361+08:00","PID":30576,"CPU":0.25,"WS":391557120,"Private":513236992}
CLI_EXIT_OBSERVED
```

清理核对：r65两个源session49608/67385均-1（按身份清理），VLC30380已清理；本轮前缀文件无残留，CLI自然exit1。

## r66：准备期间第二源触及捕获总预算

Release全量重建配置/构建exit0，最终lead/FIFO接线双独立源码PASS。CLI PID21068自然exit1；源0完成CUDA首帧/缩放，源1在获取描述符快照时拒绝：`raw RTP prepared A/V capture exceeded aggregate byte capacity: stream=probe retained_bytes=4877153 capacity=5000000`。零payload副本已生效，但设备及顺序准备期间原捕获持续增长，剩余预算不足描述符；尚不能验证r65的lead/FIFO接线运行结果。随后QSV缺少packet retention事实失败。无输出DAG/画面/A-V漂移，FAIL。

CPU累计0.015625→0.625s，WS13508608→503390208，private19812352→626900992。源0 PID1876/session55454、源1 PID3296/session58176、VLC21348；记录后按中止清理，不记自然源结束。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r66 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r66.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r66-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r66-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r66-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r66-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r66- --snapshot-format=png rtp://@127.0.0.1:62020
```

### CLI采样

```text
CLI_PID=21068
{"At":"2026-10-04T20:06:26.7331134+08:00","PID":21068,"CPU":0.015625,"WS":13508608,"Private":19812352}
{"At":"2026-10-04T20:06:27.2638065+08:00","PID":21068,"CPU":0.015625,"WS":13520896,"Private":19812352}
{"At":"2026-10-04T20:06:27.7780787+08:00","PID":21068,"CPU":0.015625,"WS":13520896,"Private":19812352}
{"At":"2026-10-04T20:06:28.2792568+08:00","PID":21068,"CPU":0.03125,"WS":66875392,"Private":73207808}
{"At":"2026-10-04T20:06:28.7876434+08:00","PID":21068,"CPU":0.09375,"WS":120344576,"Private":126939136}
{"At":"2026-10-04T20:06:29.2961655+08:00","PID":21068,"CPU":0.109375,"WS":157511680,"Private":164274176}
{"At":"2026-10-04T20:06:29.8129560+08:00","PID":21068,"CPU":0.125,"WS":181465088,"Private":188354560}
{"At":"2026-10-04T20:06:30.3262294+08:00","PID":21068,"CPU":0.125,"WS":198340608,"Private":205242368}
{"At":"2026-10-04T20:06:30.8343718+08:00","PID":21068,"CPU":0.125,"WS":231415808,"Private":238456832}
{"At":"2026-10-04T20:06:31.3459191+08:00","PID":21068,"CPU":0.140625,"WS":263036928,"Private":270282752}
{"At":"2026-10-04T20:06:31.8678035+08:00","PID":21068,"CPU":0.3125,"WS":472596480,"Private":562294784}
{"At":"2026-10-04T20:06:32.3838408+08:00","PID":21068,"CPU":0.625,"WS":503390208,"Private":626900992}
CLI_EXIT_OBSERVED
```

## r67：同规格复验越过启动与FIFO合同

使用r66同一Release产物及完全相同媒体参数再运行，CLI PID18696自然exit1。双CUDA首帧/缩放与两源规划完成，r65的最终启动lead/FIFO合同拒绝不再出现；新失败：`InvalidArgument: Composition output allocation facts differ from prepared encoder readback`。未输出DAG/画面/A-V漂移，FAIL。r66容量失败仍保留为未关闭风险，不能因本次越过而宣称稳定启动。CPU累计0.015625→0.359375s，WS13496320→548478976，private19808256→710164480。

源0 PID10048/session85615、源1 PID18412/session84842、VLC34360；诊断归档后中止清理。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r67 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r67.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r67-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r67-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r67-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r67-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r67- --snapshot-format=png rtp://@127.0.0.1:62020
```

### CLI采样

```text
CLI_PID=18696
{"At":"2026-10-04T20:09:08.8869581+08:00","PID":18696,"CPU":0.015625,"WS":13496320,"Private":19808256}
{"At":"2026-10-04T20:09:09.4207191+08:00","PID":18696,"CPU":0.015625,"WS":13508608,"Private":19808256}
{"At":"2026-10-04T20:09:09.9348345+08:00","PID":18696,"CPU":0.015625,"WS":13508608,"Private":19808256}
{"At":"2026-10-04T20:09:10.4445336+08:00","PID":18696,"CPU":0.0625,"WS":66834432,"Private":73191424}
{"At":"2026-10-04T20:09:10.9510031+08:00","PID":18696,"CPU":0.0625,"WS":121970688,"Private":128602112}
{"At":"2026-10-04T20:09:11.4579493+08:00","PID":18696,"CPU":0.0625,"WS":158375936,"Private":165167104}
{"At":"2026-10-04T20:09:11.9707645+08:00","PID":18696,"CPU":0.0625,"WS":181407744,"Private":188338176}
{"At":"2026-10-04T20:09:12.4782649+08:00","PID":18696,"CPU":0.078125,"WS":198307840,"Private":205225984}
{"At":"2026-10-04T20:09:12.9975738+08:00","PID":18696,"CPU":0.078125,"WS":232394752,"Private":239419392}
{"At":"2026-10-04T20:09:13.5221459+08:00","PID":18696,"CPU":0.140625,"WS":382615552,"Private":393666560}
{"At":"2026-10-04T20:09:14.0432384+08:00","PID":18696,"CPU":0.359375,"WS":548478976,"Private":710164480}
CLI_EXIT_OBSERVED
```

清理核对：r66源session55454/58176及r67源session85615/84842均-1（按身份清理）；两轮VLC已清理，各轮指定文件清单无残留，原120秒源保留。

## r68：实际encoder witness绑定通过，节点选项缺失

实际emission接线Release全量构建exit0、双独立源码PASS。CLI PID33284自然exit1，越过r67的readback全等门禁，当前失败：`InvalidArgument: VideoEncodeNode requires node option: pipeline.filter_active`。尚未运行输出DAG、无画面/A-V漂移，FAIL。CPU累计0.015625→0.328125s，WS13475840→381423616，private19795968→472031232；准备阶段线程数8→10，并未观察到自动多核frame线程池。

源0 PID2428/session65158于20:18:04.960启动，源1 PID19400/session87460于20:18:03.296启动，实际相差约1.66秒；VLC12316于20:18:06.544启动，CLI已于20:18:05.735结束。本轮命令并发提交不代表进程同时启动；r66容量归因不能只断言GPU冷启动。记录后清理本轮源/VLC及产物。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r68 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r68.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r68-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r68-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r68-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r68-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r68- --snapshot-format=png rtp://@127.0.0.1:62020
```

### CLI采样

```text
CLI_PID=33284
{"At":"2026-10-04T20:18:01.8379948+08:00","PID":33284,"CPU":0.015625,"WS":13475840,"Private":19795968,"Threads":8}
{"At":"2026-10-04T20:18:02.3780669+08:00","PID":33284,"CPU":0.015625,"WS":13484032,"Private":19795968,"Threads":8}
{"At":"2026-10-04T20:18:02.8872939+08:00","PID":33284,"CPU":0.015625,"WS":13484032,"Private":19795968,"Threads":8}
{"At":"2026-10-04T20:18:03.3951029+08:00","PID":33284,"CPU":0.046875,"WS":59592704,"Private":65929216,"Threads":8}
{"At":"2026-10-04T20:18:03.9109854+08:00","PID":33284,"CPU":0.109375,"WS":112537600,"Private":119107584,"Threads":8}
{"At":"2026-10-04T20:18:04.4208586+08:00","PID":33284,"CPU":0.15625,"WS":153862144,"Private":160563200,"Threads":8}
{"At":"2026-10-04T20:18:04.9362676+08:00","PID":33284,"CPU":0.1875,"WS":176959488,"Private":183730176,"Threads":8}
{"At":"2026-10-04T20:18:05.4440339+08:00","PID":33284,"CPU":0.328125,"WS":381423616,"Private":472031232,"Threads":10}
CLI_EXIT_OBSERVED
```

## r69：输出域形状修正后复验（失败）

Release全量重建配置/编译exit0，双独立实施源码审查PASS。固定双路120秒H.264/AAC RTP输入→HEVC/AAC 1280×720 30fps CBR8Mbps MPEG-TS/RTP，源规格不变。

实际命令：
```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r69 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r69.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r69-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r69-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r69-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r69-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r69- --snapshot-format=png rtp://@127.0.0.1:62020
```

CLI PID8016自然exit1；新错误：Project MPEG-TS runtime output product conflicts with its synchronization planner product。已跨过r68 filter option检查，未获得实际输出/A-V漂移或画面证据。VLC24492启动在CLI失败后，不算播放观察。source0 PID6772、source1 PID3224仍在发送，测试判定失败后按身份清理，停止不计自然结束。CPU累计0.015625→0.421875秒；WS13,504,512→376,135,680 bytes；private19,820,544→466,210,816 bytes；threads8→10，短暂preflight不代表稳定内存。

监控原始摘要：
```text

ProcessId    : 8016
CreationDate : 2026/10/4 20:29:35
CommandLine  : "D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe" --media-id 
               composition-wiring-r69 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analy
               ze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264
                --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-
               rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rt
               p-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --i
               nputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --p
               robe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 
               96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-
               payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1
               ;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0
                --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transpor
               t rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/compositi
               on-wiring-r69.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60
                --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2

{"Time":"2026-10-04T20:29:37.7996460+08:00","PID":8016,"CPU":0.015625,"WS":13504512,"Private":19820544,"Threads":8}
{"Time":"2026-10-04T20:29:38.3319912+08:00","PID":8016,"CPU":0.015625,"WS":13512704,"Private":19820544,"Threads":8}
{"Time":"2026-10-04T20:29:38.8539219+08:00","PID":8016,"CPU":0.015625,"WS":13512704,"Private":19820544,"Threads":8}
{"Time":"2026-10-04T20:29:39.3635398+08:00","PID":8016,"CPU":0.046875,"WS":59637760,"Private":65949696,"Threads":8}
{"Time":"2026-10-04T20:29:39.8730615+08:00","PID":8016,"CPU":0.0625,"WS":112500736,"Private":119058432,"Threads":8}
{"Time":"2026-10-04T20:29:40.3854983+08:00","PID":8016,"CPU":0.0625,"WS":153870336,"Private":160579584,"Threads":8}
{"Time":"2026-10-04T20:29:40.8934960+08:00","PID":8016,"CPU":0.078125,"WS":175546368,"Private":182280192,"Threads":8}
{"Time":"2026-10-04T20:29:41.4064804+08:00","PID":8016,"CPU":0.421875,"WS":376135680,"Private":466210816,"Threads":10}
CLI exit observed

```

记录后清理上述精确PID及r69 cli/source0/source1/vlc四日志；SDP/截图若未产生不计残留。保留复用120秒源，不运行RKMPP/local，不创建成功提交。

## r70：进入生产DAG后的extradata失败

Release配置/编译exit0，r68/r69修正均双审PASS。输入/输出规格沿r69不变，实际命令：
```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r70 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r70.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r70-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r70-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r70-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r70-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r70- --snapshot-format=png rtp://@127.0.0.1:62020
```

CLI PID9372自然exit1；已通过预检，实际选择h264 CUDA双解码、scale_cuda=640:360双tile、hevc_nvenc，启动63个worker。FileMuxNode63首先报FFmpeg codec parameters require non-empty extradata，其余Cancelled为取消传播。encodedPacketsPushed=0，发送committed_datagrams=0，无合屏画面/A-V漂移通过证据。最终payload reservations=releases=407，current bytes/objects=0，highwater bytes=21783865；pressure failures=10需后续解释，不能视为稳定通过。CPU累计0.015625→1秒，外部采样WS峰385593344/private峰511901696 bytes；CLI内报WS486227968，采样位置不同。source0 PID24920、source1 PID36356、VLC30144在记录后核验身份清理；停止不算自然结束。

进程监控：
```text

ProcessId    : 9372
CreationDate : 2026/10/4 20:38:31

{"Time":"2026-10-04T20:38:32.9981278+08:00","PID":9372,"CPU":0.015625,"WS":13475840,"Private":19800064,"Threads":8}
{"Time":"2026-10-04T20:38:33.5279849+08:00","PID":9372,"CPU":0.015625,"WS":13484032,"Private":19800064,"Threads":8}
{"Time":"2026-10-04T20:38:34.0447981+08:00","PID":9372,"CPU":0.015625,"WS":13484032,"Private":19800064,"Threads":8}
{"Time":"2026-10-04T20:38:34.5619869+08:00","PID":9372,"CPU":0.015625,"WS":13791232,"Private":20082688,"Threads":8}
{"Time":"2026-10-04T20:38:35.0827414+08:00","PID":9372,"CPU":0.03125,"WS":103247872,"Private":109764608,"Threads":8}
{"Time":"2026-10-04T20:38:35.5999315+08:00","PID":9372,"CPU":0.078125,"WS":152190976,"Private":158961664,"Threads":8}
{"Time":"2026-10-04T20:38:36.1205873+08:00","PID":9372,"CPU":0.09375,"WS":174043136,"Private":180875264,"Threads":8}
{"Time":"2026-10-04T20:38:36.6420357+08:00","PID":9372,"CPU":0.4375,"WS":385593344,"Private":511901696,"Threads":10}
{"Time":"2026-10-04T20:38:37.1598903+08:00","PID":9372,"CPU":1.0,"WS":238972928,"Private":488247296,"Threads":36}
CLI exit observed

```

关键运行诊断：
```text

out\acceptance\composition-wiring-r70-cli.log:1:[CLI] composition_inputs=2 grid=fixed audio_source=0 audio=on max_durat
ion=source_driven hw=planner-highest-score
out\acceptance\composition-wiring-r70-cli.log:2:[2026-10-04 20:38:36.464] [info] [graph][runtime.node] codec_resolver.d
ecoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=requ
ired pkt_tb=1/90000
out\acceptance\composition-wiring-r70-cli.log:3:[2026-10-04 20:38:36.465] [info] [graph][runtime.node] codec_resolver.d
ecoder.runtime name=h264 hwaccel_flags=1
out\acceptance\composition-wiring-r70-cli.log:6:[2026-10-04 20:38:36.543] [info] [graph][runtime.node] codec_resolver.d
ecoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=requ
ired pkt_tb=1/90000
out\acceptance\composition-wiring-r70-cli.log:7:[2026-10-04 20:38:36.543] [info] [graph][runtime.node] codec_resolver.d
ecoder.runtime name=h264 hwaccel_flags=1
out\acceptance\composition-wiring-r70-cli.log:12:[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_o
utput_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
out\acceptance\composition-wiring-r70-cli.log:13:[CLI] prepared_output_id=0
out\acceptance\composition-wiring-r70-cli.log:14:[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264 
filter=scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
out\acceptance\composition-wiring-r70-cli.log:128:[2026-10-04 20:38:37.020] [info] [graph][runtime.node] register node=
1 name=composition-wiring-r70.output.video.codec_resolver kind=CodecResolver
out\acceptance\composition-wiring-r70-cli.log:130:[2026-10-04 20:38:37.020] [info] [graph][runtime.node] register node=
3 name=composition-wiring-r70.output.audio.codec_resolver kind=AudioCodecResolver
out\acceptance\composition-wiring-r70-cli.log:149:[2026-10-04 20:38:37.020] [info] [graph][runtime.node] register node=
24 name=composition-wiring-r70.source_0.video.codec_resolver kind=CodecResolver
out\acceptance\composition-wiring-r70-cli.log:154:[2026-10-04 20:38:37.020] [info] [graph][runtime.node] register node=
29 name=composition-wiring-r70.source_0.audio.encode.codec_resolver kind=AudioCodecResolver
out\acceptance\composition-wiring-r70-cli.log:175:[2026-10-04 20:38:37.020] [info] [graph][runtime.node] register node=
51 name=composition-wiring-r70.source_1.video.codec_resolver kind=CodecResolver
out\acceptance\composition-wiring-r70-cli.log:180:[2026-10-04 20:38:37.020] [info] [graph][runtime.node] register node=
56 name=composition-wiring-r70.source_1.audio.encode.codec_resolver kind=AudioCodecResolver
out\acceptance\composition-wiring-r70-cli.log:261:[CLI] runtime report: queued=79, peakQueued=198, threads=69, processT
hreads=69, workers=63, workerProgress=5134, workerProcessCalls=6613, workerWaits=1468, workerWakeups=1410, workerErrors
=0, errors=0, stalledIntervals=0, cpuSamples=0, logicalProcessors=22, averageSystemMachineCpuPercent=0.000000, averageP
rocessMachineCpuPercent=0.000000, peakProcessMachineCpuPercent=0.000000, averageProcessSingleCoreCpuPercent=0.000000, p
eakProcessSingleCoreCpuPercent=0.000000, initialWorkingSetBytes=486227968, workingSetBytes=486227968, peakWorkingSetByt
es=486227968, totalPushed=1172, totalPopped=1093, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, bac
kpressureItems=110, graphPayloadCurrentBytes=4566961, graphPayloadHighWaterBytes=21783865, graphPayloadCurrentObjects=2
90, graphPayloadHighWaterObjects=290, graphPayloadReservations=298, graphPayloadReleases=8, graphPayloadPressureFailure
s=2, lastInputReceivedAtNs=8557017220100
out\acceptance\composition-wiring-r70-cli.log:262:[CLI] control_eof media_session_continues=true
out\acceptance\composition-wiring-r70-cli.log:283:[2026-10-04 20:38:37.130] [info] [graph][runtime.node] process.failed
 node=63 name=FileMuxNode kind=FileMux inputs=[channel=100 edge=100 stream=Video edge_kind=Metadata payload=CodecContex
t queued=0; channel=102 edge=102 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=103 edge=103 st
ream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=106 edge=106 stream=Any edge_kind=EncodedPacket payl
oad=Unknown queued=0] outputs=[channel=108 edge=108 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=I
nvalidArgument: FFmpeg codec parameters require non-empty extradata
out\acceptance\composition-wiring-r70-cli.log:284:[2026-10-04 20:38:37.130] [info] [graph][runtime.node] worker.failed 
node=63 kind=FileMux name=Project MPEG-TS mux error=InvalidArgument: FFmpeg codec parameters require non-empty extradat
a
out\acceptance\composition-wiring-r70-cli.log:287:[2026-10-04 20:38:37.130] [info] [graph][runtime.node] process.failed
 node=28 name=VideoFilterNode kind=VideoFilter inputs=[channel=31 edge=31 stream=Video edge_kind=Metadata payload=Codec
Context queued=0; channel=35 edge=35 stream=Video edge_kind=RawFrame payload=Frame queued=3] outputs=[channel=44 edge=4
4 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
out\acceptance\composition-wiring-r70-cli.log:298:[2026-10-04 20:38:37.130] [info] [graph][runtime.node] process.failed
 node=55 name=VideoFilterNode kind=VideoFilter inputs=[channel=74 edge=74 stream=Video edge_kind=Metadata payload=Codec
Context queued=0; channel=78 edge=78 stream=Video edge_kind=RawFrame payload=Frame queued=3] outputs=[channel=87 edge=8
7 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
out\acceptance\composition-wiring-r70-cli.log:299:[2026-10-04 20:38:37.130] [info] [graph][runtime.node] process.failed
 node=57 name=AudioDecodeNode kind=AudioDecode inputs=[channel=80 edge=80 stream=Audio edge_kind=Metadata payload=Codec
Context queued=0; channel=81 edge=81 stream=Audio edge_kind=InputPacket payload=Packet queued=48] outputs=[channel=83 e
dge=83 stream=Audio edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cance
lled
out\acceptance\composition-wiring-r70-cli.log:310:[2026-10-04 20:38:37.130] [info] [graph][runtime.node] process.failed
 node=34 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=47 edge=47 stream=Metadata edge_kind=Event pa
yload=GraphEvent queued=0; channel=48 edge=48 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=55 e
dge=55 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=72 edge=72 stream=Metadata edge_kind=Metadat
a payload=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
out\acceptance\composition-wiring-r70-cli.log:321:[2026-10-04 20:38:37.130] [info] [graph][runtime.node] process.failed
 node=35 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=49 edge=49 stream=Metadata edge_kind=Event pa
yload=GraphEvent queued=0; channel=50 edge=50 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=56 e
dge=56 stream=Audio edge_kind=InputPacket payload=Packet queued=0; channel=79 edge=79 stream=Metadata edge_kind=Metadat
a payload=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
out\acceptance\composition-wiring-r70-cli.log:324:[2026-10-04 20:38:37.130] [info] [graph][runtime.node] worker.seconda
ry_failure node=28 kind=VideoFilter name=Video filter error=Cancelled: graph payload credit waiters were cancelled
out\acceptance\composition-wiring-r70-cli.log:335:[2026-10-04 20:38:37.130] [info] [graph][runtime.node] process.failed
 node=8 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=6 edge=6 stream=Metadata edge_kind=Event paylo
ad=GraphEvent queued=0; channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=13 edge=1
3 stream=Audio edge_kind=InputPacket payload=Packet queued=0; channel=36 edge=36 stream=Metadata edge_kind=Metadata pay
load=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
out\acceptance\composition-wiring-r70-cli.log:350:[2026-10-04 20:38:37.130] [info] [graph][runtime.node] process.failed
 node=25 name=VideoDecodeNode kind=VideoDecode inputs=[channel=30 edge=30 stream=Video edge_kind=Metadata payload=Codec
Context queued=0; channel=32 edge=32 stream=Video edge_kind=InputPacket payload=Packet queued=56] outputs=[channel=33 e
dge=33 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cance
lled
out\acceptance\composition-wiring-r70-cli.log:356:[2026-10-04 20:38:37.371] [info] [graph][runtime.node] scheduled_data
gram_sender stage=aborted generation=0 service_scope= execution_mode=1 committed_batches=0 committed_datagrams=0 commit
ted_payload_bytes=0 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 a
mbiguous_submitted_failures=0 ordered_queue_maximum_batches=0 ordered_queue_maximum_datagrams=0 ordered_queue_maximum_w
ire_bytes=0 pacing_reserved=0 pacing_cancelled=0 pacing_submitted=0 pacing_maximum_submit_lateness_ns=0 pacing_rate_ada
ptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=0 delivery_evidence=not_proven s
hared_scope_charged_datagrams=0 shared_scope_charged_wire_bytes=0 shared_scope_waiting_members=0 shared_scope_failed=0
out\acceptance\composition-wiring-r70-cli.log:427:[2026-10-04 20:38:37.402] [info] [graph][runtime.node] codec_resolver
.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
out\acceptance\composition-wiring-r70-cli.log:431:[2026-10-04 20:38:37.402] [info] [graph][runtime.node] codec_resolver
.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
out\acceptance\composition-wiring-r70-cli.log:445:[2026-10-04 20:38:37.410] [info] [graph][runtime.node] codec_resolver
.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
out\acceptance\composition-wiring-r70-cli.log:447:[CLI] realtime video runtime failed: InvalidArgument: FFmpeg codec pa
rameters require non-empty extradata
out\acceptance\composition-wiring-r70-cli.log:448:[CLI] final runtime report: queued=0, peakQueued=480, threads=69, pro
cessThreads=69, workers=0, workerProgress=5895, workerProcessCalls=8119, workerWaits=2210, workerWakeups=2162, workerEr
rors=2, errors=2, stalledIntervals=0, cpuSamples=0, logicalProcessors=22, averageSystemMachineCpuPercent=0.000000, aver
ageProcessMachineCpuPercent=0.000000, peakProcessMachineCpuPercent=0.000000, averageProcessSingleCoreCpuPercent=0.00000
0, peakProcessSingleCoreCpuPercent=0.000000, initialWorkingSetBytes=486227968, workingSetBytes=486227968, peakWorkingSe
tBytes=486227968, totalPushed=1891, totalPopped=1611, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0,
 backpressureItems=110, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=21783865, graphPayloadCurrentObjects=0, 
graphPayloadHighWaterObjects=299, graphPayloadReservations=407, graphPayloadReleases=407, graphPayloadPressureFailures=
10, lastInputReceivedAtNs=8557032637800
src\internal\graph\nodes\mux\MediaTsFfmpegStreamConfigMaterializer.cpp:37:                "FFmpeg codec parameters require non-empty extradata"));


```

## r71：120秒双源真实合屏，源结束后代次失败（未通过）

Release全量配置/编译exit0，头部策略与实际参数集预检双独立源码PASS。实际命令：
```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r71 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r71.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r71-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r71-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r71-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r71-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r71- --snapshot-format=png rtp://@127.0.0.1:62020
```

CLI PID19432/session16218自然exit1；source0 PID35552/session29798、source1 PID21220/session6725均自然exit0，各3600帧、120.00秒，源未降低参数。VLC PID32832。VLC RC63020发送snapshot两次：20:48:55.144与20:49:59.973，主流程实际打开两张1280×720图片，均为左右两路游戏视频、上下黑边，图像内容发生变化；不能据两帧宣称全程无卡顿。截图文件分别composition-wiring-r71-2026-10-04-20h48m55s144.png和composition-wiring-r71-2026-10-04-20h49m59s973.png，记录后删除。

约60秒时0worker errors/0stalls，WS231780352、private479297536，输出编码包19559/19558。源端av_drift_trace filtered_phase为0或156ns、filtered_frequency_ppm=0，至约94秒仍同量级；这是内部源校正证据，不等于播放器端同步完美。VLC出现late frames/drop、audio flush/up-sampling，RC stats仅video decoded12/audio12，不将该累计计数当完整播放证明。第二次截图能确认仍更新。稳定运行期工作集约222→233MB，private约470→481MB，CPU累计44.09375s，最终机器CPU均值1.665366%、峰4.958678%（22逻辑核）；120秒不证明长期内存硬界。

20:50:23.570源1进入generation1→2 purging，purge_ack complete后acquiring；20:50:25.197源0同样purging，随即AudioDecode30与AudioResample33报Audio lineage state rejects a stale generation，CLI失败。最终编码包39757/39756，0queued、0payload bytes但2objects、122158 reservations/122156 releases，完整终止/释放门禁未通过。RTCP BYE在此协议只是SenderLeft失效，不是可信媒体EOF；按源码本应全源loss后继续黑画面/静音，不能将这次退出记为源驱动成功。未做恢复、单源回归或RKMPP。

关键诊断：
```text
[2026-10-04 20:50:23.570] [info] [graph][runtime.node] av_reacquisition group=composition-wiring-r71.source.1 phase=purging old=1 next=2 transition=1
[2026-10-04 20:50:23.572] [info] [graph][runtime.node] av_reacquisition group=composition-wiring-r71.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-10-04 20:50:25.197] [info] [graph][runtime.node] av_reacquisition group=composition-wiring-r71.source.0 phase=purging old=1 next=2 transition=1
[2026-10-04 20:50:25.198] [info] [graph][runtime.node] process.failed node=30 name=AudioDecodeNode kind=AudioDecode inputs=[channel=37 edge=37 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=38 edge=38 stream=Audio edge_kind=InputPacket payload=Packet queued=43] outputs=[channel=40 edge=40 stream=Audio edge_kind=RawFrame payload=Frame queued=0] error=InvalidArgument: Audio lineage state rejects a stale generation
[2026-10-04 20:50:25.198] [info] [graph][runtime.node] worker.failed node=30 kind=AudioDecode name=Audio decode error=InvalidArgument: Audio lineage state rejects a stale generation
[2026-10-04 20:50:25.198] [info] [graph][runtime.node] process.failed node=28 name=VideoFilterNode kind=VideoFilter inputs=[channel=31 edge=31 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=35 edge=35 stream=Video edge_kind=RawFrame payload=Frame queued=4] outputs=[channel=44 edge=44 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-04 20:50:25.198] [info] [graph][runtime.node] process.failed node=33 name=AudioResampleNode kind=AudioResample inputs=[channel=39 edge=39 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=42 edge=42 stream=Audio edge_kind=RawFrame payload=Frame queued=4; channel=43 edge=43 stream=Audio edge_kind=Event payload=GraphEvent queued=0] outputs=[channel=46 edge=46 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0] error=InvalidArgument: Audio lineage state rejects a stale generation
[2026-10-04 20:50:25.198] [info] [graph][runtime.node] process.failed node=8 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=13 edge=13 stream=Audio edge_kind=InputPacket payload=Packet queued=0; channel=36 edge=36 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
[2026-10-04 20:50:25.198] [info] [graph][runtime.node] process.failed node=7 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=29 edge=29 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
[2026-10-04 20:50:25.198] [info] [graph][runtime.node] process.failed node=35 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=49 edge=49 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=50 edge=50 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=56 edge=56 stream=Audio edge_kind=InputPacket payload=Packet queued=0; channel=79 edge=79 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
[2026-10-04 20:50:25.198] [info] [graph][runtime.node] process.failed node=34 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=48 edge=48 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=55 edge=55 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=72 edge=72 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
[2026-10-04 20:50:25.198] [info] [graph][runtime.node] worker.secondary_failure node=33 kind=AudioResample name=Audio resample error=InvalidArgument: Audio lineage state rejects a stale generation
[2026-10-04 20:50:25.325] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=5048 completion_waits=5048 completed_datagrams=5129 drained_datagrams=80 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-04 20:50:25.326] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=59779 completion_waits=59779 completed_datagrams=103840 drained_datagrams=44058 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-04 20:50:25.327] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=5081 completion_waits=5081 completed_datagrams=5199 drained_datagrams=114 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-04 20:50:25.327] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=57629 completion_waits=57629 completed_datagrams=105128 drained_datagrams=47498 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[CLI] realtime video runtime failed: InvalidArgument: Audio lineage state rejects a stale generation
[CLI] final runtime report: queued=0, peakQueued=453, threads=69, processThreads=97, workers=0, workerProgress=393881, workerProcessCalls=1009002, workerWaits=615108, workerWakeups=606784, workerErrors=3, errors=3, stalledIntervals=0, cpuSamples=388, logicalProcessors=22, averageSystemMachineCpuPercent=13.439254, averageProcessMachineCpuPercent=1.665366, peakProcessMachineCpuPercent=4.958678, averageProcessSingleCoreCpuPercent=36.638044, peakProcessSingleCoreCpuPercent=109.090909, initialWorkingSetBytes=465145856, workingSetBytes=221900800, peakWorkingSetBytes=465145856, totalPushed=285074, totalPopped=285021, droppedBuffers=0, encodedPacketsPushed=39757, encodedPacketsPopped=39756, backpressureItems=110, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=24537074, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=296, graphPayloadReservations=122158, graphPayloadReleases=122156, graphPayloadPressureFailures=24323, lastInputReceivedAtNs=9265115534900
[CLI] realtime video runtime failed: InvalidArgument: Audio lineage state rejects a stale generation

LastWriteTime      Length
-------------      ------
2026/10/4 20:49:59  29152


```

VLC诊断包括picture late/drop、playback too late flushing、snapshot转换错误；两张PNG实际成功生成并检查。测试已结束，记录后按精确PID关闭VLC，清理4日志、SDP及上述2张截图；源与CLI均已自然结束。原120秒源保留。

## r72：旧媒体入口修正后的校正命令失败

Release全量配置/编译exit0，四音频入口双审PASS；原规格命令：
```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r72 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r72.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r72-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r72-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r72-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r72-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r72- --snapshot-format=png rtp://@127.0.0.1:62020
```

CLI PID9408/session12921自然exit1；source0 PID14008/session89932、source1 PID14584/session15265均3600帧120秒自然exit0。VLC PID34176，21:02:24.843 snapshot实际查看左右合屏与黑边；21:04:12.812截图在CLI退出后生成，只是播放器保留帧，不作为all-loss输出证据。运行约118秒期间无worker error，内部drift filtered_phase156ns、frequency0；稳定WS约230→240MB/private478→486MB，外部最后CPU45.421875s，最终CPU机器均值1.741961%、峰6.724512%。

source0 BYE触发purging1→2后，AudioResample33首先报audio correction command violates generation, sequence, or window ordering，purge_ack仍complete。旧媒体stale错误本次未出现，但完整all-loss/recovery仍失败；尚未启动恢复源。最终编码包39234/39233，0queued/0bytes、2objects，120528 reservations/120526 releases。不得记成功验收。

诊断：
```text
[2026-10-04 21:03:55.389] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5007 source_master_ns=116534492148 raw_phase_ns=155 filtered_phase_ns=156 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-10-04 21:03:56.370] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5007 source_master_ns=116534743848 raw_phase_ns=155 filtered_phase_ns=156 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-10-04 21:03:56.429] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5051 source_master_ns=117556170153 raw_phase_ns=156 filtered_phase_ns=156 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-10-04 21:03:57.392] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5051 source_master_ns=117556421853 raw_phase_ns=156 filtered_phase_ns=156 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-10-04 21:03:57.435] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5095 source_master_ns=118577848157 raw_phase_ns=155 filtered_phase_ns=156 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
[2026-10-04 21:03:57.466] [info] [graph][runtime.node] av_reacquisition group=composition-wiring-r72.source.0 phase=purging old=1 next=2 transition=1
[2026-10-04 21:03:57.467] [info] [graph][runtime.node] process.failed node=33 name=AudioResampleNode kind=AudioResample inputs=[channel=39 edge=39 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=42 edge=42 stream=Audio edge_kind=RawFrame payload=Frame queued=5; channel=43 edge=43 stream=Audio edge_kind=Event payload=GraphEvent queued=0] outputs=[channel=46 edge=46 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0] error=InvalidArgument: audio correction command violates generation, sequence, or window ordering
[2026-10-04 21:03:57.467] [info] [graph][runtime.node] worker.failed node=33 kind=AudioResample name=Audio resample error=InvalidArgument: audio correction command violates generation, sequence, or window ordering
[2026-10-04 21:03:57.467] [info] [graph][runtime.node] process.failed node=7 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=29 edge=29 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
[2026-10-04 21:03:57.467] [info] [graph][runtime.node] process.failed node=8 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=13 edge=13 stream=Audio edge_kind=InputPacket payload=Packet queued=0; channel=36 edge=36 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
[2026-10-04 21:03:57.467] [info] [graph][runtime.node] av_reacquisition group=composition-wiring-r72.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-10-04 21:03:57.468] [info] [graph][runtime.node] process.failed node=35 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=49 edge=49 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=50 edge=50 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=56 edge=56 stream=Audio edge_kind=InputPacket payload=Packet queued=0; channel=79 edge=79 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
[2026-10-04 21:03:57.468] [info] [graph][runtime.node] process.failed node=34 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=48 edge=48 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=55 edge=55 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=72 edge=72 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
[CLI] final runtime report: queued=0, peakQueued=532, threads=69, processThreads=97, workers=0, workerProgress=396728, workerProcessCalls=1004909, workerWaits=608170, workerWakeups=600100, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=386, logicalProcessors=22, averageSystemMachineCpuPercent=11.601698, averageProcessMachineCpuPercent=1.741961, peakProcessMachineCpuPercent=6.724512, averageProcessSingleCoreCpuPercent=38.323150, peakProcessSingleCoreCpuPercent=147.939262, initialWorkingSetBytes=454004736, workingSetBytes=239513600, peakWorkingSetBytes=454004736, totalPushed=282142, totalPopped=281970, droppedBuffers=0, encodedPacketsPushed=39234, encodedPacketsPopped=39233, backpressureItems=110, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=24670320, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=289, graphPayloadReservations=120528, graphPayloadReleases=120526, graphPayloadPressureFailures=24085, lastInputReceivedAtNs=10077385062200

src/internal/graph\nodes\audio\AudioResampleNode.cpp:135:::media::Result<bool> AudioResampleNode::consumeCorrection(
src/internal/graph\nodes\audio\AudioResampleNode.cpp:254:        auto consumedCorrection = consumeCorrection(context);
src/internal/graph\nodes\audio\AudioResampleNode.cpp:313:        auto consumedCorrection = consumeCorrection(context);
src/internal/graph\nodes\audio\AudioResampleNode.h:52:    ::media::Result<bool> consumeCorrection(MediaGraphExecutionContext& context);
src/internal/graph\nodes\audio\AudioSwrCompensationExecutor.cpp:68:            "audio correction command violates generation, sequence, or window ordering"));
Name                                               Length
----                                               ------
composition-wiring-r72-2026-10-04-21h02m24s843.png 555509
composition-wiring-r72-2026-10-04-21h04m12s812.png 692968
[out#1/rtp @ 000001b56d92a340] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
[out#1/rtp @ 0000012dd356d300] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    


```

记录后按精确身份关闭VLC34176；源与CLI已自然退出，删除r72四日志/SDP/两截图。复用120秒源保留，无远程测试产物。

## r73：双路 RTP H264/AAC → MPEG-TS over RTP HEVC/AAC，1280×720 30fps 8000kbps CBR（2026-10-04）

Release clean-first/all-target重建成功（configure/build exit0），旧校正命令入口修复经两个未参与实现智能体同时源码PASS。真实运行约7分钟；本项取得双路画面、全断流黑场、逐源恢复及再次断流的运行证据，但音频静音/恢复、可信EOF/明确stop等完整门禁尚缺，不标记完整验收PASS。

### 实际命令（依次为CLI、两个初始源、VLC、两个重入源）

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r73 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r73.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r73-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r73-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r73-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r73-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r73- --snapshot-format=png rtp://@127.0.0.1:62020

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r73-source0-reentry.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r73-source1-reentry.log 2>&1
exit $LASTEXITCODE
```

### 结果与边界

- CLI PID36860；初始FFmpeg PID36984/7988，重入PID3184/15956，四个源均自然exit0、3600帧、120.00秒，无循环或降规格。VLC PID33768。
- 初始两路源21:14:51.734/21:14:55.050离开，purge 1→2完成；21:15:42实际查看1280×720截图为全黑，输出继续。source0于21:16:23恢复，21:16:56截图左实画右黑；source1于21:17:47恢复，21:18:21截图两格实画；source0于21:18:23.103再次离开且purge 2→3完成，21:18:50截图左黑右实画；source1于21:19:46.889完成purge 2→3，21:20:07截图再次全黑。
- 初始21:13:21左右640×360实画、上下等比黑边；六张截图已实际查看。截图证明采样时画面状态，不等于连续无卡顿证明。
- 至21:20:07 workerErrors/errors/stalledIntervals=0，encodedPacketsPushed/Popped=144550/144550，queued=0。此时CLI存活且继续编码；测试结束后将按精确身份清理，不记为自然退出。
- CPU累计125.11s，CLI机器平均CPU1.310435%、峰值7.654321%；工作集启动峰573931520B，稳定运行约235MB，末次216752128B；私有内存近期约483MB，末次460083200B。有限时长未见持续增长，不替代长稳门禁。
- 初始source drift filtered phase 0/156ns；恢复generation2 source0约18140ns、source1约9199ns，filtered_frequency_ppm=0。没有audio timeline discontinuity、旧generation报错。时钟/画面恢复不等于已直接证明播放音频恢复或静音；当前聚合real/silence计数仅在stop/abort输出，强制进程清理无法获取该summary。
- graphPayloadHighWaterBytes=25913672，HighWaterObjects=391，PressureFailures=47222（有界背压wait登记次数，并非错误数）。运行中采样不是归还终态。
- VLC日志只有启动期晚帧/upsampling等记录；保留播放器连续A/V质量未完全验证边界。
- 对此前失败最终2objects的源码复审：finalReport在stop/abort后、runtime reset前；prepared encoder仍可持外部硬件帧对象credit，metadata本身不申请此credit。不能据此断言析构后泄漏，也未取得精确owner和析构后归零证据。


证据记录后，按精确PID身份清理CLI36860和VLC33768（CLI为测试后强制清理，非自然结束）。四源已自然exit0；逐项删除六日志、SDP和六截图，r73文件及进程残留检查为空。120秒复用源保留；未运行远程测试。

## r74：四路 RTP H264/AAC → MPEG-TS over RTP HEVC/AAC，1280×720 30fps 8000kbps CBR（2026-10-04）

同一r73 Release产物。CLI36432自然exit1；尚未建图、没有输出和A/V drift，结果FAIL。源0/1/2/3 PID37528/30236/392/12676实际启动21:22:28/30/32/33，VLC12324。并行提交独立命令但进程启动存在约5秒偏差，不能宣称源同时启动。21:22:33.988 CUDA source0 probe retained_bytes=4936021 capacity=5000000触发aggregate byte capacity，后续QSV无authoritative packet retention。第一CUDA失败是准备保留预算，不是CUDA分辨率filter启动错误；最终QSV错误不代表第一原因。CPU累计0.84375s，采样WS最大937754624B/private1030848512B（硬件准备阶段），无生产稳态证据。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r74 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61042 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61052 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r74.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r74-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r74-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r74-source1.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61042?rtcpport=61043&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r74-source2.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61052?rtcpport=61053&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r74-source3.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r74-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r74- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 原始失败诊断

```
[CLI] composition_inputs=4 grid=fixed audio_source=0 audio=on max_duration=source_driven hw=planner-highest-score
[2026-10-04 21:22:33.987] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-04 21:22:33.988] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-04 21:22:33.988] [info] [graph][planner.capability] composition_source_candidate_rejected source=0 candidate=cuda-nvenc error=AllocationFailed: raw RTP prepared A/V capture exceeded aggregate byte capacity: stream=probe retained_bytes=4936021 capacity=5000000
[2026-10-04 21:22:34.005] [info] [graph][planner.capability] composition_output_candidate_rejected encoder=hevc_nvenc error=AllocationFailed: raw RTP prepared A/V capture exceeded aggregate byte capacity: stream=probe retained_bytes=4936021 capacity=5000000
[D3D11VA @ 000001fa3b351d40] Using device 8086:7d55 (Intel(R) Arc(TM) Graphics).
[2026-10-04 21:22:34.626] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264_qsv planned=h264_qsv hardware=true hwaccel=qsv hw_pix_fmt=qsv hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-04 21:22:34.626] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264_qsv hwaccel_flags=1
[2026-10-04 21:22:34.626] [info] [graph][planner.capability] composition_source_candidate_rejected source=0 candidate=qsv error=Unsupported: Composition prepared decoder has no authoritative packet retention
[2026-10-04 21:22:34.637] [info] [graph][planner.capability] composition_output_candidate_rejected encoder=hevc_qsv error=Unsupported: Composition prepared decoder has no authoritative packet retention
[CLI] realtime video graph preflight failed: Unsupported: Composition prepared decoder has no authoritative packet retention
```

试验已判定失败，将核对精确PID和命令后清理四源/VLC；清理退出不计自然exit0。保留r66准备总预算未修复状态，不增probe-size、不缩短源、不调低规格。无截图、抓包、SDP或远程产物。

记录后已核对并清理上述四源/VLC及六日志；r74目录和PID残留检查为空。120秒源保留。

## r75：四路 RTP H264/AAC → MPEG-TS over RTP HEVC/AAC，1280×720 30fps 8000kbps CBR（2026-10-04）

固定frontier只读借用8源码经双独立PASS，Release clean-first/all-target configure/build exit0。以下是实际命令，输出录制由VLC自身duplicate display/file完成，后续FFmpeg只离线分析该文件。尚未判定完整验收。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r75 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61042 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61052 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-source1.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61042?rtcpport=61043&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-source2.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61052?rtcpport=61053&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-source3.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r75- --snapshot-format=png rtp://@127.0.0.1:62020

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-vlc-record.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r75- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-recording.ts}}" rtp://@127.0.0.1:62020

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-source0-reentry.log 2>&1
exit $LASTEXITCODE
```

CLI35800启动21:38:18；四源34000/31100/35544/35856于21:38:21/23/25/26启动，仍约5秒错开，均原120秒且自然exit0。VLC16264实际看到21:39:09、21:40:01四格640×360更新画面及21:41:02全黑；21:41:21以RC quit正常关闭旧VLC后启动带录制的VLC35248，21:41:22生成recording.ts。source0重入PID29040于21:41:50启动，原120秒自然exit0，21:43:49.869完成purge2→3。录制VLC下RC snapshot未生成新PNG，不能把它当作画面证据；后续离线取帧核实。

初始四路purge1→2于21:40:21.201/22.854/24.593/26.350完成；至21:44:11持续编码115242包、workerErrors/errors/stalledIntervals=0。CPU111.734375s，机器平均1.465657%、峰11.637931%；工作集启动峰1082380288B、四源稳态约350MB、末次324714496B；private末次665477120B，单次恢复后未回到此前全黑底值，长期趋势未验证。source drift初始0/156ns，重入source0约20344ns且frequency0。graphPayloadHighWaterBytes51336746、objects786，背压登记54136；此为运行中采样非释放终态。原stream=probe描述符分配失败未复现，已跨过r74同规格准备点；不外推任意较晚输入或完整准备保留预算闭环。计划结束观测后按身份清理CLI，不能记自然退出。


### r75 离线核验与退出记录

实际离线命令（真实流全部停止后运行）：

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -ss 5 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-recording.ts -t 10 -map 0:a:0 -vn -af astats=metadata=0:reset=0 -f null -

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -ss 60 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-recording.ts -t 10 -map 0:a:0 -vn -af astats=metadata=0:reset=0 -f null -

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -ss 180 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-recording.ts -t 10 -map 0:a:0 -vn -af astats=metadata=0:reset=0 -f null -

& D:/mabs/local64/bin-video/ffprobe.exe -v error -show_entries stream=codec_name,codec_type,sample_rate,channels,width,height,r_frame_rate:format=duration,start_time -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-recording.ts

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -ss 60 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-recording.ts -map 0:v:0 -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-reentry-frame.png

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-recording.ts -ss 60 -map 0:v:0 -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-reentry-sequential-frame.png

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -v warning -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r75-recording.ts -map 0:v:0 -map 0:a:0 -f null -
```

以上命令均exit0，但不能仅据退出码判定通过。录制211.436300秒，HEVC 1280×720 30fps、AAC 44100Hz双声道。5–15秒及180–190秒音频样本全零（RMS/Peak=-inf）；60–70秒恢复段RMS=-18.255257dB、Peak=+0.380409dB，浮点峰值最大1.044769，未据此判定音质或无削波。三段样本分别442368/442218/442230，NaN/Inf均0。

输入侧-ss取帧出现HEVC缺参考/RPS告警；改为从文件开始解码、输出侧-ss60后无此告警，实际查看PNG确认左上源0恢复、其余三格黑。完整顺序音视频解码无HEVC缺参考告警，但null mux报告3次音频非单调DTS（53306>=50963/51988/53011）。ffprobe全9105音频包仅发现一次回退：index49，1.514544→1.461433秒；位于录制启动阶段，早于约28秒的源0恢复。VLC自身日志同时存在启动播放延迟、141179us漂移停止重采样、late buffer for mux input及新增视频后PCR PID调整。现有证据不能区分生产输出与VLC重封装责任，须在后续真实链路用原始输出包时序交叉核验；不能标记完整音视频验收通过。

四个初始源和重入源均自然exit0。两VLC通过RC quit结束；CLI35800于21:44:54在观测结束后按身份清理，属于强制清理退出，不是自然退出/显式停止语义验证。记录后清理本轮15项日志、SDP、PNG及VLC临时录制，保留指定120秒源；未创建远程产物。准备任意延迟、纯视频混合输入、长期内存、完整A/V同步、C API及RKMPP仍未完成。

## r76：四路 RTP H264/AAC → MPEG-TS over RTP HEVC/AAC，1280×720 30fps 8000kbps CBR 原始包交叉核验

实际命令：

```powershell
& D:/Wireshark/dumpcap.exe -i 10 -f "udp dst port 62020" -a duration:360 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-output.pcapng

& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r76 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61042 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61052 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-source1.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61042?rtcpport=61043&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-source2.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61052?rtcpport=61053&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-source3.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r76- --snapshot-format=png rtp://@127.0.0.1:62020

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-vlc-record.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r76- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-recording.ts}}" rtp://@127.0.0.1:62020

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-source0-reentry.log 2>&1
exit $LASTEXITCODE

& D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-output.pcapng -d udp.port==62020,rtp -Y rtp -T fields -e rtp.payload | Out-File -LiteralPath D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-payload.hex -Encoding ascii

& C:/Windows/System32/certutil.exe -decodehex D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-payload.hex D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-wire.ts 12

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -v warning -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-recording.ts -map 0:v:0 -map 0:a:0 -f null -
```

未改源码/二进制，沿用21:37:49的Release。CLI32624于21:52:01启动，初始源36056/27664/34976/12192于21:52:12/14/16/17启动，均原120秒自然exit0；VLC20512实际截图21:53:20四格、21:54:32全黑。旧VLC以RC quit关闭，录制VLC11628于21:54:37启动；source0重入31624于21:55:01启动、原120秒自然exit0。录制VLC正常RC quit。至观测尾声workerErrors/errors/stalledIntervals=0、encodedPackets约97458、全部失活仍输出；CPU121.66s、WS318734336B/private666918912B（启动WS1088643072B，全黑最低约288.6MB），重入后未回落至此前全黑基线，长期内存未通过。时钟初始phase0/156ns。

dumpcap36084按360秒自然exit0；159208包，pcap/dumpcap/flushed/ifdrop均0；RTP SSRC0x30A5F084，159208包、lost0、窗口330.693395秒。期间读取尚在写入的抓包一次tshark报cut short、exit1；关闭后重新读取exit0，不能把中间读尾部当作网络损坏。

VLC录制完整顺序解码exit0但再次出现3次音频DTS告警：53765>=51334/52358/53382。ffprobe7521音频包仅index50回退1.537778→1.482656，早于恢复。原始RTP payload经tshark字段导出、certutil十六进制解码，未经过媒体重封装，继续离线核验。观测已结束，将按身份清理CLI32624；不计自然结束或显式停止PASS。最终离线证据记录后按文件清单清理。

### r76 原始输出与录制对照结论

最初hex提取混入tshark尾行“No matching RTP/H.264 payload dumped.”，certutil两种type均exit1/ERROR_INVALID_DATA；另一次生成过滤命令失误exit1。修正仅保留纯十六进制payload行后，tshark及certutil type12均exit0，恢复原TS178310292B。这是离线提取失败记录，不是链路错误。

```powershell
& D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-output.pcapng -d udp.port==62020,rtp -Y rtp -T fields -e rtp.payload | Where-Object { $_ -match '^[0-9a-fA-F]+$' } | Out-File -LiteralPath D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-payload.hex -Encoding ascii

& C:/Windows/System32/certutil.exe -decodehex D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-payload.hex D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-wire.ts 12

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -v warning -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-wire.ts -map 0:v:0 -map 0:a:0 -f null -

$rows=& D:/mabs/local64/bin-video/ffprobe.exe -v error -select_streams a:0 -show_packets -show_entries packet=pts,dts,pts_time -of csv=p=0 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-wire.ts; $prior=-1L; $n=0; $bad=0; foreach($row in $rows) { $parts=$row -split ','; $v=0L; if([long]::TryParse($parts[0],[ref]$v)) { if($v -le $prior) { $bad++; Write-Output "index=$n previous=$prior pts=$v" }; $prior=$v; $n++ } }; Write-Output "audioPackets=$n nonIncreasing=$bad lastPTS=$prior"

$wire=(& D:/mabs/local64/bin-video/ffprobe.exe -v error -select_streams a:0 -show_packets -show_data_hash sha256 -show_entries packet=pts_time,data_hash -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-wire.ts | ConvertFrom-Json).packets; $record=(& D:/mabs/local64/bin-video/ffprobe.exe -v error -select_streams a:0 -show_packets -show_data_hash sha256 -show_entries packet=pts_time,data_hash -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r76-recording.ts | ConvertFrom-Json).packets; $counts=@{}; foreach($p in $wire) { $counts[$p.data_hash]++ }; $matched=0; $unique=0; $missing=0; foreach($p in $record) { if($counts.ContainsKey($p.data_hash)) { $matched++; if($counts[$p.data_hash] -eq 1) { $unique++ } } else { $missing++ } }; Write-Output "wire=$($wire.Count) recorded=$($record.Count) matched=$matched uniquelyMatched=$unique missing=$missing"
```

原始TS顺序音视频全解码exit0且无告警；ffprobe音频14238包，PTS非递增0，最后PTS29752424（90k）。已关闭抓包的RTP lost0、TS cc.drop/analysis.skips/analysis.drops查询为空。录制7521个AAC payload SHA256全部存在于wire，4889包在wire唯一匹配，missing0；静音hash重复不构成唯一AU定位。结合原始输出全区间连续而录制index50回退，可将本次回退限定在VLC录制/重封装路径，不归为生产输出回退，不宣称已查明VLC内部具体缺陷，也不扩外部依赖修复。VLC官方packetizer和mux会重新建立时间戳/PES，可见 https://raw.githubusercontent.com/videolan/vlc/3.0.x/modules/mux/mpeg/ts.c 。本次不是完整A/V同步/音质PASS。

重入source0末尾phase6994/6995ns、frequency0，21:57:01.388完成purge2→3。CLI已在抓包结束、观测记录后按身份强制清理；原始源均自然exit0。无本轮媒体进程残留。离线审核结束后清理精确r76文件清单，保留120秒源。

## r77：AV＋纯V混合输入原始失败证据

源仍为原120秒H264 1280×72030fps约8Mbps；source0带AAC、source1只发视频，输出意图仍HEVC/AAC 1280×72030fps 8000kbps CBR。实际命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r77 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r77.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r77-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r77-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r77-source1.log 2>&1
exit $LASTEXITCODE
```

FFmpeg PID2212/31088于22:02:05/07启动，CLI于22:02:16前后自然exit1、尚未构图：`InvalidArgument: raw RTP automatic signaling requires explicit companion audio identity`。当前InputPreparer把全局AudioVideo传给每源preflight，导致合法非选中纯V被索取音频身份。这是下一切片原始生产入口失败证据；无DAG运行/漂移/画面，不计验收通过。失败已判定，按精确PID清理两源（中止退出，不冒充120秒自然结束），然后删除本轮3日志；未创建SDP/截图/抓包。

r76经未参与实现的独立智能体复算wire/record PTS、完整解码及SHA256，限定归责PASS；不扩展为完整验收PASS。随后按15项清单清理本轮文件。

## r78：每源成员准备切片真实混合输入复验

五源码经双独立复审PASS。首次全量构建失败：新增inputs与CompositionPreparer同一函数末尾inputs重名；修正为plannedInputs后再次Release clean-first/all-target configure/build exit0（682项）。首次审查遗漏明确保留，不把源码PASS当构建PASS。

实际命令（源/输出规格同r77）：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r78 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r78.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r78-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r78-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r78-source1.log 2>&1
exit $LASTEXITCODE
```

FFmpeg36284/33668于22:17:05/07启动。CLI自然exit1，已越过原companion audio identity失败，但源事实规划中的同步RTP规划仍要求双成员：`InvalidArgument: Synchronized separate RTP requires explicit audio/video payload types and clock rates`。DAG未运行，无输出/播放/漂移指标；此为下一成员合同断点，不计混合输入通过。CLI在下一次PID观测前退出，未取得可靠CLI PID/CPU/内存样本，不补造数据。失败已判定后按精确PID清理两源，不能记自然120秒结束。记录后清理本轮build、CLI及两源4日志，无截图/抓包/SDP产物。

## r79：双 AV RTP H264/AAC → MPEG-TS over RTP HEVC/AAC，1280×72030fps 8000kbps CBR 共享路径回归

12文件准备时钟/retention切片双独立源码PASS，Release全量configure/build exit0。实际实流命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r79 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r79.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r79-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r79-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r79-source1.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r79-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r79- --snapshot-format=png rtp://@127.0.0.1:62020
```

CLI34988于22:26:37启动，源37516/8104于22:26:48/50启动，均原120秒自然exit0；VLC22324。实际查看22:27:27、22:28:12左右两画面更新及等比上下黑边，22:28:54全部黑。全源失活仍输出，至22:29:13编码47637包，workerErrors/errors/stalledIntervals=0；初始WS473714688B、双源稳态约230MB、末215789568B/private463052800B，CPU约39.77s（22:28:54采样），机器平均1.364284%、峰3.676471%。63workers、69graph threads；phase约156ns、frequency0。graphPayload峰24493963B/283objects，压力登记24335为背压观测。运行中保留对象不是释放终态。本次仅正常双AV→全失活回归观察，未做重入/端到端音频同步、长期内存和显式停止，不计完整验收PASS。

观测结束后VLC以RC quit退出，CLI按核实PID强制清理，不记自然退出。记录后清理本轮build、CLI/两源/VLC日志、SDP及3张截图，共9项；保留原120秒源。

## r80：AV＋纯V输入，原规格HEVC/AAC CBR输出意图

使用r79同一Release，实际命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r80 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r80.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r80-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r80-source0.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r80-source1.log 2>&1
exit $LASTEXITCODE
```

CLI31316，源13276/11740于22:30:31/32启动。监测CLI CPU0.015625→0.3125s，WS13524992→383709184B/private505257984B；进程自然exit1。已越过原双成员PT/clock错误，两路CUDA H264 decoder与scale_cuda准备实际执行；选中AV源生成retention后，在非选中纯V的CompositionSourcePlanner前置条件失败：`NotInitialized: Composition source requires sealed A/V input and prepared decoder facts`。filter准备伴随既有range unknown→tv提示，保留该现象，不解释为本次阻塞根因。未进入DAG、无输出/VLC/漂移证据，不计验收通过。

此失败与当前仍固定AV的source runtime门禁一致，下一步必须一次贯通source planner、sync domain、assembly、builder及资源consumer，不能删除前置条件后传入伪音频。失败后按精确PID清理两源（中止非自然结束），记录后删除三日志；无SDP/截图/抓包。

### r81：AV＋V 源 runtime 合同复验（2026-10-04）

Release clean-first/all-target全构建退出0；16文件双独立阶段PASS。120秒原始1280×720/30 H264/AAC源；目标HEVC/AAC、1280×720/30、8000kbps CBR、MPEG-TS/RTP。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r81 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r81.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r81-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r81-source0.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r81-source1.log 2>&1
exit $LASTEXITCODE
```

CLI自然退出1：`Aggregate source normalization, identity or clock contract differs`。两路CUDA解码探测完成，V retention video_units=334，无audio产品；原r80失败不再出现。未建生产DAG，无画面或漂移证据，未启动VLC。后台CPU/内存采样未取得CLI样本，不能报告峰值。失败后清理源PID36720/12320，非自然结束。本轮FAIL。记录后清除三份运行日志和构建日志，保留复用源。

### r82：聚合可选音频合同复验（2026-10-04）

4文件双独立阶段PASS；Release全量构建成功。输入AV＋V RTP，原120秒1280×720/30 H264/AAC；输出目标HEVC/AAC、1280×720/30、8000kbps CBR、MPEG-TS/RTP。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r82 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r82.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r82-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r82-source0.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r82-source1.log 2>&1
exit $LASTEXITCODE
```

CLI自然exit1：`Composition source requires its own synchronized A/V source plan`，发生于CompositionGraphBuilder::validateOptions，r81 aggregate规划错误已越过。尚未建立DAG，无VLC或漂移证据。后台采样未取得CLI样本，不宣称CPU/内存峰值。失败后按精确PID36696/6008清理源（非自然结束）；记录后清除本轮三运行日志和build.log。完整混合仍FAIL，后续必须按实际members贯通builder/startup/资源及shape，保留严格契约。

### r83：双AV合屏共享合同回归（2026-10-04）

输入两路原始120秒H264/AAC RTP/RTCP，输出HEVC_NVENC/AAC、1280×720/30、8000kbps CBR、MPEG-TS/RTP。使用r82全量Release产物。实际命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r83 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61032 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r83.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r83-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r83-source0.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61032?rtcpport=61033&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r83-source1.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r83-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r83- --snapshot-format=png rtp://@127.0.0.1:62020
```

CLI PID19616，源PID37068/35704均完整120秒自然exit0，VLC PID17372。22:58:30及22:59:17两张VLC快照见两幅不同时间的横向640×360画面，上下黑边；23:00:16快照为全黑。源均结束后CLI继续编码，23:00:29记录encodedPackets=48810、workerErrors/errors/stalledIntervals/droppedBuffers均0，graph threads=69/workers=63/processThreads=97。峰值WS457584640，流期间WS约228–231MB、Private约480–483MB；全黑末尾WS214716416、Private463069184，CPU累计42.40625s，平均机器占用1.306506%、峰值4.326923%。最后音频servo generation1 raw_phase155ns/filtered156ns/frequency0ppm，不等同端到端A/V漂移证明。graph payload峰值23913579B/279objects；pressureFailures24353为已有有限背压登记，并非据此认定媒体损坏或完整通过。

有限回归结论：双AV实时合屏和全失活持续黑场未见本轮runtime退化。未覆盖重入、端到端音频质量/同步、显式stop、长稳及单源共享域，完整验收仍未完成。记录后VLC经RC quit，CLI在测试结束后按精确身份清理；清理退出不记自然结束/PASS。随后逐项删除3截图、4日志及SDP，共8项，保留复用源；本轮无抓包/录制。

### r84：AV＋V实际图装配复验（2026-10-04）

9文件装配切片Release全量重建成功。A审查阶段PASS；B发现V clock group仍写maximum_inter_stream_clock_offset_skew_ns，违反底层V选项合同，故本冻结整体审查FAIL。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r84 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r84.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r84-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r84-source0.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r84-source1.log 2>&1
exit $LASTEXITCODE
```

CLI PID20172自然exit1，已越过原GraphBuilder及资源合同门禁，在runtime factory注册失败：`MediaAvBoundReleaseExtractorNode requires a planned audio branch mode`。工厂只接受CopyPacket/TranscodeFrame，而V规划正确为Drop；底层空audio release可复用。尚未进入运行，VLC未启动，无画面/漂移。后台23:13:40–50取得CPU .015625→.03125s、WS13520896/Private19730432；探测峰值未捕获，不据此声明内存上界。测试结束清理源PID34380/29024及监控PID8792（核验身份），源非自然结束。记录后清除本轮日志/SDP（按实际存在逐项），保留复用源。两个合同问题需修正并双审，非PASS。

### r85：AV＋V合屏及独立重入（2026-10-04）

12文件最终冻结双独立PASS，包含Extractor六事务成员修复的第二次Release全量构建成功（第一次构建不含末次修改，未用于本实流）。输入原120秒H264/AAC 1280×720/30，source0 AV/source1纯V；输出HEVC_NVENC/AAC 1280×720/30、8000kbps CBR，MPEG-TS/RTP。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r85 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r85.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r85-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r85-source0.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r85-source1.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r85-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r85- --snapshot-format=png rtp://@127.0.0.1:62020
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r85-source1-reentry.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r85-source0-reentry.log 2>&1
exit $LASTEXITCODE
```

CLI PID38328，VLC20328；初始AV38788/V35684、V重入34336、AV重入19320均原120秒自然exit0。23:27:53与23:28:47双画面更新；23:29:21全部黑场；23:30:03仅右V恢复，左黑；23:31:01双方恢复；23:31:35 V结束后右黑、左AV继续；23:32:43再次全黑。CLI全过程持续输出，记录108658包，errors/workerErrors/stalledIntervals/droppedBuffers均0。V clock明确members=video_only并重入generation2，音频servo generation2 phase20216ns/frequency0ppm。此为内部校正证据，不等同端到端音频质量/A/V漂移证明。

线程60/worker55/process90。WS峰473550848B；初次流稳定约222–227MB，全黑211423232B，双重入后约224MB，再全黑213831680B/Private454361088B，CPU累计93.3125s、平均机器1.304633%、峰3.703704%。相较首轮全黑Private453853184B并非精确归零，不据此证明长稳；graph payload峰22488013B/196objects，pressure登记47640（有界背压观测）。有限混合启动/断流/重入及持续画面链路取得证据；未核验音频波形/端到端同步、明确stop、3/4混合、其他编码RC矩阵、单源共享域及RKMPP，整体门禁仍未完成。测试结束后VLC RC quit、CLI精确PID清理，非自然退出/PASS；monitor随CLI消失退出。记录后逐项清除7截图、8日志与1SDP，共16项，源保留。

### r86：单源共享AV回归（2026-10-04）

同一最终Release，原120秒H264/AAC RTP→HEVC_NVENC/AAC、1280×720/30、8000kbps CBR、MPEG-TS/RTP，完整单源共享域。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r86 --input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r86.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r86-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r86-source0.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r86-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r86- --snapshot-format=png rtp://@127.0.0.1:62020
```

源PID21616完整120秒自然exit0；CLI20452自然exit1，VLC38112。23:34:57快照为完整1280×720画面；23:36:16快照仅证明退出后VLC留存的末帧，不能称退出后持续播放。23:36:05.955源unavailable(reason1)，23:36:05.956代际purge完成，ack_count=7、old1→next2、continuation=acquiring；23:36:11.279 stop.begin，最终`NotInitialized: realtime runtime made no progress before timeout`。这是本次复现，不能将RTP BYE擅自改为可信EOF或增加timeout掩盖；待核对共享源生命周期与应用无进展策略边界。

最终encodedPackets入出40003，errors/workerErrors/droppedBuffers0，stalledIntervals1；39threads/37运行workers，退出workers0。payload最终0bytes/0objects、71918预留=71918释放，峰12221104B/77objects。WS telemetry峰194691072B（独立采样194699264B）、末190210048B，Private采样末397012992B，CPU末采样30.203125s，平均机器1.105957%、峰2.912621%；最后servo phase156ns/frequency0ppm。sender purged时committed86360datagrams、deadline/pressure/partial failures0，但delivery_evidence=not_proven，无抓包不声明完整网络交付。回归运行期有效，但源停止后的自然失败使本项完整验收FAIL。

记录后VLC RC quit；CLI/源/监控已自然结束，无强停CLI。逐项删除2截图、3日志、1SDP共6项，复用源保留。没有本轮远程产物，未开展RKMPP。

### r87：四路混合 AV+V+V+V 实流（2026-10-04）

沿用 r85 最终 Release。输入为指定原120秒 H264/AAC 源，source0 AV、source1/2/3 V；2×2合屏输出 HEVC_NVENC/AAC、1280×720/30、8000kbps CBR、MPEG-TS/RTP。未降低源规格。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r87 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r87.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r87-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r87-source0.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r87-source1.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r87-source2.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r87-source3.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r87-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r87- --snapshot-format=png rtp://@127.0.0.1:62020
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r87-source3-reentry.log 2>&1
exit $LASTEXITCODE
```

初始 CLI38612、VLC38472，源16084/34080/18940/34484均完整发送120秒自然exit0。23:43:45 VLC快照全黑，CLI仍持续输出；本轮未留取并核验初始四格画面，不能把该缺失证据记为四格画面通过。第四路V重入PID21476，23:44:25已看到仅右下格恢复，其他三格黑场；RTP clock members=video_only、generation2。

23:45:04第二幅右下格画面已更新；第四路重入完整120秒自然exit0，23:46:19再次全黑，CLI仍持续输出。该时点encodedPackets入出129524、errors/workerErrors/stalledIntervals/droppedBuffers均0；96图线程/89workers/process153。CPU累计103.203125s、平均机器1.206150%、峰10.064240%；WS准备峰1060016128B，首次全黑280748032B，重入后全黑305573888B、Private643510272B，存在相对首轮黑场驻留增加，不能声明长稳/回收通过。payload峰50946414B/668objects、末372856B/10objects；pressure44588为有界背压观测。缺初始四格快照、音频波形及端到端A/V漂移、明确stop、长稳、三路/矩阵/C API/RKMPP，因此仅记四源建图及第四格隔离重入/全黑持续证据，整体未通过。

测试观测结束，按已授权规则核验身份后清理CLI38612；该结束是人工清理，非自然退出或PASS。VLC38472经RC quit。记录后清理本轮4截图、7日志及1SDP共12项，指定源保留；没有远程测试产物。

r86独立复核补充：RTCP BYE在MediaRtcpSenderReportTracker转为SenderLeft，RawRtpInputNode失效并等待新证据，不发布可信EOF；controller在输出与输入均无进展后返回ProgressTimeout。历史r24记录同链路，本轮AV两成员分支保留原行为，目前不能归因optional audio回归。维持“运行期有限回归成立、完整优雅结束门禁未通过”，不修改单源终止语义。

### r88：三路混合 AV+V+V 实流（2026-10-04）

同一Release、原120秒源，H264/AAC RTP→HEVC_NVENC/AAC、1280×720/30、8000kbps CBR、MPEG-TS/RTP。CLI36336，源38444/29544/35576，VLC37100。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r88 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r88.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r88-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r88-source0.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r88-source1.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r88-source2.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r88-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r88- --snapshot-format=png rtp://@127.0.0.1:62020
```

23:48:58与23:49:26两幅VLC快照确认三画面持续更新、按输入顺序左上/右上/左下排列，右下空格为黑场。内部av_drift_trace末次记录generation1、raw/filtered phase0ns、frequency0ppm；不是端到端音画同步证明。

三源均完整120秒自然exit0，23:50:32 VLC再次全黑，CLI持续编码。该时点encoded入47380/出47379，errors/workerErrors/stalledIntervals/droppedBuffers均0；78图线程/72workers/process121。WS峰853467136B、末241205248B，Private532344832B，CPU48.12s、平均机器1.534248%、峰5.755396%；payload峰41479165B/647objects、末372862B/10objects、pressure30637。仅记录三格更新与全部失活持续输出；未覆盖本轮重入、音频质量、端到端A/V、长稳与明确stop，整体门禁仍未完成。记录后CLI36336身份核验并人工清理（非自然退出/PASS），VLC37100 RC quit；逐项清理3截图、5日志、1SDP共9项，源保留，未开展远程验证。

### r89：四路 H264 VBR 启动未完成（2026-10-04）

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r89 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r89.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r89-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r89-source0.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r89-source1.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r89-source2.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r89-source3.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r89-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r89- --snapshot-format=png rtp://@127.0.0.1:62020
```

CLI自然exit1，准备阶段raw RTP video signaling probe open timeout，matching_rtp_packets=0、matching_datagram_bytes=0、parameter_sets=none，未建图。无法据此判断H264 VBR能力；源与CLI启动衔接未取得有效接收证据，重试不得增大timeout或降低规格。本轮源36388/37316/35440/27576和VLC38120在失败后按身份核验清理，未记完整源自然结束或PASS；监控随CLI退出结束。保存记录后逐项清理6日志，无SDP/截图。下一轮将CLI与源命令一起调度。

### r90：四路混合 H264 VBR 有限链路（2026-10-04）

CLI与四源同批直接命令调度，保持30秒准备超时。原120秒H264/AAC输入，AV+V+V+V→H264_NVENC VBR/AAC、1280×720/30、最小/目标/最大8000/8000/12000kbps、MPEG-TS/RTP。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r90 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r90.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r90-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r90-source0.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r90-source1.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r90-source2.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r90-source3.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r90-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r90- --snapshot-format=png rtp://@127.0.0.1:62020
```

CLI32212、VLC13460，四源37884/9444/30672/34724完整120秒均自然exit0。23:55:28与23:56:38两幅VLC四格画面更新，全部源停止后快照全黑、CLI持续输出。记录encoded入出43399，errors/workerErrors/stalledIntervals/droppedBuffers均0；96图线程/89workers/process153。WS准备峰1061666816B、末282009600B，Private623198208B，CPU51.25s、平均机器1.772717%、峰9.307359%；payload峰50963809B/747objects、末372850B/10objects，pressure37613。音频内部phase156ns/frequency0ppm不替代端到端A/V证明。仅完成四格H264 VBR运行与全失活输出证据，未覆盖本轮重入、完整音频/同步、长稳和明确stop，整体仍未通过。

记录后核验CLI32212身份并人工清理（非自然exit/PASS），VLC13460 RC quit；逐项清理3截图、6日志、1SDP共10项，原源保留。无远程产物。

### r91：末位音源 HEVC VBR 断流失败（2026-10-04～05）

V+V+V+AV，audio-source=3，原120秒H264/AAC→HEVC_NVENC VBR（8000/8000/12000kbps）/AAC，1280×720/30，MPEG-TS/RTP。日志source0对应末位AV，source1/2/3对应输入索引0/1/2。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r91 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r91.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r91-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r91-source0.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r91-source1.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r91-source2.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r91-source3.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r91-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r91- --snapshot-format=png rtp://@127.0.0.1:62020
```

```powershell
& D:/Wireshark/dumpcap.exe -i 10 -f "udp port 62020" -a duration:190 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r91-output.pcapng > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r91-capture.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r91-selected-reentry.log 2>&1
exit $LASTEXITCODE
```

CLI12036、VLC30824，初始四源27992/31232/8216/37448均完整120秒自然exit0。23:59:17、00:00:13四格画面更新。00:00:50快照仍有左下格末帧，不是全黑成功。末位AV source3于00:00:45.235失活并purge完成ack2；source0于46.889/46.890、source1于48.547/48.548、source2于50.295/50.296失活/purge完成ack1。source2完成后恰10秒，00:01:00.296 node46 MediaRtpPacketClockBinderNode报Cancelled: Initial clock acquisition master deadline expired，CLI自然exit1。选中源重入37828没有取得CLI恢复证据，测试FAIL。不可称全失活持续或音频恢复通过。最终workerErrors/errors1，stalls0，encoded入43121/出43120；payload0bytes但2objects、114708预留/114706释放，回收完整性未通过。WS峰1013297152B、末284405760B，CPU平均机器1.794811%、峰9.333333%，payload峰52760612B/768objects，pressure37580。

输出抓包dumpcap33612已启动，但未完成预期音频恢复分析，不能作为音频通过证据。发现失败后按精确PID核验清理重入FFmpeg37828、dumpcap33612及VLC30824；CLI已自然失败。诊断重点为source2时钟binder的重采集deadline，而非尚未证明有问题的audio-source索引。记录后清理本轮截图、日志、SDP与pcapng，源保留。

### r93：RTP binder 边界取证与末位 AV 重入（2026-10-05）

沿用 r91 的 V+V+V+AV、audio-source=3，指定120秒H264/AAC源→HEVC_NVENC VBR 8000/8000/12000kbps、1280×720/30、AAC CBR192kbps/44100Hz/2ch、MPEG-TS/RTP。仅增加 binder 状态边界日志；Release 全量构建exit0，两个独立审查者均对诊断切片PASS，不代表功能修复。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r93 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r93.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r93-cli.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r93-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r93-source1.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r93-source2.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r93-source3.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r93-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r93- --snapshot-format=png rtp://@127.0.0.1:62020
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r93-selected-reentry.log 2>&1
```

CLI PID10644，初始FFmpeg source0/1/2/3为3644/22796/17424/29600，VLC18256，末位AV重入FFmpeg1824。初始四源已完整120秒自然exit0；10:52:34与10:53:19四格更新，10:54:32全黑。10:54:03.080/source3、03.560/source0、05.062/source1、06.653/source2完成purge；超过r91的10秒期限没有expired。末位AV重入10:54:49.009音频node66以rawSSRC2683151904、rawGeneration1、sequence5195建立deadline，当时source transition old1/next2/sequence1/published1；随后generation2工作，10:55:27右下格恢复，其他三格保持黑色。

重入FFmpeg1824完整120秒自然exit0。10:56:48.496 source3第二次purge完成（old2/next3/transition2，ack2），10:56:56画面全黑；至10:57:09仍连续输出，encoded入/出101010、errors/workerErrors/stalls均0。平均机器CPU1.250260%、峰10.250569%；WS峰1038782464B，首次全失活后284090368B，第二次全失活后311250944B，存在约27MB差异，不能以payload有界推断总内存长稳通过。payload峰49333050B/663objects，末372856B/10objects；运行中的预留198363/释放198359，尚非停止回收证明。重入generation2漂移日志raw/filtered phase为0；未抓包或分析输出音频内容，不能据此宣称静音/恢复质量和端到端同步通过。

本轮未复现r91；不能从一次不复现推断r92已确定修复。binder尚未消费provenance、健康peer frontier和初次准入/重采集期限合同仍未闭合。测试观测已结束，随后按身份核验的PID10644清理仍持续输出的CLI，VLC18256退出；强制清理不计自然退出或停止API通过。未运行local与RKMPP，未推送/合并/部署。结果为有限运行证据，完整合屏门禁仍未通过。
清理前末条报告：encoded116272/116271，WS311255040B，平均机器CPU1.185938%，errors/workerErrors/stalls0；payload372862B/11objects，预留208341/释放208336。231条drift记录raw phase范围0～156ns，不等同端到端测量。CLI清理后工具exit=-1；7个精确PID均无残留。记录后逐项清除本轮5张截图、7份媒体链日志、1份SDP及1份构建日志，共14件；本轮未产生抓包、录制媒体或远程文件，指定120秒源保留。

### r94：owner屏障预检失败（2026-10-05）

18文件修正版双审PASS、Release全量重建exit0。实际执行以下四源原规格命令，但11:25:43.956 CLI预检报InvalidArgument: Synchronized graph contains an unsupported group consumer，自然exit1；未进入运行DAG，无CPU/内存/漂移/画面通过证据。根因：Raw新增rtp.sync_group未加入MediaAvCommonCoreShapeValidator的明确kind/key合同。不能关闭验证，须补齐注册。FFmpeg source0/1/2/3 PID29812/31108/6800/30044、VLC18408，在测试判失败后核验身份并中止；源未完成120秒，不算成功验收。本轮不重入。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r94 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r94.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r94-cli.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r94-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r94-source1.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r94-source2.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r94-source3.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r94-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r94- --snapshot-format=png rtp://@127.0.0.1:62020
```

记录后逐项清理CLI/4源/VLC日志、SDP（若生成）和两份构建日志；没有截图、抓包或录制媒体。保留指定源，待修正后使用原规格重新验证。

r94实际清理完成：8份日志逐项删除；未生成SDP。四个源均为中止后工具exit=-1，不能计自然完成；精确PID无残留，指定120秒源保持124427809字节。

### r95：Raw源域合同修正后的原规格复验（2026-10-05）

19文件双独立源码PASS，Release全量重建exit0。实际命令如下；运行结果随后记录。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r95 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r95.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r95-cli.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r95-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r95-source1.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r95-source2.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r95-source3.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r95-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r95- --snapshot-format=png rtp://@127.0.0.1:62020
```

选定末位AV重入实际命令：
```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r95-selected-reentry.log 2>&1
```

r95结果：CLI28784、VLC11644，初始FFmpeg源4672/26752/29504/15288及AV重入31572均完整120秒自然exit0。VLC11:36:16、11:37:05四格更新；11:38:06全黑；11:38:59仅右下恢复；11:40:59再次全黑。首次各源purge完成时间11:37:35.880/37.519/39.210/40.827，Raw与binder逐流cutoff相等（视频3625、音频5193）；重入结束11:40:37.508第二屏障视频7220/音频10386，old2→3。超过10秒失败窗口仍持续，无expired，CLI errors/workerErrors/stalls0。11:40:59输出107160/107160包，平均整机CPU1.332992%、峰8.595388%；WS准备峰1061609472B，第一次全黑273969152B，第二次294490112B，不能据此证明长稳或完全回收。payload峰51115111B/762objects，末372856B/10objects，202033预留/202029释放为运行态；229条内部drift raw phase155～2204ns，不是端到端音频同步证明。VLC的Failed to create video converter发生于PNG截图转换候选链，后续d3d11_filters/swscale成功并生成截图；HEVC与FAAD播放成立，未声称VLC日志零错误。
本轮仅有限屏障/画面/断流恢复证据，未关闭全部clock分支退休、已激活源期限、音频质量、内存长稳、停止API、C API与RKMPP门禁。测试已结束，CLI按精确身份强制清理，不能记自然退出或停止API通过；随后逐项清除14件本轮产物，保留指定源。

r95清理确认：CLI工具exit=-1；14件产物已逐项删除，7个精确PID无残留；无本轮抓包/录制媒体/远程文件。

### r96：共享单路realtime AV回归（2026-10-05）

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r96 --input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r96.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r96-cli.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r96-source0.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r96-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r96- --snapshot-format=png rtp://@127.0.0.1:62020
```

r96结果：CLI23560、FFmpeg3560、VLC26164。源完整120秒3600帧自然exit0；11:43:11与11:44:16两次VLC画面更新。11:44:41.622 Raw1/2与binder5/6的purge视频cutoff3625、音频5193一致，ack_count7完成。11:44:44.992 CLI自然exit1：NotInitialized: realtime runtime made no progress before timeout。未出现binder expired；最终errors/workerErrors0、stalledIntervals1，输出40003/40003；payload0B/0objects，预留/释放71903/71903，压力失败0。CPU平均1.156976%、峰4.132231%；WS峰196435968B、末191451136B；118条内部drift raw phase20627～20628ns。
当前controller条件为输出无进展且真实输入不活跃超过progressTimeout即返回ProgressTimeout；RTCP BYE不等同可信EOF。r86相同链路已有同样退出记录，因此不能直接归因本次Raw/binder修改，也不能标记本项完整成功。源和CLI均自然结束；VLC随后quit，记录后清理2截图、3日志、1SDP共6件。

r96独立复核：A确认11:44:39.620 SenderLeft→41.622屏障完成→44.920 stop.begin为既有controller路径，无证据归因19文件回归；owner屏障约2秒，controller期限仍从最后媒体进展计算。6件产物及3个精确PID清理确认无残留，指定源保留。

### r97：重入音频SR缺失单变量诊断（2026-10-05）

保持原四源完整120秒、原HEVC VBR规格。仅在初始源自然结束后的末位AV重入，将音频rtpflags改为skip_rtcp+send_bye，视频SR正常；这是补充诊断，不替代原规格验收。FFmpeg官方https://ffmpeg.org/ffmpeg-protocols.html与本机muxer=rtp帮助均确认该选项。实际初始命令：
```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r97 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r97.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r97-cli.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r97-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r97-source1.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r97-source2.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r97-source3.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r97-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r97- --snapshot-format=png rtp://@127.0.0.1:62020
```

重入实际命令：
```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags skip_rtcp+send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r97-selected-no-audio-sr.log 2>&1
```

初始CLI17732、VLC8332，FFmpeg3256/31756/16896/17556均完整120秒自然exit0；四格截图11:49:18，全黑11:50:45。四源purge于11:50:35.023/36.638/38.375/39.934完成。重入FFmpeg17424仅音频不发SR，视频保持正常；11:51:18.527 node65 binder超时，buffered300、phase2、preserves_output1、rawSSRC1653469649/rawGeneration1/rawSequence3627，source old1/next2/transition1/published1。CLI自然exit1，错误为Cancelled: Initial clock acquisition master deadline expired。此为已激活源等待组clock的确定生产失败，不能再将正常SR重入通过视为完整恢复合同。

最终encoded53383/53382，errors/workerErrors1、stalls0；平均整机CPU1.516832%、峰10.430839%；WS峰1051746304B、末295534592B；payload0B/2objects、预留121824/释放121822，完全回收未证明。测试判失败后按身份清理尚在发送的诊断源17424，非完整120秒成功；VLC退出，随后删除本轮产物。外部FFmpeg未修改。

期限建立11:51:08.527，到期11:51:18.527，精确10秒；112条内部drift raw phase155～156ns，非端到端指标。诊断源清理工具exit=-1，最后读取进度1903帧/63.43秒，不计完整源。清理清单：2截图、CLI/4初始源/重入/VLC共7日志、1SDP，共10件，无抓包/录制/远程文件。

10件产物和7个精确PID已核对无残留；指定120秒源保留。

### r98：未绑定候选驻留修复复验（2026-10-05）

两文件双独立源码PASS，Release全量重建exit0。初始实际命令：
```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r98 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r98.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r98-cli.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r98-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r98-source1.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r98-source2.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r98-source3.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r98-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r98- --snapshot-format=png rtp://@127.0.0.1:62020
```

缺音频SR阶段实际命令：
```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags skip_rtcp+send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r98-selected-no-audio-sr.log 2>&1
```

正常SR恢复实际命令：
```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r98-selected-recovery.log 2>&1
```

r98结果：CLI30572、VLC1076；初始FFmpeg30952/28624/11160/32576、缺音频SR31964、正常SR恢复31680，六个源均完整120秒自然exit0。初始四格12:04:37/12:05:55更新；12:06:35全黑；缺SR12:07:26仍全黑；恢复12:09:17/12:10:21仅右下更新；12:11:29再次全黑。初始四源purge12:06:00.057/01.527/03.181/04.819；末位再次断流purge12:10:53.107 old2→3完成。

缺SR视频候选于12:06:37.662建立期限、12:06:47.666退休301个完整未绑定AU，未抛expired、未反复建立候选期限。12:08:52.116恢复：node65 rawSSRC2274790118/rawGeneration2/rawSequence7222，protocolGeneration2/evidenceRevision74，audio rawGeneration2/SSRC1000505055，仍同一source transition1/target2；随后第2代音频同步记录恢复。r97确定的10秒会话退出在原失败条件下不再出现，并恢复播放。本证据未测线端静音/音频质量，不将画面与内部音频telemetry替代该项。

12:11:29末报告encoded149187/149186、errors/workerErrors/stalls0；平均整机CPU1.201786%、峰8.433735%；WS准备峰1054576640B，首全黑282779648B，缺SR中299372544B，末307470336B，仍不证明长稳。payload峰50923503B/735objects、末372862B/11objects，233284预留/233279释放为运行态。229条内部drift raw phase155～2204ns，不是端到端漂移。完整合屏门禁及共享单路新版本回归尚未关闭。观测结束后按精确身份清理持续输出CLI，不能记自然结束/stop API通过；VLC quit。记录后逐项清理7截图、9日志（含构建）及1SDP共17件，保留指定源。

r98清理确认：CLI工具exit=-1；17件产物已逐项删除，8个精确PID无残留，无抓包/录制媒体/远程文件，指定源保留。

### r99：r98修复后的共享单路realtime回归（2026-10-05）

实际命令：
```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r99 --input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r99.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r99-cli.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r99-source0.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r99-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r99- --snapshot-format=png rtp://@127.0.0.1:62020
```

r99结果：CLI13516、源2356、VLC31336。源完整120秒3600帧自然exit0；12:13:30和12:14:48画面更新。12:15:03.935 purge ack7完成，随后CLI自然exit1：NotInitialized: realtime runtime made no progress before timeout，与r86/r96既有终止路径一致；没有candidates_retired/candidate_binding_resumed/expired。最终encoded40003/40003、errors/workerErrors0、stalls1；payload0B/0objects，预留/释放71901/71901、压力失败0。平均整机CPU1.327346%、峰4.408353%；WS峰196214784B、末191418368B；118条内部drift raw phase155～156ns。只认可运行期共享链路与回收证据，不记完整优雅结束成功。记录后退出VLC，删除2截图/3日志/1SDP共6件，指定源保留。

r99清理确认：6件产物、3个精确PID无残留，无本轮抓包/录制/远程文件，指定120秒源保留。


### r100：CLI信号停止入口（2026-10-05）

四文件（RealtimeStopSignal.h/.cpp、main.cpp接线、CMake）修正后A/B独立源码PASS；Release clean-first all-target构建684任务exit0。POSIX媒体线程继承blocked信号mask，仅桥接线程接收信号；Linux/RKMPP未运行。Windows运行命令如下，CLI使用PTY会话10297，PID3632；四源PID22804（AV）、21176/5152/9228（V），VLC19464。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r100 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r100.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r100-cli.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r100-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r100-source1.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r100-source2.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r100-source3.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r100-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r100- --snapshot-format=png rtp://@127.0.0.1:62020
```

r100结果：四源完整120秒自然exit0；VLC截图12:31:59、12:32:41四格内容更新，12:33:51全黑，CLI仍持续输出。12:34:09经PTY write_stdin U+0003后宿主会话exit1，CLI3632同时消失；无end_reason=caller_stop、final或成功报告，因此停止与回收验收FAIL/证据不足，不能按自然退出成功记录。最后运行errors/workerErrors/stalls0；CPU均值1.503993%、峰值7.113821%（整机归一）；WS峰值1050488832B、最终295821312B；payload372856B/10objects，峰值50836244B/598objects，121487预留/121483释放，不能据此声称退出后归零。运行期缺少资源回收证据。随后退出VLC并逐项删除本轮3PNG、build/CLI/4源/VLC共7日志及SDP共11件；指定120秒源保留。下一轮先用独立控制台验证信号投递，避免PTY PowerShell中止子进程。
r100内部漂移样本116条，raw_phase_ns 155–156ns；仅内部同步证据，不代表线端A/V漂移。


### r101/r102：独立控制台准备取消诊断

r101无源等待期间30秒open timeout先到，PID32084已自然exit1；信号helper核验PID失败后未投递。该项不是信号通过。r102 PID11848准备期间用Windows控制台API确认成员只有目标CLI与helper，再GenerateConsoleCtrlEvent(CTRL_C_EVENT,0)，约0.75秒后进程自然exit1，stdout end_reason=caller_stop，stderr Cancelled: raw RTP source preparation cancelled。只证明合屏准备取消，不替代原规格真实链路；没有启动FFmpeg/VLC，无运行CPU/漂移报告。所有辅助代码仅内联执行，无测试脚本文件。

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r101 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r101.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r101-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r101-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R100Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$target=32084
$p=Get-CimInstance Win32_Process -Filter "ProcessId=$target"
if($p.Name -ne 'media_transcode_realtime_video_cli.exe' -or $p.CommandLine -notlike '*composition-wiring-r101*'){throw 'PID identity mismatch'}
[R100Console]::FreeConsole() | Out-Null
if(-not [R100Console]::AttachConsole($target)){throw 'AttachConsole failed'}
try {
if(-not [R100Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$ids=[uint32[]]::new(32)
$count=[R100Console]::GetConsoleProcessList($ids,32)
if($count -ne 2 -or $ids[0..($count-1)] -notcontains $target -or $ids[0..($count-1)] -notcontains $PID){throw "Unexpected console members: $ids"}
if(-not [R100Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R100Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'

$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r102 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r102.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r102-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r102-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R100Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$target=11848
$p=Get-CimInstance Win32_Process -Filter "ProcessId=$target"
if($p.Name -ne 'media_transcode_realtime_video_cli.exe' -or $p.CommandLine -notlike '*composition-wiring-r102*'){throw 'PID identity mismatch'}
[R100Console]::FreeConsole() | Out-Null
if(-not [R100Console]::AttachConsole($target)){throw 'AttachConsole failed'}
try {
if(-not [R100Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$ids=[uint32[]]::new(32)
$count=[R100Console]::GetConsoleProcessList($ids,32)
if($count -ne 2 -or $ids[0..($count-1)] -notcontains $target -or $ids[0..($count-1)] -notcontains $PID){throw "Unexpected console members: $ids"}
if(-not [R100Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R100Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'

```

r101/r102各2件CLI日志记录后逐项删除，PID32084/11848无残留；r100的11件产物及6个进程已核验清理。


### r103：四源H264/AAC→HEVC VBR 8000/8000/12000kbps、1280×720/30、AAC CBR192kbps、MPEG-TS/RTP显式停止

使用r100同一Release二进制，不改源参数；CLI独立隐藏控制台PID14624，AV源19688，V源3632/31484/21036，VLC26316。以下直接命令执行，信号helper为内联OS调用，不落地测试脚本。

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r103 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r103.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r103-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r103-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r103-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r103-source1.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r103-source2.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r103-source3.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r103-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r103- --snapshot-format=png rtp://@127.0.0.1:62020
```


停止命令：
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R100Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$target=14624
$p=Get-CimInstance Win32_Process -Filter "ProcessId=$target"
if($p.Name -ne 'media_transcode_realtime_video_cli.exe' -or $p.CommandLine -notlike '*composition-wiring-r103*'){throw 'PID identity mismatch'}
[R100Console]::FreeConsole() | Out-Null
if(-not [R100Console]::AttachConsole($target)){throw 'AttachConsole failed'}
try {
if(-not [R100Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$ids=[uint32[]]::new(32)
$count=[R100Console]::GetConsoleProcessList($ids,32)
if($count -ne 2 -or $ids[0..($count-1)] -notcontains $target -or $ids[0..($count-1)] -notcontains $PID){throw "Unexpected console members: $ids"}
if(-not [R100Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R100Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'

```

结果：四源均完整120秒自然exit0。VLC12:38:07、12:38:46四格内容更新，12:39:37全黑，CLI仍连续输出；定向CTRL_C_EVENT后CLI自身exit0，12:40:00.250 stop.begin→12:40:00.319 stop.done state=Stopped（内部停止段69ms），end_reason=caller_stop及成功报告。最终workers0、errors/workerErrors/stalledIntervals0、payload0B/0objects、120996预留=120996释放。encodedPacketsPushed52020/popped52019；调用方停止为取消语义，不据此宣称所有尾包已发出。stderr AAC关闭有2 frames left in queue，此记录保留，不能将本项等同可信EOF排空。

CPU整机归一均值1.599089%、峰值5.819296%；WS准备峰值1058017280B，运行约314→321MB，全失活272715776B，非长稳证据。payload峰值50131675B/662objects，pressureFailures37794（背压计数），停止后归零。114个内部漂移样本155–156ns，不代表线端音频质量、静音或端到端A/V同步通过。r103关闭的是Windows四源全失活后的显式停止与受管理资源回收；不能关闭可信EOF、单源prepare即时取消、POSIX信号实测、C API、长稳及完整合屏门禁。

记录后通过VLC RC quit关闭PID26316，精确清理3PNG、CLI stdout/stderr与4源及VLC共7日志、1SDP，共11件；CLI14624与四源19688/3632/31484/21036已自然退出。本轮r100–r103合计26件临时产物，指定120秒源保留。没有抓包/录制文件，没有临时测试脚本，未连接RKMPP或改动FFmpeg依赖。清理完成状态另行核验。

最终核验：A/B分别直接读取r103最终日志，均明确本场景PASS；两者保留可信EOF、活动媒体停止、完整堆/GPU审计及跨平台边界。26件临时产物已逐项删除，r100–r103文件名前缀无残留；当前无realtime CLI/FFmpeg/VLC进程，指定源仍124427809B。工作分支feat/realtime-video-composition，未推送、合并、部署。


### r104：H264 CBR四源与接收端音频恢复验证

同一Release二进制。原120秒源四路V+V+V+AV（音源3）→H264 CBR8000kbps、1280×720/30、AAC CBR192kbps、MPEG-TS/RTP。CLI19340、AV8692、V32616/22804/32000、VLC17536。VLC duplicate display+std file保存接收到的音视频，未增加在线FFmpeg监控。官方依据：https://docs.videolan.me/vlc-user/desktop/3.0/en/advanced/stream_out_introduction.html 。录制经VLC重新封装，不能等同原始线上TS字节/包抖动证据。

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r104 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r104.sdp --video-codec h264 --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r104-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r104-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r104-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r104-source1.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r104-source2.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r104-source3.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r104-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r104- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r104-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

源离线基线命令：`D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:a:0 -af volumedetect -f null NUL` exit0，120秒/10584000双声道样本，mean_volume -17.4dB、max_volume0.0dB；此为离线源核验，不替代实时验收。


r104失败：12:44:36启动，12:44:59.131 node90 FileMux 首错 `InvalidArgument: MPEG-TS access unit has no transport service window`，CLI自身exit1。随后其他输入/decoder的Cancelled是协调取消。四个FFmpeg源各完整120秒自然exit0，但CLI仅运行约22.7秒，不构成120秒输出通过；未开始重入。VLC接收文件17457304B、ffprobe21.193233秒，H2641280×720及AAC44100Hz2ch；未取得有效实时快照，不能声称完整画面/音频/恢复通过。

最终workers0、workerErrors1/errors1/stalls0，CPU整机均值2.319956%峰值10.873786%，WS峰1019142144B末319938560B；payload最终0B/2objects，22344预留/22342释放，不能声称完全归零。23个内部漂移样本155–156ns。MPEGTS diagnostics：access_units1614、pending_bytes752、peak_pending112048、pressure_failures0、maximum_scheduling_debt_ns0、max_mux_input_after_emit286505162ns/after_dispatch186505162ns（最坏ready22724236300、emit22437731138、dispatch22537731138）；max_protocol_batch_produced_after_release101358856ns。上述累计最坏值不证明就是失败AU。

双独立源码追踪：TS session.writeAccessUnit先advanceThroughAvailable维护PCR/PSI，再beginAccessUnit；后者dispatch-emit合同及payload边界已通过，真正条件为dispatch-max(emit,availableAt)<=0。availableAt由此前AU/maintenance提交推进，非“字段缺失”，当前无足够信息判定具体上游原因。r105仅补失败现场与最后commit类别，保留原失败语义，无增窗、降规格或时间戳重写。FFmpeg官方mpegtsenc同样明确DTS落后PCR会使TS无效：https://ffmpeg.org/doxygen/trunk/mpegtsenc_8c_source.html ，不能通过忽略错误冒充修复。

离线检查命令：`D:/mabs/local64/bin-video/ffprobe.exe -v error -show_entries format=duration,size:stream=codec_name,codec_type,width,height,sample_rate,channels -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r104-received.ts` exit0。VLC RC quit后无残留；记录后清理7日志、1TS、1SDP共9件，无PNG。指定源保留。


### r105：同规格确定复现与AU现场

Release全量684任务exit0，单文件诊断双独立PASS，未变调度。CLI11408自然exit1，四源PID27016/560/14912/31336，VLC30260。命令：
```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r105 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r105.sdp --video-codec h264 --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r105-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r105-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r105-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r105-source1.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r105-source2.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r105-source3.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r105-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r105- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r105-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

12:54:26.798确定复现，失败AU是audio：emit18409292239ns、dispatch18509292239ns、available/service_start18520947932ns、actual_now18643260800ns、remaining=-11655693ns、payload564B/wire576B、last_committed_kind=maintenance。此次最坏mux输入ready18643248900与失败AU同一emit/dispatch，明确该AU到mux时比emit晚233956661ns、比dispatch晚133956661ns；最近maintenance时间线超过该AU dispatch11.655693ms。尚不能凭这些值判定音频具体在哪个上游阶段被延迟。

最终workerErrors/errors1、stalls0、workers0；payload0B/2objects、18441预留/18439释放；CPU均值2.381279%峰11.568627%，WS峰1065664512B末326078464B；maxPayload51461792B/794objects，encoded6077/6076。诊断复现成功但功能验收FAIL，未开始恢复，不能记120秒通过。12:54:45源0已运行43.30秒；本项已失败，记录后按本轮四个精确PID核验身份并中止源，随后VLC RC quit，强制中止不算自然结束。源日志精确末帧/退出码及产物清理后补。

关键上游关联：同一失败音频dispatch18509292239在12:54:26.546已经是scheduler present head，now18391370100，早于emit18409292239约17.922139ms，却在missing=video/deadline=none状态等待。这排除该音频直到mux截止后才由AAC首次产出的解释，后续需量化scheduler成功提交与mux接收的差值。

[2026-10-05 12:54:25.785] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=17630023400 present_generation=1 present_canonical_target_ns=17300000000 present_canonical_dispatch_ns=17300000000 present_target_master_ns=17720947568 present_dispatch_master_ns=17720947568
[2026-10-05 12:54:25.785] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=17630513700 present_generation=1 present_canonical_target_ns=17322086167 present_canonical_dispatch_ns=17322086167 present_target_master_ns=17743033735 present_dispatch_master_ns=17743033735
[2026-10-05 12:54:26.013] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=17858360600 present_generation=1 present_canonical_target_ns=17533333333 present_canonical_dispatch_ns=17533333333 present_target_master_ns=17954280901 present_dispatch_master_ns=17954280901
[2026-10-05 12:54:26.014] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=17858743800 present_generation=1 present_canonical_target_ns=17554285714 present_canonical_dispatch_ns=17554285714 present_target_master_ns=17975233282 present_dispatch_master_ns=17975233282
[2026-10-05 12:54:26.315] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=18159935900 present_generation=1 present_canonical_target_ns=17833333333 present_canonical_dispatch_ns=17833333333 present_target_master_ns=18254280901 present_dispatch_master_ns=18254280901
[2026-10-05 12:54:26.316] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=18161458400 present_generation=1 present_canonical_target_ns=17856145124 present_canonical_dispatch_ns=17856145124 present_target_master_ns=18277092692 present_dispatch_master_ns=18277092692
[2026-10-05 12:54:26.545] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=18390557900 present_generation=1 present_canonical_target_ns=18066666666 present_canonical_dispatch_ns=18066666666 present_target_master_ns=18487614234 present_dispatch_master_ns=18487614234
[2026-10-05 12:54:26.546] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=18391370100 present_generation=1 present_canonical_target_ns=18088344671 present_canonical_dispatch_ns=18088344671 present_target_master_ns=18509292239 present_dispatch_master_ns=18509292239

四源最终因已失败测试清理exit=-1，日志最后媒体进度分别92.20/91.66/90.70/88.76秒，均非完整120秒自然完成；VLC已退出。19个内部漂移样本均0ns，不能当端到端同步。接收文件最终13820444B，未作音频恢复验收。r105共build/CLI双日志/4源/VLC共8日志、1TS、1SDP，10件产物记录后逐项删除，精确PID核验无残留。


### r106：scheduler成功提交时刻关联

两个诊断文件双独立PASS，Release全量重建684任务exit0。CLI6896，AV12712，V9876/13032/9820，VLC29764。同规格命令：
```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r106 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106.sdp --video-codec h264 --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-source1.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-source2.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-source3.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r106- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```


初始四源均完整120秒自然exit0。13:05:18和13:05:45使用screenshot技能获取VLC窗口，因显示缩放截图仅覆盖部分窗口，但可见四格布局内容变化；13:06:31可见区域全黑。完整收到的画面另在录制结束后离线抽帧核验，不把裁剪窗口当完整四格证据。复用原selectedAV命令启动PID32016，非短源循环：
```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-reentry.log 2>&1
```

系统截图命令：`C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1 -WindowHandle 5179258 -Mode temp`。临时路径：C:/Users/ROG/AppData/Local/Temp/codex-shot-2026-10-05_13-05-18.png；C:/Users/ROG/AppData/Local/Temp/codex-shot-2026-10-05_13-05-45.png；C:/Users/ROG/AppData/Local/Temp/codex-shot-2026-10-05_13-06-31.png，最终统一逐项清理。


r106结束与离线核验：初始四源及重入源32016均原规格完整120秒自然exit0；全源丢失后持续输出，最终向经进程身份和控制台成员核验的CLI6896发送Ctrl+C，CallerStop/exit0。不是源结束自动退出，也不是强杀。VLC29764经RC quit退出。本轮精确PID均已核验无残留。

最终workers/workerErrors/errors/stalls均0；CPU整机占比均值1.290530%、峰9.381663%；WS峰1056870400B、末309211136B；payload最终0B/0对象，203332预留=203332释放，峰51267227B/750对象；encoded109092推入=弹出。pressureFailures54860为背压计数，不是运行错误。generation2内部漂移约4407–4408ns，不代表接收端A/V同步。无late_output_commit或service-window失败日志；诊断未改变调度，因此本次未复现不能证明r104/r105已修复。

接收录制206368916B、324.185644秒，H2641280×720/30及AAC44100Hz双声道。离线60秒完整四格，135秒全黑，200秒仅右下格恢复，315秒全黑。silencedetect(-80dB、0.5秒)检出112.635283→158.856032及277.309546→324.185646静音区间，支持失源/恢复内容变化；不等于逐样本零值、选定音频内容一致或端到端同步通过。

音频13965包、视频9696包；各出现一次PTS回退：音频第49包1.491322→1.436211秒(-4960/90000)，视频第2包1.447978→1.402978秒(-4050/90000)。均在启动阶段；VLC日志另有playback too late(85880)和resampling stopped(drift:-8906us)。录制经VLC mux=ts重新封装，不能据此判断生产线端PTS回退或声称连续性PASS；后续需原始线端证据。完整功能仍FAIL。

离线命令均在实时进程结束后运行，exit0：
```powershell
& D:/mabs/local64/bin-video/ffprobe.exe -v error -show_entries format=start_time,duration,size:stream=index,codec_name,codec_type,width,height,sample_rate,channels,time_base,start_time,duration -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-received.ts > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-probe.json
& D:/mabs/local64/bin-video/ffprobe.exe -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time,dts_time,duration_time -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-received.ts > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-packets.json
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-received.ts -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-silence.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 60 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-received.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-frame-60.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 135 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-received.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-frame-135.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 200 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-received.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-frame-200.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 315 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-received.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r106-frame-315.png
```
定向停止实际命令：
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R100Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$target=6896
$p=Get-CimInstance Win32_Process -Filter "ProcessId=$target"
if($p.Name -ne 'media_transcode_realtime_video_cli.exe' -or $p.CommandLine -notlike '*composition-wiring-r106*'){throw 'PID identity mismatch'}
[R100Console]::FreeConsole() | Out-Null
if(-not [R100Console]::AttachConsole($target)){throw 'AttachConsole failed'}
try {
if(-not [R100Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$ids=[uint32[]]::new(32)
$count=[R100Console]::GetConsoleProcessList($ids,32)
if($count -ne 2 -or $ids[0..($count-1)] -notcontains $target -or $ids[0..($count-1)] -notcontains $PID){throw "Unexpected console members: $ids"}
if(-not [R100Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R100Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'

```

本轮清理清单（18件仓库产物及3件临时截图；原120秒源保留）：
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-build.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-cli.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-cli-error.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-source0.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-source1.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-source2.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-source3.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-reentry.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-vlc.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-received.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-probe.json
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-packets.json
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-silence.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-frame-60.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-frame-135.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-frame-200.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106-frame-315.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r106.sdp
- C:/Users/ROG/AppData/Local/Temp/codex-shot-2026-10-05_13-05-18.png
- C:/Users/ROG/AppData/Local/Temp/codex-shot-2026-10-05_13-05-45.png
- C:/Users/ROG/AppData/Local/Temp/codex-shot-2026-10-05_13-06-31.png

上述21件逐项清理完成、清单残留0；本轮未运行远程测试。原源文件124427809B保留。

最终双独立复核：rtp_provenance_review_a与rtp_provenance_review_b均明确诊断/证据归档切片PASS，完整合屏目标FAIL。复核范围为此前已审源码与归档一致性；临时媒体已清理，未声称重新分析原始媒体。


### r107：原规格复跑与唤醒路径复核

未修改调度或编码设置，复用r106 Release。CLI8692；FFmpeg17064/32048/11244/6112；VLC12912。原四路RTP/RTCP H264源→H264 CBR8000kbps1280×720/30、AAC CBR192kbps→MPEG-TS/RTP。实际命令：
```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r107 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r107.sdp --video-codec h264 --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r107-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r107-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r107-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r107-source1.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r107-source2.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r107-source3.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r107-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r107- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r107-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```
独立源码调查：worker在process之前取通知序号，等待前再比较；channel每次发布通知consumer，未发现lost wake。scheduler已有audio head时不再pop audio，缺video返回普通Waiting；新audio通知导致再次检查，不等于队列非空自身忙等。missing_media_wait仅缺失类型变化才记录，不能把约230ms日志间隔当视频批量周期。下一诊断需区分视频编码包产出与成功发布、scheduler取head时间。

r107结果：四个源完整120秒自然exit0；源停后CLI继续运行，定向Ctrl+C后CallerStop/exit0，非源结束自动退出。VLC RC quit，精确六PID已核验无残留。诊断未复现，不能证明r104/r105已修复。

最终workers/errors/workerErrors/stalls均0，CPU整机占比均值1.660563%峰10.583153%；WS峰1072959488B末288235520B；payload0B/0对象，117897预留=释放，峰51745780B/687对象；encoded47417推入/47416弹出，取消不等于完整排空。AAC关闭时仍有2帧、filter属性变化警告保留；接收文件101303424B。本项仅失败复现诊断，没有进行恢复/接收音频/线端连续性完整验收，不记功能成功项。

停止实际命令：
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R100Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$target=8692
$p=Get-CimInstance Win32_Process -Filter "ProcessId=$target"
if($p.Name -ne 'media_transcode_realtime_video_cli.exe' -or $p.CommandLine -notlike '*composition-wiring-r107*'){throw 'PID identity mismatch'}
[R100Console]::FreeConsole() | Out-Null
if(-not [R100Console]::AttachConsole($target)){throw 'AttachConsole failed'}
try {
if(-not [R100Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$ids=[uint32[]]::new(32)
$count=[R100Console]::GetConsoleProcessList($ids,32)
if($count -ne 2 -or $ids[0..($count-1)] -notcontains $target -or $ids[0..($count-1)] -notcontains $PID){throw "Unexpected console members: $ids"}
if(-not [R100Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R100Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'

```
补充只读调查：本地FFmpeg nvenc.c nvenc_recalc_surfaces会将async_depth限制为nb_surfaces-1；nvenc_h264.c的delay AVOption映射async_depth。项目applyLowLatency仅设置AV_CODEC_FLAG_LOW_DELAY，通用readback记录c.delay不含此专用值。这是观测缺口，不能直接证明本次故障由编码器缓冲造成，也未修改外部依赖或设置。官方资料：https://ffmpeg.org/doxygen/trunk/nvenc__h264_8c_source.html ；https://docs.nvidia.com/video-technologies/video-codec-sdk/13.0/ffmpeg-with-nvidia-gpu/index.html 。后续需实际AU时刻和生效配置证据，不能凭日志间隔猜batch或调参数。

清理清单（out/acceptance下逐项删除）：
- composition-wiring-r107-cli.log
- composition-wiring-r107-cli-error.log
- composition-wiring-r107-source0.log
- composition-wiring-r107-source1.log
- composition-wiring-r107-source2.log
- composition-wiring-r107-source3.log
- composition-wiring-r107-vlc.log
- composition-wiring-r107-received.ts
- composition-wiring-r107.sdp

9件清理完成、残留0，原120秒源保留；无远程测试，无截图生成。


### r108：代际一致的head到达诊断

logMediaHeadArrival扩展原首head诊断，只在同一playbackEpoch值快照的generation匹配且now>=mapped dispatch时记录后续late head。首次审查B指出旧/未来代误映射风险，已修正，A/B均明确PASS；不改变实际调度映射、错误及发布。两次Release全量684任务exit0，第二次包含修正。取head后采样不是入队/编码产包时刻；无late日志不证明emit前已到，也不能当功能修复。

CLI6148，四源17380/32040/32156/11832，VLC13532。实际命令：
```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r108 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r108.sdp --video-codec h264 --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r108-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r108-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r108-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r108-source1.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r108-source2.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r108-source3.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r108-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r108- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r108-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

只读审查新证据：MediaMpegTsOutputTimingPlanner.cpp startupEmissionPreroll=min(transportDecodeLead,max(videoCadence,audioCadence,pcrInterval))，ProtocolOutputPlanner将transportDecodeLead+preroll传为activation lead，AggregatePlanner取最大值作为持续preparationLead。未独立覆盖合成/编码/发布耗时；maximumEncoderRetainedFrames=max_b_frames+1是容量事实，deployment.encode().latency.targetResidence源于wire burst/pacing网络预算，均不可直接冒充处理时间上界。确定是预算语义缺口，尚非r105直接根因证明；禁止增加经验提前量。

r108结果：四源均完整120秒自然exit0；全源结束后持续输出，定向Ctrl+C后CLI CallerStop/exit0，VLC RC quit，精确六PID核验无残留。late head、late_output_commit、service-window失败均0；本次未复现，不证明r104/r105修复。内部raw drift约19759–19760ns，仅内部时钟证据。最终payload0B/0对象、116285预留=释放；encoded45535推入/45534弹出，AAC关闭2帧残留，不宣称排空。接收文件101061656B，本项未做完整画面/恢复/音频/同步验收，不标成功测试项。

实际定向停止命令：
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R100Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$target=6148
$p=Get-CimInstance Win32_Process -Filter "ProcessId=$target"
if($p.Name -ne 'media_transcode_realtime_video_cli.exe' -or $p.CommandLine -notlike '*composition-wiring-r108*'){throw 'PID identity mismatch'}
[R100Console]::FreeConsole() | Out-Null
if(-not [R100Console]::AttachConsole($target)){throw 'AttachConsole failed'}
try {
if(-not [R100Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$ids=[uint32[]]::new(32)
$count=[R100Console]::GetConsoleProcessList($ids,32)
if($count -ne 2 -or $ids[0..($count-1)] -notcontains $target -or $ids[0..($count-1)] -notcontains $PID){throw "Unexpected console members: $ids"}
if(-not [R100Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R100Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'

```

[CLI] final runtime report: queued=0, peakQueued=1001, threads=96, processThreads=154, workers=0, workerProgress=511421, workerProcessCalls=1187754, workerWaits=676321, workerWakeups=666898, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=461, logicalProcessors=22, averageSystemMachineCpuPercent=14.425144, averageProcessMachineCpuPercent=1.635224, peakProcessMachineCpuPercent=9.887640, averageProcessSingleCoreCpuPercent=35.974934, peakProcessSingleCoreCpuPercent=217.528090, initialWorkingSetBytes=1062584320, workingSetBytes=281329664, peakWorkingSetBytes=1062584320, totalPushed=317472, totalPopped=317471, droppedBuffers=0, encodedPacketsPushed=45535, encodedPacketsPopped=45534, backpressureItems=145, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=50895251, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=754, graphPayloadReservations=116285, graphPayloadReleases=116285, graphPayloadPressureFailures=37621, lastInputReceivedAtNs=11632409026800

清理清单（out/acceptance下逐项删除）：
- composition-wiring-r108-build.log
- composition-wiring-r108-reviewed-build.log
- composition-wiring-r108-cli.log
- composition-wiring-r108-cli-error.log
- composition-wiring-r108-source0.log
- composition-wiring-r108-source1.log
- composition-wiring-r108-source2.log
- composition-wiring-r108-source3.log
- composition-wiring-r108-vlc.log
- composition-wiring-r108-received.ts
- composition-wiring-r108.sdp

11件已清理、残留0，原120秒源保留；无远程测试，无截图生成。


### r109：NVENC立即输出契约与线端重入验证

内部Immediate/BackendManaged产品由planner选择，NVENC低延迟映射官方delay=0/zerolatency=1，主capability probe、替代packet-layout probe、prepared及生产builder共用应用与打开后计划值校验（含lookahead/B帧无冲突）。不改变preset/tune/分辨率/码率/帧率，不改FFmpeg依赖及公共参数。主probe漏verify经双审指出后修正。首次Release编译FAIL C2664（avcodec_is_open参数非const），核对本地官方实现只读internal后在adapter查询处局部const_cast；双独立复审PASS，第二次Release全量684任务exit0。源码修复低延迟契约缺口，不证明r105直接根因已关闭。

实际CLI15192、初始FFmpeg4668/23204/3172/6784、VLC8748。命令：
```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r109 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109.sdp --video-codec h264 --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-source1.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-source2.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-source3.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r109- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```
启动两次回读均verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0。首video head generation/mapping_epoch_generation1，dispatch376325468、now224772400ns，提前151553068ns；仅单次启动观测，不是耗时上界或对照benchmark。

初始四源完整120秒自然exit0，首AV源日志最后写入13:43:46。VLC截图命令：`C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1 -WindowHandle 4917078 -Path D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-live.png`；已观察到四格内容，窗口截图受缩放裁剪，非完整画面边界证据。

被动抓包PID29296在13:43:49启动，晚于首次源结束3秒，不覆盖首次失源边界；覆盖全黑、selectedAV重入及再次失源。命令：
```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp dst port 62020" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-capture.log 2>&1
```
selectedAV重入PID22828，13:44:20启动，仍原120秒源，无循环：
```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-reentry.log 2>&1
```

重入源完整120秒自然exit0。dumpcap自然exit0，94048包/工具报告drop0；CLI经定向Ctrl+C CallerStop/exit0，VLC RC quit。精确8个PID核验无残留。停止命令：
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R100Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$target=15192
$p=Get-CimInstance Win32_Process -Filter "ProcessId=$target"
if($p.Name -ne 'media_transcode_realtime_video_cli.exe' -or $p.CommandLine -notlike '*composition-wiring-r109*'){throw 'PID identity mismatch'}
[R100Console]::FreeConsole() | Out-Null
if(-not [R100Console]::AttachConsole($target)){throw 'AttachConsole failed'}
try {
if(-not [R100Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$ids=[uint32[]]::new(32)
$count=[R100Console]::GetConsoleProcessList($ids,32)
if($count -ne 2 -or $ids[0..($count-1)] -notcontains $target -or $ids[0..($count-1)] -notcontains $PID){throw "Unexpected console members: $ids"}
if(-not [R100Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R100Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'

```

原始线端证据：抓包首末epoch1791179029.493780600→1791179209.477939100；94048个RTP包、一SSRC，无序号缺口；原样剥RTP头拼接106444848B TS，TS CC无缺口、discontinuity flags0。ffprobe：H2641280×720/30，AAC44100Hz双声道，180.000000秒。视频5400包PTS116.766667→296.733333，5399步均3000/90000秒；音频7751包PTS116.773156→296.727800，1582步2089、6168步2090；均PTS=DTS，无重复或回退。独立PES头计数stream_id224=5400/192=7751与ffprobe完全一致。PCR PID257共2700个，步长1800000/1800001（27MHz），无重复/回退。以上仅覆盖抓包期间黑场/重入/再次失源，不覆盖首次源结束边界或整轮输出；未用VLC remux时间戳替代线端。

离线接收文件抽帧60/135/210/290秒分别完整四格、全黑、仅右下恢复、全黑。原始TS静音检测-80dB/0.5秒：0→33.757619与152.210998→179.977868秒静音；支持失源静音及恢复有音频，不是逐样本零值或端到端内容同步证明。抓包从已有流中途开始，ffprobe/离线解码初期出现non-existing PPS/no frame，记录保留；全部PES计数匹配，不隐藏该采样起点局限。tshark stdout另附一行既有“No matching RTP/H.264 payload dumped.”通知；提取明确登记此非包行，实际94048数据行与dumpcap计数一致，没有忽略媒体包。

离线实际命令均exit0，全部在CLI/VLC结束后运行；Python通过-c内联，无测试脚本文件：
```powershell
& D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-wire.pcapng -T fields -e frame.time_epoch -e udp.payload > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-wire.tsv 2> D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-tshark.log
& D:/Python/python.exe -c 'from pathlib import Path
import json
root=Path("D:/Code/MyCode/MediaTranscode/out/acceptance")
count=0; previous={}; gaps=[]; cc={}; cc_errors=[]; notices=[]; discontinuities=0; ts_bytes=0; first=None; last=None
with (root/"composition-wiring-r109-wire.ts").open("wb") as out:
 for line in (root/"composition-wiring-r109-wire.tsv").open(encoding="utf-8-sig"):
  if not line.strip(): continue
  if line.strip()=="No matching RTP/H.264 payload dumped.":
   notices.append(line.strip()); continue
  stamp,hexdata=line.strip().split("\t")
  data=bytes.fromhex(hexdata.replace(":",""))
  assert len(data)>=12 and data[0]>>6==2 and data[1]&127==33
  seq=int.from_bytes(data[2:4],"big"); ssrc=int.from_bytes(data[8:12],"big")
  if ssrc in previous and (seq-previous[ssrc])%65536!=1: gaps.append([count,previous[ssrc],seq])
  previous[ssrc]=seq
  offset=12+4*(data[0]&15)
  if data[0]&16:
   assert len(data)>=offset+4
   offset+=4+4*int.from_bytes(data[offset+2:offset+4],"big")
  end=len(data)-data[-1] if data[0]&32 else len(data)
  payload=data[offset:end]
  assert payload and len(payload)%188==0
  for pos in range(0,len(payload),188):
   ts=payload[pos:pos+188]; assert ts[0]==71 and not ts[1]&128
   pid=((ts[1]&31)<<8)|ts[2]; afc=(ts[3]>>4)&3; counter=ts[3]&15
   assert afc
   reset=bool(afc&2 and ts[4] and ts[5]&128)
   if reset: discontinuities+=1; cc.pop(pid,None)
   if afc&1 and pid!=8191:
    if pid in cc and counter!=(cc[pid]+1)%16: cc_errors.append([count,pid,cc[pid],counter])
    cc[pid]=counter
  out.write(payload); ts_bytes+=len(payload); count+=1
  if first is None: first=stamp
  last=stamp
report=dict(rtp_packets=count,ssrc_count=len(previous),rtp_sequence_gaps=gaps,ts_continuity_errors=cc_errors,ts_discontinuity_flags=discontinuities,ts_bytes=ts_bytes,first_epoch=first,last_epoch=last,non_packet_notices=notices)
assert count==94048
print(json.dumps(report))
'
& D:/mabs/local64/bin-video/ffprobe.exe -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time,dts_time,duration_time -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-wire.ts > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-wire-packets.json 2> D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-wire-probe.log
& D:/Python/python.exe -c 'import json
from collections import Counter
from pathlib import Path
p=json.loads(Path("out/acceptance/composition-wiring-r109-wire-packets.json").read_text(encoding="utf-8-sig"))["packets"]
for s in sorted(set(x["stream_index"] for x in p)):
 a=[x for x in p if x["stream_index"]==s and "pts" in x]
 d=[int(y["pts"])-int(x["pts"]) for x,y in zip(a,a[1:])]
 print(json.dumps(dict(stream=s,packets=len(a),first=a[0]["pts_time"],last=a[-1]["pts_time"],pts_steps=dict(Counter(d)),negative_steps=sum(v<0 for v in d),zero_steps=sum(v==0 for v in d),pts_dts_differences=sum(x.get("pts")!=x.get("dts") for x in a))))
'
& D:/Python/python.exe -c 'from pathlib import Path
from collections import Counter
import json
counts=Counter(); pcrs={}
with Path("out/acceptance/composition-wiring-r109-wire.ts").open("rb") as f:
 while (ts:=f.read(188)):
  assert len(ts)==188 and ts[0]==71
  pid=((ts[1]&31)<<8)|ts[2]; afc=(ts[3]>>4)&3; offset=4
  if afc&2:
   length=ts[4]; offset=5+length
   if length>=7 and ts[5]&16:
    b=ts[6:12]; base=(b[0]<<25)|(b[1]<<17)|(b[2]<<9)|(b[3]<<1)|(b[4]>>7); ext=((b[4]&1)<<8)|b[5]
    assert ext<300
    pcrs.setdefault(pid,[]).append(base*300+ext)
  if afc&1 and ts[1]&64 and ts[offset:offset+3]==b"\x00\x00\x01":
   counts[ts[offset+3]]+=1
print("PES_stream_id_counts",dict(counts))
for pid,a in pcrs.items():
 modulus=(1<<33)*300
 d=[((y-x+modulus//2)%modulus)-modulus//2 for x,y in zip(a,a[1:])]
 print(json.dumps(dict(pcr_pid=pid,count=len(a),negative=sum(x<0 for x in d),zero=sum(x==0 for x in d),minimum_step_ticks=min(d),maximum_step_ticks=max(d),clock_hz=27000000)))
'
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 60 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-received.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-frame-60.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 135 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-received.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-frame-135.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 210 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-received.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-frame-210.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 290 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-received.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-frame-290.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-wire.ts -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-silence.log 2>&1
& D:/mabs/local64/bin-video/ffprobe.exe -v error -show_entries stream=index,codec_name,codec_type,width,height,r_frame_rate,sample_rate,channels:format=duration,size -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-wire.ts 2>> D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r109-wire-probe.log
```

[CLI] final runtime report: queued=0, peakQueued=1019, threads=96, processThreads=155, workers=0, workerProgress=777758, workerProcessCalls=1994147, workerWaits=1216377, workerWakeups=1179968, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1097, logicalProcessors=22, averageSystemMachineCpuPercent=12.457869, averageProcessMachineCpuPercent=1.353000, peakProcessMachineCpuPercent=11.060948, averageProcessSingleCoreCpuPercent=29.766001, peakProcessSingleCoreCpuPercent=243.340858, initialWorkingSetBytes=1071427584, workingSetBytes=316661760, peakWorkingSetBytes=1071427584, totalPushed=538103, totalPopped=538102, droppedBuffers=0, encodedPacketsPushed=107763, encodedPacketsPopped=107762, backpressureItems=145, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=51962994, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=768, graphPayloadReservations=202418, graphPayloadReleases=202418, graphPayloadPressureFailures=55020, lastInputReceivedAtNs=12593543925900

最终补充：generation1内部raw drift112样本155–156ns，generation2共117样本2203–2204ns；late head/late commit/service-window失败均0。AAC关闭2帧残留保留，不将CallerStop当完整排空。
双独立运行复核均PASS（有限切片）：A独立逐字节确认TS与94048个RTP有效负载完全一致，并重算PTS；B独立检查回读、资源报告、packet JSON和四张图片。两者均明确完整合屏FAIL、r105因果根因未闭合。

本轮25件清理清单：
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-build.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-rebuild.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-cli.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-cli-error.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-source0.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-source1.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-source2.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-source3.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-vlc.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-received.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-reentry.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-capture.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-wire.pcapng
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-wire.tsv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-tshark.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-wire.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-wire-packets.json
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-wire-probe.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-silence.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-live.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-frame-60.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-frame-135.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-frame-210.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109-frame-290.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r109.sdp

25件已逐项清理，清单残留0；指定原源124427809B保留。本轮无远程测试、无新测试脚本，无推送/合并/部署。

## 2026-10-05 r110：四路 RTP/RTCP → HEVC CBR 8Mbps 1280×720/30 + AAC CBR192kbps → MPEG-TS/RTP

沿用r109 Release（13:41:22，5609984B），本轮无源码修改。原指定120秒源，四路V/V/V/AV、选定index3，首轮和选定AV重入均完整120秒自然exit0。CLI7504，首轮源10468/28804/26304/8772，VLC8428，dumpcap22680；重入源会话43350自然exit0但未及时记录精确PID，此项证据不足。源结束后持续输出黑场；定向Ctrl+C后CallerStop/exit0，不是源结束自动退出。VLC经RC退出、capture330秒自然exit0，当前相关进程残留0。

- capture182888包/drop0，1SSRC，RTP序号缺口0、TS CC错误0、discontinuity0。原始TS213738704B；媒体线端epoch1791179770.627380200至1791180068.510050900。tshark插件尾行明确作为非packet提示排除，包数完整匹配。
- 视频8935包，PTS=DTS，0至297.8秒，步长3000；音频12826包，PTS=DTS，0至297.795922秒，步长2089/2090。PES计数完全一致。PCR PID257共4469个，27MHz步长1800000/1800001，无重复/回退。
- 原始TS在60/130/210/280秒离线帧分别为四格、黑场、右下格、黑场。live截图仅局部窗口，不作为全画幅证据。
- final errors/workerErrors/stalledIntervals=0，workers=0，queued=0；CPU机器口径平均1.564269%、峰9.318182%；WS峰1029341184B、末317005824B。payload最终0B/0对象、203585预留=释放；encodedPackets109217推入=取出。55078次credit压力计数是背压观测，不隐去。AAC关闭仍有2帧残留，CallerStop不等于完整排空。
- 两次Immediate选项回读成功；内部drift共230样本raw155–2204ns，不能替代内容A/V对齐；无late head/late commit/transport service window失败。本次未复现r105，不等于因果修复。
- 原源/线端音频解码后分别960000/2382553个8000Hz分析样本，ashowinfo逐块PTS连续且样本总数匹配。原源3600帧、线端8935帧，以实际showinfo timebase/PTS匹配，不用帧索引猜时间。64×36灰度与8000Hz仅离线分析产品，实时规格未降低，也未重复扣除decoder已处理的priming。

### 内容匹配结论：UNKNOWN

5帧图像MSE和2秒音频归一化互相关给出相隔数十秒的近似同分候选。20秒图像最佳源20.033333、次77.9秒，MSE0.443576/0.448524；音频最佳36.558625、次86.158625秒，相关0.997359/0.997149。100秒图像最佳42.166667、次100.033333秒，MSE0.343229/0.343316。其余170/210/250秒亦未形成全程唯一音频匹配。不能把最佳峰相减所得数十秒偏差称为实际A/V漂移，也不能按预期位置挑峰宣称同步通过。后续需在原源寻找唯一且连续一致的内容锚点；若无则保留证据缺口。

方法依据：SciPy官方correlate（https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.correlate.html）与FFmpeg ashowinfo（https://ffmpeg.org/ffmpeg-filters.html#ashowinfo）。未人为设同步通过阈值。本轮不是物理屏幕/声卡延迟测量。

### 复审与仍存边界

A/B独立只读复审均对有限运行/连续性/回收PASS；完整合屏仍FAIL，内容同步UNKNOWN，重入PID记录不足。AVOption查询不是GPU最终配置查询；delay=0移除FFmpeg软件积攒门槛，不保证GPU即时就绪。preset rc-lookahead可能保留正值而实际关闭，当前校验可保守拒绝；不声称已验证全部preset或LL/ULL最终配置。r105因果闭环、完整矩阵、共享单路回归、长稳、C API与RKMPP仍待完成；保留75/100，不提高评分。本轮不推送、合并、部署。

### 实际命令（均本机直接执行；非保存测试脚本）

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp dst port 62020" -a duration:330 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-capture.log 2>&1
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r110 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-source1.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-source2.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-source3.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r110- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-reentry.log 2>&1
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R100Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$target=7504
$p=Get-CimInstance Win32_Process -Filter "ProcessId=$target"
if($p.Name -ne 'media_transcode_realtime_video_cli.exe' -or $p.CommandLine -notlike '*composition-wiring-r110*'){throw 'PID identity mismatch'}
[R100Console]::FreeConsole() | Out-Null
if(-not [R100Console]::AttachConsole($target)){throw 'AttachConsole failed'}
try {
if(-not [R100Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$ids=[uint32[]]::new(32)
$count=[R100Console]::GetConsoleProcessList($ids,32)
if($count -ne 2 -or $ids[0..($count-1)] -notcontains $target -or $ids[0..($count-1)] -notcontains $PID){throw "Unexpected console members: $ids"}
if(-not [R100Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R100Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'

& D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire.pcapng -T fields -e frame.time_epoch -e udp.payload > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire.tsv 2> D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-tshark.log
& D:/Python/python.exe -c 'from pathlib import Path
import json
root=Path("D:/Code/MyCode/MediaTranscode/out/acceptance")
count=0; previous={}; gaps=[]; cc={}; cc_errors=[]; notices=[]; discontinuities=0; ts_bytes=0; first=None; last=None
with (root/"composition-wiring-r110-wire.ts").open("wb") as out:
 for line in (root/"composition-wiring-r110-wire.tsv").open(encoding="utf-8-sig"):
  if not line.strip(): continue
  if line.strip()=="No matching RTP/H.264 payload dumped.":
   notices.append(line.strip()); continue
  stamp,hexdata=line.strip().split("\t")
  data=bytes.fromhex(hexdata.replace(":",""))
  assert len(data)>=12 and data[0]>>6==2 and data[1]&127==33
  seq=int.from_bytes(data[2:4],"big"); ssrc=int.from_bytes(data[8:12],"big")
  if ssrc in previous and (seq-previous[ssrc])%65536!=1: gaps.append([count,previous[ssrc],seq])
  previous[ssrc]=seq
  offset=12+4*(data[0]&15)
  if data[0]&16:
   assert len(data)>=offset+4
   offset+=4+4*int.from_bytes(data[offset+2:offset+4],"big")
  end=len(data)-data[-1] if data[0]&32 else len(data)
  payload=data[offset:end]
  assert payload and len(payload)%188==0
  for pos in range(0,len(payload),188):
   ts=payload[pos:pos+188]; assert ts[0]==71 and not ts[1]&128
   pid=((ts[1]&31)<<8)|ts[2]; afc=(ts[3]>>4)&3; counter=ts[3]&15
   assert afc
   reset=bool(afc&2 and ts[4] and ts[5]&128)
   if reset: discontinuities+=1; cc.pop(pid,None)
   if afc&1 and pid!=8191:
    if pid in cc and counter!=(cc[pid]+1)%16: cc_errors.append([count,pid,cc[pid],counter])
    cc[pid]=counter
  out.write(payload); ts_bytes+=len(payload); count+=1
  if first is None: first=stamp
  last=stamp
report=dict(rtp_packets=count,ssrc_count=len(previous),rtp_sequence_gaps=gaps,ts_continuity_errors=cc_errors,ts_discontinuity_flags=discontinuities,ts_bytes=ts_bytes,first_epoch=first,last_epoch=last,non_packet_notices=notices)
assert count==182888
print(json.dumps(report))
'
& D:/mabs/local64/bin-video/ffprobe.exe -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time,dts_time,duration_time -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire.ts > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire-packets.json 2> D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire-probe.log
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -copyts -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "scale=64:36:flags=area,format=gray,showinfo" -fps_mode passthrough -c:v rawvideo -f rawvideo D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-source.gray > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-source-video.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -copyts -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:a:0 -vn -af "pan=mono|c0=c0,aresample=8000,ashowinfo" -c:a pcm_f32le -f f32le D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-source.f32 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-source-audio.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -copyts -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire.ts -map 0:v:0 -an -vf "crop=640:360:640:360,scale=64:36:flags=area,format=gray,showinfo" -fps_mode passthrough -c:v rawvideo -f rawvideo D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire.gray > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire-video.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -copyts -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire.ts -map 0:a:0 -vn -af "pan=mono|c0=c0,aresample=8000,ashowinfo" -c:a pcm_f32le -f f32le D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire.f32 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire-audio.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 60 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-frame-60.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 130 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-frame-130.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 210 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-frame-210.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 280 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-wire.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r110-frame-280.png
& D:/Python/python.exe -c 'import re,json,numpy as np
from pathlib import Path
from scipy.signal import correlate
base=Path(''out/acceptance/composition-wiring-r110-'')
def path(s): return Path(str(base)+s)
def vid(k):
 log=path(k+''-video.log'').read_text(encoding=''utf-8-sig'')
 tb=re.search(r''config in time_base: (\d+)/(\d+)'',log); scale=int(tb[1])/int(tb[2])
 pts=np.array([int(x) for x in re.findall(r'' n:\s*\d+ pts:\s*(-?\d+) pts_time:'',log)])*scale
 frames=np.fromfile(path(k+''.gray''),np.uint8).reshape(-1,2304).astype(np.float64)
 assert len(pts)==len(frames)
 return pts,frames
def aud(k):
 log=path(k+''-audio.log'').read_text(encoding=''utf-8-sig'')
 rows=[tuple(map(int,x)) for x in re.findall(r'' n:\d+ pts:(-?\d+) pts_time:.*? rate:(\d+) nb_samples:(\d+)'',log)]
 a=np.fromfile(path(k+''.f32''),np.float32).astype(np.float64)
 assert len(a)==sum(x[2] for x in rows)
 gaps=[(i,y[0]-x[0]-x[2]) for i,(x,y) in enumerate(zip(rows,rows[1:])) if y[0]!=x[0]+x[2]]
 print(k,''audio_samples'',len(a),''first_pts'',rows[0][0],''gaps'',gaps[:20],''gap_count'',len(gaps))
 assert not gaps and all(x[1]==8000 for x in rows)
 return rows[0][0]/8000,a
sp,s=vid(''source''); wp,w=vid(''wire''); sa0,sa=aud(''source'');wa0,wa=aud(''wire'')
print(''video_frames'',len(s),len(w),''first_last_pts'',sp[[0,-1]],wp[[0,-1]])
sn=(s*s).sum(axis=1)
for target in [20,60,100,170,210,250]:
 i=int(np.argmin(abs(wp-target))); q=w[i-2:i+3]
 costs=sum((sn[j:j+len(s)-4]+(q[j]*q[j]).sum()-2*s[j:j+len(s)-4]@q[j]) for j in range(5))/(5*2304)
 order=np.argsort(costs); best=int(order[0]); second=next(int(x) for x in order if abs(int(x)-best)>1)
 t=float(wp[i]); start=int(round((t-1-wa0)*8000)); template=wa[start:start+16000]; template=template-template.mean()
 corr=correlate(sa,template,mode=''valid'',method=''fft'')
 cs=np.r_[0,np.cumsum(sa)]; cq=np.r_[0,np.cumsum(sa*sa)]; n=len(template)
 var=cq[n:]-cq[:-n]-(cs[n:]-cs[:-n])**2/n
 scores=corr/np.sqrt(np.maximum(var,1e-30)*np.dot(template,template))
 ai=int(np.argmax(scores)); mask=scores.copy();mask[max(0,ai-267):ai+268]=-np.inf; aj=int(np.argmax(mask))
 ap=sa0+ai/8000+(t-(wa0+start/8000));vp=float(sp[best+2])
 print(json.dumps(dict(output_pts=t,source_video_pts=vp,video_mse=float(costs[best]),video_runner_pts=float(sp[second+2]),video_runner_mse=float(costs[second]),source_audio_pts=ap,audio_correlation=float(scores[ai]),audio_runner_pts=sa0+aj/8000+1,audio_runner_correlation=float(scores[aj]),audio_minus_video_ms=(ap-vp)*1000)))
'
& D:/Python/python.exe -c 'import json
from collections import Counter
from pathlib import Path
p=json.loads(Path("out/acceptance/composition-wiring-r110-wire-packets.json").read_text(encoding="utf-8-sig"))["packets"]
for s in sorted(set(x["stream_index"] for x in p)):
 a=[x for x in p if x["stream_index"]==s and "pts" in x]
 d=[int(y["pts"])-int(x["pts"]) for x,y in zip(a,a[1:])]
 print(json.dumps(dict(stream=s,packets=len(a),first=a[0]["pts_time"],last=a[-1]["pts_time"],pts_steps=dict(Counter(d)),negative_steps=sum(v<0 for v in d),zero_steps=sum(v==0 for v in d),pts_dts_differences=sum(x.get("pts")!=x.get("dts") for x in a))))
'
& D:/Python/python.exe -c 'from pathlib import Path
from collections import Counter
import json
counts=Counter(); pcrs={}
with Path("out/acceptance/composition-wiring-r110-wire.ts").open("rb") as f:
 while (ts:=f.read(188)):
  assert len(ts)==188 and ts[0]==71
  pid=((ts[1]&31)<<8)|ts[2]; afc=(ts[3]>>4)&3; offset=4
  if afc&2:
   length=ts[4]; offset=5+length
   if length>=7 and ts[5]&16:
    b=ts[6:12]; base=(b[0]<<25)|(b[1]<<17)|(b[2]<<9)|(b[3]<<1)|(b[4]>>7); ext=((b[4]&1)<<8)|b[5]
    assert ext<300
    pcrs.setdefault(pid,[]).append(base*300+ext)
  if afc&1 and ts[1]&64 and ts[offset:offset+3]==b"\x00\x00\x01":
   counts[ts[offset+3]]+=1
print("PES_stream_id_counts",dict(counts))
for pid,a in pcrs.items():
 modulus=(1<<33)*300
 d=[((y-x+modulus//2)%modulus)-modulus//2 for x,y in zip(a,a[1:])]
 print(json.dumps(dict(pcr_pid=pid,count=len(a),negative=sum(x<0 for x in d),zero=sum(x==0 for x in d),minimum_step_ticks=min(d),maximum_step_ticks=max(d),clock_hz=27000000)))
'
```

本轮30件清理清单（记录后逐项删除）：
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-capture.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-cli-error.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-cli.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-frame-130.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-frame-210.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-frame-280.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-frame-60.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-live.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-received.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-reentry.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-source-audio.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-source-video.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-source.f32
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-source.gray
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-source0.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-source1.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-source2.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-source3.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-tshark.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-vlc.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-wire-audio.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-wire-packets.json
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-wire-probe.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-wire-video.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-wire.f32
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-wire.gray
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-wire.pcapng
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-wire.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110-wire.tsv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r110.sdp

30件已逐项清理，残留0，指定源124427809B保留；相关媒体进程残留0，无本轮远程产物。

## 2026-10-05 r111：共享单路 RTP H.264/AAC → HEVC CBR8Mbps 1280×720/30、AAC CBR192kbps → MPEG-TS/RTP

沿用r109 Release，原120秒源，无源码变更。CLI21648、FFmpeg16380、VLC8640。源3600帧/120秒自然exit0；两张VLC截图14:09:58和14:10:42内容更新。源结束后CLI于14:11:38自然exit1，NotInitialized: realtime runtime made no progress before timeout；不作CallerStop或优雅EOF。VLC身份核验后RC quit，三PID已无残留。

两次Immediate校验成功；118条内部drift raw_phase_ns=0（不是内容同步）。final errors/workerErrors0、stalledIntervals1、workers0、queued0；encoded39995/39995，payload71867预留=释放、最终0B/0对象、pressure0。CPU机器口径平均1.099536%、峰3.881279%；WS峰196812800B、末191791104B。保留既有ProgressTimeout失败，未取得新编码契约导致该失败的证据；有限运行/回收PASS，完整单路门禁FAIL。

实际命令：
```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r111 --input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r111.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r111-cli.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r111-source0.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r111-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r111- --snapshot-format=png rtp://@127.0.0.1:62020
```

VLC RC连接127.0.0.1:63020，分别发送snapshot换行（两次）及quit换行；截图均已人工查看。本轮没有抓包、录制、测试脚本或远程产物。
本轮清理清单：
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r111-2026-10-05-14h09m58s562.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r111-2026-10-05-14h10m42s411.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r111-cli.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r111-source0.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r111-vlc.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r111.sdp

A/B均明确有限运行回收PASS、完整结束FAIL。6件产物已按清单清理，残留0，指定源保留。

## 2026-10-05 r112：双路 RTP/RTCP → H.264 VBR 8–12Mbps 1280×720/30、AAC CBR192kbps → MPEG-TS/RTP

沿用r109 Release，无源码改动。左VideoOnly61030、右AV61020/61022，audio-source1；VBR目标/最低8000、最高12000kbps，原120秒源规格不变。右源560于14:13:56启动，左源27116于14:14:07启动；两者3600帧/120秒自然exit0。选定右源31112于14:16:25重入，完整120秒自然exit0。CLI32116在源均结束后持续输出，抓包结束后定向Ctrl+C，CallerStop/exit0；VLC13636经RC quit。dumpcap29104从14:13:32捕获330秒自然exit0。六个精确PID均已无残留。

- 原始172858 RTP包/drop0、单SSRC、序号缺口0、TS CC错误0/discontinuity0；剥离后TS200712184B，媒体首末epoch1791180848.735159600至1791181142.731551000。tshark非packet插件尾行显式排除，数据行数与capture完全一致。
- 视频8818包，PTS=DTS且步3000，0至293.9秒；音频12659包，PTS=DTS且步2089/2090，0至293.918189秒。PES数一致；PCR PID257共4410，27MHz步1800000/1800001，无重复或回退。
- 原始TS五帧60/112/125/190/280秒分别：双路左右并列、仅左、全黑、仅右恢复、全黑；16:9图像置于640×720单元，实际640×360并上下留黑。五张窗口截图仅辅助现场观察，窗口被桌面边缘裁切，不当全画幅证据。
- -80dB/0.5秒检测静音106.942585–138.0622及256.514943–293.941406，支持选定源失活/恢复期间静音行为，不声称bit-exact零或多AV源音频身份对比。左侧本轮无音频；内容同步仍UNKNOWN。
- 额外启动静音0.085873–2.527324秒，长2.441451秒，原源同阈值完整检测无静音段。该项阻止完整音频门禁通过，不能忽略。
- final workers/errors/workerErrors/stalls/queued均0；payload170757预留=释放、0B/0对象，encoded106679推入=取出。CPU机器口径平均1.321956%、峰5.909091%；运行报告采样WS峰550916096B、末211709952B，Get-Process进程峰574713856B（不同采样口径明确保留）。credit压力39750次；AAC关闭2帧残留，CallerStop不等于排空。
- 两次Immediate校验；内部drift generation1为2条155–156ns，generation2为103条17756–17758ns，generation3为117条2203–2204ns。未见late head/late commit/service-window失败，但不关闭r105因果问题。

### 启动缺口现场与边界

14:14:08.781：video RTP sequence_gap，first_missing=10048、resumed=13450，raw generation0→1。
14:14:08.782：source.1 AV clock invalidated generation1，进入generation2；08.812 purge complete，09.681 complete keyframe received，11.245 AV clock重新锁定generation2。
这条链支持“输入序号不连续触发源域重新捕获并补静音”；尚未证明序号缺口来自发送、内核socket、prepared缓存交接或应用读取。本轮抓包只覆盖output，不能用output无丢包否定input缺口。下一根因调查围绕多源准备期间接收与prepared→runtime交接，不扩大外部依赖，不凭延长缓冲/降规格修复。

A/B独立复核有限运行、采样布局与回收PASS；两者均保留完整门禁FAIL、启动音频缺口及内容同步UNKNOWN。PTS计数A复算；RTP/CC/PCR由主智能体计算，两位未独立重算，准确保留审查边界。本轮无源码改变，不重评分，保留75/100。未运行local CLI或RKMPP，未推送/合并/部署。

### 实际命令

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp dst port 62020" -a duration:330 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-capture.log 2>&1
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r112 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-source1.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r112- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-reentry.log 2>&1
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R100Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$target=32116
$p=Get-CimInstance Win32_Process -Filter "ProcessId=$target"
if($p.Name -ne 'media_transcode_realtime_video_cli.exe' -or $p.CommandLine -notlike '*composition-wiring-r112*'){throw 'PID identity mismatch'}
[R100Console]::FreeConsole() | Out-Null
if(-not [R100Console]::AttachConsole($target)){throw 'AttachConsole failed'}
try {
if(-not [R100Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$ids=[uint32[]]::new(32)
$count=[R100Console]::GetConsoleProcessList($ids,32)
if($count -ne 2 -or $ids[0..($count-1)] -notcontains $target -or $ids[0..($count-1)] -notcontains $PID){throw "Unexpected console members: $ids"}
if(-not [R100Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R100Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'

& C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1 -WindowHandle 4982270 -Path D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-live.png
& C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1 -WindowHandle 4982270 -Path D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-selected-ended.png
& C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1 -WindowHandle 4982270 -Path D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-all-ended.png
& C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1 -WindowHandle 4982270 -Path D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-recovered.png
& C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1 -WindowHandle 4982270 -Path D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-final-black.png
& D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-wire.pcapng -T fields -e frame.time_epoch -e udp.payload > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-wire.tsv 2> D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-tshark.log
& D:/Python/python.exe -c 'from pathlib import Path
import json
root=Path("D:/Code/MyCode/MediaTranscode/out/acceptance")
count=0; previous={}; gaps=[]; cc={}; cc_errors=[]; notices=[]; discontinuities=0; ts_bytes=0; first=None; last=None
with (root/"composition-wiring-r112-wire.ts").open("wb") as out:
 for line in (root/"composition-wiring-r112-wire.tsv").open(encoding="utf-8-sig"):
  if not line.strip(): continue
  if line.strip()=="No matching RTP/H.264 payload dumped.":
   notices.append(line.strip()); continue
  stamp,hexdata=line.strip().split("\t")
  data=bytes.fromhex(hexdata.replace(":",""))
  assert len(data)>=12 and data[0]>>6==2 and data[1]&127==33
  seq=int.from_bytes(data[2:4],"big"); ssrc=int.from_bytes(data[8:12],"big")
  if ssrc in previous and (seq-previous[ssrc])%65536!=1: gaps.append([count,previous[ssrc],seq])
  previous[ssrc]=seq
  offset=12+4*(data[0]&15)
  if data[0]&16:
   assert len(data)>=offset+4
   offset+=4+4*int.from_bytes(data[offset+2:offset+4],"big")
  end=len(data)-data[-1] if data[0]&32 else len(data)
  payload=data[offset:end]
  assert payload and len(payload)%188==0
  for pos in range(0,len(payload),188):
   ts=payload[pos:pos+188]; assert ts[0]==71 and not ts[1]&128
   pid=((ts[1]&31)<<8)|ts[2]; afc=(ts[3]>>4)&3; counter=ts[3]&15
   assert afc
   reset=bool(afc&2 and ts[4] and ts[5]&128)
   if reset: discontinuities+=1; cc.pop(pid,None)
   if afc&1 and pid!=8191:
    if pid in cc and counter!=(cc[pid]+1)%16: cc_errors.append([count,pid,cc[pid],counter])
    cc[pid]=counter
  out.write(payload); ts_bytes+=len(payload); count+=1
  if first is None: first=stamp
  last=stamp
report=dict(rtp_packets=count,ssrc_count=len(previous),rtp_sequence_gaps=gaps,ts_continuity_errors=cc_errors,ts_discontinuity_flags=discontinuities,ts_bytes=ts_bytes,first_epoch=first,last_epoch=last,non_packet_notices=notices)
assert count==172858
print(json.dumps(report))
'
& D:/mabs/local64/bin-video/ffprobe.exe -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time,dts_time,duration_time -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-wire.ts > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-wire-packets.json 2> D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-wire-probe.log
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-wire.ts -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-silence.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 60 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-wire.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-frame-60.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 112 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-wire.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-frame-112.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 125 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-wire.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-frame-125.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 190 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-wire.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-frame-190.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -loglevel error -nostdin -ss 280 -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-wire.ts -frames:v 1 -update 1 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-frame-280.png
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r112-source-silence.log 2>&1
& D:/Python/python.exe -c 'import json
from collections import Counter
from pathlib import Path
p=json.loads(Path("out/acceptance/composition-wiring-r112-wire-packets.json").read_text(encoding="utf-8-sig"))["packets"]
for s in sorted(set(x["stream_index"] for x in p)):
 a=[x for x in p if x["stream_index"]==s and "pts" in x]
 d=[int(y["pts"])-int(x["pts"]) for x,y in zip(a,a[1:])]
 print(json.dumps(dict(stream=s,packets=len(a),first=a[0]["pts_time"],last=a[-1]["pts_time"],pts_steps=dict(Counter(d)),negative_steps=sum(v<0 for v in d),zero_steps=sum(v==0 for v in d),pts_dts_differences=sum(x.get("pts")!=x.get("dts") for x in a))))
'
& D:/Python/python.exe -c 'from pathlib import Path
from collections import Counter
import json
counts=Counter(); pcrs={}
with Path("out/acceptance/composition-wiring-r112-wire.ts").open("rb") as f:
 while (ts:=f.read(188)):
  assert len(ts)==188 and ts[0]==71
  pid=((ts[1]&31)<<8)|ts[2]; afc=(ts[3]>>4)&3; offset=4
  if afc&2:
   length=ts[4]; offset=5+length
   if length>=7 and ts[5]&16:
    b=ts[6:12]; base=(b[0]<<25)|(b[1]<<17)|(b[2]<<9)|(b[3]<<1)|(b[4]>>7); ext=((b[4]&1)<<8)|b[5]
    assert ext<300
    pcrs.setdefault(pid,[]).append(base*300+ext)
  if afc&1 and ts[1]&64 and ts[offset:offset+3]==b"\x00\x00\x01":
   counts[ts[offset+3]]+=1
print("PES_stream_id_counts",dict(counts))
for pid,a in pcrs.items():
 modulus=(1<<33)*300
 d=[((y-x+modulus//2)%modulus)-modulus//2 for x,y in zip(a,a[1:])]
 print(json.dumps(dict(pcr_pid=pid,count=len(a),negative=sum(x<0 for x in d),zero=sum(x==0 for x in d),minimum_step_ticks=min(d),maximum_step_ticks=max(d),clock_hz=27000000)))
'
```
本轮27件清理清单：
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-all-ended.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-capture.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-cli-error.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-cli.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-final-black.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-frame-112.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-frame-125.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-frame-190.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-frame-280.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-frame-60.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-live.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-received.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-recovered.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-reentry.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-selected-ended.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-silence.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-source-silence.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-source0.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-source1.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-tshark.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-vlc.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-wire-packets.json
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-wire-probe.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-wire.pcapng
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-wire.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112-wire.tsv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r112.sdp

27件已逐项清理，残留0，指定源124427809B保留。独立代码调查一致：InputPreparer每源jthread并行begin，并非顺序开socket；所有future完成前capture继续。MediaRawRtpPreparedInputBuffer::capture在剩余byteBudget不足maximumDatagram且probeActive=false时无captureError返回，停止drain；probeActive=true才失败。r112选定AV prepared replay跨度3970/3993ms，源早起约11秒，与预算满停读路径相容；但未记录capture停止原因/时刻，也无入站抓包，不能判定本轮已命中或socket丢包根因。下一项需关联input线端序号、capture停止与replay/runtime交接；不采用猜测扩容或降低源规格。

## r113：prepared接收边界诊断（实施中）

r112真实序号缺口与启动静音驱动本轮取证。沿GStreamer官方rtpjitterbuffer的seq/reason丢弃事件观测做法（https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html），只在既有capacity-stop和seal边界记录预算/队列及最后RTP SSRC、序号、观察时刻；复用parser与State日志，已有mutex保护，诊断异常隔离，不修改capture/回放/错误语义或公共参数。共享Windows/Linux路径，同规格Windows先验。后续以入站和出站被动抓包关联实际缺口；不移植drop-on-latency策略，不扩大FFmpeg依赖。

### r113构建、复现与责任边界（2026-10-05）

诊断初版B审查FAIL：重新采budget可能不等于触发停止的快照；首轮全量构建exit1，C2819/C2039（Result无operator->）。已改传入分支原budget、sealed单次采样，并用packet.value()；A/B完整复审均PASS。第二次Release clean-first all-target 684任务成功、configure/build exit0；realtime产物14:35:49、5615616B。只新增一个cpp内边界诊断，不修改接收语义或公共参数。

同原双路V左/AV右、audio-source1、H264 VBR目标/最低8Mbps最高12Mbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP；AV19688于14:39:39、V27932于14:39:51启动，两个原源均3600帧120秒自然exit0。CLI13620源结束后持续输出，定向Ctrl+C后CallerStop exit0。VLC2748经RC退出，dumpcap19388按180秒自然exit0；五PID无残留。此次为启动缺口专项复现，没有重入，不能替代r112完整恢复或修复验收。

最终workers/errors/stalls0，payload92095预留=释放、0B/0对象；encoded55880/55879，AAC关闭2帧残留，取消不等于排空。CPU机器口径均1.361513%、峰7.5%；WS采样峰576675840B、末211734528B。VLC窗口采样内容可见但被桌面边缘裁切；不作为全画幅证据。

#### 已闭合事实

- 全capture304326/drop0；输入61020视频106671包、61022音频5229包、61030视频106671包，各自单SSRC且模65536序号连续；三个RTCP端口各25包，输出62020共85680包。插件尾行非packet提示显式排除，包数总和一致。
- 选定视频SSRC1923380861：14:39:43.663 capacity_stop，capacity5000000、retained4934821、stream4771261、maximumDatagram65507、queued4173、probeActive0、尾序号5451；音频14:39:43.694也停，shared retained4935655、stream164394、queued197、SSRC3452762935尾3207。
- 14:39:52.197 seal仍为同一AV尾包；视频停读至seal8.534秒。晚到V的seal为14:39:52.195、SSRC2876722965尾5231、retained1841452。
- 应用14:39:52.450报first_missing9822、resumed13379。对应启动期扩展序列/时间窗内9822–13378共3557包在线端全部存在；完整120秒有16bit回绕，相同数值序号全文各出现两次，不声称全文件唯一。
- wire尾5451 epoch1791182383.662851；5452约44.5µs后仍到达。9821 epoch1791182388.3155098，9822 epoch1791182388.31552；13378 epoch1791182392.3492916，13379 epoch1791182392.3794134。首次5452–9821的UDP payload合计5000112B，与readback socket5000000B高度相容，不当作OS精确计费。
- 首次purge14:39:52.481晚于gap，后续主动清理不能解释首次缺口。三个adapter均truncations0、pressure_failures0；审查未发现正常接收时静默跳过这段有效包的策略。

A/B独立复算input序列与关键区间，责任分类PASS：线端完整但本机接收覆盖缺失，prepared容量停读及有限socket构成强机制证据。尚无kernel丢弃计数或逐批recv/adapter交付序号，不能把精确丢弃位置唯一归因Windows内核。禁止继续把此缺口归因发送源或外部FFmpeg，亦不能据此声称已修复。

#### 输出与音频

输出RTP85680单SSRC无gap、TS CC无错误/discontinuity0；原始TS99661808B，媒体epoch1791182392.335252000至1791182535.852161900。视频4304包PTS=DTS步3000，音频6179包PTS=DTS步2089/2090，无回退/重复。-80dB/0.5秒检测再次得到启动静音0.139864–2.410862（2.270998秒），选定源结束后106.85059–143.4761静音。诊断复现成立，功能修复和完整音频门禁均未通过。

#### 下一修复设计门禁

两位独立调查均否定“满了删队首”直接修补：probe lease按固定frontier借用append-only deque的span；活跃租约下删除会使视图失效。observedBytes为累计探测事实，不能因release驻留就清零或提前runtimeActive绕过seal。需要显式分离有限探测与live保留阶段，在同一总预算内计入pin、live和接收批次，以稳定身份表示退休及交接；保持单一socket读取owner，复用现有RTP/RTCP tracker、关键帧/startup门禁，不另建准备期媒体算法。若源参数/SSRC/协议代变化，旧prepared authority必须验证。

成熟依据：GStreamer有界jitterbuffer退休与带seq/reason事件（https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html）；FFmpeg UDP overrun_nonfatal只是溢出失败策略，不保证完整性（https://ffmpeg.org/ffmpeg-protocols.html#udp）。仅证明诊断方法与候选方向，rolling所有权/clock合同尚未设计通过，不先扩容、调时限或让异步源准备失败来替代合屏目标。Windows先验证，再RKMPP；不扩大外部依赖。

#### 实际命令

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-capture.log 2>&1
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r113 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-source1.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r113- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
& C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1 -WindowHandle 4000198 -Path D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-live.png
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R100Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$target=13620
$p=Get-CimInstance Win32_Process -Filter "ProcessId=$target"
if($p.Name -ne 'media_transcode_realtime_video_cli.exe' -or $p.CommandLine -notlike '*composition-wiring-r113*'){throw 'PID identity mismatch'}
[R100Console]::FreeConsole() | Out-Null
if(-not [R100Console]::AttachConsole($target)){throw 'AttachConsole failed'}
try {
if(-not [R100Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$ids=[uint32[]]::new(32)
$count=[R100Console]::GetConsoleProcessList($ids,32)
if($count -ne 2 -or $ids[0..($count-1)] -notcontains $target -or $ids[0..($count-1)] -notcontains $PID){throw "Unexpected console members: $ids"}
if(-not [R100Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R100Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'

& D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-wire.pcapng -T fields -e frame.time_epoch -e udp.dstport -e udp.payload > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-all-wire.tsv 2> D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-tshark.log
& D:/Python/python.exe -c 'from pathlib import Path
from collections import Counter,defaultdict
import json
ports=Counter(); streams={}; target=[]; rows=0; notices=[]
out=Path(''out/acceptance/composition-wiring-r113-wire.tsv'').open(''w'',encoding=''utf-8'')
with Path(''out/acceptance/composition-wiring-r113-all-wire.tsv'').open(encoding=''utf-8-sig'') as f:
 for line in f:
  fields=line.rstrip().split(''\t'')
  if len(fields)!=3: notices.append(line.strip());continue
  stamp,port,payload=fields;port=int(port);data=bytes.fromhex(payload.replace('':'',''''));rows+=1;ports[port]+=1
  if port==62020:out.write(stamp+''\t''+payload+''\n'')
  if port not in [61020,61022,61030,62020]:continue
  assert len(data)>=12 and data[0]>>6==2
  seq=int.from_bytes(data[2:4],''big'');ssrc=int.from_bytes(data[8:12],''big'');key=(port,ssrc)
  if key not in streams:streams[key]=dict(count=0,first_epoch=stamp,first_sequence=seq,previous=None,gaps=[])
  s=streams[key]
  if s[''previous''] is not None and seq!=(s[''previous'']+1)%65536:s[''gaps''].append([stamp,s[''previous''],seq])
  s[''previous'']=seq;s[''last_epoch'']=stamp;s[''count'']+=1
  if port==61020 and ssrc==1923380861:
   target.append((float(stamp),seq,len(data)))
out.close()
assert rows==304326
print(''rows'',rows,''ports'',dict(ports),''notices'',notices)
for k,s in streams.items():print(json.dumps(dict(port=k[0],ssrc=k[1],**s)))
initial=target[0][0]
early=[x for x in target if x[0]<initial+20]
for seq in [5451,5452,9821,9822,13378,13379]:
 print(''boundary'',seq,[x for x in early if x[1]==seq])
missing=[x for x in early if 9822<=x[1]<13379]
print(''reported_missing_range_on_wire'',len(missing),''expected'',13379-9822,''unique'',len(set(x[1] for x in missing)))
print(''wire_bytes_after_capture_tail_until_last_before_gap'',sum(x[2] for x in early if 5451<x[1]<=9821))
'
& D:/Python/python.exe -c 'from pathlib import Path
import json
root=Path("D:/Code/MyCode/MediaTranscode/out/acceptance")
count=0; previous={}; gaps=[]; cc={}; cc_errors=[]; notices=[]; discontinuities=0; ts_bytes=0; first=None; last=None
with (root/"composition-wiring-r113-wire.ts").open("wb") as out:
 for line in (root/"composition-wiring-r113-wire.tsv").open(encoding="utf-8-sig"):
  if not line.strip(): continue
  if line.strip()=="No matching RTP/H.264 payload dumped.":
   notices.append(line.strip()); continue
  stamp,hexdata=line.strip().split("\t")
  data=bytes.fromhex(hexdata.replace(":",""))
  assert len(data)>=12 and data[0]>>6==2 and data[1]&127==33
  seq=int.from_bytes(data[2:4],"big"); ssrc=int.from_bytes(data[8:12],"big")
  if ssrc in previous and (seq-previous[ssrc])%65536!=1: gaps.append([count,previous[ssrc],seq])
  previous[ssrc]=seq
  offset=12+4*(data[0]&15)
  if data[0]&16:
   assert len(data)>=offset+4
   offset+=4+4*int.from_bytes(data[offset+2:offset+4],"big")
  end=len(data)-data[-1] if data[0]&32 else len(data)
  payload=data[offset:end]
  assert payload and len(payload)%188==0
  for pos in range(0,len(payload),188):
   ts=payload[pos:pos+188]; assert ts[0]==71 and not ts[1]&128
   pid=((ts[1]&31)<<8)|ts[2]; afc=(ts[3]>>4)&3; counter=ts[3]&15
   assert afc
   reset=bool(afc&2 and ts[4] and ts[5]&128)
   if reset: discontinuities+=1; cc.pop(pid,None)
   if afc&1 and pid!=8191:
    if pid in cc and counter!=(cc[pid]+1)%16: cc_errors.append([count,pid,cc[pid],counter])
    cc[pid]=counter
  out.write(payload); ts_bytes+=len(payload); count+=1
  if first is None: first=stamp
  last=stamp
report=dict(rtp_packets=count,ssrc_count=len(previous),rtp_sequence_gaps=gaps,ts_continuity_errors=cc_errors,ts_discontinuity_flags=discontinuities,ts_bytes=ts_bytes,first_epoch=first,last_epoch=last,non_packet_notices=notices)
assert count==85680
print(json.dumps(report))
'
& D:/mabs/local64/bin-video/ffprobe.exe -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time,dts_time,duration_time -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-wire.ts > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-wire-packets.json 2> D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-wire-probe.log
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-wire.ts -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r113-silence.log 2>&1
& D:/Python/python.exe -c 'import json
from collections import Counter
from pathlib import Path
p=json.loads(Path("out/acceptance/composition-wiring-r113-wire-packets.json").read_text(encoding="utf-8-sig"))["packets"]
for s in sorted(set(x["stream_index"] for x in p)):
 a=[x for x in p if x["stream_index"]==s and "pts" in x]
 d=[int(y["pts"])-int(x["pts"]) for x,y in zip(a,a[1:])]
 print(json.dumps(dict(stream=s,packets=len(a),first=a[0]["pts_time"],last=a[-1]["pts_time"],pts_steps=dict(Counter(d)),negative_steps=sum(v<0 for v in d),zero_steps=sum(v==0 for v in d),pts_dts_differences=sum(x.get("pts")!=x.get("dts") for x in a))))
'
```
本轮19件清理清单：
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-all-wire.tsv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-build.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-capture.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-cli-error.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-cli.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-live.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-rebuild.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-received.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-silence.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-source0.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-source1.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-tshark.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-vlc.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-wire-packets.json
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-wire-probe.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-wire.pcapng
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-wire.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113-wire.tsv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r113.sdp

19件产物已按清单清理，残留0；五PID无残留，指定源124427809B保留，无本轮远程产物。

## 2026-10-05 r117：共享接收缓冲与实际长度保留复验

状态：接收增量双独立源码 PASS、Release 全量构建 exit0，以下命令已执行；完整验收 FAIL，见本节补齐结果。MediaUdpSocket 改为 receiveInto(span)，transport 单 receiver 复用 scratch 并按实际收到长度构造 owned vector；Linux 拒绝截断，prepared 在共同交接点先释放 scratch 再建 runtime arena。PMR 预算与消费者租约尚未接入，r113 启动停读仍未宣称修复。

本轮保持原120秒连续源、双路V左/AV右、audio-source1、约11秒启动间隔，H264 VBR目标/最低8000最高12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP。当前端口无占用，源124427809B。只测试realtime。

### 实际启动命令（执行后更新PID和结果）

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r117-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r117-capture.log 2>&1
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r117 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r117.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r117-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r117-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r117-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r117-source1.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r117-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r117- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r117-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

### 2026-10-08 补齐结果与清理（完整验收 FAIL）

接收改动经 A/B 两个独立源码审查 PASS、Release clean-first/all-target 重建 exit0；realtime 产物 2026-10-05 20:46:33、5617152B。未运行 local CLI，未修改外部 FFmpeg。

- 原进程：dumpcap3452（20:46:59）、CLI28628（20:47:12）、AV6048（20:47:26）、V27856（20:47:40）、VLC29248（20:48:02）。两源日志均完整3600帧/120秒。恢复会话后旧exec句柄均不存在，进程级退出码无法恢复，不能补写exit0；2026-10-08只读CIM复核五个PID均不存在，无需强杀。
- 输入首包时间差13.335948秒，比原约11秒略长。AV准备video于20:47:31.616满预算停读，seal20:47:41.705，间隔10.089秒；retained4936021B、video4772461B/4174包、audio163560B/196包。应用随后报告first_missing8625 resumed13492（4867包）。
- 抓包303315/drop0。61020与61030视频各106671包、61022音频5229包，三个RTCP端口各25包；62020输出84669包。四条RTP序列按SSRC与16位回绕核验无缺口；AV视频SSRC3136431661启动首20秒内4867个缺失序号全部在线端存在。不能把这个结论扩展为内核唯一责任证明。
- 从线端RTP提取TS99144620B/527365包，TS CC错误0；视频4116包、PTS步3000，音频5909包、PTS步2089/2090，PTS=DTS。这里只覆盖180秒抓包窗口，不覆盖后续长时间运行。
- 离线帧20秒为两格/DAR黑边，110秒仅左格，130秒全黑。截图技能CopyFromScreen返回“句柄无效”，VLC RC snapshot未生成文件，因此VLC实际桌面画面观察缺失；离线输出帧不能代替该验收项。
- 启动额外静音0.0858277–0.903810秒（0.817982秒）；失源静音105.306644–137.206712秒。未做本轮内容A/V匹配，仍UNKNOWN。drift trace共105条，raw_phase_ns范围155–13734，不能等同内容同步通过。
- 监控CSV3217点：12:48:33.802–14:36:28.847 UTC；抓包窗口内43点CPU机器均值1.505639%、峰2.301211%，工作集采样峰227958784B。采样开始晚于准备期，不能用它证明准备期峰值或与r113做同范围峰值比较。全失源时段3130点，首/末30点工作集均值213247454/213364190B，仅是采样趋势。
- 本轮没有完成原定及时停止/清理：会话恢复后发现CLI持续到22:36:25.576，最终失败为 `IoFailure: scheduled datagram submit exceeded its original deadline`；不得将中间全失源持续输出标为完整长稳PASS。sender deadline_misses1、submitted776368、reserved776369、最大submit lateness85893420ns，protocol batch最大产生延迟289523200ns。180秒后的线端证据缺失，尚未归因该延后失败。
- 最终workers0、errors2、payload预约1485487=释放1485487，0B/0对象；encoded2187475/2187471，AAC关闭余2帧。这是失败回收记录，不是正常停止或完整drain通过。

PMR组件仍未接入，旧size预约、初始视频预约空档、出队过早释放和prepared/runtime arena总账重叠仍待修复。当前接收scratch只减少每包最大容量保留，不解决预算满后停读。继续统一存储owner/预算与持续live接收交接；完整矩阵、重入、可信终止、C API与RKMPP门禁保持未完成。

### 本轮补充诊断命令

```powershell
& 'D:\Wireshark\tshark.exe' -r 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-wire.pcapng' -T fields -e frame.time_epoch -e udp.dstport -e udp.payload > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-all-wire.tsv' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-tshark.log'
& 'D:\mabs\local64\bin-video\ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts,duration -of json 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-wire.ts' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-wire-packets.json' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-probe.log'
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-wire.ts' -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-silence.log' 2>&1
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -ss 20 -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-wire.ts' -frames:v 1 -update 1 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-both.png' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-both-frame.log' 2>&1
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -ss 110 -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-wire.ts' -frames:v 1 -update 1 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-left.png' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-left-frame.log' 2>&1
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -ss 130 -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-wire.ts' -frames:v 1 -update 1 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-black.png' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-black-frame.log' 2>&1
& 'D:\Python\python.exe' -c @'
from pathlib import Path
from collections import Counter
import json,csv,re
root=Path("D:/Code/MyCode/MediaTranscode/out/acceptance")
ports=Counter();streams={}; missing=set(); tsbytes=0
with (root/"composition-wiring-r117-wire.ts").open("wb") as out:
 for line in (root/"composition-wiring-r117-all-wire.tsv").open(encoding="utf-8-sig"):
  fields=line.rstrip().split("\t")
  if fields==["No matching RTP/H.264 payload dumped."]: continue
  if len(fields)!=3: raise ValueError(fields)
  stamp,port,payload=fields; port=int(port); data=bytes.fromhex(payload.replace(":","")); ports[port]+=1
  if port not in (61020,61022,61030,62020): continue
  assert len(data)>=12 and data[0]>>6==2
  seq=int.from_bytes(data[2:4],"big");ssrc=int.from_bytes(data[8:12],"big");key=(port,ssrc)
  s=streams.setdefault(key,dict(count=0,first_epoch=float(stamp),first_sequence=seq,previous=None,gaps=[]))
  if s["previous"] is not None and seq!=(s["previous"]+1)%65536:s["gaps"].append([stamp,s["previous"],seq])
  s["previous"]=seq;s["count"]+=1;s["last_epoch"]=float(stamp)
  if port==61020 and ssrc==3136431661 and float(stamp)-s["first_epoch"]<20 and 8625<=seq<13492:missing.add(seq)
  if port==62020:
   offset=12+4*(data[0]&15)
   if data[0]&16:offset+=4+4*int.from_bytes(data[offset+2:offset+4],"big")
   end=len(data)-(data[-1] if data[0]&32 else 0)
   packet=data[offset:end];assert len(packet)%188==0 and all(packet[i]==0x47 for i in range(0,len(packet),188))
   out.write(packet);tsbytes+=len(packet)
print("packets",sum(ports.values()),"ports",dict(ports),"tsbytes",tsbytes)
for (port,ssrc),s in streams.items():print(json.dumps(dict(port=port,ssrc=ssrc,**s)))
print("startup_missing_range_present",len(missing),"expected",13492-8625)
metrics=list(csv.DictReader((root/"composition-wiring-r117-metrics.csv").open(encoding="utf-8-sig")))
print("metrics",len(metrics),"first",metrics[0]["utc"],"last",metrics[-1]["utc"],"peak_ws",max(int(x["working_set"]) for x in metrics),"peak_private",max(int(x["private_bytes"]) for x in metrics))
log=(root/"composition-wiring-r117-cli.log").read_text(encoding="utf-8-sig")
phases=[int(x) for x in re.findall(r"raw_phase_ns=(-?\d+)",log)]
print("phase_ns",min(phases),max(phases),"samples",len(phases))
'@
```

tshark导出尾部出现已识别插件提示“No matching RTP/H.264 payload dumped.”，初次严格解析因此exit1；仅排除这条精确提示后复核303315行数据并成功解析。Python同时提示安装real location不可定位，但脚本实际运行并exit0；不隐去这些工具诊断。

### 清理清单（23件）

- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-all-wire.tsv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-black-frame.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-black.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-both-frame.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-both.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-capture.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-cli-error.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-cli.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-left-frame.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-left.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-metrics.csv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-probe.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-received.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-silence.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-source0.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-source1.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-tshark.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-vlc.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-wire-packets.json
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-wire.pcapng
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117-wire.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r117.sdp
- D:\Code\MyCode\MediaTranscode\out\r117-build.log


## 2026-10-08 r119：容量契约构建通过，启动组织失败

独立容量产品、精确载荷分配及固定描述符已实现，两个独立源码审查（r119_storage_review_a/b）最终 PASS；Release 全量构建 exit0，realtime 2026-10-08 13:28:20，5627904B。完整 ledger/rolling 未完成。

本轮实流 FAIL，非媒体功能通过：VLC33612于13:29:52，dumpcap34184及CLI35232约13:29:54，AV33184于13:30:17，V28428于13:30:28。第二路超过CLI原30秒open期限，CLI exit1，matching_rtp_packets=0/parameter_sets=none。没有合屏输出，不能评价内容同步或正常停止。AV captured wire4936021B；allocation cap605262268B，retained405241995B，原wire cap5000000B未用于限制元数据。准备期已采样private626872320B、WS19865600B；这是失败准备阶段数据，不是完整链路峰值。监控随CLI退出结束（工具exit1），没有A/V drift样本。

实际命令：
```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r119-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r119-capture.log 2>&1
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r119 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r119.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r119-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r119-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r119-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r119-source1.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r119-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r119- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r119-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
$r119Started=Get-Date; $r119LastTime=$r119Started; $r119LastCpu=0; 'time_utc,cpu_seconds,cpu_machine_percent,working_set,private_bytes,threads' | Set-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r119-metrics.csv' -Encoding UTF8; while (((Get-Date)-$r119Started).TotalSeconds -lt 190) { $r119Process=Get-Process -Id 35232 -ErrorAction SilentlyContinue; if (!$r119Process) { break }; $r119Now=Get-Date; $r119Delta=($r119Now-$r119LastTime).TotalSeconds; $r119Cpu=if ($r119Delta -gt 0) {100*($r119Process.CPU-$r119LastCpu)/$r119Delta/[Environment]::ProcessorCount} else {0}; '{0},{1},{2},{3},{4},{5}' -f $r119Now.ToUniversalTime().ToString('o'),$r119Process.CPU,$r119Cpu,$r119Process.WorkingSet64,$r119Process.PrivateMemorySize64,$r119Process.Threads.Count | Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r119-metrics.csv' -Encoding UTF8; $r119LastTime=$r119Now; $r119LastCpu=$r119Process.CPU; Start-Sleep -Seconds 1 }
```

已判定本轮结束，按精确PID核对身份后清理尚存AV/V/VLC/dumpcap；不把清理记为源自然结束或通过。临时清单：r119-build.log；acceptance/composition-wiring-r119-{wire.pcapng,capture.log,cli.log,cli-error.log,source0.log,source1.log,vlc.log,metrics.csv,received.ts}及composition-wiring-r119.sdp（后两项若未生成则不删除）。指定120秒源保留。后续使用统一启动调度保持同样参数和错峰间隔重跑。

清理复核：五个精确PID均不存在；r119文件清单残留0。未运行远程测试。


## 2026-10-08 r120：独立容量契约原规格重跑（完整验收 FAIL）

使用r119同一Release产物5627904B（13:28:20），未修改参数。统一调度：VLC11136 13:32:17、dumpcap35720/CLI34352 13:32:18、AV39176 13:32:20、V39268 13:32:31。两源完整3600帧/120秒，执行句柄均exit0；dumpcap180秒exit0，306351包/drop0。约11秒错峰，未降低源、分辨率、码率、帧率或时长。

AV wire4936021B，allocation cap605262268B，retained405241995B；V cap405131134B。AV video capacity_stop13:32:24.397、sealed13:32:32.030，停读7.633秒；后续 first_missing8577 resumed11188（2611包）。因此启动handoff仍FAIL，容量分离不等于连续接收修复。VLC实际截图可见双路/DAR黑边→仅左→黑场，但窗口超出屏幕，右侧截图未完整，完整几何不判PASS。全失源后约47秒抓包窗口内CLI持续输出，运行错误0；不会把显式结束记为源驱动自然退出。准备至运行监控已启动，内存/漂移及线端详细分析随后补齐，内容同步仍UNKNOWN。

### 实际启动与监控命令
```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r120-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r120-capture.log 2>&1
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r120 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r120.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r120-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r120-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r120-source0.log 2>&1
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r120-source1.log 2>&1
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r120-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r120- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r120-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
$r120Pid=(Get-CimInstance Win32_Process -Filter "Name='media_transcode_realtime_video_cli.exe'" | Where-Object { $_.CommandLine -like '*composition-wiring-r120*' }).ProcessId; if(!$r120Pid){throw 'r120 CLI not found'}; $r120Started=Get-Date; $r120LastTime=$r120Started; $r120LastCpu=0; 'time_utc,cpu_seconds,cpu_machine_percent,working_set,private_bytes,threads' | Set-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r120-metrics.csv' -Encoding UTF8; while (((Get-Date)-$r120Started).TotalSeconds -lt 190) { $r120Process=Get-Process -Id $r120Pid -ErrorAction SilentlyContinue; if (!$r120Process) { break }; $r120Now=Get-Date; $r120Delta=($r120Now-$r120LastTime).TotalSeconds; $r120Cpu=if ($r120Delta -gt 0) {100*($r120Process.CPU-$r120LastCpu)/$r120Delta/[Environment]::ProcessorCount} else {0}; '{0},{1},{2},{3},{4},{5}' -f $r120Now.ToUniversalTime().ToString('o'),$r120Process.CPU,$r120Cpu,$r120Process.WorkingSet64,$r120Process.PrivateMemorySize64,$r120Process.Threads.Count | Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r120-metrics.csv' -Encoding UTF8; $r120LastTime=$r120Now; $r120LastCpu=$r120Process.CPU; Start-Sleep -Seconds 1 }
```

观测阶段已结束；接下来仅向核验后的CLI专属console发送CtrlC结束本轮，VLC通过本轮RC关闭。保留退出原因与最后telemetry后按清单删除临时产物。


### r120 最终结果与证据边界

- 首包错峰10.920139秒。四条RTP流按SSRC检查序号回绕均无缺口：61020/61030各106671，61022为5229，62020为87705；三个RTCP端口各25。应用报告缺失的8577–11187共2611个序号全部在该SSRC启动前20秒线端存在。抓包306351/drop0，不唯一归因内核。
- 输出TS99552016B、529532包，CC错误0；视频4983包、PTS步3000，音频7154包、步2089/2090；无负/零步，PTS=DTS。覆盖约166.15秒线端输出，不是长期稳定性证明。
- 启动静音0.108662–3.363129秒，持续3.254467秒；失源静音107.787347–166.115556秒。完整验收FAIL；无内容级A/V匹配，仍UNKNOWN。105条drift raw_phase_ns=155..9199仅为运行数据。
- 188个进程样本覆盖05:32:19.9498408Z–05:35:29.8295623Z（包含准备期）：WS峰228687872B、private峰1072721920B；排除首样本后的机器CPU均值1.362578%、峰2.576036%。05:34:40之后50个全失源样本，首/末10点WS均值214487859/214564045B、private1058320384/1058397798B；短窗口趋势，不能标长稳。N=wireBytes固定描述符造成高提交量，不能声称内存优化。
- VLC截图技能三次exit0，已实际查看：双路、右黑仅左、全黑；右侧窗口超出屏幕，完整几何仍不足。两源均自然exit0/full120秒。CLI直到13:36:03保持运行，观测结束后向专属console显式CtrlC，end_reason=caller_stop、exit0。最终workers0/errors0、encoded70964入=出、payload预约102129=释放102129、0B/0对象。该结果证明本次显式回收，不是源驱动自动退出或完整合屏PASS。
- VLC经本轮127.0.0.1:63020 RC发送quit关闭；五个精确PID复核无残留。没有部署/推送/外部FFmpeg修复/远程测试。

### 实际停止与离线分析命令
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R120Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r120Target=34352
$r120Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r120Target"
if($r120Process.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r120Process.CommandLine -notlike '*composition-wiring-r120*'){throw 'PID identity mismatch'}
[R120Console]::FreeConsole() | Out-Null
if(-not [R120Console]::AttachConsole($r120Target)){throw 'AttachConsole failed'}
try {
if(-not [R120Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r120Ids=[uint32[]]::new(32)
$r120Count=[R120Console]::GetConsoleProcessList($r120Ids,32)
if($r120Count -ne 2 -or $r120Ids[0..($r120Count-1)] -notcontains $r120Target -or $r120Ids[0..($r120Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R120Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R120Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
& 'D:\Wireshark\tshark.exe' -r 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r120-wire.pcapng' -T fields -e frame.time_epoch -e udp.dstport -e udp.payload > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r120-all-wire.tsv' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r120-tshark.log'
& 'D:\Python\python.exe' -c @'
from pathlib import Path
from collections import Counter
import json,csv,re
root=Path("D:/Code/MyCode/MediaTranscode/out/acceptance")
prefix="composition-wiring-r120"
streams={}; ports=Counter(); notices=[]; packets=0; found=set(); cc={}; ccerrors=0; tsbytes=0
with (root/(prefix+"-wire.ts")).open("wb") as out:
 for line in (root/(prefix+"-all-wire.tsv")).open(encoding="utf-8-sig"):
  if not line.strip(): continue
  if line.strip()=="No matching RTP/H.264 payload dumped.": notices.append(line.strip()); continue
  stamp,port,hexdata=line.rstrip("\r\n").split("\t")
  t=float(stamp); port=int(port); data=bytes.fromhex(hexdata.replace(":","")); ports[port]+=1; packets+=1
  if port not in (61020,61022,61030,62020): continue
  assert len(data)>=12 and data[0]>>6==2
  seq=int.from_bytes(data[2:4],"big"); ssrc=int.from_bytes(data[8:12],"big"); key=(port,ssrc)
  s=streams.setdefault(key,dict(count=0,gaps=0,first=t,last=t,previous=None))
  if s["previous"] is not None and (seq-s["previous"])%65536!=1: s["gaps"]+=1
  s["count"]+=1; s["last"]=t; s["previous"]=seq
  if port==61020 and ssrc==528586289 and t<s["first"]+20 and 8577<=seq<11188: found.add(seq)
  if port!=62020: continue
  offset=12+4*(data[0]&15)
  if data[0]&16: offset+=4+4*int.from_bytes(data[offset+2:offset+4],"big")
  end=len(data)-data[-1] if data[0]&32 else len(data); payload=data[offset:end]
  assert payload and len(payload)%188==0
  for i in range(0,len(payload),188):
   ts=payload[i:i+188]; assert ts[0]==71 and not ts[1]&128
   pid=((ts[1]&31)<<8)|ts[2]; afc=(ts[3]>>4)&3; counter=ts[3]&15; assert afc
   if afc&2 and ts[4] and ts[5]&128: cc.pop(pid,None)
   if afc&1 and pid!=8191:
    if pid in cc and counter!=(cc[pid]+1)%16: ccerrors+=1
    cc[pid]=counter
  out.write(payload); tsbytes+=len(payload)
print(json.dumps(dict(packets=packets,ports=dict(ports),streams=[dict(port=k[0],ssrc=k[1],**v) for k,v in streams.items()],missing_on_wire=len(found),expected_missing=2611,ts_bytes=tsbytes,ts_cc_errors=ccerrors,notices=notices)))
metrics=list(csv.DictReader((root/(prefix+"-metrics.csv")).open(encoding="utf-8-sig")))
print(json.dumps(dict(samples=len(metrics),first=metrics[0]["time_utc"],last=metrics[-1]["time_utc"],peak_working_set=max(int(x["working_set"]) for x in metrics),peak_private=max(int(x["private_bytes"]) for x in metrics),mean_cpu=sum(float(x["cpu_machine_percent"]) for x in metrics[1:])/len(metrics[1:]),peak_cpu=max(float(x["cpu_machine_percent"]) for x in metrics[1:]))))
logs=(root/(prefix+"-cli.log")).read_text(encoding="utf-8-sig")
drift=[int(x) for x in re.findall(r"av_drift_trace[^\n]*?raw_phase_ns=(-?\d+)",logs)]
print(json.dumps(dict(drift_count=len(drift),raw_phase_min=min(drift),raw_phase_max=max(drift))))

'@
& 'D:\mabs\local64\bin-video\ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time -of json 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r120-wire.ts' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r120-packets.json' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r120-probe.log'
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r120-wire.ts' -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r120-silence.log' 2>&1
& 'D:\Python\python.exe' -c @'
from pathlib import Path
from collections import Counter
import json,csv
root=Path("D:/Code/MyCode/MediaTranscode/out/acceptance")
p=json.loads((root/"composition-wiring-r120-packets.json").read_text(encoding="utf-8-sig"))["packets"]
for s in sorted(set(x["stream_index"] for x in p)):
 a=[x for x in p if x["stream_index"]==s and "pts" in x]; d=[int(y["pts"])-int(x["pts"]) for x,y in zip(a,a[1:])]
 print(json.dumps(dict(stream=s,packets=len(a),first=a[0]["pts_time"],last=a[-1]["pts_time"],pts_steps=dict(Counter(d)),negative=sum(x<0 for x in d),zero=sum(x==0 for x in d),pts_dts_diff=sum(x.get("pts")!=x.get("dts") for x in a))))
m=list(csv.DictReader((root/"composition-wiring-r120-metrics.csv").open(encoding="utf-8-sig")))
loss=[x for x in m if x["time_utc"]>="2026-10-08T05:34:40"]
print(json.dumps(dict(loss_samples=len(loss),first10_ws=sum(int(x["working_set"]) for x in loss[:10])/len(loss[:10]),last10_ws=sum(int(x["working_set"]) for x in loss[-10:])/len(loss[-10:]),first10_private=sum(int(x["private_bytes"]) for x in loss[:10])/len(loss[:10]),last10_private=sum(int(x["private_bytes"]) for x in loss[-10:])/len(loss[-10:]))))

'@
& 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -ExecutionPolicy Bypass -File 'C:\Users\ROG\.codex\skills\screenshot\scripts\take_screenshot.ps1' -Path 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r120-live.png' -WindowHandle 6226624
& 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -ExecutionPolicy Bypass -File 'C:\Users\ROG\.codex\skills\screenshot\scripts\take_screenshot.ps1' -Path 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r120-left.png' -WindowHandle 6226624
& 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -ExecutionPolicy Bypass -File 'C:\Users\ROG\.codex\skills\screenshot\scripts\take_screenshot.ps1' -Path 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r120-black.png' -WindowHandle 6226624
```

### 清理清单

- out/acceptance/composition-wiring-r120-all-wire.tsv
- out/acceptance/composition-wiring-r120-black.png
- out/acceptance/composition-wiring-r120-capture.log
- out/acceptance/composition-wiring-r120-cli-error.log
- out/acceptance/composition-wiring-r120-cli.log
- out/acceptance/composition-wiring-r120-left.png
- out/acceptance/composition-wiring-r120-live.png
- out/acceptance/composition-wiring-r120-metrics.csv
- out/acceptance/composition-wiring-r120-received.ts
- out/acceptance/composition-wiring-r120-source0.log
- out/acceptance/composition-wiring-r120-source1.log
- out/acceptance/composition-wiring-r120-tshark.log
- out/acceptance/composition-wiring-r120-vlc.log
- out/acceptance/composition-wiring-r120-wire.pcapng
- out/acceptance/composition-wiring-r120-wire.ts
- out/acceptance/composition-wiring-r120-packets.json
- out/acceptance/composition-wiring-r120-probe.log
- out/acceptance/composition-wiring-r120-silence.log
- out/acceptance/composition-wiring-r120.sdp

上述19件临时产物已逐项删除（最终归档顺序偏差见下文），保留指定120秒连续源；构建日志已在r119失败记录后清理。不得将删除证据改写为通过。

归档顺序偏差：最终追加文档的命令因嵌套 here-string 解析失败，随后清理命令已执行；本段立即依据本轮已返回的完整分析输出、命令及内存中的汇总补写。结果与失败结论未丢弃，但原始临时文件现已删除，不能再作新分析。清理复核19件残留0、五PID残留0，指定源124427809B保留。

## 2026-10-08 r121：最终 prepared 账本 Release 复验

原规格双路 H264 RTP/RTCP（一路AAC）→固定横排→H264 VBR 8000–12000kbps、1280×720/30、AAC CBR192kbps/44100Hz/stereo→MPEG-TS/RTP。全长120秒源，VideoOnly比AV晚约11秒；结果待运行，不构成PASS。

### 本轮实际执行命令（构建成功后启动）

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r121-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r121-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r121 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r121.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r121-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r121-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r121-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r121-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r121-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r121- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r121-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

### 构建与运行中证据

第一次全量构建因 std::none_of 缺少 <algorithm> 失败，补直接依赖后双审重新PASS；第二次Release clean-first/all-target configure/build exit0，realtime CLI 5648896B，2026-10-08 13:57:41。

进程：VLC36976、dumpcap6496（13:58:17）；CLI30928（13:58:18）；AV FFmpeg14500（13:58:20）；VideoOnly FFmpeg14324（13:58:31）。两源均3600帧、完整120秒、exit0。CLI已进入生产DAG，all-loss后仍持续输出，尚未执行显式停止。AV capacity_stop13:58:24.294至seal13:58:31.999为7.705秒；应用视频first_missing11258/resumed13949，2691包待线端核对。VLC截图部分窗口被其他应用遮挡，不能证明完整几何。

CPU/内存监控实际命令：
```powershell
$r121Pid=(Get-CimInstance Win32_Process -Filter "Name='media_transcode_realtime_video_cli.exe'" | Where-Object { $_.CommandLine -like '*composition-wiring-r121*' }).ProcessId; if(!$r121Pid){throw 'r121 CLI not found'}; $r121Started=Get-Date; $r121LastTime=$r121Started; $r121LastCpu=0; 'time_utc,cpu_seconds,cpu_machine_percent,working_set,private_bytes,threads' | Set-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r121-metrics.csv' -Encoding UTF8; while (((Get-Date)-$r121Started).TotalSeconds -lt 190) { $r121Process=Get-Process -Id $r121Pid -ErrorAction SilentlyContinue; if (!$r121Process) { break }; $r121Now=Get-Date; $r121Delta=($r121Now-$r121LastTime).TotalSeconds; $r121Cpu=if ($r121Delta -gt 0) {100*($r121Process.CPU-$r121LastCpu)/$r121Delta/[Environment]::ProcessorCount} else {0}; '{0},{1},{2},{3},{4},{5}' -f $r121Now.ToUniversalTime().ToString('o'),$r121Process.CPU,$r121Cpu,$r121Process.WorkingSet64,$r121Process.PrivateMemorySize64,$r121Process.Threads.Count | Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r121-metrics.csv' -Encoding UTF8; $r121LastTime=$r121Now; $r121LastCpu=$r121Process.CPU; Start-Sleep -Seconds 1 }
```

### 最终结果：整体 FAIL，账本接线源码/构建/运行切片通过

实际双源首包间隔10.9757375秒。两源exit0；capture180秒exit0，306244包/drop0；输入视频61020/61030各106671包、音频61022为5229包，四条RTP序号均连续，RTCP三个端口各25包。应用缺口11258..13948（SSRC1982237051）共2691包均在线端存在。容量停读7.705秒仍复现，不能归为发送端缺包，也未凭抓包唯一归因内核。

输出RTP87598包，99459520B TS，CC错误0；4976视频包PTS步3000，7144音频包步2090（5685次）/2089（1458次），无负/零步、PTS=DTS。输出录得约165.92秒。启动静音0.108753–3.246984（3.138231秒）；失源静音107.673401–165.883356。105条drift raw_phase155..11403ns；仅证明内部时间线，内容A/V仍UNKNOWN。VLC截图可见运行图像、左格与黑场，但其他窗口遮挡，完整画面/几何门禁未通过。

182个进程采样覆盖05:58:19.9303439Z至06:01:23.7254305Z，峰WS228950016B、峰private1077166080B，排除首点CPU均值1.453195%、峰2.603398%。全失源后44点，首/尾10点均值WS214704947/214626714B，private1063047987/1062969344B；仅短期趋势，非长稳证明。监控helper在CLI退出时结束码1，CSV182点已核验，不能把监控helper标exit0。

14:01:24 CLI显式caller_stop，exit0，workers0/errors0，payload93426预约=释放，0B/0对象；encoded pushed57604/popped57603，不宣称完全排空。停止命令见下。VLC RC quit，五个精确PID36976/6496/30928/14500/14324复核均已不存在。两源自然结束后的持续输出与调用方停止是局部证据，不等于可信EOF自然退出或完整验收通过。

```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R121Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r121Target=30928
$r121Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r121Target"
if($r121Process.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r121Process.CommandLine -notlike '*composition-wiring-r121*'){throw 'PID identity mismatch'}
[R121Console]::FreeConsole() | Out-Null
if(-not [R121Console]::AttachConsole($r121Target)){throw 'AttachConsole failed'}
try {
if(-not [R121Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r121Ids=[uint32[]]::new(32)
$r121Count=[R121Console]::GetConsoleProcessList($r121Ids,32)
if($r121Count -ne 2 -or $r121Ids[0..($r121Count-1)] -notcontains $r121Target -or $r121Ids[0..($r121Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R121Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R121Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

离线分析仅在真实链路结束后执行；未增加实时FFmpeg监控。实际离线命令：
```powershell
& 'D:\Wireshark\tshark.exe' -r 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r121-wire.pcapng' -T fields -e frame.time_epoch -e udp.dstport -e udp.payload > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r121-all-wire.tsv' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r121-tshark.log'
& 'D:\mabs\local64\bin-video\ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time -of json 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r121-wire.ts' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r121-packets.json' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r121-probe.log'
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r121-wire.ts' -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r121-silence.log' 2>&1
```

完整合屏仍FAIL/75分；未运行local CLI/RKMPP，未推送/合并/部署，未修改外部FFmpeg。稳定引用与有序协议/准入交接仍未实现。

本节保存成功后清理的精确清单（原120秒源保留）：
- composition-wiring-r121-all-wire.tsv
- composition-wiring-r121-black.png
- composition-wiring-r121-capture.log
- composition-wiring-r121-cli-error.log
- composition-wiring-r121-cli.log
- composition-wiring-r121-left.png
- composition-wiring-r121-live.png
- composition-wiring-r121-metrics.csv
- composition-wiring-r121-received.ts
- composition-wiring-r121-source0.log
- composition-wiring-r121-source1.log
- composition-wiring-r121-tshark.log
- composition-wiring-r121-visible.png
- composition-wiring-r121-vlc.log
- composition-wiring-r121-wire.pcapng
- composition-wiring-r121-wire.ts
- composition-wiring-r121-packets.json
- composition-wiring-r121-probe.log
- composition-wiring-r121-silence.log
- composition-wiring-r121.sdp
- out/r121-build.log
- out/r121-build-retry.log

清理结果：以上22项文件已全部删除；all-wire.tsv与wire.pcapng首次短暂占用，核对无本轮工具进程后重试成功。本机r121文件及精确PID无残留，本轮未产生远程文件。

## 2026-10-08 r122：稳定条目引用原规格复验

独立源码双审PASS，Release全量重建完成；下列命令使用原120秒源，同r121协议/编码/码率/分辨率/帧率/约11秒错峰，不运行local CLI。结果待观测。

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r122-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r122-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r122 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r122.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r122-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r122-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r122-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r122-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r122-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r122- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r122-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

后台CPU/内存监控：
```powershell
$r122Pid=(Get-CimInstance Win32_Process -Filter "Name='media_transcode_realtime_video_cli.exe'" | Where-Object { $_.CommandLine -like '*composition-wiring-r122*' }).ProcessId; if(!$r122Pid){throw 'r122 CLI not found'}; $r122Started=Get-Date; $r122LastTime=$r122Started; $r122LastCpu=0; 'time_utc,cpu_seconds,cpu_machine_percent,working_set,private_bytes,threads' | Set-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r122-metrics.csv' -Encoding UTF8; while (((Get-Date)-$r122Started).TotalSeconds -lt 190) { $r122Process=Get-Process -Id $r122Pid -ErrorAction SilentlyContinue; if (!$r122Process) { break }; $r122Now=Get-Date; $r122Delta=($r122Now-$r122LastTime).TotalSeconds; $r122Cpu=if ($r122Delta -gt 0) {100*($r122Process.CPU-$r122LastCpu)/$r122Delta/[Environment]::ProcessorCount} else {0}; '{0},{1},{2},{3},{4},{5}' -f $r122Now.ToUniversalTime().ToString('o'),$r122Process.CPU,$r122Cpu,$r122Process.WorkingSet64,$r122Process.PrivateMemorySize64,$r122Process.Threads.Count | Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r122-metrics.csv' -Encoding UTF8; $r122LastTime=$r122Now; $r122LastCpu=$r122Process.CPU; Start-Sleep -Seconds 1 }
```

### r122 最终结果：所有权基础可运行，完整合屏仍FAIL

Release clean-first/all-target configure/build exit0，realtime CLI5652480B（2026-10-08 14:14:11），独立审查r121_ledger_trace与r121_ledger_review_b均PASS。进程：VLC35688（14:18:05）、dumpcap20516（14:18:06）、CLI34704（14:18:07）、AV FFmpeg35676（14:18:08）、VideoOnly FFmpeg34352（14:18:19）。两源各3600帧/120秒exit0，实际首包错峰10.9740174秒。capture180秒exit0，306232包/drop0；各视频106671、音频5229、RTCP各25、输出87586包，四条RTP序号连续。

AV容量停读14:18:12.509至seal14:18:20.198为7.689秒。应用first_missing8657/resumed11348，SSRC3150499658，在首20秒线端全部找到2691个对应包；停读缺口仍复现，不能标修复。wire观测4936021B，AV保留165801355B、allocation cap1085262308B；V-only cap1005131174B。引用队列实际占用下降，但保守规划上限上升，不是容量上限优化。

输出99400300B TS、CC错误0；4984视频包PTS步3000、7155音频包步2090（5694次）/2089（1460次），无负/零步、PTS=DTS。启动静音0.108458–3.223560（3.115102秒），失源静音107.671383–166.138776。105条drift raw_phase155..15938ns；内容A/V仍UNKNOWN。VLC live/left截图可见双格与左格/右黑，但右侧窗口超屏截断；black截图被桌面其他窗口遮挡，不能作为黑场视觉证据，不补造PASS。

188个采样06:18:08.0378441Z..06:21:17.9373227Z：峰WS227713024B、峰private714657792B（r121为1077166080B，减少362508288B）；排除首点CPU均值1.370049%、峰2.812883%。全失源后48点首/末10点WS均212537344B，private均699801600B，仅短期稳定，不证明长稳。monitor helper exit0。

CLI14:21:19.709 caller_stop、exit0，workers0/errors0，encoded60110入=出，payload95082预约=释放/0B/0对象。显式停止不等于可信EOF自然结束。VLC RC quit；五个精确PID复核均不存在。本轮未运行local/RKMPP或外部依赖修改，无push/merge/deploy。整体仍FAIL/75分，稳定引用源码PASS不能替代持续接收/RTCP移交、完整矩阵、恢复、内容同步等门禁。

显式停止命令：
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R122Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r122Target=34704
$r122Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r122Target"
if($r122Process.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r122Process.CommandLine -notlike '*composition-wiring-r122*'){throw 'PID identity mismatch'}
[R122Console]::FreeConsole() | Out-Null
if(-not [R122Console]::AttachConsole($r122Target)){throw 'AttachConsole failed'}
try {
if(-not [R122Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r122Ids=[uint32[]]::new(32)
$r122Count=[R122Console]::GetConsoleProcessList($r122Ids,32)
if($r122Count -ne 2 -or $r122Ids[0..($r122Count-1)] -notcontains $r122Target -or $r122Ids[0..($r122Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R122Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R122Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

真实链路结束后的离线分析命令：
```powershell
& 'D:\Wireshark\tshark.exe' -r 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r122-wire.pcapng' -T fields -e frame.time_epoch -e udp.dstport -e udp.payload > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r122-all-wire.tsv' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r122-tshark.log'
& 'D:\mabs\local64\bin-video\ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time -of json 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r122-wire.ts' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r122-packets.json' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r122-probe.log'
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r122-wire.ts' -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r122-silence.log' 2>&1
```

本节保存成功后清理以下20项；原120秒源保留：
- composition-wiring-r122-all-wire.tsv
- composition-wiring-r122-black.png
- composition-wiring-r122-capture.log
- composition-wiring-r122-cli-error.log
- composition-wiring-r122-cli.log
- composition-wiring-r122-left.png
- composition-wiring-r122-live.png
- composition-wiring-r122-metrics.csv
- composition-wiring-r122-received.ts
- composition-wiring-r122-source0.log
- composition-wiring-r122-source1.log
- composition-wiring-r122-tshark.log
- composition-wiring-r122-vlc.log
- composition-wiring-r122-wire.pcapng
- composition-wiring-r122-wire.ts
- composition-wiring-r122-packets.json
- composition-wiring-r122-probe.log
- composition-wiring-r122-silence.log
- composition-wiring-r122.sdp
- out/r122-build.log

清理复核：本轮20项文件已全部删除，原源保留；本机精确PID无残留，本轮无远程产物。

## 2026-10-08 r123：共享时钟会话原规格复验

时钟会话提取双独立源码PASS，Release clean-first/all-target成功。以下完整命令保持原120秒源、双路约11秒错峰与H264 VBR8000–12000kbps/1280×720/30、AAC CBR192kbps→MPEG-TS/RTP，结果待观测。

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r123-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r123-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r123 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r123.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r123-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r123-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r123-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r123-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r123-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r123- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r123-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

后台采样命令：
```powershell
$r123Pid=(Get-CimInstance Win32_Process -Filter "Name='media_transcode_realtime_video_cli.exe'" | Where-Object { $_.CommandLine -like '*composition-wiring-r123*' }).ProcessId; if(!$r123Pid){throw 'r123 CLI not found'}; $r123Started=Get-Date; $r123LastTime=$r123Started; $r123LastCpu=0; 'time_utc,cpu_seconds,cpu_machine_percent,working_set,private_bytes,threads' | Set-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r123-metrics.csv' -Encoding UTF8; while (((Get-Date)-$r123Started).TotalSeconds -lt 190) { $r123Process=Get-Process -Id $r123Pid -ErrorAction SilentlyContinue; if (!$r123Process) { break }; $r123Now=Get-Date; $r123Delta=($r123Now-$r123LastTime).TotalSeconds; $r123Cpu=if ($r123Delta -gt 0) {100*($r123Process.CPU-$r123LastCpu)/$r123Delta/[Environment]::ProcessorCount} else {0}; '{0},{1},{2},{3},{4},{5}' -f $r123Now.ToUniversalTime().ToString('o'),$r123Process.CPU,$r123Cpu,$r123Process.WorkingSet64,$r123Process.PrivateMemorySize64,$r123Process.Threads.Count | Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r123-metrics.csv' -Encoding UTF8; $r123LastTime=$r123Now; $r123LastCpu=$r123Process.CPU; Start-Sleep -Seconds 1 }
```

### r123 最终结果：会话封装可运行，完整合屏仍FAIL

Release configure/build exit0，realtime5654528B/2026-10-08 14:31:40；r121_ledger_trace和r121_ledger_review_b两位独立源码审查PASS。进程：VLC21480（14:32:11）、dumpcap35604（14:32:12）、CLI30848（14:32:13）、AV FFmpeg3696（14:32:14）、VideoOnly FFmpeg22280（14:32:25）。两源完整120秒/各3600帧exit0，实际首包错峰10.9646356秒。capture180秒exit0，306226包/drop0；视频各106671、音频5229、RTCP各25、输出87580包，四条RTP序号连续。

AV capacity_stop14:32:18.556至seal14:32:26.273为7.717秒；应用first_missing8861/resumed11571、SSRC806636943，2710对应包在线端首20秒全部存在。停读缺口未修复。输出99396728B TS、CC错误0；4986视频包PTS步3000，7158音频包步2090（5696次）/2089（1461次），无负/零步、PTS=DTS。启动静音0.107982–3.223923（3.115941秒）；失源静音107.651088–166.208435。105条drift raw_phase155..20346ns，内容A/V UNKNOWN。

182个采样06:32:14.1691002Z..06:35:18.1173456Z：峰WS229527552B、峰private717029376B，排除首点CPU均值1.825238%、峰3.772661%。全失源后38点首/末10点WS均值214110618/214111027B，private均701919232B，仅短期趋势。monitor helper在CLI消失后结束码1，CSV已核验，不标helper exit0。

14:35:18.654 CLI caller_stop exit0，workers0/errors0，encoded57733入=出，payload93415预约=释放、0B/0对象。VLC RC quit；精确PID21480/35604/30848/3696/22280全部不存在。桌面截图被其他应用遮挡；RC snapshot返回0但未产生文件，画面门禁证据不足，不称视觉PASS。显式停止非自然EOF，全失源后仍输出为局部证据。

没有local CLI/RKMPP、外部FFmpeg修改或push/merge/deploy。完整FAIL/75分保持；本次时钟session只有现有生产算法封装，prepared接管、AU归属、持续接收与恢复/完整矩阵等门禁尚未完成。

实际显式停止：
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R123Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r123Target=30848
$r123Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r123Target"
if($r123Process.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r123Process.CommandLine -notlike '*composition-wiring-r123*'){throw 'PID identity mismatch'}
[R123Console]::FreeConsole() | Out-Null
if(-not [R123Console]::AttachConsole($r123Target)){throw 'AttachConsole failed'}
try {
if(-not [R123Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r123Ids=[uint32[]]::new(32)
$r123Count=[R123Console]::GetConsoleProcessList($r123Ids,32)
if($r123Count -ne 2 -or $r123Ids[0..($r123Count-1)] -notcontains $r123Target -or $r123Ids[0..($r123Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R123Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R123Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

仅链路结束后执行的离线分析命令：
```powershell
& 'D:\Wireshark\tshark.exe' -r 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r123-wire.pcapng' -T fields -e frame.time_epoch -e udp.dstport -e udp.payload > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r123-all-wire.tsv' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r123-tshark.log'
& 'D:\mabs\local64\bin-video\ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time -of json 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r123-wire.ts' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r123-packets.json' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r123-probe.log'
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r123-wire.ts' -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r123-silence.log' 2>&1
```

本节保存成功后按以下18项清理，保留原120秒源：
- composition-wiring-r123-all-wire.tsv
- composition-wiring-r123-capture.log
- composition-wiring-r123-cli-error.log
- composition-wiring-r123-cli.log
- composition-wiring-r123-live.png
- composition-wiring-r123-metrics.csv
- composition-wiring-r123-received.ts
- composition-wiring-r123-source0.log
- composition-wiring-r123-source1.log
- composition-wiring-r123-tshark.log
- composition-wiring-r123-vlc.log
- composition-wiring-r123-wire.pcapng
- composition-wiring-r123-wire.ts
- composition-wiring-r123-packets.json
- composition-wiring-r123-probe.log
- composition-wiring-r123-silence.log
- composition-wiring-r123.sdp
- out/r123-build.log

清理复核：本轮18项已全部删除，原源保留；本机精确PID无残留，无远程产物。

## 2026-10-08 r124 每源事实规划 realtime 复验（待执行）

原120秒源，双路RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP。以下命令按VLC/抓包/CLI/AV源/延后11秒VideoOnly源启动；完整验收尚未通过。

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r124-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r124-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r124 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r124.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r124-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r124-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r124-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r124-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r124-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r124- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r124-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

### r124 实际结果：FAIL，首次时钟获取超时

Release全量configure/build exit0，realtime5652992B/2026-10-08 14:47:18；两名独立源码审查PASS（仅五文件迁移，不代表媒体通过）。VLC8596(14:47:30)、dumpcap37396(14:47:31)、CLI35432、AV30280(14:47:33)、VideoOnly35324(14:47:44)。CLI14:47:54.862在LockedPacketGate node33/34报Initial clock acquisition master deadline expired，自行exit1；video/audio commits=0，encodedPackets=0，workerErrors=1，payload1468预约=释放，最终0B/0对象。未达到完整120秒验收；判定失败后14:48:24按已核对身份的精确PID Stop-Process -Force清理两源、capture及VLC（源/capture shell exit1），不是源自然结束；CLI没有被清理结束。monitor23样本后exit1，不记工具通过。

AV视频capacity_stop14:47:37.088→sealed14:47:44.797，停读7.709秒；seq8747→11376缺2629，SSRC2698435219，抓包全部找到这2629包，不能归因源未发送，也不能单凭此定位内核。wire85450包：video61020=46398/audio61022=2261/video61030=36760，三个RTP流序号零跳变；RTCP61021/23各11、61031=9；输出62020=0，TS0B。强制终止capture没有正常drop统计，不能声称drop0。两源首包差10.977710秒。

AV组在gen0两路evidence到齐仍state0，gap后video gen1而audio再无发布；VideoOnly组正常locked。线端audio SR在14:47:48.160、53.165仍到达，初始AV SR NTP完全相同；尚需核对group有效性与上游背压，不能将超时直接归因facts迁移。23个CPU/内存样本06:47:32.6650092Z–06:47:55.0013675Z：WS峰229289984B/private峰682975232B，排首样本CPU均0.599930%/峰1.700136%；无A/V drift样本，无可验收画面。仍FAIL，Windows门禁未通过，不进入RKMPP，不推送/合并/部署。

## 2026-10-08 r125：首次时钟配对诊断（待执行）

与r124同规格，不改变阈值。新增候选NTP/arrival与拒绝原因日志；初审发现nodeId强类型日志编译问题，已改.value，两名审查者复审PASS。命令：

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r125-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r125-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r125 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r125.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r125-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r125-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r125-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r125-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r125-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r125- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r125-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

### r125 实际结果：链路运行结束，整体仍FAIL

Release全量configure/build exit0，realtime5656576B/2026-10-08 14:54:05。VLC37396/dumpcap31400均14:54:22，CLI31200 14:54:23，AV32444 14:54:24，VideoOnly33060 14:54:35；两源完整120秒/3600帧并exit0。180秒capture exit0，306023包/drop0。CLI观察全源结束后持续输出，再由核对console成员的GenerateConsoleCtrlEvent明确停止，caller_stop exit0；VLC经127.0.0.1:63020 RC quit退出。五PID均无残留。monitor185样本，CLI退出后helper exit1，不记工具PASS。

首视频包差10.924672秒。AV视频capacity_stop14:54:28.520→sealed14:54:36.191=7.671秒；SSRC2766658312 seq9768→12460缺2692包，线端均存在。wire：video61020/61030各106671，audio61022=5229，RTCP61021/23/31各25，输出62020=87377；四个RTP流零序号跳变。输出TS99228656B，CCerrors0；视频4969包PTS步长3000，音频7134包步长2090(5677)/2089(1456)，均无倒退/重复且PTS=DTS。capture对应输出约165.65秒，不能冒充完整caller_stop输出录制。

CLI最终workers0/errors0，encoded58545入=出，payload93994预约=释放、0B/0对象。185个样本06:54:24.0958749Z–06:57:30.8734267Z，WS峰227491840B/private峰713629696B；排首样本CPU均1.764565%/峰3.556920%。全失源06:56:40之后51样本，前后10均WS212892058→212914995B/private699197850→699233075B，仅短时趋势。105个raw_phase_ns样本155..16066，不等于内容A/V通过。

启动静音0.108798–3.269841=3.161043秒；全失源静音107.696984–165.651156。截图helper按VLC PID37396窗口抓取，能见左右两路画面及黑边，但右侧/下侧不完整，不能作为完整画面通过。截图路径composition-wiring-r125-vlc.png，已记录后按清单清理。

无rtp_clock_group_rejected；初始AV候选NTP4000431264/2310692405、arrival21021683883800完全一致，先video后audio完成配对。r124超时本轮未复现，新增诊断未改变任何策略，不能宣称修复或据此推断确定因果。保留停读缺口、启动静音、内容A/V UNKNOWN及原完整矩阵/恢复/可信EOF/C API/Windows→RKMPP门禁。整体FAIL，75分不变，无成功提交、push、merge或部署。

## 2026-10-08 r126 共享协议会话回归（待执行）

四文件提取经两个独立审查PASS，Release全量configure/build exit0。本轮先验证抽取前后生产链语义，新获授权逐源input-latency字段尚未接线。保持原120秒源、完整同规格，命令如下：

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r126-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r126-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r126 --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r126.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r126-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r126-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r126-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r126-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r126-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r126- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r126-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

### r126 实际结果：抽取回归运行结束，合屏完整门禁FAIL

Release全量configure/build exit0，realtime5665280B/2026-10-08 15:10:05。独立审查A/B均PASS，范围为4文件共享协议状态及node接线，不包含连续接管。VLC30392/dumpcap31332均15:11:13，CLI12412 15:11:14，AV7132 15:11:15，VideoOnly29836 15:11:26。两源完整120秒/3600帧exit0；capture180秒exit0，306049包/drop0。全源结束后CLI持续输出，最后验证console成员后GenerateConsoleCtrlEvent明确停止，caller_stop exit0；VLC经RC quit退出。五PID无残留。monitor188样本后exit0；监控成功不等于媒体验收通过。

AV首包差10.949849秒。capacity_stop15:11:19.715→sealed15:11:27.360=7.645秒；SSRC4281823047 seq10288→12964缺2676，线端全部存在。wire视频61020/61030各106671，audio61022=5229，RTCP61021/23/31各25，output62020=87403；四RTP流零序号跳变，TS99184476B/CCerrors0。视频4983包PTSstep3000；音频7154包PTSstep2090(5693)/2089(1460)，无负/零步长且PTS=DTS。capture输出约166.12秒，非完整caller_stop录制。

最终workers0/errors0，payload94764预约=释放、0B/0对象；encoded59767入/59766出，明确停止时有1包未推出，不记完全drain通过。188样本07:11:15.2697263Z–07:14:25.2371647Z，WS峰228773888B/private峰712232960B，排首样本CPU均1.654925%/峰3.771203%。全失源07:13:35后50样本，前后10均WS213488026B/private697491456B；仅短时趋势。105个raw_phase155..6996ns，内容A/V仍UNKNOWN。

启动静音0.131905–3.246825=3.114921秒；全失源107.743696–166.115556静音。VLC窗口截图未取得媒体画面，不能作为视觉通过。无process.failed/clock_group_rejected；r124时钟超时未复现未判修复。原停读与缺包仍在，整体75/FAIL，未进入RKMPP，无push/merge/deploy。新授权每源input-latency尚未接线，本轮命令保持旧策略，不能用本轮替代新策略验证。

## 2026-10-08 r127 逐源等待预算验证（待执行）

用户已明确授权逐源可配置input-latency；源0=100ms、源1=200ms仅为显式本机验证值，无产品默认。新增typed wait贯穿bootstrap/probe/final，合屏每源必填；旧C ABI不变，尚未实现新C合屏入口。媒体规格保持原120秒1280×720/30、H264 VBR8000–12000kbps、AAC CBR192kbps，双RTP/RTCP→MPEG-TS/RTP。命令：

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r127 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r127- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

### r127 首次构建失败及修复

首次Release全量失败，build exit2/script exit1：MediaRealtimeInputPlanner.cpp:206新增validateInput(request.input)误置于仅接收avSync的clockTransportPolicy helper，C2065/C3536；初次两名源码审查未发现该编译问题，不以初审PASS掩盖失败。已将校验移到planRawRtp(request,avSync)入口，两名重新复审，随后重新全量构建。未据失败产物运行媒体。

### r127 参数拒绝补充验证（待执行）

仅验证真实CLI参数校验，不替代完整媒体验收；不启动源。分别省略源0值、将源1置0，预期立即失败：

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r127-missing --inputs "--input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-missing.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-missing-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-missing-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r127-zero --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 0 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-zero.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-zero-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r127-zero-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

r127第二次Release全量configure/build exit0，realtime5669888B/2026-10-08 15:31:28。两个参数拒绝CLI均立即exit2：missing PID24452报each composition source requires --input-latency-ms；zero PID38140报--input-latency-ms requires RTP input and a positive value。无源启动、无CLI残留，仅参数校验补充证据。4项stdout/stderr临时日志在本记录后删除。

### r127 实流结果：逐源值已生效，合屏整体FAIL

修复后两名独立审查重新PASS；第二次Release全量configure/build exit0，realtime5669888B/2026-10-08 15:31:28。VLC32384/dumpcap30280均15:32:36，CLI4664 15:32:37，AV31008 15:32:38，VideoOnly27464 15:32:49。两源完整120秒/3600帧exit0；capture180秒exit0，306140包/drop0。全源结束后CLI持续输出，核对console成员后GenerateConsoleCtrlEvent明确停止，caller_stop exit0；VLC经RC quit退出，五PID均无残留。monitor187样本exit0。

节点rtp_reorder于15:32:50.719/.720依次报告100000000/200000000/200000000ns，证明源0独立100ms、源1视频与音频共用200ms；window均76，时间和容量界限独立。无clock_group_rejected/process.failed，不宣称r124超时已修复。首视频包差10.930748秒。AV视频capacity_stop15:32:42.990→sealed15:32:50.658=7.668秒；SSRC3668704099 seq11013→13704缺2691，线端全部存在。

wire视频61020/61030各106671、audio61022=5229、RTCP61021/23/31各25、output62020=87494；四RTP流零序号跳变，TS99286372B/CCerrors0。视频4979包PTSstep3000；音频7147包PTSstep2090(5688)/2089(1458)，无负/零步长，PTS=DTS。capture约165.95秒输出，非完整caller_stop录制。

最终workers0/errors0，encoded64034入=出；payload97545预约=释放、0B/0对象。187样本07:32:38.5929658Z–07:35:47.8269137Z，WS峰228589568B/private峰713986048B；排首样本CPU均1.816124%/峰3.363967%。全失源07:35:00后48样本，前后10均WS213360640→213442560B/private698474496→698556416B，仅短时趋势。105个raw_phase155..7124ns，内容A/V仍UNKNOWN。

启动静音0.178005–3.293583=3.115578秒，全失源107.720317–165.953016静音。VLC截图可见左右媒体画面和黑边，但右侧/下侧不完整，不记完整视觉通过。参数校验/传递证据充分不等于合屏验收通过；停读/缺包仍FAIL，完整矩阵、恢复、可信EOF、C API、Windows→RKMPP仍未通过，保持75分，无成功提交/push/merge/deploy。

## 2026-10-08 r128 无音频候选等待补充验证（待执行）

只发送两个原规格视频流，不发送所选音频；观察候选是否等待真实证据，截止仍是原30秒。既有有限prepared容量可能先耗尽并结束capture，这是合法失败边界，不应冒充完整30秒等待。此负例可能中止源，不能替代后续完整120秒验收。实际命令：

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r128-noaudio --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-noaudio.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-noaudio-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-noaudio-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-noaudio-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-noaudio-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-noaudio-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r128-noaudio- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-noaudio-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

### r128 无音频负例结果（补充证据，不是完整验收）

Release configure/build exit0，realtime5679616B/2026-10-08 15:56:06。CLI8200约15:56:44启动、约8.42秒后自然exit1；VLC34232于15:56:43.947，FFmpeg24508/61020于15:56:46.584、2424/61030于15:56:47.099。视频capacity_stop分别15:56:50.796/.51.300，AV共享wire观察4935627/5000000B；音频零报文，15:56:52.704接收线程capacity_stop后报RTP ingress observation cannot become ready in the current capture phase。没有把音频未到误判为立即失败，但由于有限容量先结束capture，不能记为等待完整30秒或连续接收通过。

CLI已自然失败后，核验Name/CommandLine，Stop-Process -Id 24508与2424结束本例源（两个session exit1，非源自然完成）；VLC34232通过TCP63020 quit退出。四PID复核无残留。monitor7样本exit1（CLI消失），峰WS28897280B/private273489920B，无DAG输出和A/V漂移样本。没有进行完整120秒验收，结果仅证明候选等待和容量结束失败路径。本例命令如上，源码修改仍需下述原规格全链验证。

临时清单为composition-wiring-r128-noaudio-前缀的cli.log、cli-error.log、metrics.csv、source0.log、source1.log、vlc.log、received.ts（仅删除实际存在项）；记录后逐项删除，保留原源和Release产物。

## 2026-10-08 r128 原规格全链复验（待执行）

双独立源码复审PASS及Release全量成功后，原120秒双路RTP/RTCP、H264/AAC输入→固定双格1280×720/30、H264 VBR8000–12000kbps/AAC CBR192kbps→MPEG-TS/RTP；显式输入等待源0=100ms、源1=200ms，AV先启动，VideoOnly约11秒后启动。capture180秒；源自然完整结束后观察all-loss持续输出，再显式caller_stop。临时指标按秒采集CPU/内存，检查内部A/V telemetry；本轮不运行local CLI。

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r128 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r128- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r128-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

### r128 原规格实流结果：快照候选接线有效，合屏整体FAIL

两名独立审查修复后均PASS；Release全量configure/build exit0，realtime5679616B/2026-10-08 15:56:06。dumpcap10596于15:58:51.740，VLC9576于15:58:52.117，CLI18512于15:58:52.740，AV32660于15:58:54.246，VideoOnly32236于15:59:05.212。两源各完整120秒/3600帧自然exit0；capture180秒exit0、305961包/drop0。全源结束后持续输出；核验CLI命令行与console精确成员后GenerateConsoleCtrlEvent，16:02:14.132 caller_stop exit0。VLC经TCP63020 quit退出，五个PID均无残留，monitor187样本exit0。

节点100000000/200000000/200000000ns、window76。AV Video capacity_stop15:58:58.248→sealed15:59:05.942=7.694秒；Audio15:58:58.277→15:59:05.943=7.666秒。首视频源到达差10.962309秒；SSRC2808208024 seq8959→11650缺2691，线端首20秒内2691均存在。wire61020/61030各106671、61022=5229、RTCP61021/23/31各25、62020=87315；四RTP流序号跳变0。TS99155336B、CCerrors0。

输出视频4967包、PTS步长3000；音频7131包、PTS步长2090(5675)/2089(1455)，两者负步长/零步长0、PTS=DTS。抓包覆盖约165.62秒输出，不等同完整caller_stop录制。启动静音0.139456–3.246757=3.107302秒；全失源静音107.69737–165.581497。105个raw_phase155..6996ns仅内部时序，不是内容A/V同步证明，内容A/V仍UNKNOWN。

187指标样本07:58:53.8350943Z–08:02:02.8894976Z，WS峰228970496B/private峰707592192B；排首样本CPU均1.707066%/峰3.424354%。全失源08:01:08后55样本，前后10均WS215133388.8→215154688B/private695090380.8→695123968B，仅短时趋势。最终workers0/errors0，encoded63029入=出，payload96931预约=释放/0B/0对象；totalPushed260432/totalPopped260431，不把显式停止记成全图所有事件完全drain。无clock_group_rejected；仅16:02:14.081显式停止时4条process.failed均Cancelled（sender reservation wait和3路Windows receive），无运行期失败结论被隐藏。

VLC双源截图可见两画面/黑边但受DPI影响裁切，不记完整双源视觉PASS；第二张在同一截图进程先调用SetProcessDPIAware，再运行既有take_screenshot.ps1，获得完整VLC窗口的全黑all-loss画面。截图命令使用本轮精确VLC9576 MainWindowHandle，路径分别composition-wiring-r128-vlc.png与-vlc-all-loss.png。后续窗口截图应先设置进程DPI感知，避免把裁切当作合屏几何错误。

本轮证明候选/最终容量核验进入真实生产路径及等待修复；未实现持续接收、AU归属/滚动退休或会话交接。原停读/线端缺包/启动静音仍FAIL，r124初始时钟超时本轮未复现不判修复；完整矩阵、恢复、可信EOF、C API与Windows→RKMPP门禁仍未通过。无成功测试提交/push/merge/deploy。

清理清单：composition-wiring-r128-前缀all-wire.tsv、capture.log、cli-error.log、cli.log、metrics.csv、packets.json、probe.log、received.ts、silence.log、source0.log、source1.log、tshark.log、vlc.log、vlc.png、vlc-all-loss.png、wire.pcapng、wire.ts，以及composition-wiring-r128.sdp、out/r128-build.log，共19项；本记录后逐项删除，保留原源124427809B和正式Release构建产物。无远程运行/远程临时文件。

## 2026-10-08 r129 原规格复验（已执行，完整门禁 FAIL）

ClockGroup共享核心由真实节点消费；首次审查遗漏的mapper上界已改为共用原validateConfig，修复后两名独立复审PASS。首次构建因审查问题主动中止，修复后重新全量；必须成功后才运行以下原120秒双源RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP。保持100/200ms逐源值、约11秒错开启动、硬件按planner；不测试local。

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:180 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r129-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r129-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r129 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r129.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r129-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r129-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r129-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r129-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r129-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r129- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r129-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

### r129 实际结果：源码抽取通过，完整合屏仍 FAIL

本轮将 RTP clock group 的 mapper、validator、generation 下限和事件序号状态收敛到可移动、单所有者 MediaRtpClockGroupSession，生产节点调用同一核心；节点仍负责通道选择、调度和发布。准备接收端尚未接入此核心，不宣称连续交接完成。初审发现 session 创建缺少 mapper 的完整速率边界校验，已提取原 validateConfig 复用，两个独立审查者复审均明确 PASS。最终 Release 全量 693 项构建 exit0；实际产物 5682688B，16:25:36。此前主动取消的构建 exit1 不计构建失败修复或通过证据。

- 实际进程：VLC40216（16:26:31），dumpcap11256（16:26:32），CLI33176（16:26:32），AV FFmpeg29200（16:26:33），纯视频 FFmpeg36960（16:26:44）。上述原命令见本节前文；两源均原文件120秒、3600帧、exit0。源线端首包相差11.018577秒。逐源输入等待节点实测100/200/200ms；仅为本机命令值，无产品默认值。
- AV video capacity_stop 16:26:37.926→sealed16:26:45.705，7.779秒；audio7.757秒。SSRC948817081，first_missing10619→resumed13368，2749包；抓包确认这些包全到达线端。未修复准备停读根因。
- 抓包180秒 exit0：306093包，捕获丢包0。视频61020/61030各106671包、音频5229包，三个RTCP端口各25包，输出87447包；各RTP序号无缺口。输出线端16:26:45.880→16:29:32.034，TS99243132B，CC错误0。
- 离线 ffprobe exit0：视频4983包、PTS步长3000；音频7154包、PTS步长2090×5693、2089×1460。无负值或重复PTS，PTS=DTS。离线静音分析exit0：0.155193–3.153741秒（2.998549秒），107.581361–166.115556秒（58.534195秒）。静音和时间戳不是内容A/V同步证明；内容A/V仍UNKNOWN。
- 187个外部采样，机器归一化CPU均值1.799703%、峰3.760687%，WS峰230248448B/private峰716738560B。08:28:48 UTC之后54个样本，首/尾10样本平均WS212819968→212820787.2B，private699379712→699379712B；这是有限观测期平稳，不是无泄漏证明。监控16:29:42结束，之后到调用方停止没有外部CPU/内存逐秒覆盖。
- 105条raw_phase为155–17886ns，仅内部相位。未出现clock_group_rejected；4条process.failed均在调用方停止时为Cancelled，不是运行期媒体失败。
- 运行期完整VLC截图2582×1550确认左右双画面和上下黑边。全丢失截图仅237×39，窗口覆盖不足，不作为完整黑屏视觉证据；输出持续及静音有线端/离线证据，完整全丢失视觉证据本轮未取得。
- 两源停止后CLI继续输出符合合屏连续输出场景。抓包自然结束后，16:32:37向身份核验后的CLI控制台发送Ctrl-C；CLI exit0/end_reason=caller_stop，不是源EOF自然退出。final errors/workerErrors=0，workers=0，totalPushed/Popped362281/362281、encoded117744/117744，payload132796预约=释放、0B/0objects；graphPayloadPressureFailures22425仍须结合背压语义评估，不能写成“所有压力为零”。VLC经RC quit退出；五个精确PID均无残留。
- 完整门禁仍FAIL；r124初始化时钟超时本轮未复现，不宣称已修复。未进入RKMPP，未改外部FFmpeg，未提交/推送/合并/部署。

离线检查实际命令（真实流结束后）：
```powershell
& 'D:\mabs\local64\bin-video\ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time -of json 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-wire.ts' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-packets.json' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-probe.log'
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-wire.ts' -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-silence.log' 2>&1
```

调用方停止实际命令（先核验进程和控制台成员）：
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R129Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r129Target=33176
$r129Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r129Target"
if($r129Process.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r129Process.CommandLine -notlike '*composition-wiring-r129*'){throw 'PID identity mismatch'}
[R129Console]::FreeConsole() | Out-Null
if(-not [R129Console]::AttachConsole($r129Target)){throw 'AttachConsole failed'}
try {
if(-not [R129Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r129Ids=[uint32[]]::new(32)
$r129Count=[R129Console]::GetConsoleProcessList($r129Ids,32)
if($r129Count -ne 2 -or $r129Ids[0..($r129Count-1)] -notcontains $r129Target -or $r129Ids[0..($r129Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R129Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R129Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

临时清理清单（先记录以上证据，再逐项删除，保留原源和正式构建产物）：
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-all-wire.tsv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-capture.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-cli-error.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-cli.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-metrics.csv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-packets.json
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-probe.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-received.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-silence.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-source0.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-source1.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-tshark.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-vlc.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-vlc.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-wire.pcapng
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-wire.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129.sdp
- D:\Code\MyCode\MediaTranscode\out\r129-build.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r129-vlc-all-loss.png

清理核验：上述19项本轮文件已逐项删除，无文件残留；PID33176/40216/11256/29200/36960均不存在，原源124427809B保留。本轮无远程测试产物。独立评分复核保持75/FAIL。


## 2026-10-09 r130 连续准备接收原规格复验（执行中）

首轮两独立审查 FAIL 后修复五项，第二轮均 PASS。首次全量 Release 因首帧 probe 头文件漏 MediaBufferRef 定义失败；补显式 include 后两者复审 PASS，第二次全量 configure/build exit0。realtime 5786624B，09:40:40。仅执行 realtime；原源已 ffprobe 确认120秒、1280×720/30、3600帧、H264 8013434bps、AAC44100双声道。抓包窗口200秒，源仍各完整120秒，错开约11秒；延长抓包用于覆盖启动后全失源观察，不降低原规格。

实际启动命令：

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:200 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r130 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r130- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```


### r130 首次启动中止（测试编排未满足原打开期限，不计完整验收）

实际capture29988于09:43:01.812启动，VLC21728于09:43:05.010启动；CLI34704在源0纯视频未发送时到达原30秒打开期限，自然exit1，错误为video signaling probe timeout、matching_rtp_packets=0、parameter_sets=none。AV FFmpeg35776直到09:43:27.349才启动；上列源0纯视频61030命令未执行。CLI退出后中止该轮，不能用此不完整链路判合屏门禁通过或把超时归因于r130实现。已看到AV接收线程切入windows_overlapped_completion_queue，不能替代完整验证。

监测7样本，CPU均0.0497337%/峰0.140013%，WS峰27291648B/private峰279343104B；仅启动阶段，未形成输出、A/V漂移或视觉证据。清理前AV日志最后1579帧/52.63秒，不是完整120秒通过。核对名称/命令行后已强制清理AV35776、capture29988、VLC21728；CLI34704此前自然结束。清理退出不计自然结束，capture中止不记drop0或完整抓包。首次写入该结果遇Python默认GBK解码失败，诊断保留于工具输出，现已显式UTF8补录；尚未删除文件。

临时清单为composition-wiring-r130前缀的capture.log、cli.log、cli-error.log、metrics.csv、source0.log、source1.log、vlc.log、received.ts、wire.pcapng、sdp及预列诊断衍生文件，仅删除实际存在项；保留原120秒源和Release产物。后续复验使用r130b独立前缀，在同一次工具调用中编排启动时序。


r130首次启动清理已完成：AV/capture工具session均exit1（强制清理），四PID复核无残留，实际8项临时文件逐项删除，原源保留，无远程运行。

### r130b 固定时序复验（已结束）

同一工具调用先启动capture/VLC/CLI，约2秒后启动AV，约11秒后启动纯视频；不改变30秒open timeout，不缩短源。实际命令如下，结果另记：

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:200 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130b-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130b-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r130b --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130b.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130b-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130b-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130b-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130b-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130b-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r130b- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130b-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

### r130b 结果（2026-10-09，双源 RTP H.264/AAC → MPEG-TS/RTP H.264 VBR 1280×720/30fps 8000kbps、AAC CBR 192kbps）

Release 全量重新生成第二次完成全部699目标，exit0；第一次缺少 MediaBufferRef 显式包含导致编译失败，补充头文件后两名独立审查者均再次明确 PASS。其余五项首轮审查问题修复后，两者均已明确 PASS。仅测试 realtime CLI；外部 FFmpeg 未修改。

实际 PID：capture36104、VLC31508、CLI35468、AV源34388、纯视频源34892。两源均以原120秒文件完成3600帧、exit0；实际进程启动差10.835754秒，首个视频线端包差10.838467秒。capture完整200秒exit0，306922包、drop0。CLI全失源后保持输出，09:49:36.620经核验控制台显式停止，caller_stop/exit0；不是自然EOF。VLC通过本轮RC端口63020发送quit，五PID均已退出。

离线 tshark 提取 UDP payload，按 RTP CSRC/extension/padding 解析输出TS；输入61020/61030各106671 RTP包、61022共5229包，输出62020共88276包，各自SSRC序列均无缺号、重复或乱序。RTCP61021/61023/61031各25包。输出抓包区间186.658859秒，TS97901940字节，PAT/PMT/video/audio PID连续计数错误均0。该结果是线端证据，不能单独证明接收应用消费了全部包。

ffprobe exit0：视频5598包，PTS0..16791000，全部步长3000；音频8037包，PTS0..16793600，步长2090共6396次、2089共1640次；两流PTS=DTS，无倒退或重复。抓包在CLI停止前结束，统计只覆盖抓包区间。
离线silencedetect（-80dB、最小0.5秒）exit0，仅发现108.267166..186.618776秒静音，持续78.351610秒；未再发现此前3.114921秒启动静音。此检测不能排除低于0.5秒或阈值以上的异常，不替代内容A/V同步证明。

VLC完整窗口已观察双640×360画面位于1280×720画布，上下黑带；全失源后观察完整黑画面。首次窗口句柄失效重试成功；全失源首张被浏览器遮挡、不能作证，置顶后重拍完整VLC窗口并恢复置顶状态。未取得单源丢失画面证据。

监控207样本：机器归一化CPU均1.330625%/峰2.568902%；WS峰241356800B，private峰725454848B。全失源安全区间01:48:00..01:49:23UTC共83样本，CPU均0.921705%，WS215580672..216395776B，首尾215601152B；private701534208..702328832B，尾701837312B。短时平台不证明长期内存上界。
107条av_drift_trace：raw_phase155..156ns、filtered_phase156ns，filtered_frequency/stretch均0；仅内部时钟证据，不证明媒体内容同步。
最终workerErrors/errors/stalledIntervals/droppedBuffers均0；payload预约=释放104992，结束0B/0对象，高水16629444B/895对象；pressureFailures23039，需区分额度等待与致命失败，不能宣称从未遇背压。encoded推入73852/推出73851，显式停止差1包，不能记完全drain。
完整CLI日志未匹配capacity_stop、rtp_discontinuity、clock_group_rejected、process.failed或encoder timeline discontinuity。连续汇总：video_commits6609、audio_commits9487，source0真实3572/黑3037帧，source1真实3248/黑3361帧；real_samples4773888、silence_samples4940800。stderr存在视频color range unknown→tv属性警告，未导致启动失败。

结论：本规格启动静音回归与连续输出观测通过；不据此判定全部输入应用消费、任意突发容量或完整合屏验收通过。断流恢复、内容A/V同步、完整链路矩阵及Windows后RKMPP尚未闭合。当前不提交、不推送、不部署。

#### r130b 离线诊断与显式停止实际命令

```powershell
& 'D:\Wireshark\tshark.exe' -r 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-wire.pcapng' -T fields -e frame.time_epoch -e udp.dstport -e udp.payload > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-all-wire.tsv' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-tshark.log'
```

```powershell
@'
import pathlib,collections,json,csv
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r130b-')
states={}; counts=collections.Counter(); ts=bytearray()
for line in pathlib.Path(str(p)+'all-wire.tsv').read_text(encoding='utf-8-sig').splitlines():
 a=line.split('\t')
 if len(a)<3 or not a[1] or not a[2]: continue
 t=float(a[0]); port=int(a[1]); b=bytes.fromhex(a[2].replace(':',''));counts[port]+=1
 if port not in (61020,61022,61030,62020) or len(b)<12 or b[0]>>6!=2:continue
 seq=int.from_bytes(b[2:4],'big');ssrc=int.from_bytes(b[8:12],'big');key=(port,ssrc)
 s=states.setdefault(key,dict(count=0,first=t,last=t,gaps=0,missing=0,reorder=0,duplicate=0,prev=None))
 if s['prev'] is not None:
  d=(seq-s['prev'])%65536
  if d==0:s['duplicate']+=1
  elif d>32768:s['reorder']+=1
  elif d>1:s['gaps']+=1;s['missing']+=d-1
 s['prev']=seq;s['last']=t;s['count']+=1
 if port==62020:
  n=12+4*(b[0]&15)
  if b[0]&16:n+=4+4*int.from_bytes(b[n+2:n+4],'big')
  end=len(b)-(b[-1] if b[0]&32 else 0)
  ts.extend(b[n:end])
pathlib.Path(str(p)+'wire.ts').write_bytes(ts)
cc={};bad=collections.Counter();packets=collections.Counter()
for i in range(0,len(ts)-187,188):
 b=ts[i:i+188]
 if b[0]!=71:raise ValueError(('sync',i))
 pid=((b[1]&31)<<8)|b[2];afc=(b[3]>>4)&3;c=b[3]&15
 if pid==8191:continue
 packets[pid]+=1
 disc=bool(afc&2 and b[4]>0 and b[5]&128)
 if disc:cc.pop(pid,None)
 if afc&1:
  if pid in cc and c!=(cc[pid]+1)%16:bad[pid]+=1
  cc[pid]=c
print(json.dumps({'rtp':{str(k):v for k,v in states.items()},'udp':dict(counts),'ts_bytes':len(ts),'ts_packets':dict(packets),'ts_cc_errors':dict(bad)},indent=2))
'@ | python -
```

```powershell
& 'D:\mabs\local64\bin-video\ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time -of json 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-wire.ts' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-packets.json' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-probe.log'
```

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-wire.ts' -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-silence.log' 2>&1
```

```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R130bConsole {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r130bTarget=35468
$r130bProcess=Get-CimInstance Win32_Process -Filter "ProcessId=$r130bTarget"
if($r130bProcess.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r130bProcess.CommandLine -notlike '*composition-wiring-r130b*'){throw 'PID identity mismatch'}
[R130bConsole]::FreeConsole() | Out-Null
if(-not [R130bConsole]::AttachConsole($r130bTarget)){throw 'AttachConsole failed'}
try {
if(-not [R130bConsole]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r130bIds=[uint32[]]::new(32)
$r130bCount=[R130bConsole]::GetConsoleProcessList($r130bIds,32)
if($r130bCount -ne 2 -or $r130bIds[0..($r130bCount-1)] -notcontains $r130bTarget -or $r130bIds[0..($r130bCount-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R130bConsole]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R130bConsole]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

#### r130b 审查与清理

独立审查A明确阶段PASS、完整FAIL；B已复核原日志与两张完整截图，确认阶段证据。下一项优先所选音频源断流恢复，须区分同会话丢包和重启发送器导致SSRC变化。清理清单如下，保留原120秒源及Release正式产物；无远程测试。
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-all-wire.tsv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-capture.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-cli-error.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-cli.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-metrics.csv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-packets.json
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-probe.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-received.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-silence.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-source0.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-source1.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-tshark.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-vlc.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-vlc.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-vlc-all-loss.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-wire.pcapng
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b-wire.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r130b.sdp

清理完成：18项本轮临时文件逐项删除，r130b前缀无残留，五PID均不存在；原120秒源124427809B保留。独立审查B最终明确阶段PASS、完整FAIL，未独立重算wire TSV，线端精确统计来自主智能体已执行分析。git diff --check成功。

## r131 所选AV源完整结束后重入（已结束，有限双审PASS）

沿用r130b原规格；初始AV先于纯视频约11秒。两源完整结束后观察全失源，再以新FFmpeg进程播放完整120秒AV源。等待100/200ms为本次部署值。仅Windows realtime，无外部依赖改动。

```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:340 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r131-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r131-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r131 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r131.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r131-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r131-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r131-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r131-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r131-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r131- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r131-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

实际PID：capture38312、VLC28092、CLI30260、初始AV36048、纯视频3776；两初始源完整3600帧exit0。全黑后10:00:25启动重入AV9624，命令如下。10:00:35观察clock generation2已锁定，完整右格恢复截图已取得；仍待完整120秒及离线音频/时间线核验。
```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r131-source-reentry.log 2>&1
```

实际停止命令（caller_stop，不计自然EOF）：
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R131Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r131Target=30260
$r131Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r131Target"
if($r131Process.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r131Process.CommandLine -notlike '*composition-wiring-r131*'){throw 'PID identity mismatch'}
[R131Console]::FreeConsole() | Out-Null
if(-not [R131Console]::AttachConsole($r131Target)){throw 'AttachConsole failed'}
try {
if(-not [R131Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r131Ids=[uint32[]]::new(32)
$r131Count=[R131Console]::GetConsoleProcessList($r131Ids,32)
if($r131Count -ne 2 -or $r131Ids[0..($r131Count-1)] -notcontains $r131Target -or $r131Ids[0..($r131Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R131Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R131Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

截图使用既有 screenshot 技能脚本，按核验PID28092的窗口暂时置顶后捕获并恢复。初始、所选源失源、全失源、右格恢复四张完整1302×888图有效；最终失源图仅237×39，不计视觉证据。重入源9624完整3600帧/120秒exit0；CLI30260明确caller_stop/exit0，VLC经本轮RC端口quit关闭。

### r131 结果：原规格所选AV源结束后重入，有限双审PASS

初始AV与纯视频、重入AV均完整120秒/3600帧、exit0。重入发生在匹配BYE及源失活后，不覆盖旧身份未清除前快速SSRC切换。新AV视频SSRC2741182203、音频2525730185；原AV视频3940481817、音频1816180386。输出始终SSRC21011408，169062 RTP包无缺号/重复/乱序；三个视频发送会话各106671包、两个音频会话各5229包，各自序列完整。capture340秒exit0，499658包/drop0；六媒体PID复核均不存在。

离线tshark提取与r130b同算法，TS198048976B，PID0/256/257/258连续计数错误0。ffprobe exit0：视频8275包，PTS0..24822000，步长3000共8274；音频11879包，PTS0..24822596，2090共9454、2089共2424；两流PTS=DTS、无倒退/重复。输出线端持续275.870379秒。没有输出会话重建证据。
离线silencedetect(-80dB,d=0.5) exit0：108.135601..135.531156秒失源静音27.395556秒；重入后非静音持续至253.984127秒，再静音至275.829841秒（21.845714秒）。无旧约3秒启动静音。证明本次输出恢复非静音，不证明内容级A/V偏移或所有音频内容正确。

监控262样本CPU均1.375714%/峰2.535894%，WS峰242659328B、尾234016768B，private峰724103168B、尾715788288B。内部漂移generation1共107条155..156ns，generation2共117条2203..2204ns，不能替代内容同步。
最终caller_stop/exit0，workerErrors/errors/stalledIntervals/droppedBuffers均0，payload162868预约=释放、0B/0对象，高水16652070B/902对象；pressureFailures40204。encoded92499入/92498出，显式停止差1包，不记完全drain。完整CLI未匹配capacity_stop、rtp_discontinuity、clock_group_rejected、process.failed或encoder timeline discontinuity。

可观察链路为双格→右黑左画面→全黑→仅右格恢复；最后失源截图尺寸异常不作视觉证据，但静音及输出包延续有离线证据。完整合屏仍待快速身份切换、内容A/V、完整矩阵、可信EOF、C API及Windows之后RKMPP，不因本项提升75/FAIL。不修改外部FFmpeg、不提交/推送/部署。

#### r131 离线实际命令

TS提取使用上文r130b完整inline Python，仅将前缀替换为r131；未保存测试脚本。

```powershell
& 'D:\Wireshark\tshark.exe' -r 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-wire.pcapng' -T fields -e frame.time_epoch -e udp.dstport -e udp.payload > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-all-wire.tsv' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-tshark.log'
```

```powershell
& 'D:\mabs\local64\bin-video\ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time -of json 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-wire.ts' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-packets.json' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-probe.log'
```

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-wire.ts' -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-silence.log' 2>&1
```

r131 stderr保留非致命color range unknown→tv滤镜属性警告；显式停止时AAC报告2 frames left in the queue on closing，再次确认本轮不计完整drain。最终失源小图尺寸经PIL实际读取为237×39，其余四张1302×888。

#### r131 双独立复核与清理

A/B均明确有限阶段PASS：匹配BYE及失活后的新发送器重入。两者直接核对日志/画面，未独立重算TSV；主智能体线端统计边界保持。完整合屏仍FAIL。按以下精确清单清理本轮产物，原120秒源与正式Release保留：
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-all-wire.tsv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-capture.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-cli-error.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-cli.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-metrics.csv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-packets.json
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-probe.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-received.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-silence.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-source0.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-source1.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-tshark.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-vlc.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-vlc.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-vlc-all-loss.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-wire.pcapng
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-wire.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131.sdp
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-source-reentry.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-vlc-reentry.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-vlc-selected-loss.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r131-vlc-final-loss.png

清理已完成：22项逐项删除，r131前缀无残留，六媒体PID不存在，原120秒源124427809B保留；无远程运行。

## r132 快速发送器重入故障定位（已结束，FAIL）

仅补充诊断：双源稳定后强制停止精确AV源PID，立即启动同规格新AV源，未等待BYE/超时/purge；原120秒源与参数不变，提前故障注入不算完整验收。实际基础命令：
```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:100 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r132-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r132-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r132 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r132.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r132-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r132-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r132-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r132-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r132-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r132- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r132-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

### r132 结果：快速换SSRC确定失败（诊断FAIL，不计完整验收）

PID：capture29744、VLC37576、CLI36800、旧AV7760、纯视频33508、新AV34684。切换前代1音频工作；以下精确PID命令结束旧AV并立即启动新AV：
```powershell
$old=Get-CimInstance Win32_Process -Filter 'ProcessId=7760'; if($old.Name -ne 'ffmpeg.exe' -or $old.CommandLine -notlike '*61022*' -or $old.CommandLine -notlike '*test-continuous-120s.mp4*'){throw 'Source identity mismatch'}; Stop-Process -Id 7760 -Force; & D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r132-source-reentry.log 2>&1
```
线端旧AV最后视频1791511662.374816，新视频1791511662.443628，间隔68.812ms；SSRC3445537101→3050308399，音频26932517→3490690006。10:07:42.443 Raw25/26 process.failed：Unsupported: RTP source identity changed; preparation requires replanning，CLI自然exit1。源未经历完整BYE/失活窗口，与r131慢重入场景不同。随后取消错误来自全图abort；10:07:42.444 sender另报physical service spacing，发生在主错误之后，未将其静默忽略或宣称已修复。

调用链：原capture→PreparedRtpSourceSession::consume→protocol.observeRtp→tracker/重排产生SsrcChanged→SourceSession::drain无条件准备期拒绝→共享owner错误→Raw25/26→全图abort。原Raw节点仅在shared-playback的显式replan_session选项下拒绝身份变化，合屏不设置该选项。此为连续owner将准备期限制延伸到runtime的回归，需保留准备期拒绝与PT/媒体契约校验，runtime复用既有源代际恢复。

capture完整100秒exit0，134754包/drop0；输出SSRC2006855793，仅持续32.431805秒，不能记持续合屏成功。监控23样本CPU均1.696339%/峰2.374772%，WS峰237498368B/private峰717533184B。最终errors/workerErrors4，payload24393预约=释放、结束归零；encoded10865/10864，仍非完全drain。未截图、未取得本轮音频恢复或内容同步证据。源日志最后旧AV1315帧43.83秒、纯视频1656帧55.20秒、新AV680帧22.66秒；诊断中止均工具exit1，不记完整120秒。

CLI已自然失败后核验身份清理33508/34684/37576；原7760在故障注入时强制结束，capture自然exit0。原120秒源及Release保留。权威对照：[GStreamer rtpbin](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpbin.html)区分SSRC与PT流，[RFC3550](https://www.rfc-editor.org/rfc/rfc3550.html)按SSRC维护源状态；本项目固定端口单活动源用已有generation/purge实现替换，不引入任意多发送者或格式热切换。

r132清理完成：13项实际临时产物按本轮清单删除，r132前缀无残留，六媒体PID不存在，原120秒源124427809B保留；无远程运行。

## r133 快速重入修复复验（已结束，有限双审PASS）

采用r132原失败条件，原120秒源、同编码规格，初始AV故障注入提前结束，立即新发送器；新AV和未中断纯视频均须完成完整120秒。不是慢速重入替代，线端间隔须核验。实际待执行基础命令：
```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61031 or dst port 62020)" -a duration:230 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r133-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r133-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r133 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r133.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r133-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r133-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r133-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r133-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r133-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r133- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r133-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

### r133 结果：快速切换不再终止整图（有限双审PASS）

修复后三文件双独立源码PASS，Release全量clean-first/all-target configure/build exit0（699目标）。仅realtime验证。
PID capture23100、VLC10084、CLI34124、旧AV7632、纯视频16112、新AV12464。快速切换实际命令：
```powershell
$old=Get-CimInstance Win32_Process -Filter 'ProcessId=7632'; if($old.Name -ne 'ffmpeg.exe' -or $old.CommandLine -notlike '*61022*' -or $old.CommandLine -notlike '*test-continuous-120s.mp4*'){throw 'Source identity mismatch'}; Stop-Process -Id 7632 -Force; & D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r133-source-reentry.log 2>&1
```
旧AV故障注入停止，最后1222帧40.73秒、工具exit1；纯视频与新AV各完整3600帧120秒、exit0。旧视频SSRC1614570029最后包1791512089.919319，新SSRC2993864853首包1791512089.978013，间隔58.694ms，比失败轮68.812ms更短。音频3783585336→4027720393，间隔59.206ms。10:14:49.977/978两流SsrcChanged走原generation0→1；10:14:49.981源1 purge_completed；10:14:54.983组generation2锁定。没有原Unsupported/全图失败。

输出SSRC662298030保持，109392 RTP包无缺号/重复/乱序；输入各SSRC序列无缺口。capture230秒exit0，366688包/drop0。TS124985972B，PID0/256/257/258连续计数错误0，输出线端203.632704秒。ffprobe exit0：视频6108包PTS0..18321000，步长3000共6107；音频8768包PTS0..18321241，步长2090共6978、2089共1789；两流PTS=DTS，无倒退/重复。
silencedetect(-80dB,d=0.5) exit0：切换静音29.423764..34.431270共5.007506秒，然后非静音至148.924376；最终失源静音至203.592562共54.668186秒。非静音恢复114.493106秒，不能等价全部内容或内容级A/V同步。此5秒与等待新SR/组锁时间一致，是本轮观测，未新增等待常量。

现场恢复截图首次仅237×39无效，恢复窗口后已到全失源，仅完整黑画面有效；未取得现场恢复画面。离线从VLC实际received.ts第60秒抽帧，1280×720双格真实画面，作为录制证据，不冒充现场截图。原不完整图已被后续黑图覆盖，失败尝试在此保留。
```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -ss 60 -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-received.ts' -frames:v 1 -update 1 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-recorded-recovery.png' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-frame.log' 2>&1
```

监控191样本CPU均1.404043%/峰3.417735%；WS峰238514176B/尾219598848B，private峰721522688B/尾705892352B。内部漂移代1共30条155..156ns，代2共113条13733..13735ns；不代表内容同步。最终caller_stop/exit0，workerErrors/errors/stalledIntervals/droppedBuffers0；payload115446预约=释放，结束0B/0对象，高水16580891B/903对象，pressureFailures28241。encoded68276/68275，AAC关闭残留2帧，非完整drain。stderr仍有color range属性非致命警告。
输出停止实际命令：
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R133Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r133Target=34124
$r133Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r133Target"
if($r133Process.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r133Process.CommandLine -notlike '*composition-wiring-r133*'){throw 'PID identity mismatch'}
[R133Console]::FreeConsole() | Out-Null
if(-not [R133Console]::AttachConsole($r133Target)){throw 'AttachConsole failed'}
try {
if(-not [R133Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r133Ids=[uint32[]]::new(32)
$r133Count=[R133Console]::GetConsoleProcessList($r133Ids,32)
if($r133Count -ne 2 -or $r133Ids[0..($r133Count-1)] -notcontains $r133Target -or $r133Ids[0..($r133Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R133Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R133Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```
VLC经核验PID的RC63020 quit；源先结束观察全失源后才停止CLI。完整日志无capacity_stop、clock_group_rejected、process.failed或encoder timeline discontinuity；两条预期rtp_discontinuity已正常恢复。

#### r133 离线实际命令

提取TS复用r130b已列inline Python，仅替换文件前缀r133；未创建测试脚本。
```powershell
& 'D:\Wireshark\tshark.exe' -r 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-wire.pcapng' -T fields -e frame.time_epoch -e udp.dstport -e udp.payload > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-all-wire.tsv' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-tshark.log'
```

```powershell
& 'D:\mabs\local64\bin-video\ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts,duration,pts_time -of json 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-wire.ts' > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-packets.json' 2> 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-probe.log'
```

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-wire.ts' -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL > 'D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-silence.log' 2>&1
```

#### r133 独立审查与清理边界

独立审查A明确快速SSRC回归有限PASS、完整FAIL；5.007506秒恢复等待属于实测性能边界，不称无缝切换或仅58.694ms音频中断。B复核进行中。临时清单如下，源及正式Release保留：
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-all-wire.tsv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-capture.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-cli-error.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-cli.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-metrics.csv
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-packets.json
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-probe.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-received.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-silence.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-source0.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-source1.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-tshark.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-vlc.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-vlc.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-vlc-all-loss.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-wire.pcapng
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-wire.ts
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133.sdp
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-source-reentry.log
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-vlc-reentry.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-vlc-selected-loss.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-vlc-final-loss.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-recorded-recovery.png
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r133-frame.log

独立B最终明确原故障回归PASS、完整FAIL，现场恢复视觉仍未通过；必要证据读完后按清单清理20项实际产物，r133前缀无残留，六PID不存在，源124427809B保留。

## r134 三源V/V/AV → HEVC CBR 8Mbps、1280×720/30、AAC CBR192kbps → MPEG-TS/RTP（已结束，FAIL）

逐源等待100/150/200ms仅为本轮命令配置。原120秒源，先AV后两纯视频；全源结束后新AV完整120秒重入。复用已构建Release，无local测试。实际命令：
```powershell
& D:/Wireshark/dumpcap.exe -i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r134-wire.pcapng -q > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r134-capture.log 2>&1
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r134 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r134.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r134-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r134-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r134-source0.log 2>&1
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r134-source1.log 2>&1
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r134-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r134- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r134-received.ts}}" --sout-keep rtp://@127.0.0.1:62020
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r134-source2.log 2>&1
```

### r134 结果：三源HEVC CBR运行FAIL，重入未执行

capture29744、VLC3120、CLI30116、AV29796、纯视频37184/38400。三源日志均3600帧/媒体120秒且工具exit0，墙钟分别124.51/123.11/123.69秒。CLI于10:24:08.708自然exit1，首错node70 Shared A/V output scheduler：NotInitialized A/V sync group requires its planned reacquisition coordinator。此前10:24:08.707 late_audio_head now111363365000-target111039953398=323.411602ms；08.708 video reacquire cause=hard_phase_error sequence2785 phase_error_ns=-334012032 recovery_actions=0。源未失活、无SSRC切换；当前ContinuousOutput仍使用通用source同步策略而没有source coordinator，触发边界需修复，不能通过给输出补source coordinator、调高阈值或重置唯一输出时轴规避。
后续Cancelled来自整图abort。最终workerErrors/errors1、payload79806预约=释放、0B/0对象，encoded31123/31117差6包，非完整drain。WS内部峰281788416B；后台86样本CPU均2.314174%/峰3.327797%，WS峰282648576B/private峰895287296B；92条内部音频漂移155..156ns，不证明内容同步或无输出延迟。系统机器CPU报告均27.988021%/峰值未独立汇总，不用此归因GPU。

截图工具最初返回运行session，过早读取图片失败；完成后内容被用户其他应用遮挡，不能作VLC视觉证据。未启动所选AV重入，未进行音频恢复/全失源视觉验收。三源停止后核验本轮capture/VLC命令行并按PID29744/3120结束；capture中止exit1，不能宣称完整370秒或drop0。CLI自然exit1，源三次exit0，监控正常结束。保留原源及Release，不影响其他用户进程。

r134清理首次10项删除成功，received.ts及wire.pcapng因占用失败（当次REMOVED计数12包含失败尝试，不能当成功数）。确认无本轮媒体进程后逐项重试两文件成功；共12项实际产物清除，r134前缀无残留、六PID无残留，源124427809B保留。

## r135 调度修复后原规格复验：准备阶段失败，证据不完整

Release全量构建exit0，CLI 2026-10-09 16:43:51、5772288B；双独立源码复审PASS。下列实际命令由CLI先启动、2秒后AV、再11秒后V61030、再5秒后V61040编排，均使用原120秒源。VLC与成功的dumpcap在沙箱外启动；CLI及源在默认沙箱启动。

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r135 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r135- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r135-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

结果：CLI28904自然exit1，raw RTP signaling open timeout、matching_rtp_packets=0、matching_datagram_bytes=0、parameter_sets=none，未进入生产输出。FFmpeg PID28168/9668/42220日志分别停止于2445帧81.50秒、2152帧71.73秒、1997帧66.56秒；无完整结束记录，工具句柄随后失效，退出码未知，不能记源120秒完成。VLC33816 received.ts为0B，无画面/音频证据。初次dumpcap9680 exit1明确无Npcap权限；权限提升后dumpcap18528曾运行，但后续句柄失效，无完整370秒/退出码/drop统计。capinfos确认pcapng380B、0包、无statistics entries，不是drop0证明。

监控命令在Get-Process -Id28904时返回找不到进程，未形成CPU/内存样本；无A/V telemetry。沙箱内发送日志前进但宿主回环抓包为0，需在统一的获授权本机进程/网络环境复验；尚不能把0包归因生产核心。16:48外部CIM复查六PID均不存在，未执行强制清理，不推断消失原因或自然退出。实际capinfos命令为 `D:\Wireshark\capinfos.exe D:\Code\MyCode\MediaTranscode\out\acceptance\composition-wiring-r135-wire.pcapng`。原r134约334ms晚到及单源回归仍未验证。

本轮清理清单（13件）：
- composition-wiring-r135-capture-stdout.log
- composition-wiring-r135-capture.log
- composition-wiring-r135-cli-error.log
- composition-wiring-r135-cli.log
- composition-wiring-r135-received.ts
- composition-wiring-r135-source0-stdout.log
- composition-wiring-r135-source0.log
- composition-wiring-r135-source1-stdout.log
- composition-wiring-r135-source1.log
- composition-wiring-r135-source2-stdout.log
- composition-wiring-r135-source2.log
- composition-wiring-r135-vlc.log
- composition-wiring-r135-wire.pcapng

13件清理成功，r135前缀无残留，原源124427809B及正式Release保留；未运行远程测试。

## r136：三源 H264 RTP→HEVC CBR 8Mbps 1280×720/30＋AAC 192kbps→MPEG-TS/RTP（完整 FAIL）

2026-10-09 使用最终 Release；2026-10-10 完成离线核验。所有媒体进程在获授权宿主环境启动，未改外部 FFmpeg。capture PID29008、VLC11436、CLI34820、AV源13108、纯视频源35888/41644、AV重入42936。初始三源均3600帧120秒、exit0；重入仅2088帧69.61秒，工具宿主随后中断，CLI/重入/抓包最终退出码未知。今日宿主CIM复查七PID均不存在，不将消失记为自然结束。

实际启动命令如下；捕获/VLC先启动，CLI和三路源并行请求。重入沿同一AV源命令，日志名改为source-reentry。

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r136 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r136- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```


结果边界：日志末尾errors/workerErrors/stalledIntervals/droppedBuffers均0；encoded push/pop91772/91771，payload仍432833B/41 objects，reservations/releases158921/158882，不能声称drain或无泄漏。218次外部样本CPU均值1.229872%、峰值2.94321%，WS峰269373440B、private峰883003392B，末尾WS255512576B/private876482560B；仅为有限区间。generation1共118条raw drift118–119ns，generation2共68条18140–18141ns，不等于内容A/V同步。

vlc-three截图实际已全黑，不能证明初始三格；vlc-reentry为左下格真实画面、其余黑，支持源2局部恢复。重入generation2于16:57:39.952锁定。未复现r134错误reacquire，但未证明同等334ms晚到发生或根因消失。

原始抓包534983包、642677488B、284.7228783秒，首末16:54:04.4656838/16:58:49.1885621，无最终statistics entries，不能声称capture drop0。SHA256 afbe4b5e854d9153d50e5fe449d62bd391c74eb0cbcb3d434c8f02689faa61a1。补充抓包调用被宿主中断，无文件/PID，不记作已执行。

VLC录制162969116B、273.5659秒；离线音频解码exit0但有非单调DTS和尾部AAC错误。原始输出RTP提取144356包、SSRC0AACBC1D、序号异常0、TS164073616B；音频及视频分别离线解码exit0，均无解码错误，音频静音119.076032–205.585306秒（86.509274秒）后恢复。此对照不支持将VLC录制错误直接归因生产输出；仍不等于端到端音画漂移通过。

第一次提取遇到tshark输出末尾文本“No matching RTP/H.264 payload dumped.”，PowerShell非终止异常后仍exit0，该次结果废弃。第二次启用Stop，明确只排除该一条文本尾行，其余RTP/TS格式失败即终止，结果才用于上述统计。未使用自行假定的缺包填充。离线命令（FFmpeg仅处理已结束测试的文件，不参与实流监控）：

```powershell
& 'D:/Wireshark/capinfos.exe' 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-wire.pcapng'
& 'D:/Wireshark/tshark.exe' -n -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-wire.pcapng' -Y 'udp.dstport == 62020' -T fields -e udp.payload > 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-output-payloads.txt' 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-tshark.log'
$ErrorActionPreference='Stop'; $count=0L; $anomalies=0L; $footer=0; $last=@{}; $stream=[IO.File]::Create('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-wire.ts'); try { foreach($line in [IO.File]::ReadLines('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-output-payloads.txt')) { if($line -eq 'No matching RTP/H.264 payload dumped.') {$footer++; continue}; if(!$line){continue}; $b=[Convert]::FromHexString($line.Replace(':','')); if($b.Length -lt 12 -or ($b[0]-shr 6)-ne 2){throw 'Invalid RTP'}; $seq=[int]$b[2]*256+$b[3]; $ssrc=[Convert]::ToHexString($b[8..11]); $off=12+4*($b[0]-band 15); if($b[0]-band 16){$off+=4+4*([int]$b[$off+2]*256+$b[$off+3])}; $end=$b.Length; if($b[0]-band 32){$end-=$b[-1]}; if($off -gt $end -or ($end-$off)%188 -ne 0){throw 'Invalid TS payload'}; if($last.ContainsKey($ssrc) -and $seq -ne (($last[$ssrc]+1)%65536)){$anomalies++}; $last[$ssrc]=$seq; $stream.Write($b,$off,$end-$off); $count++ } } finally {$stream.Dispose()}; [pscustomobject]@{Packets=$count;SequenceAnomalies=$anomalies;TextFooters=$footer;SSRC=($last.Keys -join ',');Bytes=(Get-Item 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-wire.ts').Length} | ConvertTo-Json
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-wire.ts' -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL *> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-wire-silence.log'
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v error -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-wire.ts' -map 0:v:0 -an -f null NUL *> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r136-wire-video.log'
```

两名独立审查者一致：源码调度局部PASS、有限运行证据成立、完整验收FAIL。缺初始三格视觉、重入完整120秒、CLI结束/drain、内容A/V和原晚到根因证据。不得创建成功验收提交。保持Windows先行、仅realtime、C API真实调用方与完整矩阵待验。

本轮待清理清单（25件）：
- composition-wiring-r136-capture-stdout.log
- composition-wiring-r136-capture.log
- composition-wiring-r136-cli-error.log
- composition-wiring-r136-cli.log
- composition-wiring-r136-metrics.csv
- composition-wiring-r136-output-payloads.txt
- composition-wiring-r136-received.ts
- composition-wiring-r136-silence.log
- composition-wiring-r136-source-reentry-stdout.log
- composition-wiring-r136-source-reentry.log
- composition-wiring-r136-source0-stdout.log
- composition-wiring-r136-source0.log
- composition-wiring-r136-source1-stdout.log
- composition-wiring-r136-source1.log
- composition-wiring-r136-source2-stdout.log
- composition-wiring-r136-source2.log
- composition-wiring-r136-tshark.log
- composition-wiring-r136-vlc-reentry.png
- composition-wiring-r136-vlc-three.png
- composition-wiring-r136-vlc.log
- composition-wiring-r136-wire-silence.log
- composition-wiring-r136-wire-video.log
- composition-wiring-r136-wire.pcapng
- composition-wiring-r136-wire.ts
- composition-wiring-r136.sdp

25件按上述清单逐项清理成功，r136前缀无残留，原源124427809B保留；本轮未运行远程测试。

## r137：三源原规格复验中止（2026-10-10）

目标链路仍为三源H264 RTP→HEVC CBR8Mbps 1280×720/30＋AAC192kbps→MPEG-TS/RTP。第三路61040启动工具返回Rejected(rejected by user)，未重试或绕过，故不满足原三源场景。以下为已执行五条命令；第三路未执行，不混列为已运行：
```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r137-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r137-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r137-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r137-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r137- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r137-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r137 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r137.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r137-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r137-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r137-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r137-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r137-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r137-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

PID：capture30516、VLC7972、CLI4880、AV21656、纯视频30112。CLI准备阶段自然exit1：raw RTP video signaling probe open timeout，matching_rtp_packets=0/bytes=0/parameter_sets=none；与未启动输入相符，不作为新的生产回归证据。其他已接收adapter记录26101/1282/25938个datagram，truncations/pressure_failures均0。CLI未形成生产输出，VLC录制0B，无画面/A/V漂移门禁。CPU/内存后台采样尚未建立，不补造数据。

结束后按精确身份清理AV21656、纯视频30112、VLC7972、capture30516；CLI4880在清理前已不存在，原工具句柄返回exit1。两源末尾分别1023帧34.11秒和1100帧36.66秒，均为中止，不记120秒或自然结束。抓包78885764B，强制清理无最终drop统计，不宣称丢包0。完整FAIL/中止，不创建成功提交。接下来先完成不依赖被拒源启动的C API审查。

本轮清理清单（11件）：
- composition-wiring-r137-capture-stdout.log
- composition-wiring-r137-capture.log
- composition-wiring-r137-cli-error.log
- composition-wiring-r137-cli.log
- composition-wiring-r137-received.ts
- composition-wiring-r137-source0-stdout.log
- composition-wiring-r137-source0.log
- composition-wiring-r137-source1-stdout.log
- composition-wiring-r137-source1.log
- composition-wiring-r137-vlc.log
- composition-wiring-r137-wire.pcapng

清理状态：上述11件文件的删除命令被工具返回Rejected(rejected by user)，未执行删除，文件保留。没有重试或绕过该拒绝。媒体进程已清理与文件清理未完成是两个独立状态。

2026-10-10 用户明确允许删除后，以上11件临时产物按清单逐项清理成功，r137前缀无残留，指定120秒源保留。此前工具拒绝仅记录其返回值，不推断为用户主动拒绝。

## r138：三源 H264 RTP→HEVC CBR8Mbps 1280×720/30＋AAC192kbps→MPEG-TS/RTP（进行中）

2026-10-10，复用最终Release5772288B。capture31228/session75518，VLC31636/session1606，CLI29848/session65606，AV7232/session75552，纯视频8152/session67789及30244/session89045，监控session83670。启动前宿主CIM无媒体进程。实际命令：
```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r138 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r138- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

首张three.png被Codex遮挡，无效；将VLC短暂置前后three-visible.png确认左上/右上/左下视频及右下黑格。未降低源参数，等待初始完整120秒及重入。当前非PASS。

### r138 结束与根因证据（FAIL）
三源均3600帧120秒、exit0（各elapsed119.48秒）。CLI在09:07:05.855运行失败，session65606返回exit1；未执行重入。VLC31636、capture31228在确认失败后精确PID清理；三源当时均已自然退出。监控正常结束。
有效截图确认三个视频格及右下空黑格；首张被遮挡不能作为证据。
51次外部CPU样本均值2.162534742%、峰3.128510105%，WS峰262295552B，private峰876818432B。最终errors/workerErrors=1，queued=0，payload=0B/0对象，reservations/releases=57109/57109，encoded push/pop=22407/22407；AAC关闭仍有2帧残留，不等于完整drain。
capinfos实际命令为 D:/Wireshark/capinfos.exe D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r138-wire.pcapng，exit0：373132包、451051792B、121.4422189秒，首末09:05:51.4219695/09:07:52.8641884，SHA256 cdb12ca505b3a2bcd96513bfe02b8ec6a623b97fe44aded8918cfb56d15d5fea。强制结束抓包无统计条目，不声称drop0。

两名独立源码调查一致：迟视频AU在scheduler提交前已晚174.350232ms，到mux晚174.642132ms，scheduler到mux仅0.2929ms；最近maintenance将available推进到AU dispatch之后33.334672ms，是直接触发而非已证实迟到根因。原始关键行如下：

```text
[2026-10-10 09:07:05.854] [info] [graph][runtime.node] av_scheduler_trace stage=late_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=66803809523 canonical_dispatch_ns=66803809523 target_master_ns=71831807991 dispatch_master_ns=71831807991 now_ns=72001973900
[2026-10-10 09:07:05.855] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=video generation=1 canonical_dispatch_ns=66800000000 emit_ns=71727998468 dispatch_ns=71827998468 committed_at_ns=72002348700
[2026-10-10 09:07:05.855] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=22560 peak_pending_bytes=100580 pressure_failures=0 access_units=4881 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=0 maximum_mux_input_after_emit_ns=274642132 maximum_mux_input_after_dispatch_ns=174642132 worst_mux_input_ready_ns=72002640600 worst_mux_input_emit_ns=71727998468 worst_mux_input_dispatch_ns=71827998468 maximum_protocol_batch_produced_after_release_ns=100592628 worst_protocol_batch_produced_ns=71895259100 worst_protocol_batch_release_ns=71794666472 worst_protocol_batch_deadline_ns=71861333140 worst_protocol_batch_datagrams=1 exit_reason=InvalidArgument: MPEG-TS access unit has no transport service window stream=video emit_ns=71727998468 dispatch_ns=71827998468 available_ns=71861333140 last_committed_kind=maintenance service_start_ns=71861333140 actual_now_ns=72002690200 remaining_ns=-33334672 payload_bytes=22560 wire_bytes=22776
[2026-10-10 09:07:05.866] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=7231 committed_datagrams=47809 committed_payload_bytes=57956320 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=47809 pacing_cancelled=0 pacing_submitted=47809 pacing_maximum_submit_lateness_ns=100349896 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=50733400 last_materialized_sequence=47809 last_scheduled_sequence=47809 last_submitted_sequence=47809 last_committed_sequence=47809 shared_scope_charged_datagrams=47809 shared_scope_charged_wire_bytes=59294972 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=47790 endpoint_1_committed_payload_bytes=57955028 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=19 endpoint_2_committed_payload_bytes=1292
[CLI] final runtime report: queued=0, peakQueued=799, threads=78, processThreads=126, workers=0, workerProgress=146073, workerProcessCalls=814512, workerWaits=668423, workerWakeups=663747, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=217, logicalProcessors=22, averageSystemMachineCpuPercent=18.706467, averageProcessMachineCpuPercent=2.260166, peakProcessMachineCpuPercent=6.787330, averageProcessSingleCoreCpuPercent=49.723646, peakProcessSingleCoreCpuPercent=149.321267, initialWorkingSetBytes=213651456, workingSetBytes=262295552, peakWorkingSetBytes=262295552, totalPushed=153593, totalPopped=153009, droppedBuffers=0, encodedPacketsPushed=22407, encodedPacketsPopped=22407, backpressureItems=119, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16945355, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=616, graphPayloadReservations=57109, graphPayloadReleases=57109, graphPayloadPressureFailures=17772, lastInputReceivedAtNs=1490768243000
```
下一步调查编码完成、进入scheduler、跨路等待、credit和提交的实际时刻；同时核验maintenance batch超过原deadline 33.925960ms但sender deadline_misses=0的映射。不得冻结PCR、回拨availableAt或放宽期限掩盖。共享单源及跨平台均潜在受影响。依据：GStreamer latency https://gstreamer.freedesktop.org/documentation/additional/design/latency.html 、GstAggregator https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html 与FFmpeg mpegtsenc DTS/PCR校验。未修改生产核心、未改外部FFmpeg，完整FAIL。
清理清单：
- composition-wiring-r138-capture-stdout.log
- composition-wiring-r138-capture.log
- composition-wiring-r138-cli-error.log
- composition-wiring-r138-cli.log
- composition-wiring-r138-metrics.csv
- composition-wiring-r138-received.ts
- composition-wiring-r138-source0-stdout.log
- composition-wiring-r138-source0.log
- composition-wiring-r138-source1-stdout.log
- composition-wiring-r138-source1.log
- composition-wiring-r138-source2-stdout.log
- composition-wiring-r138-source2.log
- composition-wiring-r138-three-visible.png
- composition-wiring-r138-three.png
- composition-wiring-r138-vlc.log
- composition-wiring-r138-wire.pcapng
- composition-wiring-r138.sdp

以上17件本轮产物逐项清理完成，r138无残留，指定原源和Release保留；未运行远程测试。

## r139：三源 H264 RTP→HEVC CBR8Mbps 1280×720/30＋AAC192kbps→MPEG-TS/RTP（进行中）

2026-10-10 使用09:15:06 Release5774336B，新增scheduler分段诊断版本。capture30212/session85935，VLC31176/session99348，CLI9136/session31219，AV4144/session78005，纯视频2700/session89574、7752/session21443，监控session57238。启动前宿主CIM无媒体进程。截图three确认三个视频格及右下黑格。尚非PASS。实际命令：
```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r139 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r139- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$path='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-metrics.csv'; 'utc,cpu_percent,working_set_bytes,private_bytes' | Set-Content -LiteralPath $path; $p=Get-Process -Id 9136 -ErrorAction Stop; $last=$p.CPU; $t=[DateTime]::UtcNow; while(!$p.HasExited){Start-Sleep -Seconds 1; $p.Refresh(); if($p.HasExited){break}; $now=[DateTime]::UtcNow; $cpu=100*($p.CPU-$last)/($now-$t).TotalSeconds/[Environment]::ProcessorCount; "$($now.ToString('o')),$cpu,$($p.WorkingSet64),$($p.PrivateMemorySize64)" | Add-Content -LiteralPath $path; $last=$p.CPU; $t=$now}; 'MONITOR_FINISHED'
```

```powershell
Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public class R139Window { [DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h,int n); [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int cx,int cy,uint flags); }'; [R139Window]::SetProcessDPIAware() | Out-Null; $h=(Get-Process -Id 31176).MainWindowHandle; [R139Window]::ShowWindowAsync($h,9) | Out-Null; [R139Window]::SetWindowPos($h,[IntPtr](-1),0,0,0,0,3) | Out-Null; try {Start-Sleep -Milliseconds 500; & 'C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1' -WindowHandle ([int]$h) -Path 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-three.png'} finally {[R139Window]::SetWindowPos($h,[IntPtr](-2),0,0,0,0,3) | Out-Null}
```

初始三源session78005/89574/21443均返回exit0，完整120秒；black截图全黑且CLI持续编码输出。随后AV源重入PID21380/session68149，命令为上方AV源命令仅source0日志名改source-reentry。reentry截图仅左下真实画面，其余黑；generation2已锁定。尚待重入结束和退出证据。

### r139 结束、线端复核与证据边界

初始三路及重入AV源均3600帧120秒，四个源工具句柄均exit0。重入21380结束后black-final全黑，CLI继续输出；随后显式CTRL_C停止，CLI9136 caller_stop/exit0。capture30212正常CTRL_C退出0，VLC31176经RC quit退出0；监控正常结束，宿主CIM复查无媒体进程。

最终资源：workers/queued=0，payload0B/0对象，175644预留=释放，errors/workerErrors/stalledIntervals/droppedBuffers=0。但encoded88968/88967、TS pending188B、sender取消1datagram/228wire bytes、AAC关闭2frames，不宣称无损drain或可信EOF自动结束。247外部样本CPU均1.662108685%、峰3.690456312%，WS峰265326592B，private峰875835392B；有限区间稳定，不证明长稳。内部drift代1共113条2459–2461ns，代2共117条13733–13734ns，不等于内容A/V。

未出现late_output_commit；最差AU到mux早于dispatch74.779168ms，本轮未复现r138超期，所以新增head/scheduled诊断没有获得失败样本。TS最坏release延迟样本在protocol deadline前41.202408ms产生，不代表逐包协议deadline全部审计通过；原deadline传递缺口仍在。

抓包609617包、739929852B、272.413087秒，09:16:23.2094719至09:20:55.6225589，dumpcap最终interface drop0。capinfos命令：D:/Wireshark/capinfos.exe -c -u -a -e -s D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-wire.pcapng，exit0。tshark按udp.dstport==62020提取udp.payload，exit0；末尾一条已知非包文本明确排除，其余格式失败即停止。提取172325个RTP包，SSRC3280E468，序号异常0，TS203748760B。音频及视频离线解码exit0且无解码错误；ffprobe数到视频7959/音频11425包，两流DTS非递增0。音频静音114.303424–132.094512秒（17.791088秒），及250.593878–265.287982秒（14.694104秒），前段后恢复，后段持续至结束。只证明观察到静音区间，不将其直接当内容级同步值。

双独立审查：有限场景PASS，完整门禁FAIL/未完成。既有r138迟到、TS deadline合同、内容AV、完整编码矩阵、C API真实调用方与Windows之后RKMPP仍开放；不创建完整验收成功提交。以下记录重入、结束与离线命令：
```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-source-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-source-reentry.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R139Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r139Target=9136
$r139Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r139Target"
if($r139Process.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r139Process.CommandLine -notlike '*composition-wiring-r139*'){throw 'PID identity mismatch'}
[R139Console]::FreeConsole() | Out-Null
if(-not [R139Console]::AttachConsole($r139Target)){throw 'AttachConsole failed'}
try {
if(-not [R139Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r139Ids=[uint32[]]::new(32)
$r139Count=[R139Console]::GetConsoleProcessList($r139Ids,32)
if($r139Count -ne 2 -or $r139Ids[0..($r139Count-1)] -notcontains $r139Target -or $r139Ids[0..($r139Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R139Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R139Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R139Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r139Target=30212
$r139Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r139Target"
if($r139Process.Name -ne 'dumpcap.exe' -or $r139Process.CommandLine -notlike '*composition-wiring-r139*'){throw 'PID identity mismatch'}
[R139Console]::FreeConsole() | Out-Null
if(-not [R139Console]::AttachConsole($r139Target)){throw 'AttachConsole failed'}
try {
if(-not [R139Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r139Ids=[uint32[]]::new(32)
$r139Count=[R139Console]::GetConsoleProcessList($r139Ids,32)
if($r139Count -ne 2 -or $r139Ids[0..($r139Count-1)] -notcontains $r139Target -or $r139Ids[0..($r139Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R139Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R139Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

```powershell
$p=Get-CimInstance Win32_Process -Filter 'ProcessId=31176'; if($p.Name -ne 'vlc.exe' -or !$p.CommandLine.Contains('composition-wiring-r139')){throw 'VLC identity mismatch'}; $client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try{$bytes=[Text.Encoding]::ASCII.GetBytes("quit`n"); $client.GetStream().Write($bytes,0,$bytes.Length)}finally{$client.Dispose()}
```

```powershell
$ErrorActionPreference='Stop'; $count=0L; $anomalies=0L; $footer=0; $last=@{}; $stream=[IO.File]::Create('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-wire.ts'); try { foreach($line in [IO.File]::ReadLines('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-output-payloads.txt')) { if($line -eq 'No matching RTP/H.264 payload dumped.') {$footer++; continue}; if(!$line){continue}; $b=[Convert]::FromHexString($line.Replace(':','')); if($b.Length -lt 12 -or ($b[0]-shr 6)-ne 2){throw 'Invalid RTP'}; $seq=[int]$b[2]*256+$b[3]; $ssrc=[Convert]::ToHexString($b[8..11]); $off=12+4*($b[0]-band 15); if($b[0]-band 16){$off+=4+4*([int]$b[$off+2]*256+$b[$off+3])}; $end=$b.Length; if($b[0]-band 32){$end-=$b[-1]}; if($off -gt $end -or ($end-$off)%188 -ne 0){throw 'Invalid TS payload'}; if($last.ContainsKey($ssrc) -and $seq -ne (($last[$ssrc]+1)%65536)){$anomalies++}; $last[$ssrc]=$seq; $stream.Write($b,$off,$end-$off); $count++ } } finally {$stream.Dispose()}; [pscustomobject]@{Packets=$count;SequenceAnomalies=$anomalies;TextFooters=$footer;SSRC=($last.Keys -join ',');Bytes=(Get-Item 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-wire.ts').Length} | ConvertTo-Json
```

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-wire.ts' -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null NUL *> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-wire-silence.log'; Write-Output "EXIT=$LASTEXITCODE"; Get-Content 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-wire-silence.log' -Tail 8
```

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v error -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-wire.ts' -map 0:v:0 -an -f null NUL *> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-wire-video.log'; Write-Output "EXIT=$LASTEXITCODE"; Get-Content 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-wire-video.log' -Tail 5
```

```powershell
$ErrorActionPreference='Stop'; $last=@{}; $counts=@{}; $regressions=@{}; & 'D:/mabs/local64/bin-video/ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts -of csv=p=0 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r139-wire.ts' | ForEach-Object { $f=$_.Split(','); if($f.Length -ge 3 -and $f[0] -match '^\d+$' -and $f[2] -match '^-?\d+$'){$key=$f[0]; $dts=[long]$f[2]; if(!$counts.ContainsKey($key)){$counts[$key]=0; $regressions[$key]=0}; if($last.ContainsKey($key) -and $dts -le $last[$key]){$regressions[$key]++}; $last[$key]=$dts; $counts[$key]++}}; if($LASTEXITCODE -ne 0){throw 'ffprobe failed'}; [pscustomobject]@{Packets=$counts;NonIncreasingDts=$regressions} | ConvertTo-Json -Compress
```

清理清单：
- composition-wiring-r139-black-final.png
- composition-wiring-r139-black.png
- composition-wiring-r139-capture-stdout.log
- composition-wiring-r139-capture.log
- composition-wiring-r139-cli-error.log
- composition-wiring-r139-cli.log
- composition-wiring-r139-metrics.csv
- composition-wiring-r139-output-payloads.txt
- composition-wiring-r139-received.ts
- composition-wiring-r139-reentry.png
- composition-wiring-r139-source-reentry-stdout.log
- composition-wiring-r139-source-reentry.log
- composition-wiring-r139-source0-stdout.log
- composition-wiring-r139-source0.log
- composition-wiring-r139-source1-stdout.log
- composition-wiring-r139-source1.log
- composition-wiring-r139-source2-stdout.log
- composition-wiring-r139-source2.log
- composition-wiring-r139-three.png
- composition-wiring-r139-tshark.log
- composition-wiring-r139-vlc.log
- composition-wiring-r139-wire-silence.log
- composition-wiring-r139-wire-video.log
- composition-wiring-r139-wire.pcapng
- composition-wiring-r139-wire.ts
- composition-wiring-r139.sdp

26件产物按上述清单逐项清理成功，r139无残留，指定原源及Release保留；未运行远程测试。

## r140：三源 H264 RTP→HEVC CBR8Mbps 1280×720/30＋AAC192kbps→MPEG-TS/RTP（进行中）

2026-10-10，5文件期限修复双独立源码PASS，Release全量configure/build exit0（09:29:03、5775360B）。启动前宿主CIM无媒体进程。capture3580/session34242，VLC31576/session98309，CLI3052/session5846，AV16796/session69125，纯视频32940/session88212、5184/session85247，监控session95423。three截图已确认三格视频及空黑格。尚非PASS。命令：
```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r140 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r140- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$path='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-metrics.csv'; 'utc,cpu_percent,working_set_bytes,private_bytes' | Set-Content -LiteralPath $path; $p=Get-Process -Id 3052 -ErrorAction Stop; $last=$p.CPU; $t=[DateTime]::UtcNow; while(!$p.HasExited){Start-Sleep -Seconds 1; $p.Refresh(); if($p.HasExited){break}; $now=[DateTime]::UtcNow; $cpu=100*($p.CPU-$last)/($now-$t).TotalSeconds/[Environment]::ProcessorCount; "$($now.ToString('o')),$cpu,$($p.WorkingSet64),$($p.PrivateMemorySize64)" | Add-Content -LiteralPath $path; $last=$p.CPU; $t=$now}; 'MONITOR_FINISHED'
```

```powershell
Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public class R140Window { [DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h,int n); [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int cx,int cy,uint flags); }'; [R140Window]::SetProcessDPIAware() | Out-Null; $h=(Get-Process -Id 31576).MainWindowHandle; [R140Window]::ShowWindowAsync($h,9) | Out-Null; [R140Window]::SetWindowPos($h,[IntPtr](-1),0,0,0,0,3) | Out-Null; try {Start-Sleep -Milliseconds 500; & 'C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1' -WindowHandle ([int]$h) -Path 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r140-three.png'} finally {[R140Window]::SetWindowPos($h,[IntPtr](-2),0,0,0,0,3) | Out-Null}
```

### r140 运行失败与新分段证据

CLI3052自然exit1，09:31:07.172首错TS视频AU无窗口：emit44775382868、dispatch44875382868、available44942050360（maintenance）、actual44962224200、remaining−66667492ns、payload28576/wire28840。新诊断video head44732327400、scheduled44961820800、commit44961831600、mux ready44962192700；head提前143.055468ms，head→scheduled229.4934ms、scheduled→commit10.8us、commit→mux361.1us。audio head44961458000、dispatch44897922550，自身晚63.53545ms。双独立调查一致：排除本次视频最终提交长背压，支持缺音频head的有序合流等待拖晚视频；但缺逐次唤醒/上游完成证据，不能把229ms全归因audio wait或直接归因encoder。

期限修复未被该例推翻：AU到mux已晚86.809832ms，不是严格及时AU被maintenance越界；最差已记录protocol生产延迟13.6707ms，sender无deadline miss，不能替代逐包期限审计。原规格整体FAIL。最终payload0B/0对象、35388预留=释放，workerErrors/errors=1，encoded13800/13797，不能称无损drain。24外部样本CPU均1.88158177%、峰2.377574153%，WS峰255557632B/private峰867762176B；无长期稳定或内容A/V结论。

失败后清理源16796/32940/5184；末尾分别2986帧99.54秒、3034帧101.13秒、3002帧100.06秒，属于中止，不标120秒完成。capture3580用下述控制台信号正常exit0，VLC31576通过核验身份后RC63020 quit正常exit0，monitor结束；宿主CIM复查无媒体进程。capture302278包/drop0、文件364732296B；未做线端解码分析，不宣称完整连续输出通过。源清理仅Stop-Process精确PID并校验ffmpeg、指定源及610xx目的端口。

下一步：核验AudioEncoderPacketLineageMapper的精确连续sample结束是否能提供下一音频dispatch下界，配合generation与有序推进避免无必要合流等待；还须测量音频生产链迟到，不能只放行视频或猜测编码帧间隔。相关成熟watermark/GAP概念已独立调查，不新增调用方推导参数。

```text
[2026-10-10 09:31:07.171] [info] [graph][runtime.node] av_scheduler_trace stage=late_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=41122539682 canonical_dispatch_ns=41122539682 target_master_ns=44897922550 dispatch_master_ns=44897922550 now_ns=44961458600
[2026-10-10 09:31:07.171] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=video generation=1 canonical_dispatch_ns=41100000000 emit_ns=44775382868 dispatch_ns=44875382868 committed_at_ns=44961831600 head_arrived_ns=44732327400 scheduled_at_ns=44961820800
[2026-10-10 09:31:07.172] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=28576 peak_pending_bytes=106784 pressure_failures=0 access_units=3004 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=816 maximum_mux_input_after_emit_ns=186809832 maximum_mux_input_after_dispatch_ns=86809832 worst_mux_input_ready_ns=44962192700 worst_mux_input_emit_ns=44775382868 worst_mux_input_dispatch_ns=44875382868 maximum_protocol_batch_produced_after_release_ns=13670700 worst_protocol_batch_produced_ns=41955721000 worst_protocol_batch_release_ns=41942050300 worst_protocol_batch_deadline_ns=42042049534 worst_protocol_batch_datagrams=21 exit_reason=InvalidArgument: MPEG-TS access unit has no transport service window stream=video emit_ns=44775382868 dispatch_ns=44875382868 available_ns=44942050360 last_committed_kind=maintenance service_start_ns=44942050360 actual_now_ns=44962224200 remaining_ns=-66667492 payload_bytes=28576 wire_bytes=28840
[2026-10-10 09:31:07.172] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=41122539682 emit_ns=44797922550 dispatch_ns=44897922550 committed_at_ns=44962560800 head_arrived_ns=44961458000 scheduled_at_ns=44962546300
[2026-10-10 09:31:07.184] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=4454 committed_datagrams=29382 committed_payload_bytes=35631852 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=29382 pacing_cancelled=0 pacing_submitted=29382 pacing_maximum_submit_lateness_ns=13254532 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=48059200 last_materialized_sequence=29382 last_scheduled_sequence=29382 last_submitted_sequence=29382 last_committed_sequence=29382 shared_scope_charged_datagrams=29382 shared_scope_charged_wire_bytes=36454548 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=29373 endpoint_1_committed_payload_bytes=35631240 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=9 endpoint_2_committed_payload_bytes=612
[CLI] final runtime report: queued=0, peakQueued=610, threads=78, processThreads=128, workers=0, workerProgress=90640, workerProcessCalls=495738, workerWaits=405086, workerWakeups=402074, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=138, logicalProcessors=22, averageSystemMachineCpuPercent=17.679825, averageProcessMachineCpuPercent=1.866182, peakProcessMachineCpuPercent=4.889976, averageProcessSingleCoreCpuPercent=41.056014, peakProcessSingleCoreCpuPercent=107.579462, initialWorkingSetBytes=211415040, workingSetBytes=255557632, peakWorkingSetBytes=255557632, totalPushed=95065, totalPopped=94667, droppedBuffers=0, encodedPacketsPushed=13800, encodedPacketsPopped=13797, backpressureItems=119, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=12932527, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=431, graphPayloadReservations=35388, graphPayloadReleases=35388, graphPayloadPressureFailures=10964, lastInputReceivedAtNs=2932078560800
```
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R140Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r140Target=3580
$r140Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r140Target"
if($r140Process.Name -ne 'dumpcap.exe' -or $r140Process.CommandLine -notlike '*composition-wiring-r140*'){throw 'PID identity mismatch'}
[R140Console]::FreeConsole() | Out-Null
if(-not [R140Console]::AttachConsole($r140Target)){throw 'AttachConsole failed'}
try {
if(-not [R140Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r140Ids=[uint32[]]::new(32)
$r140Count=[R140Console]::GetConsoleProcessList($r140Ids,32)
if($r140Count -ne 2 -or $r140Ids[0..($r140Count-1)] -notcontains $r140Target -or $r140Ids[0..($r140Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R140Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R140Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```
清理清单：
- composition-wiring-r140-capture-stdout.log
- composition-wiring-r140-capture.log
- composition-wiring-r140-cli-error.log
- composition-wiring-r140-cli.log
- composition-wiring-r140-metrics.csv
- composition-wiring-r140-received.ts
- composition-wiring-r140-source0-stdout.log
- composition-wiring-r140-source0.log
- composition-wiring-r140-source1-stdout.log
- composition-wiring-r140-source1.log
- composition-wiring-r140-source2-stdout.log
- composition-wiring-r140-source2.log
- composition-wiring-r140-three.png
- composition-wiring-r140-vlc.log
- composition-wiring-r140-wire.pcapng
- composition-wiring-r140.sdp

清理首轮在pcapng遇到文件占用，已清除14件；宿主核验无媒体/抓包读取进程后重试剩余pcapng和sdp成功。共16件产物清理完成，r140无残留；原源及Release保留，未运行远程测试。

## r141：九阶段诊断原规格三源复验（2026-10-10）

双独立源码PASS，Release全量clean-first/all-target exit0。仅realtime，无local测试。以下为实际启动命令；结果待运行。

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r141 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r141- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r141-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

结果：启动串行工具等待导致第三路源迟于预检期限；CLI23988于09:45:58自然exit1，原错NotInitialized: raw RTP video signaling probe reached open timeout before complete parameter sets; matching_rtp_packets=0 matching_datagram_bytes=0 parameter_sets=none。未进入生产DAG，无CPU/AV运行期验收指标，不判断代码回归。三源3580/33124/12680在已判定结束后经路径及端口核验强制清理；不计源自然结束或120秒通过。VLC11776 RC quit exit0；capture27336独立控制台Ctrl-C exit0，149742包/drop0，未分析媒体连续性。Release产物09:44:28、5782016B。
清理清单：
- composition-wiring-r141-capture-stdout.log (0B)
- composition-wiring-r141-capture.log (306B)
- composition-wiring-r141-cli-error.log (214B)
- composition-wiring-r141-cli.log (1696B)
- composition-wiring-r141-received.ts (0B)
- composition-wiring-r141-source0-stdout.log (540B)
- composition-wiring-r141-source0.log (17325B)
- composition-wiring-r141-source1-stdout.log (335B)
- composition-wiring-r141-source1.log (13917B)
- composition-wiring-r141-source2-stdout.log (335B)
- composition-wiring-r141-source2.log (10377B)
- composition-wiring-r141-vlc.log (13093B)
- composition-wiring-r141-wire.pcapng (179347332B)

r141上述13件临时产物逐项清理完成。

## r142：并行启动源流后原规格复验

同一09:44:28 Release，参数不变；先capture/VLC，再三源并行启动，最后CLI，避免把工具等待消耗在CLI open期限。

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r142 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r142- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

### r142结果与诊断边界

CLI34108自然exit1：InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization。三源10188/10744/34120均完整120秒、3600帧且工具会话exit0；CLI在源结束前已失败。九阶段诊断生效，但没有r140同类late_audio_head/late_output_commit，本轮不可判音频229ms等待已修复。
VLC27564窗口截图返回CopyFromScreen句柄无效；随后RC snapshot未生成图片，三格画面未核验。09:54宿主CIM确认本轮全部媒体进程已不存在；capture54676/VLC16519/monitor11945工具句柄也已失效。抓包无最终统计，VLC无正常退出证据，不宣称两者exit0或drop0。未做离线连续性/内容AV分析，未做重入；完整验收FAIL。
监控统计：{"Samples":60,"CpuMean":2.4076909966935798,"CpuMax":3.37133099059693,"WorkingSetMax":257654784.0,"PrivateMax":868630528.0}。内部filtered_phase共80条，范围2461..2461ns，不等于内容AV。

关键原始记录：
```text
[2026-10-10 09:49:33.814] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=106972 pressure_failures=0 access_units=5842 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=1592 maximum_mux_input_after_emit_ns=122834199 maximum_mux_input_after_dispatch_ns=22834199 worst_mux_input_ready_ns=84169824700 worst_mux_input_emit_ns=84046990501 worst_mux_input_dispatch_ns=84146990501 maximum_protocol_batch_produced_after_release_ns=122948599 worst_protocol_batch_produced_ns=84169939100 worst_protocol_batch_release_ns=84046990501 worst_protocol_batch_deadline_ns=84146990501 worst_protocol_batch_datagrams=18 exit_reason=aborted
[2026-10-10 09:49:33.816] [info] [graph][runtime.node] audio_stage_observation node=70 stage=scheduler_input count=3444 last_generation=1 last_begin=3525632 last_end=3526656 last_rate=44100 last_steady_ns=4038561462800 maximum_inter_event_gap_ns=45442500 gap_begin_generation=1 gap_begin_begin=3521536 gap_begin_end=3522560 gap_begin_rate=44100 gap_begin_steady_ns=4038471199300 gap_end_generation=1 gap_end_begin=3522560 gap_end_end=3523584 gap_end_rate=44100 gap_end_steady_ns=4038516641800
[2026-10-10 09:49:33.817] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=3445 last_generation=1 last_begin=3526656 last_end=3527680 last_rate=44100 last_steady_ns=4038581453700 maximum_inter_event_gap_ns=36446800 gap_begin_generation=1 gap_begin_begin=3516416 gap_begin_end=3517440 gap_begin_rate=44100 gap_begin_steady_ns=4038347386300 gap_end_generation=1 gap_end_begin=3517440 gap_end_end=3518464 gap_end_rate=44100 gap_end_steady_ns=4038383833100
[2026-10-10 09:49:33.817] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=3445 last_generation=1 last_begin=3526656 last_end=3527680 last_rate=44100 last_steady_ns=4038581478100 maximum_inter_event_gap_ns=36462600 gap_begin_generation=1 gap_begin_begin=3516416 gap_begin_end=3517440 gap_begin_rate=44100 gap_begin_steady_ns=4038347398000 gap_end_generation=1 gap_end_begin=3517440 gap_end_end=3518464 gap_end_rate=44100 gap_end_steady_ns=4038383860600
[2026-10-10 09:49:33.818] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=3447 last_generation=1 last_begin=3528704 last_end=3529728 last_rate=44100 last_steady_ns=4038579283700 maximum_inter_event_gap_ns=35889700 gap_begin_generation=1 gap_begin_begin=3518464 gap_begin_end=3519488 gap_begin_rate=44100 gap_begin_steady_ns=4038346742800 gap_end_generation=1 gap_end_begin=3519488 gap_end_end=3520512 gap_end_rate=44100 gap_end_steady_ns=4038382632500
[2026-10-10 09:49:33.819] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=3447 last_generation=1 last_begin=3528704 last_end=3529728 last_rate=44100 last_steady_ns=4038579891100 maximum_inter_event_gap_ns=36430100 gap_begin_generation=1 gap_begin_begin=3518464 gap_begin_end=3519488 gap_begin_rate=44100 gap_begin_steady_ns=4038347341800 gap_end_generation=1 gap_end_begin=3519488 gap_end_end=3520512 gap_end_rate=44100 gap_end_steady_ns=4038383771900
[2026-10-10 09:49:33.819] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=3445 last_generation=1 last_begin=3526656 last_end=3527680 last_rate=44100 last_steady_ns=4038579915500 maximum_inter_event_gap_ns=36438300 gap_begin_generation=1 gap_begin_begin=3516416 gap_begin_end=3517440 gap_begin_rate=44100 gap_begin_steady_ns=4038347353900 gap_end_generation=1 gap_end_begin=3517440 gap_end_end=3518464 gap_end_rate=44100 gap_end_steady_ns=4038383792200
[2026-10-10 09:49:33.819] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=3445 last_generation=1 last_begin=3526656 last_end=3527680 last_rate=44100 last_steady_ns=4038579929200 maximum_inter_event_gap_ns=36446400 gap_begin_generation=1 gap_begin_begin=3516416 gap_begin_end=3517440 gap_begin_rate=44100 gap_begin_steady_ns=4038347363600 gap_end_generation=1 gap_end_begin=3517440 gap_end_end=3518464 gap_end_rate=44100 gap_end_steady_ns=4038383810000
[2026-10-10 09:49:33.821] [info] [graph][runtime.node] audio_stage_observation node=6 stage=aggregate_built count=3447 last_generation=1 last_begin=3528704 last_end=3529728 last_rate=44100 last_steady_ns=4038578933500 maximum_inter_event_gap_ns=36086100 gap_begin_generation=1 gap_begin_begin=3518464 gap_begin_end=3519488 gap_begin_rate=44100 gap_begin_steady_ns=4038346497400 gap_end_generation=1 gap_end_begin=3519488 gap_end_end=3520512 gap_end_rate=44100 gap_end_steady_ns=4038382583500
[2026-10-10 09:49:33.821] [info] [graph][runtime.node] audio_stage_observation node=6 stage=aggregate_committed count=3447 last_generation=1 last_begin=3528704 last_end=3529728 last_rate=44100 last_steady_ns=4038578949000 maximum_inter_event_gap_ns=36089800 gap_begin_generation=1 gap_begin_begin=3518464 gap_begin_end=3519488 gap_begin_rate=44100 gap_begin_steady_ns=4038346505700 gap_end_generation=1 gap_end_begin=3519488 gap_end_end=3520512 gap_end_rate=44100 gap_end_steady_ns=4038382595500
[CLI] final runtime report: queued=0, peakQueued=745, threads=78, processThreads=126, workers=0, workerProgress=173403, workerProcessCalls=1045642, workerWaits=872228, workerWakeups=866428, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=248, logicalProcessors=22, averageSystemMachineCpuPercent=24.081835, averageProcessMachineCpuPercent=2.438419, peakProcessMachineCpuPercent=5.828780, averageProcessSingleCoreCpuPercent=53.645221, peakProcessSingleCoreCpuPercent=128.233151, initialWorkingSetBytes=211255296, workingSetBytes=257458176, peakWorkingSetBytes=257654784, totalPushed=182173, totalPopped=181666, droppedBuffers=0, encodedPacketsPushed=26815, encodedPacketsPopped=26814, backpressureItems=119, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15066178, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=538, graphPayloadReservations=68059, graphPayloadReleases=68059, graphPayloadPressureFailures=21280, lastInputReceivedAtNs=4038573497400
` 

清理清单（仅本轮临时产物）：
- composition-wiring-r142-capture-stdout.log (0B)
- composition-wiring-r142-capture.log (141B)
- composition-wiring-r142-cli-error.log (2162B)
- composition-wiring-r142-cli.log (390474B)
- composition-wiring-r142-metrics.csv (4063B)
- composition-wiring-r142-received.ts (67940944B)
- composition-wiring-r142-source0-stdout.log (540B)
- composition-wiring-r142-source0.log (29403B)
- composition-wiring-r142-source1-stdout.log (335B)
- composition-wiring-r142-source1.log (28994B)
- composition-wiring-r142-source2-stdout.log (335B)
- composition-wiring-r142-source2.log (28808B)
- composition-wiring-r142-vlc.log (24993B)
- composition-wiring-r142-wire.pcapng (462796644B)
- composition-wiring-r142.sdp (260B)

上述15件临时产物逐项清理完成；原始120秒源及Release保留。r142九阶段快照只证明诊断生效，未复现r140的229ms音频等待。

补充实际监控与失败截图命令：
```powershell
$path='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-metrics.csv'; 'utc,cpu_percent,working_set_bytes,private_bytes' | Set-Content -LiteralPath $path; $p=Get-Process -Id 34108 -ErrorAction Stop; $last=$p.CPU; $t=[DateTime]::UtcNow; while(!$p.HasExited){Start-Sleep -Seconds 1; $p.Refresh(); if($p.HasExited){break}; $now=[DateTime]::UtcNow; $cpu=100*($p.CPU-$last)/($now-$t).TotalSeconds/[Environment]::ProcessorCount; "$($now.ToString('o')),$cpu,$($p.WorkingSet64),$($p.PrivateMemorySize64)" | Add-Content -LiteralPath $path; $last=$p.CPU; $t=$now}; 'MONITOR_FINISHED'
```
```powershell
Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public class R142Window { [DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h,int n); [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int cx,int cy,uint flags); }'; [R142Window]::SetProcessDPIAware() | Out-Null; $h=(Get-Process -Id 27564).MainWindowHandle; [R142Window]::ShowWindowAsync($h,9) | Out-Null; [R142Window]::SetWindowPos($h,[IntPtr](-1),0,0,0,0,3) | Out-Null; try {Start-Sleep -Milliseconds 500; & 'C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1' -WindowHandle ([int]$h) -Path 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r142-three.png'} finally {[R142Window]::SetWindowPos($h,[IntPtr](-2),0,0,0,0,3) | Out-Null}
```

VLC内置快照实际尝试（未生成图片）：
```powershell
$ErrorActionPreference='Stop'
$p=Get-CimInstance Win32_Process -Filter 'ProcessId=27564';if($p.Name -ne 'vlc.exe' -or $p.CommandLine -notlike '*composition-wiring-r142*'){throw 'VLC identity mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);try{$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n");$c.GetStream().Write($b,0,$b.Length)}finally{$c.Dispose()}
Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-wiring-r142*.png' | Select-Object FullName,Length
```
## r143：同AU阶段关联诊断原规格复验

7文件诊断双独立源码PASS，Release clean-first/all-target重建configure/build exit0。保持三源120秒720p30 H264，输出HEVC CBR8Mbps/720p30 AAC192kbps MPEGTS/RTP，原逐源等待100/150/200ms，wire100ms，egress50Mbps。capture/VLC先启动，其余并行；仅realtime测试。

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r143 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r143- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

实际监控、窗口观察和capture结束命令：

```powershell
$path='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-metrics.csv'; 'utc,cpu_percent,working_set_bytes,private_bytes' | Set-Content -LiteralPath $path; $p=Get-Process -Id 36372 -ErrorAction Stop; $last=$p.CPU; $t=[DateTime]::UtcNow; while(!$p.HasExited){Start-Sleep -Seconds 1; $p.Refresh(); if($p.HasExited){break}; $now=[DateTime]::UtcNow; $cpu=100*($p.CPU-$last)/($now-$t).TotalSeconds/[Environment]::ProcessorCount; "$($now.ToString('o')),$cpu,$($p.WorkingSet64),$($p.PrivateMemorySize64)" | Add-Content -LiteralPath $path; $last=$p.CPU; $t=$now}; 'MONITOR_FINISHED'
```

```powershell
Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public class R143Window { [DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h,int n); [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int cx,int cy,uint flags); }'; [R143Window]::SetProcessDPIAware() | Out-Null; $h=(Get-Process -Id 34116).MainWindowHandle; [R143Window]::ShowWindowAsync($h,9) | Out-Null; [R143Window]::SetWindowPos($h,[IntPtr](-1),0,0,0,0,3) | Out-Null; try {Start-Sleep -Milliseconds 500; & 'C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1' -WindowHandle ([int]$h) -Path 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r143-three.png'} finally {[R143Window]::SetWindowPos($h,[IntPtr](-2),0,0,0,0,3) | Out-Null}
```

```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R143Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r143Target=36860
$r143Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r143Target"
if($r143Process.Name -ne 'dumpcap.exe' -or $r143Process.CommandLine -notlike '*composition-wiring-r143*'){throw 'PID identity mismatch'}
[R143Console]::FreeConsole() | Out-Null
if(-not [R143Console]::AttachConsole($r143Target)){throw 'AttachConsole failed'}
try {
if(-not [R143Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r143Ids=[uint32[]]::new(32)
$r143Count=[R143Console]::GetConsoleProcessList($r143Ids,32)
if($r143Count -ne 2 -or $r143Ids[0..($r143Count-1)] -notcontains $r143Target -or $r143Ids[0..($r143Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R143Console]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R143Console]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

### r143结果：运行FAIL，同AU与音频阶段证据有效

CLI36372自然exit1，错误protocol datagram exceeded its absolute emission deadline before materialization。source AV30196、纯视频31004/28576在失败已判定后按PID/源路径/端口核验强制结束，不计源自然结束或完整120秒。VLC34116经RC quit正常exit0，capture36860经独立控制台Ctrl-C正常exit0，monitor正常退出。

VLC窗口截图实际核验三格视频与右下黑格，未做内容级AV或离线连续性审计。Release产物10:08:44、5786112B，源码双独立PASS和全量构建exit0不等于运行通过。未做重入或单源回归。

本次audio到scheduler已晚8.408071ms、video head在scheduler停留151.6268ms；同AU trace：audio scheduledCreated→sessionReady 0.4488ms，video 0.5675ms，故不能把这两个AU主要迟到归于TS adapter/mux阶段。音频aggregate相邻连续样本2779136..2780160与2780160..2781184生成间隔150.9643ms；后一个样本aggregate built→commit 8.9us，commit→encoder input36.9us，input→submit577us。主要间隔在第二个aggregate_built观测时已形成，该观测位于buildAudio末尾，仍包含音频构建自身；仍不能区分视频compose、输出背压、输入/锁处理或worker停顿，不能直接定责GPU。
监控：{"Samples":41,"CpuMean":2.506467748846765,"CpuMax":4.26831679439411,"WorkingSetMax":251637760.0,"PrivateMax":864882688.0}，内部filtered_phase 63条，范围118..118ns，不等于内容AV。
抓包最终统计：

Packets captured: 342145
Packets received/dropped on interface 'Adapter for loopback traffic capture': 342145/0 (pcap:0/dumpcap:0/flushed:0/ps_ifdrop:0) (100.0%)
源最后进度：
frame= 3251 fps= 30 q=-1.0 size=  107500KiB time=00:01:48.36 bitrate=8126.4kbits/s speed=   1x elapsed=0:01:47.84    
frame= 3266 fps= 30 q=-1.0 size=  108260KiB time=00:01:48.86 bitrate=8146.3kbits/s speed=   1x elapsed=0:01:48.35    
frame= 3297 fps= 30 q=-1.0 size=  109014KiB time=00:01:49.90 bitrate=8125.9kbits/s speed=   1x elapsed=0:01:49.39    

关键原始记录：
```text
[2026-10-10 10:10:56.605] [info] [graph][runtime.node] av_scheduler_trace stage=late_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=62995736961 canonical_dispatch_ns=62995736961 target_master_ns=66818779029 dispatch_master_ns=66818779029 now_ns=66827187600
[2026-10-10 10:10:56.606] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=62995736961 emit_ns=66718779029 dispatch_ns=66818779029 committed_at_ns=66827670400 head_arrived_ns=66827187100 scheduled_at_ns=66827638800
[2026-10-10 10:10:56.606] [info] [graph][runtime.node] au_processing_trace stage=late_mux_input generation=1 stream=audio canonical_sequence=2714 dispatch_master_ns=66818779029 ready_master_ns=66828089000 scheduled_created_steady_ns=5321524632400 adapter_dequeued_steady_ns=5321524700700 adapted_created_steady_ns=5321524702300 write_entered_steady_ns=5321525078900 session_ready_steady_ns=5321525081200
[2026-10-10 10:10:56.606] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=video generation=1 canonical_dispatch_ns=63000000000 emit_ns=66723042068 dispatch_ns=66823042068 committed_at_ns=66828115700 head_arrived_ns=66676459600 scheduled_at_ns=66828086400
[2026-10-10 10:10:56.607] [info] [graph][runtime.node] au_processing_trace stage=late_mux_input generation=1 stream=video canonical_sequence=1891 dispatch_master_ns=66823042068 ready_master_ns=66828656200 scheduled_created_steady_ns=5321525081000 adapter_dequeued_steady_ns=5321525132400 adapted_created_steady_ns=5321525132700 write_entered_steady_ns=5321525626800 session_ready_steady_ns=5321525648500
[2026-10-10 10:10:56.619] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=6821 committed_datagrams=44921 committed_payload_bytes=54443628 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=44921 pacing_cancelled=0 pacing_submitted=44921 pacing_maximum_submit_lateness_ns=41757120 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=54767000 last_materialized_sequence=44921 last_scheduled_sequence=44921 last_submitted_sequence=44921 last_committed_sequence=44921 shared_scope_charged_datagrams=44921 shared_scope_charged_wire_bytes=55701416 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=44906 endpoint_1_committed_payload_bytes=54442608 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=15 endpoint_2_committed_payload_bytes=1020
[2026-10-10 10:10:56.722] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=92872 pressure_failures=0 access_units=4607 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=70930967 maximum_mux_input_after_emit_ns=109309971 maximum_mux_input_after_dispatch_ns=9309971 worst_mux_input_ready_ns=66828089000 worst_mux_input_emit_ns=66718779029 worst_mux_input_dispatch_ns=66818779029 maximum_protocol_batch_produced_after_release_ns=51471228 worst_protocol_batch_produced_ns=60641181100 worst_protocol_batch_release_ns=60589709872 worst_protocol_batch_deadline_ns=60689708734 worst_protocol_batch_datagrams=21 exit_reason=aborted
[2026-10-10 10:10:56.722] [info] [graph][runtime.node] audio_stage_observation node=70 stage=scheduler_input count=2716 last_generation=1 last_begin=2780160 last_end=2781184 last_rate=44100 last_steady_ns=5321525630100 maximum_inter_event_gap_ns=150769500 gap_begin_generation=1 gap_begin_begin=2777088 gap_begin_end=2778112 gap_begin_rate=44100 gap_begin_steady_ns=5321373409600 gap_end_generation=1 gap_end_begin=2778112 gap_end_end=2779136 gap_end_rate=44100 gap_end_steady_ns=5321524179100
[2026-10-10 10:10:56.724] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=2717 last_generation=1 last_begin=2781184 last_end=2782208 last_rate=44100 last_steady_ns=5321526367400 maximum_inter_event_gap_ns=150794400 gap_begin_generation=1 gap_begin_begin=2777088 gap_begin_end=2778112 gap_begin_rate=44100 gap_begin_steady_ns=5321373359200 gap_end_generation=1 gap_end_begin=2778112 gap_end_end=2779136 gap_end_rate=44100 gap_end_steady_ns=5321524153600
[2026-10-10 10:10:56.724] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=2717 last_generation=1 last_begin=2781184 last_end=2782208 last_rate=44100 last_steady_ns=5321526375300 maximum_inter_event_gap_ns=150782800 gap_begin_generation=1 gap_begin_begin=2777088 gap_begin_end=2778112 gap_begin_rate=44100 gap_begin_steady_ns=5321373383500 gap_end_generation=1 gap_end_begin=2778112 gap_end_end=2779136 gap_end_rate=44100 gap_end_steady_ns=5321524166300
[2026-10-10 10:10:56.725] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=2719 last_generation=1 last_begin=2783232 last_end=2784256 last_rate=44100 last_steady_ns=5321525771700 maximum_inter_event_gap_ns=150960000 gap_begin_generation=1 gap_begin_begin=2779136 gap_begin_end=2780160 gap_begin_rate=44100 gap_begin_steady_ns=5321372587800 gap_end_generation=1 gap_end_begin=2780160 gap_end_end=2781184 gap_end_rate=44100 gap_end_steady_ns=5321523547800
[2026-10-10 10:10:56.725] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=2719 last_generation=1 last_begin=2783232 last_end=2784256 last_rate=44100 last_steady_ns=5321526327300 maximum_inter_event_gap_ns=150825500 gap_begin_generation=1 gap_begin_begin=2779136 gap_begin_end=2780160 gap_begin_rate=44100 gap_begin_steady_ns=5321373299300 gap_end_generation=1 gap_end_begin=2780160 gap_end_end=2781184 gap_end_rate=44100 gap_end_steady_ns=5321524124800
[2026-10-10 10:10:56.725] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=2717 last_generation=1 last_begin=2781184 last_end=2782208 last_rate=44100 last_steady_ns=5321526344300 maximum_inter_event_gap_ns=150811700 gap_begin_generation=1 gap_begin_begin=2777088 gap_begin_end=2778112 gap_begin_rate=44100 gap_begin_steady_ns=5321373321900 gap_end_generation=1 gap_end_begin=2778112 gap_end_end=2779136 gap_end_rate=44100 gap_end_steady_ns=5321524133600
[2026-10-10 10:10:56.725] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=2717 last_generation=1 last_begin=2781184 last_end=2782208 last_rate=44100 last_steady_ns=5321526362000 maximum_inter_event_gap_ns=150803300 gap_begin_generation=1 gap_begin_begin=2777088 gap_begin_end=2778112 gap_begin_rate=44100 gap_begin_steady_ns=5321373341700 gap_end_generation=1 gap_end_begin=2778112 gap_end_end=2779136 gap_end_rate=44100 gap_end_steady_ns=5321524145000
[2026-10-10 10:10:56.726] [info] [graph][runtime.node] audio_stage_observation node=6 stage=aggregate_built count=2719 last_generation=1 last_begin=2783232 last_end=2784256 last_rate=44100 last_steady_ns=5321525741500 maximum_inter_event_gap_ns=150964300 gap_begin_generation=1 gap_begin_begin=2779136 gap_begin_end=2780160 gap_begin_rate=44100 gap_begin_steady_ns=5321372537700 gap_end_generation=1 gap_end_begin=2780160 gap_end_end=2781184 gap_end_rate=44100 gap_end_steady_ns=5321523502000
[2026-10-10 10:10:56.726] [info] [graph][runtime.node] audio_stage_observation node=6 stage=aggregate_committed count=2719 last_generation=1 last_begin=2783232 last_end=2784256 last_rate=44100 last_steady_ns=5321525753100 maximum_inter_event_gap_ns=150952200 gap_begin_generation=1 gap_begin_begin=2779136 gap_begin_end=2780160 gap_begin_rate=44100 gap_begin_steady_ns=5321372558700 gap_end_generation=1 gap_end_begin=2780160 gap_end_end=2781184 gap_end_rate=44100 gap_end_steady_ns=5321523510900
[CLI] final runtime report: queued=0, peakQueued=392, threads=78, processThreads=125, workers=0, workerProgress=135199, workerProcessCalls=810489, workerWaits=675274, workerWakeups=670718, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=194, logicalProcessors=22, averageSystemMachineCpuPercent=20.294055, averageProcessMachineCpuPercent=2.495412, peakProcessMachineCpuPercent=5.183585, averageProcessSingleCoreCpuPercent=54.899060, peakProcessSingleCoreCpuPercent=114.038877, initialWorkingSetBytes=205881344, workingSetBytes=250802176, peakWorkingSetBytes=250867712, totalPushed=142111, totalPopped=141931, droppedBuffers=0, encodedPacketsPushed=21147, encodedPacketsPopped=21146, backpressureItems=119, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=8820212, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=216, graphPayloadReservations=53475, graphPayloadReleases=53475, graphPayloadPressureFailures=16777, lastInputReceivedAtNs=5321516012200
```
清理清单：
- composition-wiring-r143-capture-stdout.log (0B)
- composition-wiring-r143-capture.log (306B)
- composition-wiring-r143-cli-error.log (2166B)
- composition-wiring-r143-cli.log (332644B)
- composition-wiring-r143-metrics.csv (2791B)
- composition-wiring-r143-received.ts (53478856B)
- composition-wiring-r143-source0-stdout.log (540B)
- composition-wiring-r143-source0.log (26529B)
- composition-wiring-r143-source1-stdout.log (335B)
- composition-wiring-r143-source1.log (26189B)
- composition-wiring-r143-source2-stdout.log (335B)
- composition-wiring-r143-source2.log (26425B)
- composition-wiring-r143-three.png (1004682B)
- composition-wiring-r143-vlc.log (28915B)
- composition-wiring-r143-wire.pcapng (413732176B)
- composition-wiring-r143.sdp (260B)

双独立证据复核：数值与有限定位PASS；根因归责不足，完整验收FAIL。16件产物清理后无本轮文件残留，宿主CIM无媒体进程残留。三源因已失败而清理，末尾108.36/108.86/109.90秒，不计完整120秒通过。原始源124427809B与10:08:44 Release保留。


### r144：聚合阶段墙钟观测，原规格三源HEVC CBR复验

4文件诊断修改双独立源码PASS。固定阶段记录及process调用间隔；墙钟含抢占，不等于GPU时间。普通阶段cursor/next_cursor都是该阶段入口，between_calls的next_cursor是下一次process入口；pending_at_entry是purge前base pendingTransfer。各阶段最大区间不得直接相减。Release构建进行中，尚未启动实流。

实际待执行命令（按capture/VLC，然后三源与CLI并发启动）：

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode

```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r144 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode

```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode

```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode

```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r144- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode

```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode

```

Release全量重建已成功，configure/build exit0，产物2026-10-10 10:25:08、5792768B。实际启动：capture26892、VLC28132、AV源29096、V61030源14944、V61040源2492、CLI25160。CPU/内存监控已启动。截图命令返回CopyFromScreen句柄无效，未产生可验收画面。

```powershell
$path='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-metrics.csv'; 'utc,cpu_percent,working_set_bytes,private_bytes' | Set-Content -LiteralPath $path; $p=Get-Process -Id 25160 -ErrorAction Stop; $last=$p.CPU; $t=[DateTime]::UtcNow; while(!$p.HasExited){Start-Sleep -Seconds 1; $p.Refresh(); if($p.HasExited){break}; $now=[DateTime]::UtcNow; $cpu=100*($p.CPU-$last)/($now-$t).TotalSeconds/[Environment]::ProcessorCount; "$($now.ToString('o')),$cpu,$($p.WorkingSet64),$($p.PrivateMemorySize64)" | Add-Content -LiteralPath $path; $last=$p.CPU; $t=$now}; 'MONITOR_FINISHED'

```

```powershell
Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public class R144Window { [DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h,int n); [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int cx,int cy,uint flags); }'; [R144Window]::SetProcessDPIAware() | Out-Null; $h=(Get-Process -Id 28132).MainWindowHandle; [R144Window]::ShowWindowAsync($h,9) | Out-Null; [R144Window]::SetWindowPos($h,[IntPtr](-1),0,0,0,0,3) | Out-Null; try {Start-Sleep -Milliseconds 500; & 'C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1' -WindowHandle ([int]$h) -Path 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-three.png'} finally {[R144Window]::SetWindowPos($h,[IntPtr](-2),0,0,0,0,3) | Out-Null}

```

三源29096/14944/2492各自120秒、3600帧、自然exit0。CLI在全源purge后仍持续输出，无runtime错误，符合continuous output而非自然EOF退出。随后所选AV重入PID3428（session87424），已出现generation2及内部drift记录。

```powershell
$r144Vlc=Get-CimInstance Win32_Process -Filter 'ProcessId=28132'; if($r144Vlc.Name -ne 'vlc.exe' -or $r144Vlc.CommandLine -notlike '*composition-wiring-r144*'){throw 'VLC identity mismatch'}; $r144Tcp=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try{$r144Stream=$r144Tcp.GetStream(); $r144Data=[Text.Encoding]::ASCII.GetBytes("snapshot`n"); $r144Stream.Write($r144Data,0,$r144Data.Length); Start-Sleep -Milliseconds 700; if($r144Stream.DataAvailable){$r144Buffer=[byte[]]::new(8192); $r144Read=$r144Stream.Read($r144Buffer,0,$r144Buffer.Length); [Text.Encoding]::UTF8.GetString($r144Buffer,0,$r144Read)}} finally{$r144Tcp.Dispose()}; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-wiring-r144-*.png' | Select-Object FullName,Length
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-source0-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-source0-reentry.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode

```

VLC RC snapshot返回0，但项目指定快照路径无文件，不能作视觉PASS。

### r144结果：运行未复现旧故障，完整门禁仍FAIL

三源与AV重入各完整120秒/3600帧自然exit0（源PID29096/14944/2492及重入3428，对应工具会话45580/63251/98293/87424）；CLI25160由调用方Ctrl-C正常exit0（11195），end_reason=caller_stop，不是自然EOF。VLC28132 RC quit exit0（57038），capture26892 Ctrl-C exit0（59962），monitor74649正常结束。停止及离线分析后宿主CIM未见媒体/抓包/分析进程残留。

本轮无late_audio_head/late_output_commit或deadline失败；process最大16.6983ms、video16.6567ms、canvas16.5904ms属于同cursor1525/2242560且区间嵌套，只能称调用墙钟。audio最大1.4976ms；aggregate_built最大间隔32.9401ms。between_calls最大107.0106ms位于cursor0启动期、前次waiting且无deadline，不能解释r143活动期150.9643ms。两名独立运行复核均给有限证据PASS，不能推断既有间歇超期已修复。

payload177987预留=释放、结束0B/0对象，errors/workerErrors0；graphPayloadPressureFailures47337是背压尝试计数，不等于mux pressure_failures0。encoded93475/93474、AAC关闭残留2帧、mux pending188B，不计完整drain。stderr仍有源色彩range变更警告。sender deadline_misses0但delivery_evidence=not_proven。
监控：{"Samples":260,"CpuMean":2.113728902898808,"CpuMax":3.62441542193981,"WorkingSetMax":263262208,"PrivateMax":870117376}。generation1内部filtered_phase111条均20345ns，generation2共117条均13733ns，不是内容AV证明或长期无泄漏证明。

VLC窗口CopyFromScreen失败；RC snapshot返回0但无文件，视觉门禁未通过。VLC录制离线解码exit0但有以下原始DTS警告：
```text
[in#0/mpegts @ 000001e348b4b1c0] DTS 123301 < 126000 out of order
[null @ 000001e348b6f0c0] Application provided invalid, non monotonically increasing dts to muxer in stream 1: 45056 >= 43288
[null @ 000001e348b6f0c0] Application provided invalid, non monotonically increasing dts to muxer in stream 1: 45056 >= 44312
[null @ 000001e348b6f0c0] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 30 >= 29
[null @ 000001e348b6f0c0] Application provided invalid, non monotonically increasing dts to muxer in stream 0: 30 >= 30
```
为区别接收重封装与生产出口，已从同一pcap离线提取出口RTP：173558包、SSRC29801AD0、序号异常0，明确排除唯一已知非包footer；wire.ts203684840B。ffprobe统计video8362/audio12004 packets，逐流DTS非递增各0；wire完整AV解码exit0，warning日志0B。警告仅在VLC录制产物被观察到，现有证据不支持据此修改生产时间戳；不将解码正常等同内容同步、视觉或逐包发送deadline证明。

结束及离线实际命令：

```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R144CliConsole {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r144Target=25160
$r144Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r144Target"
if($r144Process.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r144Process.CommandLine -notlike '*composition-wiring-r144*'){throw 'PID identity mismatch'}
[R144CliConsole]::FreeConsole() | Out-Null
if(-not [R144CliConsole]::AttachConsole($r144Target)){throw 'AttachConsole failed'}
try {
if(-not [R144CliConsole]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r144Ids=[uint32[]]::new(32)
$r144Count=[R144CliConsole]::GetConsoleProcessList($r144Ids,32)
if($r144Count -ne 2 -or $r144Ids[0..($r144Count-1)] -notcontains $r144Target -or $r144Ids[0..($r144Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R144CliConsole]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R144CliConsole]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

```powershell
$p=Get-CimInstance Win32_Process -Filter 'ProcessId=28132'; if($p.Name -ne 'vlc.exe' -or $p.CommandLine -notlike '*composition-wiring-r144*'){throw 'VLC identity mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length)}finally{$c.Dispose()}
```

```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R144CaptureConsole {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r144Target=26892
$r144Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r144Target"
if($r144Process.Name -ne 'dumpcap.exe' -or $r144Process.CommandLine -notlike '*composition-wiring-r144*'){throw 'PID identity mismatch'}
[R144CaptureConsole]::FreeConsole() | Out-Null
if(-not [R144CaptureConsole]::AttachConsole($r144Target)){throw 'AttachConsole failed'}
try {
if(-not [R144CaptureConsole]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r144Ids=[uint32[]]::new(32)
$r144Count=[R144CaptureConsole]::GetConsoleProcessList($r144Ids,32)
if($r144Count -ne 2 -or $r144Ids[0..($r144Count-1)] -notcontains $r144Target -or $r144Ids[0..($r144Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R144CaptureConsole]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R144CaptureConsole]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v warning -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-received.ts' -map 0:v:0 -map 0:a:0 -f null - 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-offline-decode.log'; exit $LASTEXITCODE
```

```powershell
& 'D:/Wireshark/tshark.exe' -n -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-wire.pcapng' -Y 'udp.dstport == 62020' -T fields -e udp.payload > 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-output-payloads.txt' 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-tshark.log'; exit $LASTEXITCODE
```

```powershell
$ErrorActionPreference='Stop'; $count=0L; $anomalies=0L; $footer=0; $last=@{}; $stream=[IO.File]::Create('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-wire.ts'); try { foreach($line in [IO.File]::ReadLines('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-output-payloads.txt')) { if($line -eq 'No matching RTP/H.264 payload dumped.') {$footer++; continue}; if(!$line){continue}; $b=[Convert]::FromHexString($line.Replace(':','')); if($b.Length -lt 12 -or ($b[0]-shr 6)-ne 2){throw 'Invalid RTP'}; $seq=[int]$b[2]*256+$b[3]; $ssrc=[Convert]::ToHexString($b[8..11]); $off=12+4*($b[0]-band 15); if($b[0]-band 16){$off+=4+4*([int]$b[$off+2]*256+$b[$off+3])}; $end=$b.Length; if($b[0]-band 32){$end-=$b[-1]}; if($off -gt $end -or ($end-$off)%188 -ne 0){throw 'Invalid TS payload'}; if($last.ContainsKey($ssrc) -and $seq -ne (($last[$ssrc]+1)%65536)){$anomalies++}; $last[$ssrc]=$seq; $stream.Write($b,$off,$end-$off); $count++ } } finally {$stream.Dispose()}; [pscustomobject]@{Packets=$count;SequenceAnomalies=$anomalies;TextFooters=$footer;SSRC=($last.Keys -join ',');Bytes=(Get-Item 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-wire.ts').Length} | ConvertTo-Json
```

```powershell
$ErrorActionPreference='Stop'; $last=@{}; $counts=@{}; $regressions=@{}; & 'D:/mabs/local64/bin-video/ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts -of csv=p=0 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-wire.ts' | ForEach-Object { $f=$_.Split(','); if($f.Length -ge 3 -and $f[0] -match '^\d+$' -and $f[2] -match '^-?\d+$'){$key=$f[0]; $dts=[long]$f[2]; if(!$counts.ContainsKey($key)){$counts[$key]=0; $regressions[$key]=0}; if($last.ContainsKey($key) -and $dts -le $last[$key]){$regressions[$key]++}; $last[$key]=$dts; $counts[$key]++}}; if($LASTEXITCODE -ne 0){throw 'ffprobe failed'}; [pscustomobject]@{Packets=$counts;NonIncreasingDts=$regressions} | ConvertTo-Json -Compress
```

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v warning -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-wire.ts' -map 0:v:0 -map 0:a:0 -f null - 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-wire-decode.log'; exit $LASTEXITCODE
```

关键原始记录：
```text
[2026-10-10 10:30:34.063] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=188 peak_pending_bytes=98324 pressure_failures=0 access_units=20367 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=5564 maximum_mux_input_after_emit_ns=9337132 maximum_mux_input_after_dispatch_ns=-90662868 worst_mux_input_ready_ns=12048675800 worst_mux_input_emit_ns=12039338668 worst_mux_input_dispatch_ns=12139338668 maximum_protocol_batch_produced_after_release_ns=9412388 worst_protocol_batch_produced_ns=12048751200 worst_protocol_batch_release_ns=12039338812 worst_protocol_batch_deadline_ns=12139338668 worst_protocol_batch_datagrams=22 exit_reason=completed
[2026-10-10 10:30:34.064] [info] [graph][runtime.node] audio_stage_observation node=6 stage=aggregate_built count=12007 last_generation=1 last_begin=12294144 last_end=12295168 last_rate=44100 last_steady_ns=6498953748000 maximum_inter_event_gap_ns=32940100 gap_begin_generation=1 gap_begin_begin=778240 gap_begin_end=779264 gap_begin_rate=44100 gap_begin_steady_ns=6237821947100 gap_end_generation=1 gap_end_begin=779264 gap_end_end=780288 gap_end_rate=44100 gap_end_steady_ns=6237854887200
[2026-10-10 10:30:34.065] [info] [graph][runtime.node] audio_stage_observation node=6 stage=aggregate_committed count=12007 last_generation=1 last_begin=12294144 last_end=12295168 last_rate=44100 last_steady_ns=6498953772500 maximum_inter_event_gap_ns=32930600 gap_begin_generation=1 gap_begin_begin=778240 gap_begin_end=779264 gap_begin_rate=44100 gap_begin_steady_ns=6237821986700 gap_end_generation=1 gap_end_begin=779264 gap_end_end=780288 gap_end_rate=44100 gap_end_steady_ns=6237854917300
[2026-10-10 10:30:34.065] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=87231 maximum_ns=16698300 begin_steady_ns=6271007998600 end_steady_ns=6271024696900 video_cursor=1525 audio_cursor=2242560 next_video_cursor=1525 next_audio_cursor=2242560 error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-10 10:30:34.065] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=87231 maximum_ns=195000 begin_steady_ns=6480052732300 end_steady_ns=6480052927300 video_cursor=7797 audio_cursor=11460608 next_video_cursor=7797 next_audio_cursor=11460608 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:30:34.065] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=87231 maximum_ns=198400 begin_steady_ns=6439162243800 end_steady_ns=6439162442200 video_cursor=6570 audio_cursor=9658368 next_video_cursor=6570 next_audio_cursor=9658368 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:30:34.065] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=87231 maximum_ns=1295200 begin_steady_ns=6409976143900 end_steady_ns=6409977439100 video_cursor=5694 audio_cursor=8371200 next_video_cursor=5694 next_audio_cursor=8371200 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:30:34.065] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=15 maximum_ns=121300 begin_steady_ns=6220174398000 end_steady_ns=6220174519300 video_cursor=0 audio_cursor=0 next_video_cursor=0 next_audio_cursor=0 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:30:34.065] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=8364 maximum_ns=16656700 begin_steady_ns=6271008019800 end_steady_ns=6271024676500 video_cursor=1525 audio_cursor=2242560 next_video_cursor=1525 next_audio_cursor=2242560 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:30:34.065] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_audio count=12007 maximum_ns=1497600 begin_steady_ns=6474154804700 end_steady_ns=6474156302300 video_cursor=7620 audio_cursor=11200512 next_video_cursor=7620 next_audio_cursor=11200512 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:30:34.065] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=8364 maximum_ns=16590400 begin_steady_ns=6271008067200 end_steady_ns=6271024657600 video_cursor=1525 audio_cursor=2242560 next_video_cursor=1525 next_audio_cursor=2242560 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:30:34.065] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=20371 maximum_ns=6627600 begin_steady_ns=6220175121500 end_steady_ns=6220181749100 video_cursor=1 audio_cursor=0 next_video_cursor=1 next_audio_cursor=0 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:30:34.065] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=87230 maximum_ns=107010600 begin_steady_ns=6220064882900 end_steady_ns=6220171893500 video_cursor=0 audio_cursor=0 next_video_cursor=0 next_audio_cursor=0 error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=769, threads=78, processThreads=126, workers=0, workerProgress=459567, workerProcessCalls=2450885, workerWaits=1991311, workerWakeups=1971034, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=873, logicalProcessors=22, averageSystemMachineCpuPercent=16.689480, averageProcessMachineCpuPercent=2.161042, peakProcessMachineCpuPercent=5.378486, averageProcessSingleCoreCpuPercent=47.542917, peakProcessSingleCoreCpuPercent=118.326693, initialWorkingSetBytes=214908928, workingSetBytes=254509056, peakWorkingSetBytes=262598656, totalPushed=464970, totalPopped=464969, droppedBuffers=0, encodedPacketsPushed=93475, encodedPacketsPopped=93474, backpressureItems=119, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15681698, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=564, graphPayloadReservations=177987, graphPayloadReleases=177987, graphPayloadPressureFailures=47337, lastInputReceivedAtNs=6477063902800
Capturing on 'Adapter for loopback traffic capture'
File: D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r144-wire.pcapng

Packets captured: 610850
Packets received/dropped on interface 'Adapter for loopback traffic capture': 610850/0 (pcap:0/dumpcap:0/flushed:0/ps_ifdrop:0) (100.0%)
```

本轮22件清理清单（记录后逐项删除）：
- composition-wiring-r144-capture-stdout.log (0B)
- composition-wiring-r144-capture.log (306B)
- composition-wiring-r144-cli-error.log (2032B)
- composition-wiring-r144-cli.log (1076630B)
- composition-wiring-r144-metrics.csv (17442B)
- composition-wiring-r144-offline-decode.log (563B)
- composition-wiring-r144-output-payloads.txt (411882227B)
- composition-wiring-r144-received.ts (202659488B)
- composition-wiring-r144-source0-reentry-stdout.log (540B)
- composition-wiring-r144-source0-reentry.log (29521B)
- composition-wiring-r144-source0-stdout.log (540B)
- composition-wiring-r144-source0.log (29521B)
- composition-wiring-r144-source1-stdout.log (335B)
- composition-wiring-r144-source1.log (28926B)
- composition-wiring-r144-source2-stdout.log (335B)
- composition-wiring-r144-source2.log (28926B)
- composition-wiring-r144-tshark.log (0B)
- composition-wiring-r144-vlc.log (28394B)
- composition-wiring-r144-wire-decode.log (0B)
- composition-wiring-r144-wire.pcapng (739959640B)
- composition-wiring-r144-wire.ts (203684840B)
- composition-wiring-r144.sdp (260B)

r144清理完成：首轮删除19件，pcap遇占用；宿主CIM未见媒体/分析进程后，重试剩余pcap、wire.ts、sdp成功。22件清单全部清除、r144前缀无残留，原始源124427809B、Release10:25:08/5792768B保留。两名独立审查者对线端有限证据均明确PASS，完整验收仍FAIL；未声称VLC内部根因已定位。

### r145：沿用r144 Release的原规格三源复验

本轮无生产代码修改，沿用10:25:08/5792768B已双审Release；当前非master且无旧测试进程。quser不可用，Win32 OpenInputDesktop检查可访问且执行器/explorer在session1，不把此前截图失败等同链路失败。

原三源V/V/AV：H264/AAC RTP输入→HEVC CBR8Mbps、1280×720/30、AAC192kbps→MPEG-TS/RTP，逐源等待100/150/200ms、出口50Mbps/100ms保持。以下命令先capture/VLC，再三源与CLI并发：

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r145 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-cli-error.log' -PassThru; Write-Output "CLI_PID=$($cli.Id)"; $cli.WaitForExit(); Write-Output "CLI_EXIT=$($cli.ExitCode)"; exit $cli.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r145- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

r145启动进程：capture24664(session18734)、VLC26272(28880)、AV9920(70698)、V61030源9304(76812)、V61040源16576(14625)、CLI11572(96788)、monitor86415。第一轮三源均120秒/3600帧自然exit0；随后三源同时重入，AV13152(81748)、V61030源34612(98986)、V61040源30356(30002)，均沿用原源与参数。已出现各源generation2。

截图精确HWND528362，GetWindowRect成功但CopyFromScreen返回句柄无效；进一步Win32只读检查得到input=Screen-saver、thread=Default、station=WinSta0、virtualscreen4480×1080。已请用户恢复正常桌面，未切换/解锁桌面，视觉门禁仍未完成。初始OpenInputDesktop成功不等于正在显示Default桌面。

```powershell
$path='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-metrics.csv'; 'utc,cpu_percent,working_set_bytes,private_bytes' | Set-Content -LiteralPath $path; $p=Get-Process -Id 11572 -ErrorAction Stop; $last=$p.CPU; $t=[DateTime]::UtcNow; while(!$p.HasExited){Start-Sleep -Seconds 1; $p.Refresh(); if($p.HasExited){break}; $now=[DateTime]::UtcNow; $cpu=100*($p.CPU-$last)/($now-$t).TotalSeconds/[Environment]::ProcessorCount; "$($now.ToString('o')),$cpu,$($p.WorkingSet64),$($p.PrivateMemorySize64)" | Add-Content -LiteralPath $path; $last=$p.CPU; $t=$now}; 'MONITOR_FINISHED'
```

```powershell
Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public class R145Window { [DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h,int n); [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int cx,int cy,uint flags); }'; [R145Window]::SetProcessDPIAware() | Out-Null; $h=(Get-Process -Id 26272).MainWindowHandle; "VLC_HWND=$h"; [R145Window]::ShowWindowAsync($h,9) | Out-Null; [R145Window]::SetWindowPos($h,[IntPtr](-1),0,0,0,0,3) | Out-Null; try {Start-Sleep -Milliseconds 500; & 'C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1' -WindowHandle ([int]$h) -Path 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-three.png'} finally {[R145Window]::SetWindowPos($h,[IntPtr](-2),0,0,0,0,3) | Out-Null}
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-source0-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-source0-reentry.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-source1-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-source1-reentry.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-source2-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-source2-reentry.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

### r145结果：三源全部重入有限双审PASS，完整75/FAIL

本轮无生产代码改动。六次源发送均完整120秒/3600帧自然exit0；各PID及工具会话见启动记录。CLI11572以caller_stop正常exit0（96788），VLC26272 RC quit exit0（28880），capture24664 Ctrl-C exit0（18734），monitor86415正常结束。源自然退出后CLI按连续合屏策略保持输出，不计自然EOF。

process/video/canvas最大19.5471/19.5079/19.4483ms是同cursor5451/8013824嵌套墙钟区间；audio最大1.0668ms、built最大间隔31.7151ms。between_calls106.264ms属于cursor0启动期。r143活动期150.9643ms及旧deadline故障仍未复现，不宣称已修复或GPU根因。各源real/black为6974/1155、7021/1108、6929/1200，各计8129帧；音频real10186752+silence1762304=11949056=11669×1024。结束contribution_generation0符合最终失源，不否定代2恢复。

payload205107预约=释放、结束0B/0对象，errors/workerErrors0；账本背压尝试61660，不能称零背压。encoded90845/90844、AAC关闭残留2帧、mux pending188B，未证明完整drain。监控：{"Samples":250,"CpuMean":2.171766820391115,"CpuMax":3.6984269132739,"WorkingSetMax":265408512,"PrivateMax":877867008}。generation1内部drift111条均18014ns、generation2共117条均15937ns，不等于内容AV同步。sender交付仍not_proven。

capture822589包/drop0；出口171905RTP、SSRC7E023FE3、序号异常0，显式排除一条已知非包文本footer。wire.ts202502320B；ffprobe视频8127/音频11666包，逐流DTS非递增各0；完整AV离线decode exit0、warning日志0B。decoded frame序号900/3750/4800/7980提取4张1280×720图，主智能体及两名独立审查者均实际查看：三格且右下黑→全黑→三格恢复且右下黑→全黑。仅为出口抽帧证据，不代表全时段或现场VLC。silencedetect(-80dB,0.5s)区间112.561293–133.348435秒、251.801429–270.883991秒，后一段至文件结束；不将阈值检测等同逐样本贡献或内容AV。

两个独立审查者均明确有限证据PASS、整体FAIL。现场捕获受Screen-saver/Default桌面差异影响，已请求用户恢复桌面；不修改屏保配置、不绕过锁屏。恢复后补现场证据；同时转向独立C API真实调用方生命周期门禁，不无限重复未复现。完整矩阵、单源、内容AV及Windows后RKMPP等仍未关闭。

结束与离线实际命令：

```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R145CliConsole {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r145Target=11572
$r145Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r145Target"
if($r145Process.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r145Process.CommandLine -notlike '*composition-wiring-r145*'){throw 'PID identity mismatch'}
[R145CliConsole]::FreeConsole() | Out-Null
if(-not [R145CliConsole]::AttachConsole($r145Target)){throw 'AttachConsole failed'}
try {
if(-not [R145CliConsole]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r145Ids=[uint32[]]::new(32)
$r145Count=[R145CliConsole]::GetConsoleProcessList($r145Ids,32)
if($r145Count -ne 2 -or $r145Ids[0..($r145Count-1)] -notcontains $r145Target -or $r145Ids[0..($r145Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R145CliConsole]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R145CliConsole]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R145CaptureConsole {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r145Target=24664
$r145Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r145Target"
if($r145Process.Name -ne 'dumpcap.exe' -or $r145Process.CommandLine -notlike '*composition-wiring-r145*'){throw 'PID identity mismatch'}
[R145CaptureConsole]::FreeConsole() | Out-Null
if(-not [R145CaptureConsole]::AttachConsole($r145Target)){throw 'AttachConsole failed'}
try {
if(-not [R145CaptureConsole]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r145Ids=[uint32[]]::new(32)
$r145Count=[R145CaptureConsole]::GetConsoleProcessList($r145Ids,32)
if($r145Count -ne 2 -or $r145Ids[0..($r145Count-1)] -notcontains $r145Target -or $r145Ids[0..($r145Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R145CaptureConsole]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R145CaptureConsole]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'
```

```powershell
$p=Get-CimInstance Win32_Process -Filter 'ProcessId=26272'; if($p.Name -ne 'vlc.exe' -or $p.CommandLine -notlike '*composition-wiring-r145*'){throw 'VLC identity mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length)}finally{$c.Dispose()}
```

```powershell
& 'D:/Wireshark/tshark.exe' -n -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-wire.pcapng' -Y 'udp.dstport == 62020' -T fields -e udp.payload > 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-output-payloads.txt' 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-tshark.log'; exit $LASTEXITCODE
```

```powershell
$ErrorActionPreference='Stop'; $count=0L; $anomalies=0L; $footer=0; $last=@{}; $stream=[IO.File]::Create('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-wire.ts'); try { foreach($line in [IO.File]::ReadLines('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-output-payloads.txt')) { if($line -eq 'No matching RTP/H.264 payload dumped.') {$footer++; continue}; if(!$line){continue}; $b=[Convert]::FromHexString($line.Replace(':','')); if($b.Length -lt 12 -or ($b[0]-shr 6)-ne 2){throw 'Invalid RTP'}; $seq=[int]$b[2]*256+$b[3]; $ssrc=[Convert]::ToHexString($b[8..11]); $off=12+4*($b[0]-band 15); if($b[0]-band 16){$off+=4+4*([int]$b[$off+2]*256+$b[$off+3])}; $end=$b.Length; if($b[0]-band 32){$end-=$b[-1]}; if($off -gt $end -or ($end-$off)%188 -ne 0){throw 'Invalid TS payload'}; if($last.ContainsKey($ssrc) -and $seq -ne (($last[$ssrc]+1)%65536)){$anomalies++}; $last[$ssrc]=$seq; $stream.Write($b,$off,$end-$off); $count++ } } finally {$stream.Dispose()}; [pscustomobject]@{Packets=$count;SequenceAnomalies=$anomalies;TextFooters=$footer;SSRC=($last.Keys -join ',');Bytes=(Get-Item 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-wire.ts').Length} | ConvertTo-Json
```

```powershell
$ErrorActionPreference='Stop'; $last=@{}; $counts=@{}; $regressions=@{}; & 'D:/mabs/local64/bin-video/ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts -of csv=p=0 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-wire.ts' | ForEach-Object { $f=$_.Split(','); if($f.Length -ge 3 -and $f[0] -match '^\d+$' -and $f[2] -match '^-?\d+$'){$key=$f[0]; $dts=[long]$f[2]; if(!$counts.ContainsKey($key)){$counts[$key]=0; $regressions[$key]=0}; if($last.ContainsKey($key) -and $dts -le $last[$key]){$regressions[$key]++}; $last[$key]=$dts; $counts[$key]++}}; if($LASTEXITCODE -ne 0){throw 'ffprobe failed'}; [pscustomobject]@{Packets=$counts;NonIncreasingDts=$regressions} | ConvertTo-Json -Compress
```

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v warning -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-wire.ts' -map 0:v:0 -map 0:a:0 -f null - 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-wire-decode.log'; exit $LASTEXITCODE
```

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v warning -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-wire.ts' -map 0:v:0 -an -vf "select='eq(n,900)+eq(n,3750)+eq(n,4800)+eq(n,7980)'" -fps_mode vfr -frames:v 4 -start_number 0 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-frame-%d.png' 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-frame-extract.log'; exit $LASTEXITCODE
```

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-wire.ts' -map 0:a:0 -af silencedetect=noise=-80dB:d=0.5 -f null - *> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-wire-silence.log'; exit $LASTEXITCODE
```

关键原始记录：
```text
composition-wiring-r145-source0-reentry.log: frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
composition-wiring-r145-source0.log: frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.50    
composition-wiring-r145-source1-reentry.log: frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
composition-wiring-r145-source1.log: frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
composition-wiring-r145-source2-reentry.log: frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
composition-wiring-r145-source2.log: frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
[2026-10-10 10:43:28.574] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=188 peak_pending_bytes=107348 pressure_failures=0 access_units=19794 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=5410 maximum_mux_input_after_emit_ns=11392266 maximum_mux_input_after_dispatch_ns=-88607734 worst_mux_input_ready_ns=236724052200 worst_mux_input_emit_ns=236712659934 worst_mux_input_dispatch_ns=236812659934 maximum_protocol_batch_produced_after_release_ns=11467932 worst_protocol_batch_produced_ns=236724132500 worst_protocol_batch_release_ns=236712664568 worst_protocol_batch_deadline_ns=236812659934 worst_protocol_batch_datagrams=19 exit_reason=completed
[2026-10-10 10:43:28.576] [info] [graph][runtime.node] audio_stage_observation node=6 stage=aggregate_built count=11669 last_generation=1 last_begin=11948032 last_end=11949056 last_rate=44100 last_steady_ns=7273466336000 maximum_inter_event_gap_ns=31715100 gap_begin_generation=1 gap_begin_begin=10218496 gap_begin_end=10219520 gap_begin_rate=44100 gap_begin_steady_ns=7234247694600 gap_end_generation=1 gap_end_begin=10219520 gap_end_end=10220544 gap_end_rate=44100 gap_end_steady_ns=7234279409700
[2026-10-10 10:43:28.576] [info] [graph][runtime.node] audio_stage_observation node=6 stage=aggregate_committed count=11669 last_generation=1 last_begin=11948032 last_end=11949056 last_rate=44100 last_steady_ns=7273466367500 maximum_inter_event_gap_ns=31655000 gap_begin_generation=1 gap_begin_begin=10218496 gap_begin_end=10219520 gap_begin_rate=44100 gap_begin_steady_ns=7234247772300 gap_end_generation=1 gap_end_begin=10219520 gap_end_end=10220544 gap_end_rate=44100 gap_end_steady_ns=7234279427300
[2026-10-10 10:43:28.576] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=84796 maximum_ns=19547100 begin_steady_ns=7184235764000 end_steady_ns=7184255311100 video_cursor=5451 audio_cursor=8013824 next_video_cursor=5451 next_audio_cursor=8013824 error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-10 10:43:28.576] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=84796 maximum_ns=178800 begin_steady_ns=7264835905600 end_steady_ns=7264836084400 video_cursor=7869 audio_cursor=11568128 next_video_cursor=7869 next_audio_cursor=11568128 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:43:28.576] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=84796 maximum_ns=172400 begin_steady_ns=7272535852700 end_steady_ns=7272536025100 video_cursor=8101 audio_cursor=11907072 next_video_cursor=8101 next_audio_cursor=11907072 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:43:28.576] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=84796 maximum_ns=790700 begin_steady_ns=7247305864300 end_steady_ns=7247306655000 video_cursor=7344 audio_cursor=10795008 next_video_cursor=7344 next_audio_cursor=10795008 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:43:28.577] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=18 maximum_ns=58600 begin_steady_ns=7002535376300 end_steady_ns=7002535434900 video_cursor=0 audio_cursor=0 next_video_cursor=0 next_audio_cursor=0 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:43:28.577] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=8130 maximum_ns=19507900 begin_steady_ns=7184235778300 end_steady_ns=7184255286200 video_cursor=5451 audio_cursor=8013824 next_video_cursor=5451 next_audio_cursor=8013824 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:43:28.577] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_audio count=11669 maximum_ns=1066800 begin_steady_ns=7042427998900 end_steady_ns=7042429065700 video_cursor=1197 audio_cursor=1759232 next_video_cursor=1197 next_audio_cursor=1759232 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:43:28.577] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=8130 maximum_ns=19448300 begin_steady_ns=7184235817200 end_steady_ns=7184255265500 video_cursor=5451 audio_cursor=8013824 next_video_cursor=5451 next_audio_cursor=8013824 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:43:28.577] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=19799 maximum_ns=2874200 begin_steady_ns=7002535797200 end_steady_ns=7002538671400 video_cursor=0 audio_cursor=0 next_video_cursor=0 next_audio_cursor=0 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 10:43:28.577] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=84795 maximum_ns=106264000 begin_steady_ns=7002426021600 end_steady_ns=7002532285600 video_cursor=0 audio_cursor=0 next_video_cursor=0 next_audio_cursor=0 error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-10 10:43:28.577] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-wiring-r145.output output_generation=1 video_commits=8129 audio_commits=11669 real_samples=10186752 silence_samples=1762304 source[0].group=composition-wiring-r145.source.0 source[0].real_frames=6974 source[0].black_frames=1155 source[0].contribution_generation=0 source[1].group=composition-wiring-r145.source.1 source[1].real_frames=7021 source[1].black_frames=1108 source[1].contribution_generation=0 source[2].group=composition-wiring-r145.source.2 source[2].real_frames=6929 source[2].black_frames=1200 source[2].contribution_generation=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=821, threads=78, processThreads=126, workers=0, workerProgress=526363, workerProcessCalls=3068130, workerWaits=2541760, workerWakeups=2522308, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=846, logicalProcessors=22, averageSystemMachineCpuPercent=17.097467, averageProcessMachineCpuPercent=2.201052, peakProcessMachineCpuPercent=5.630631, averageProcessSingleCoreCpuPercent=48.423150, peakProcessSingleCoreCpuPercent=123.873874, initialWorkingSetBytes=218046464, workingSetBytes=241881088, peakWorkingSetBytes=264630272, totalPushed=546865, totalPopped=546864, droppedBuffers=0, encodedPacketsPushed=90845, encodedPacketsPopped=90844, backpressureItems=119, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17300851, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=619, graphPayloadReservations=205107, graphPayloadReleases=205107, graphPayloadPressureFailures=61660, lastInputReceivedAtNs=7254689548000
Capturing on 'Adapter for loopback traffic capture'
File: D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r145-wire.pcapng

Packets captured: 822589
Packets received/dropped on interface 'Adapter for loopback traffic capture': 822589/0 (pcap:0/dumpcap:0/flushed:0/ps_ifdrop:0) (100.0%)
[Parsed_silencedetect_0 @ 0000020989dd4840] silence_start: 112.561293
[Parsed_silencedetect_0 @ 0000020989dd4840] silence_end: 133.348435 | silence_duration: 20.787143
[Parsed_silencedetect_0 @ 0000020989dd4840] silence_start: 251.801429
[Parsed_silencedetect_0 @ 0000020989dd4840] silence_end: 270.883991 | silence_duration: 19.082562
```

本轮31件清理清单（记录后逐项清除）：
- composition-wiring-r145-capture-stdout.log (0B)
- composition-wiring-r145-capture.log (306B)
- composition-wiring-r145-cli-error.log (2030B)
- composition-wiring-r145-cli.log (1076977B)
- composition-wiring-r145-frame-0.png (614307B)
- composition-wiring-r145-frame-1.png (4348B)
- composition-wiring-r145-frame-2.png (660163B)
- composition-wiring-r145-frame-3.png (4348B)
- composition-wiring-r145-frame-extract.log (0B)
- composition-wiring-r145-metrics.csv (16778B)
- composition-wiring-r145-output-payloads.txt (409474209B)
- composition-wiring-r145-received.ts (201504792B)
- composition-wiring-r145-source0-reentry-stdout.log (540B)
- composition-wiring-r145-source0-reentry.log (29521B)
- composition-wiring-r145-source0-stdout.log (540B)
- composition-wiring-r145-source0.log (29521B)
- composition-wiring-r145-source1-reentry-stdout.log (335B)
- composition-wiring-r145-source1-reentry.log (28925B)
- composition-wiring-r145-source1-stdout.log (335B)
- composition-wiring-r145-source1.log (28926B)
- composition-wiring-r145-source2-reentry-stdout.log (335B)
- composition-wiring-r145-source2-reentry.log (28926B)
- composition-wiring-r145-source2-stdout.log (335B)
- composition-wiring-r145-source2.log (28925B)
- composition-wiring-r145-tshark.log (0B)
- composition-wiring-r145-vlc.log (28442B)
- composition-wiring-r145-wire-decode.log (0B)
- composition-wiring-r145-wire-silence.log (1255B)
- composition-wiring-r145-wire.pcapng (995643236B)
- composition-wiring-r145-wire.ts (202502320B)
- composition-wiring-r145.sdp (261B)

r145清理完成：31件清单全部删除，r145前缀无残留；原源124427809B与Release5792768B保留。宿主CIM无本轮媒体/分析进程。
