# Windows 第17轮：长 GOP 动态加入失败诊断

结论：FAIL，仅为补充诊断。使用第16轮相同二进制，唯一变化为 GOP750。输入连续120秒 H.264 RTP 1280×720、30fps、8,013,434bps；输出 HEVC MPEG-TS/RTP 1920×1080、25fps、CBR 6Mbps。

## 实际命令

```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-17 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-17.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 750 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-17-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-17-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61623' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-17.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-17-vlc.log rtp://@127.0.0.1:61620
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-17-vlc-add.log rtp://@127.0.0.1:61622
```

CLI 标准输入：
```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-17-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 750
```

## 结果

- PID：CLI 36772、FFmpeg 36896、dumpcap 11124、VLC 34876/7804。
- 19:07:16.011 输出2复用 group1/encoding segment1 等待随机访问；19:07:26.011 调度节点报 `VideoOnly scheduler exceeded the planner startup deadline`；26.209 failed→retired，新增输出发送0数据报。原输出继续。
- GOP750/25 对应30秒媒体周期。超时拒绝正确，缺陷在 planner 固定10秒等待合同。
- FFmpeg 完成3600帧/120秒，退出0。源结束后 CLI 因 RTP 无媒体 EOS 的无进展超时退出1。
- 抓包170538包、捕获丢包0；原输出63813个 RTP 包、序号丢包0；新增输出无 RTP。VLC 原输出使用 D3D11VA；未取得画面截图，不宣称完整视觉验收。
- CPU 414个样本，单核等效均值21.743038%、峰值60.273973%；工作集122372096→197451776字节，峰值197451776；最终 payload 字节/对象为0，37972次申请与释放相等。仅视频，无 A/V 漂移指标。
- 原始日志、抓包及 SDP 保存在 D 盘；报告整理后删除并检查本轮进程残留。
