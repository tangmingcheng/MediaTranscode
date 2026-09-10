# Windows 第21轮：初始VLC晚帧，统计缓存不可用

结论：FAIL。连续120秒H.264 RTP 1280×720、30fps、8,013,434bps → HEVC/H.264 MPEG-TS/RTP 1920×1080、25fps、CBR6Mbps、GOP50。二进制SHA256：9D8B050C439CDEB994CA9868DDA820779E854F8D5EC697DD4A652BE77C0D2E72。

## 实际命令
```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-21 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-21.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-21-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-21-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-21.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-21-vlc.log --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-21-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-21-vlc-add.log --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-21-add- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61622
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-21-vlc-third.log --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-21-third- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61624
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-21-vlc-readd.log --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-21-readd- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61626
```

标准输入依Running/Retired衔接：
```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-21-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-21-third.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
remove 1
remove 2
remove 3
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61626 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-21-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```
原生RC阶段采样：向62720/22/24/26发送`stats\r\n`，等待150ms并读取可用响应，附观察时间写D盘stats.log；媒体及CLI退出后发送`stats\r\nsnapshot\r\n`，查看四张PNG再关闭VLC。

## 结果
- PID：CLI7624、FFmpeg23544、dumpcap27936；VLC18996/27368/28272/13040。
- 第二路08:55:10.179复用group1、11.346 Running；第三路19.731新group2、20.036 Running。第一/二/三路31.062、41.821、43.825正常Retired；第四路48.175新group3、48.468 Running。
- 源3600帧/120秒退出0；源结束后08:56:52.932按裸RTP既有无进展策略退出1。所有主动删除无错误，最后payload0字节/对象，42163次申请释放相等，workerErrors/errors/pressure0。
- 四张画面实际查看正常，四路D3D11VA；初始VLC出现90/51ms too-late-to-display，故FAIL。其余仅11/17/9/8/2ms debug late。截图转换候选失败、关闭窗口SetThumbNailClip及VLC自身媒体库并发保存错误不冒充媒体解码错误；测试原始材料均在D盘。
- 捕获188745包/drop0；四路RTP22360/16044/11331/32272包、零丢失；TS TEI/AFC/连续性及expert error无命中。
- 聚合82050数据报、103274676 IP字节，与scope一致；50Mbps服务曲线超额1356B、最大包1356B。各sender deadline/pressure/partial/ambiguous/WouldBlock0，最后backlog0。
- CPU430样本，单核等效均值20.107502%、峰值71.627907%；工作集124329984→204582912B、峰值242688000B。纯视频A/V漂移不适用。
- RC统计长期停在启动时video decoded=2、displayed/lost=0，不能作为零丢帧证据。VLC3.0.23 RTP模块pf_demux=NULL，由独立线程接收；input MainLoop可在ControlPop(-1)等待控制，RC只读item缓存而不强制更新。Windows文件日志也有缓冲，实时文件未出现告警不能推断最终无告警。依据：[RTP](https://github.com/videolan/vlc/blob/3.0.23/modules/access/rtp/rtp.c)、[input MainLoop](https://github.com/videolan/vlc/blob/3.0.23/src/input/input.c)、[oldrc](https://github.com/videolan/vlc/blob/3.0.23/modules/control/oldrc.c)。
- 整理本报告后删除D盘本轮pcap、日志、stats、SDP、PNG；精确PID已核查退出。本轮不创建成功验收提交。
