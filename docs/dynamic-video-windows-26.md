# Windows26：RTP H264 720p30 → MPEGTS/RTP HEVC/H264 1080p25 CBR6Mbps（补充诊断）

完整120秒固定源，输入8,013,434bps，输出GOP50。冻结98742fd6，CLI SHA256 EABCDBD7370A166A7A92DE06038CDC653ED3082A909895CB2C534C7FCE1B0053。只增加VLC RC统计唤醒观测，未改变媒体参数、缓存、硬解或窗口布局。本轮不替代正式验收，不关闭Windows25的12帧晚帧问题，不作为进入RKMPP实流的依据。
## 实际命令
```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-26 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-26.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-26-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-26-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61627 or tcp portrange 62720-62726' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-26.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-26-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-26-vlc.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-26-add- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61622 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-26-vlc-add.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-26-third- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61624 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-26-vlc-third.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-26-readd- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61626 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-26-vlc-readd.log' -Encoding UTF8
```

标准输入按Running/Retired衔接：
```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-26-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-26-third.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
remove 1
remove 2
remove 3
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61626 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-26-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

## 观测及进程

CLI PID11484，FFmpeg15528，dumpcap28820，VLC27736/10104/8064/4412，RC观测PowerShell27524。源3600帧/120秒自然退出0；CLI按源结束后的既有NoProgress策略退出1。监测随CLI结束退出0，抓包Ctrl+C停止，256786包、内核丢包0。源与CLI结束后通过RC snapshot取得四张1920×1080游戏画面并查看，再正常关闭VLC。已检查以上PID无残留。

RC通过四个持久TcpClient连接127.0.0.1:62720/22/24/26；源出现后、CLI存活期间逐路发送ASCII `atrack -1\r\nstats\r\n`，Poll(50000微秒)、8192字节接收缓冲，记录发送/接收UTC、端口和原始响应为D盘JSON行，每轮Sleep(300ms)，finally释放连接及StreamWriter。使用内联PowerShell，未编写本机测试脚本。原始完整内联命令未持久保存，本节记录执行行为，不能声称具备逐字命令溯源。

仅适用于本次纯视频、每进程一个活动视频输出的诊断。atrack同值设置仍触发输入控制队列，使原本阻塞的输入线程刷新统计；stats同批读取可能仍是旧值，不能作为控制已完成的确认。它可能取得ES/资源锁并清理闲置vout，不能宣称普遍无副作用。源码依据：[VLC输入循环](https://raw.githubusercontent.com/videolan/vlc/3.0.23/src/input/input.c)、[RC命令与统计](https://raw.githubusercontent.com/videolan/vlc/3.0.23/modules/control/oldrc.c)。日志UTC是父进程读取时间，不能代替事件时间。没有使用WPR，也未触发UAC。

## 结果

- 初始Running10:23:11.382；第二路复用group1，28.133开始、29.604运行；第三路group2，37.545开始、37.854运行；1/2/3路44.860/51.883/54.204正常退役；第四路group3于57.944开始、58.242运行，10:25:15.782按源结束NoProgress退役。
- CPU428样本，单核等效平均18.961238%、峰70.098039%；工作集119017472→205078528B，峰245579776B。最终workers/errors/workerErrors/drop/pressure均0，stalledIntervals1；payload0字节/0对象，40366次申请和释放相等。纯视频A/V漂移不适用。
- 四路RTP18002/11840/7770/39028包，最终丢失0；按抓包到达顺序逐SSRC检查相邻序号模65536，全部严格+1，无间隙、重复或倒序。TS TEI/AFC/连续性及expert error无命中。
- scope76677数据报/96544844 IP字节，与抓包完全相符；50Mbps服务曲线超额1356B等于最大包。sender全部deadline/pressure/partial/ambiguous失败0，最终backlog0。停止时最大提交迟延100.514ms不能与deadline失败混为一谈。
- 四路均D3D11VA。完整关闭后日志无实际too-late丢帧；截图转换候选失败后PNG成功、关闭期RC/缩略图报错不属于媒体丢帧。
- 每端口399次RC响应，拼接后398个完整lost字段，均为0；显示计数最大808/519/368/1780，确实持续刷新。video decoded不能直接当作唯一呈现帧数；计数与输入AU间的尾部差额尚无完整归因，不声称逐帧完整显示。

| 输出端口 | PCR/PTS样本数 | PCR/PTS步进 | PCR到包偏移跨度 | PTS到包偏移跨度 |
|---|---:|---|---:|---:|
|61620|415/827|80/40ms|117.9424ms|147.1525ms|
|61622|271/538|80/40ms|113.8915ms|144.1871ms|
|61624|194/384|80/40ms|34.7137ms|44.0436ms|
|61626|964/1799|80/40ms|103.4647ms|31.8008ms|

PCR/PTS严格前进。前两路到包偏移跨度大于Windows25却没有晚帧，不能据此推断唤醒修复了调度；仍需区分观测扰动与播放器偶发调度。分析曾使用错误pes.pts字段及反向RC正则，失败结果已弃用；以tshark字段表确认mpeg-pes.pts、PCR十六进制UInt64/27MHz并完整重跑成功。tshark附加插件提示不计作数据记录。

## 清理与未完成项

本轮原始数据仅存D盘；独立审查完成提取后，16份原始日志、抓包、截图和SDP已删除，匹配残留0，进程残留0。保留Windows25 FAIL；当前待查的是播放器调度及统计唤醒影响，尚不能确认生产缺陷或排除生产影响。RKMPP新依赖和CLI已完成全量构建，但仍须等待Windows正式门禁后运行真实媒体链路。
