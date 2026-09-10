# Windows 第22轮：RTP输入动态MPEG-TS/RTP输出通过

结论：本轮完整门禁PASS。冻结生产源码41bb17a1，二进制SHA256：72BCDF64B3BB406F70FDCB0B78D9CB565750D3909C2782D642CEE378228065E5。连续120秒H.264 RTP 1280×720、30fps、8,013,434bps → HEVC/H.264 MPEG-TS/RTP 1920×1080、25fps、CBR6Mbps、GOP50。

## 实际命令
```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-22 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-22.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-22-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-22-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-22.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-22-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-22-vlc.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-22-add- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61622 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-22-vlc-add.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-22-third- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61624 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-22-vlc-third.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-22-readd- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61626 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-22-vlc-readd.log' -Encoding UTF8
```

标准输入依Running/Retired衔接：
```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-22-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-22-third.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
remove 1
remove 2
remove 3
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61626 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-22-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

媒体及CLI退出后，对62720/22/24/26发送RC `snapshot\r\n`；四张PNG均实际查看，随后正常关闭播放器。

## 结果
- PID：CLI13020、源6776、dumpcap25732；VLC28260/7880/1692/23168。源3600帧/120秒自然退出0；CLI在源结束后09:09:20.712按裸RTP无EOS既有无进展策略退出1，不冒充媒体自然EOF成功退出。
- 第二路09:07:43.680复用group1，44.801 Running；第三路09:08:03.201新group2，03.500 Running。第一/二/三路09.516、16.378、18.381无错误Retired；第四路32.561新group3，32.851 Running。主动删除、共享消费者继续发送、零输出后恢复均通过。
- 四路D3D11VA和实际游戏画面正常，完整日志无too-late-to-display、deadlock、corrupt；仅初始12ms、第三/四路14ms debug late。PNG导出候选转换失败后成功导出，RC短连接及关闭窗口错误单列，不当作生产解码错误。
- console日志的UTC是父进程观察时间，消息仍成批可见，不能用于精确晚帧定位。没有以未刷新的日志提前判通过。
- 完整抓包181855包/drop0；输入106671 RTP包零丢失，3600个marker对应时间戳步长全部3000/90000秒，最大marker到达间隔62.1815ms。四路RTP28149/16500/7109/23361包零丢失，TS TEI/AFC/连续性及expert error无命中。
- 三次主动删除分别在抓包53.0342556/59.2977499/61.3202306秒捕获SR/SDES/BYE。
- 聚合75160数据报、94577128 IP字节，与共享scope账目一致；50Mbps服务曲线超额1356B、最大包1356B。各sender deadline/pressure/partial/ambiguous/WouldBlock0，backlog最终0。
- CPU438样本：单核等效平均16.644774%、峰值75.305623%；工作集121356288→204558336B、峰值241848320B。最终payload0字节/对象，37712次申请释放相等，workerErrors/errors/pressure/drop0。stalledIntervals3与最后无进展终止同时保留，不抹掉源结束语义；纯视频A/V漂移不适用。

## 抓包时钟检查

使用tshark分别解码四个输出端口为RTP，提取 `frame.time_relative, mp2t.af.pcr, mpeg-pes.pts, mpeg-pes.dts`。PCR字段按Wireshark的base×300+extension换算27MHz；PTS是已换算秒。下表是解析到的样本，不冒充所有编码帧的完整计数。

| 输出 | PCR样本 | PTS样本 | PCR步进 | PTS步进 | PTS到包偏移跨度 |
|---|---:|---:|---|---|---:|
| 初始HEVC | 656 | 1308 | 80ms | 40ms | 47.6713ms |
| 复用HEVC | 384 | 765 | 80ms | 40ms | 52.2788ms |
| 新H.264 | 176 | 348 | 80ms | 40ms | 25.2283ms |
| 恢复HEVC | 600 | 1070 | 80ms | 40ms | 31.1893ms |

所有解析序列严格前进；两路共享编码有608个重合PTS，到包差0.593—8.3622ms。恢复路媒体期PCR到包偏移跨度5.1393ms；包含源结束后仅PCR维护尾部则101.9855ms，两者不能混同为媒体漂移。此抓包没有证明VLC内部调度，也不解释此前失败轮次的间歇晚帧。

依据：[RFC2250](https://datatracker.ietf.org/doc/html/rfc2250)、[Wireshark PCR解析](https://raw.githubusercontent.com/wireshark/wireshark/master/epan/dissectors/packet-mp2t.c)。

本轮原始pcap、日志、SDP与PNG只放D盘，提取本报告后全部删除并核查精确PID无残留。长GOP、RKMPP及最终PR复核继续执行；本轮单独提交，不与其他成功测试合并。
