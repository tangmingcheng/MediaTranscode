# Windows 动态视频第 12 轮诊断（未通过）

2026-09-09；第十七次全量 Release 构建 SHA-256：D565A4E853EB69A0F0DEE9130E533A916DB99EBF8ACF5E37EAD57AEDEE34BB73。

## 实际命令
```text
D:\Wireshark\dumpcap.exe -i 10 -q -f "udp portrange 60620-60621 or udp portrange 61620-61625" -w D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-12.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-12-vlc.log rtp://@127.0.0.1:61620
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-win-12 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-12.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
```

CLI/source分别重定向到D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-12-cli.log和dynamic-win-12-source.log。新增接收与控制：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-12-vlc-add.log rtp://@127.0.0.1:61622
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-12-add.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

后续 CLI 控制依次执行：

    remove 1
    remove 2
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-12-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

第三路接收：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-12-vlc-readd.log rtp://@127.0.0.1:61624


抓包分析：

    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-12.pcapng -d udp.port==61620,rtp -d udp.port==61622,rtp -d udp.port==61624,rtp -q -z rtp,streams

附加抓包检查：

    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-12.pcapng -d udp.port==61620,rtp -d udp.port==61622,rtp -d udp.port==61624,rtp -q -z expert,error
    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-12.pcapng -d udp.port==61620,rtp -d udp.port==61622,rtp -d udp.port==61624,rtp -Y "(udp.dstport == 61620 || udp.dstport == 61622 || udp.dstport == 61624) && (mp2t.analysis.drops || mp2t.cc.drop || mp2t.msg.fragment.error || mp2t.tei != 0 || mp2t.afc.invalid)" -T fields -e frame.number
    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-12.pcapng -d udp.port==61621,rtcp -d udp.port==61623,rtcp -d udp.port==61625,rtcp -Y "rtcp.pt == 203" -T fields -e frame.time_relative -e udp.dstport -e rtcp.pt
    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-12.pcapng -Y "udp.dstport >= 61620 && udp.dstport <= 61625" -T fields -e frame.time_relative -e ip.len -e udp.dstport

## 结果

- 固定 H.264 1280×720、30fps、8,013,434bps 连续源自然完成 3600 帧、120 秒，FFmpeg 退出 0。初始 HEVC、新增 H.264、重新新增 HEVC 均为 1920×1080、25fps、CBR 6Mbps、GOP50、MPEG-TS/RTP。
- output1 在 13:26:21.409 Running；output2 在 13:26:31.571 Preparing、32.189 Running。remove1：13:26:48.839 Draining、50.016 正常 Retired；remove2：13:27:50.185 Draining、50.495 正常 Retired。删除没有失败，尾部发送与资源释放正常。
- 零输出后 output3 在 13:28:08.748 Preparing、09.342 Running；61624 的 VLC 游戏画面已截图确认。三路 VLC 日志均确认 D3D11VA 硬解；分别只有 11/13/11ms 单次画面迟到提示。关闭播放器时 SetThumbNailClip 报错属于窗口清理日志，不作为媒体成功证据。
- 源结束后第三路按真实输入无进展退出，13:28:25.501 Failed、25.782 Retired，CLI 退出 1；该 RTP 源没有会话 EOS，不能写自然媒体 EOF。三路 sender 提交 15288/38663/6100 datagrams，backlog 最终均 0，网络 deadline/pressure/partial/ambiguous/WouldBlock 均 0。
- 捕获 166746 包、drop 0。RTP 三路分别 15281/38646/6096 包、丢失均 0；TS 连续性、TEI、AFC、fragment 和 expert error 均无命中。两路主动删除均捕获最后 SR/SDES/BYE（200,202,203），时间 28.9470998/89.5145548 秒。
- 关键失败：以接口 R=6,250,000B/s、IP wire bytes计算服务曲线，超额为 max_i(S_after_i−R*t_i−min_j≤i(S_before_j−R*t_j))。各输出（含自己的 RTCP）最大超额均 1356B；合计 60051 个 datagrams、75208692B，聚合最大超额 3494B（16.0260077秒），超过当前单报文突发界 1356B。独立 agent 复算一致。当前每 sender 独享完整接口速率，service_scope 只是字符串，无共同整形状态；不得以 OS 抖动或放宽突发界解释。因此整轮未通过，不能创建成功提交。
- 最终 payload currentBytes/currentObjects=0，reservations/releases 均 34236，高水位 15230384B、33 对象；workerErrors/drop/payloadPressure=0。CPU 429 个样本平均单核 18.289308%、峰值 50.113895%；working set 121810944→202317824B，峰值 236609536B。含变化的输出数，不作为固定吞吐基准；VideoOnly A/V 漂移不适用。
- PID：CLI 6468、源 14020、dumpcap 30184、VLC 16948/28684/27024。源和 CLI 结束后停止抓包，VLC 使用 CloseMainWindow 正常关闭并刷新硬解日志；原始日志、抓包、截图、SDP 仅存 D 盘，诊断提取后删除。

工业依据：[Linux TBF](https://www.man7.org/linux/man-pages/man8/tc-tbf.8.html) 的共同根整形器与子队列；[RFC2212](https://www.rfc-editor.org/rfc/rfc2212.html) 的服务曲线与链路排队边界。后续必须共享真实接口 scope 的整形权威，不平均分配带宽、不克隆接口峰值、不样例调参。