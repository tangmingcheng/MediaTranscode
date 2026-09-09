# Windows 第16轮：编码组复用与动态输出验收通过

链路：H.264 RTP 1280×720、30fps、8,013,434bps 连续输入 → HEVC/H.264 MPEG-TS/RTP 1920×1080、25fps、CBR 6Mbps、GOP50。范围覆盖同编码组复用、不同编码组创建、删除共享组的一个消费者、删除至零及重新添加；不代表RKMPP或最终代码双审通过。

Release CLI SHA256：C848499793E2236298E1FA62CD23FA597B48938F1BFDB6BD9225EC88B76C75E9。

## 实际命令

```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-16 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16-vlc.log --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-16-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16-vlc-add.log --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-16-add- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61622
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16-vlc-third.log --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-16-third- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61624
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16-vlc-readd.log --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-16-readd- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61626
```

CLI依实际Running/Retired状态衔接，删除第一路后观察第二路继续运行：

```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16-third.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
remove 1
remove 2
remove 3
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61626 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

对本机RC端口62720/62724/62722/62726分别发送snapshot（以下命令中的端口逐次替换）；播放器自行从实际vout生成PNG到上述D盘目录，没有新增测试脚本文件：

```powershell
$snapshotClient=[Net.Sockets.TcpClient]::new(); try { $snapshotClient.Connect('127.0.0.1',62720); $snapshotStream=$snapshotClient.GetStream(); $snapshotCommand=[Text.Encoding]::ASCII.GetBytes("snapshot`r`n"); $snapshotStream.Write($snapshotCommand,0,$snapshotCommand.Length); $snapshotStream.Flush() } finally { $snapshotClient.Dispose() }
```

抓包分析：

```text
D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16.pcapng -d udp.port==61620,rtp -d udp.port==61622,rtp -d udp.port==61624,rtp -d udp.port==61626,rtp -q -z rtp,streams
D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16.pcapng -d udp.port==61620,rtp -d udp.port==61622,rtp -d udp.port==61624,rtp -d udp.port==61626,rtp -q -z expert,error
D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16.pcapng -d udp.port==61620,rtp -d udp.port==61622,rtp -d udp.port==61624,rtp -d udp.port==61626,rtp -Y "(udp.dstport >= 61620 && udp.dstport <= 61626) && (mp2t.analysis.drops || mp2t.cc.drop || mp2t.msg.fragment.error || mp2t.tei != 0 || mp2t.afc.invalid)" -T fields -e frame.number
D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16.pcapng -d udp.port==61621,rtcp -d udp.port==61623,rtcp -d udp.port==61625,rtcp -d udp.port==61627,rtcp -Y "rtcp.pt == 203" -T fields -e frame.time_relative -e udp.dstport -e rtcp.pt
D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-16.pcapng -Y "udp.dstport >= 61620 && udp.dstport <= 61627" -T fields -e frame.time_relative -e ip.len
```

## 结果

- 固定连续源自然完成3600帧/120秒，FFmpeg退出0；没有循环、降规格或关闭硬件。第二路18:07:34.526 action=reused/group1/encodingSegment1/protocolSegment3，36.283 Running；第三路38.369新group2，38.674 Running。全程生产encoder首包只有初始、第三路和第四路三次，第二路没有新增encoder。
- 第一输出18:07:56.510正常Retired；共享第二路仍持续发送，18:08:05.933截图确认新画面，06.806才退役。第三路08.292正常Retired；第四路11.555新group3/segment6，11.863 Running。所有主动删除无失败，零输出后源仍持续并可恢复。
- 四路实际VLC PNG均由root查看确认游戏画面，包含共享消费者删除后的第二路和零输出后的第四路。四路始终D3D11VA解码，无too-late-to-display或deadlock；初始11/3ms、其他9/10/10ms仅debug late。
- 截图转换候选第一次报Failed to create video converter后选择d3d11_filters→swscale，日志明确snapshot taken且PNG已查看；这是VLC截图导出，不是生产解码回退。RC短连接关闭产生read/write error，窗口关闭SetThumbNailClip单列，不将其隐藏为零日志错误。
- 完整capture193909包/drop0；四路RTP19797/16505/14476/36393包零丢失，TS连续性/TEI/AFC/fragment与expert error均无命中。三次主动删除捕获SR/SDES/BYE，时间37.0322221/46.9290371/48.4760477秒。
- 聚合87214 datagrams、109722460 IP wire bytes，与shared scope计费完全一致；R=6250000B/s，max_i(S_after_i−R*t_i−min_j≤i(S_before_j−R*t_j))=1356B，最大datagram1356B。四sender提交19808/16513/14483/36410，最终backlog0，deadline/pressure/partial/ambiguous/WouldBlock均0。
- 首路backlog_max_residence=103333800ns从materializedAt开始，包含canonicalRelease前等待；硬期限由MediaDatagramWireDeadlinePlan规划为max(canonicalRelease,materializedAt)+100ms，MediaDatagramPacingController::markSubmitted在实际发送返回后核对notAfter（还扣serviceDuration）。首路19808次均通过完成校验，不把两种起点不同的指标误判为超限；未放宽100ms参数。
- 源结束后18:09:24.404按既有无会话EOS的RTP无进展超时策略退出，CLI退出1；不是媒体自然EOF。最终payload bytes/objects=0，reservations/releases=43819，高水位12594828B/13对象，workerErrors/errors/drop/pressure均0。CPU425样本平均单核21.574138%、峰值67.519182%；WS122388480→206077952B，峰值243359744B。VideoOnly A/V漂移不适用。
- 源、CLI结束后停止抓包，VLC24680/30404/12652/32824正常关闭，相关进程核查无残留。CLI/source/capture执行会话分别69894/95169/37706（工具会话号，不冒充OS PID）。所有原始日志、抓包、PNG、SDP均仅写D盘，提取本报告后删除。

工业依据：GStreamer动态tee独立队列/移除生命周期；RFC6184/RFC7798 IDR加入条件；Linux TBF共同scope整形。生产实现复用原NAL扫描器，以完整AU的VCL NAL证据开放加入，不单凭AV_PKT_FLAG_KEY；协议Ready要求首IDR最后datagram实际提交。后续RKMPP固定帧池适配、整体双审、质量评分与PR尚未完成。
