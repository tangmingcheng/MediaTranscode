# Windows 动态视频第 13 轮：基础动态链路通过

链路：H.264 RTP 1280×720、30fps、8,013,434bps 连续输入 → HEVC/H.264 MPEG-TS/RTP 1920×1080、25fps、CBR 6Mbps、GOP50。范围为不同编码输出的动态增删、零输出恢复与共享出口整形；不代表编码组复用、RKMPP 或最终交付通过。

Release CLI SHA256：`CC84CD2087773256A3AD52F87F14DC2DFDF8705C54577AF8A03DBD6DC60625A7`。

## 实际命令
```text
D:\Wireshark\dumpcap.exe -i 10 -q -f "udp portrange 60620-60621 or udp portrange 61620-61625" -w D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-13.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-13-vlc.log rtp://@127.0.0.1:61620
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-win-13 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-13.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
```

CLI/source分别重定向到D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-13-cli.log和dynamic-win-13-source.log。新增接收与控制：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-13-vlc-add.log rtp://@127.0.0.1:61622
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-13-add.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

后续 CLI 控制依次执行：

    remove 1
    remove 2
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-13-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

第三路接收：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-13-vlc-readd.log rtp://@127.0.0.1:61624


抓包分析：

    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-13.pcapng -d udp.port==61620,rtp -d udp.port==61622,rtp -d udp.port==61624,rtp -q -z rtp,streams

附加抓包检查：

    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-13.pcapng -d udp.port==61620,rtp -d udp.port==61622,rtp -d udp.port==61624,rtp -q -z expert,error
    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-13.pcapng -d udp.port==61620,rtp -d udp.port==61622,rtp -d udp.port==61624,rtp -Y "(udp.dstport == 61620 || udp.dstport == 61622 || udp.dstport == 61624) && (mp2t.analysis.drops || mp2t.cc.drop || mp2t.msg.fragment.error || mp2t.tei != 0 || mp2t.afc.invalid)" -T fields -e frame.number
    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-13.pcapng -d udp.port==61621,rtcp -d udp.port==61623,rtcp -d udp.port==61625,rtcp -Y "rtcp.pt == 203" -T fields -e frame.time_relative -e udp.dstport -e rtcp.pt
    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-13.pcapng -Y "udp.dstport >= 61620 && udp.dstport <= 61625" -T fields -e frame.time_relative -e ip.len -e udp.dstport

## 结果

- 固定连续源自然完成 3600 帧、120 秒，FFmpeg 退出 0；未循环、未降低源规格。初始 HEVC、新增 H.264、零输出后重新新增 HEVC，均按上述输出规格运行。
- output1 Running 16:40:53.642；output2 Preparing 16:41:03.790、Running 04.423。remove1：16:41:28.373 Draining、29.598 正常 Retired；remove2：16:41:31.121 Draining、31.461 正常 Retired。output3：16:41:32.710 Preparing、33.353 Running，输入仍持续。
- VLC 第三路游戏画面已截图确认；三路均记录 NVIDIA D3D11VA 硬解。各有一次 15/10/15ms 的 debug late 提示，无持续晚帧、解码错误或 deadlock；CloseMainWindow 后 SetThumbNailClip 错误属于窗口关闭日志。
- 捕获 181948 包、drop 0；三路 RTP 分别 19217/13284/42713 包、丢失均 0。TS 连续性、TEI、AFC、fragment 与 expert error 均无命中。主动删除两路均捕获尾部 SR/SDES/BYE（200,202,203），相对时间 36.2479048/38.1963675 秒。
- 同一接口 RTP+RTCP 共 75253 datagrams、94,559,860 IP wire bytes，与共享整形累计计费完全一致。R=6,250,000B/s，服务曲线 max_i(S_after_i−R*t_i−min_j≤i(S_before_j−R*t_j)) 最大超额 1356B，等于最大单 datagram 1356B；共享 scope failed=0、最终 waitingMembers=0。修复前第12轮聚合超额3494B，本轮没有放宽边界。
- 三路 sender committed_datagrams=19226/13292/42735，最终 backlog 均0；deadline/pressure/partial/ambiguous/WouldBlock均0。第三路 maximumSubmitLateness=100486100ns 是相对预留释放点的迟到，不是不可变 deadline 违约；maximum wire residence=78158000ns，未超过100ms契约。
- 源结束后按现有 RTP 无会话 EOS 的策略，16:42:57.965 报 no-progress timeout，58.257 第三路退役，CLI 退出1；不是媒体自然EOF，也不能声称正常退出码0。输入活动期间没有该错误。
- 最终 payload currentBytes/currentObjects=0，reservations/releases 均38965，高水位15185501B/31对象；workerErrors/errors/drop/payloadPressure均0。CPU409样本平均单核23.921854%、峰值77.102804%；working set122220544→201007104B、峰值234528768B。动态输出数变化不作为固定吞吐基准；纯视频A/V漂移不适用。
- PID：CLI35764、源22056、dumpcap2884、VLC32040/24404/19516。源与CLI结束后停止抓包，播放器正常关闭；进程核查无残留。日志/抓包/截图/SDP只在D盘，提取本报告后删除。

工业依据：[Linux TBF](https://www.man7.org/linux/man-pages/man8/tc-tbf.8.html)共同根整形与FIFO子队列、[RFC2212](https://www.rfc-editor.org/rfc/rfc2212.html)服务曲线。本实现范围为同一输入控制器与时钟域内的输出；不宣称跨独立会话或其他进程的出口协调。

后续门禁：完整编码契约相同的生产编码组复用、固定池RKMPP共享拷贝与同规格真实验收、两名未参与实现的独立审查及质量评分尚未完成。