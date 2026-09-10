# Windows 第19轮：播放器正常，FileMux 排空超时

结论：FAIL。与第18轮相同二进制和媒体参数，只将截图导出移至源与CLI结束后。输入连续120秒H.264 RTP 1280×720、30fps、8,013,434bps；输出HEVC/H.264 MPEG-TS/RTP 1920×1080、25fps、CBR6Mbps、GOP50。SHA256：BD81B15EC1438202381825C94BFD8292E52A9D01161AA5206696E48A3B924E65。

## 实际命令

```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-19 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-19.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-19-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-19-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-19.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-19-vlc.log --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-19-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-19-vlc-add.log --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-19-add- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61622
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-19-vlc-third.log --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-19-third- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61624
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-19-vlc-readd.log --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-19-readd- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61626
```

标准输入依Running/Retired衔接：
```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-19-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-19-third.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
remove 1
remove 2
remove 3
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61626 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-19-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

媒体结束后，对62720/22/24/26发送RC `snapshot\r\n`，实际查看四张保留画面，然后正常关闭VLC。

## 结果

- PID：CLI13100、FFmpeg24480、dumpcap9724；VLC38488/27372/13660/29396。
- 第二路19:39:10.895复用group1，12.654 Running；第三路20.565新group2，20.869 Running；第四路50.370新group3，50.679 Running。零输出后可恢复。
- remove1在19:39:25.797进入Draining，其他协议worker于25.857—25.925完成，sender16914数据报完成且backlog0；FileMux节点13未报告Finished，31.369被排空无进展超时停止，31.952带Drain错误Retired。
- remove2同构FileMux节点21未完成，19:39:43.256被超时停止，44.431带Drain错误Retired；第三路对应FileMux36正常完成，46.489正常Retired。因此不是异步Retiring驱动回收卡住，需修FileMux终态/唤醒合同，不能扩大超时。
- FFmpeg3600帧/120秒退出0；CLI源结束后无进展退出1，最终workerErrors/errors0不代表输出级Drain失败不存在。
- 捕获183853包、丢包0；RTP16906/13860/12453/33901包，零序号丢失；TS TEI/AFC/连续性及expert error无结果。
- 出口77158数据报、97040720个IP字节；聚合50Mbps服务曲线超额1356B，最大包1356B。
- 四路均D3D11VA，四张实际画面正常；无too-late-to-display/deadlock/corrupt告警。截图移到媒体结束后，本轮未再出现第18轮的晚帧，但不因此把第18轮回写为通过。
- CPU428样本，单核等效均值21.250158%、峰值75.342466%；工作集119590912→205512704、峰值237940736字节。最终payload字节/对象0，42582次申请与释放相等。视频链路无A/V漂移指标。
- 整理后删除D盘本轮日志、pcap、SDP与PNG，核查精确PID无残留；本轮不创建成功验收提交。
