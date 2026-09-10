# Windows 动态视频第15轮：播放与视觉门禁未通过

H.264 RTP 1280×720、30fps、8,013,434bps 连续输入 → HEVC/H.264 MPEG-TS/RTP 1920×1080、25fps、CBR 6Mbps、GOP50。CLI SHA256：C848499793E2236298E1FA62CD23FA597B48938F1BFDB6BD9225EC88B76C75E9。

## 实际命令

```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-15 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-15.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-15-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-15-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-15.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-15-vlc.log rtp://@127.0.0.1:61620
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-15-vlc-add.log rtp://@127.0.0.1:61622
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-15-vlc-third.log rtp://@127.0.0.1:61624
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-15-vlc-readd.log rtp://@127.0.0.1:61626
```

CLI 控制按实际 Running/Retired 状态衔接：

```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-15-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-15-third.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
remove 1
remove 2
remove 3
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61626 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-15-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

## 结果

- 固定源自然完成3600帧/120秒，FFmpeg退出0。第二路18:00:08.393 action=reused/group1/encodingSegment1/protocolSegment3，10.446 Running；第三路12.524 action=created/group2/encodingSegment4/protocolSegment5，12.831 Running。整个过程中第一组仅一次生产encoder首包；同编码请求没有新增encoder。
- 删除第一路18:00:18.973正常Retired后第二路继续发送至26.958；第二路27.503正常Retired，第三路28.712正常Retired。第四路31.966新group3/encodingSegment6/protocolSegment7，32.273 Running。删除至零后恢复时输入仍活跃。
- source结束后18:02:02.153按现有无进展超时策略失败，02.411第四路退役，CLI退出1；不是自然媒体EOF。
- 完整capture180445包/drop0，四路RTP11072/8982/7565/46092包零丢失，expert error无命中；三次主动删除尾部均有SR/SDES/BYE。合计73750 datagrams、92750032 IP wire bytes，与shared scope计费一致；50Mbps服务曲线超额1356B，最大datagram1356B，无放宽边界。sender所有deadline/pressure/partial/ambiguous/WouldBlock为0，最终backlog0。
- 四路VLC均D3D11VA；初始路出现两次44/45ms too-late-to-display warning，不能当作全部画面通过。其他late为初始12/4/5ms、第三/第四各11ms debug提示；窗口关闭SetThumbNailClip单列。当前证据不能确定两次晚帧属于生产时序还是Windows显示调度，不据此随意修改核心。
- 两次运行中与一次结束后的桌面捕获均CopyFromScreen无效句柄，未得到图片。控制链路与协议证据通过，但播放与视觉门禁不足，整轮FAIL，不创建成功提交；下一轮改用同一VLC进程自带snapshot读取实际解码画面，保持源/转码/接收硬解规格不变。
- 最终payload bytes/objects=0，reservations/releases=40662，高水位12553850B/13对象；workerErrors/errors/drop/pressure均0。CPU423样本平均单核21.327773%、峰值68.421053%；WS119570432→204644352B，峰值242085888B。VideoOnly A/V漂移不适用。
- PID：CLI34268、source19336、capture37720、VLC37164/25276/2508/34548。源和CLI结束后停止抓包，VLC正常关闭。原始材料只存D盘，报告提取后删除，并核查进程无残留。
