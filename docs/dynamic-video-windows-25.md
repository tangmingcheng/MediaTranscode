# Windows25：RTP H264 720p30 8Mbps → MPEGTS/RTP HEVC/H264 1080p25 CBR6Mbps（失败）

结论FAIL：动态状态、网络及资源释放正常，但VLC发生实际晚帧丢弃。不得据此形成成功验收提交或进入RKMPP实流。完整120秒固定源，GOP50；共享VideoDecode时间基及RK版本身份修复已双审源码PASS，全量构建退出0，CLI SHA256 EABCDBD7370A166A7A92DE06038CDC653ED3082A909895CB2C534C7FCE1B0053。

## 实际命令
```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-25 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-25.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-25-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-25-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-25.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-25-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-25-vlc.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-25-add- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61622 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-25-vlc-add.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-25-third- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61624 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-25-vlc-third.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-25-readd- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61626 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-25-vlc-readd.log' -Encoding UTF8
```

标准输入按Running/Retired衔接：
```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-25-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-25-third.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
remove 1
remove 2
remove 3
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61626 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-25-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

PID：CLI25228，源9636，dumpcap3228，VLC25220/4184/15044/2336。源3600帧/120秒自然退出0；CLI源结束后09:54:11按裸RTP既有NoProgress策略退出1。捕获停止后发送RC snapshot到62720/22/24/26，四张1920×1080游戏画面已查看，再正常关闭VLC；没有停止CLI或降低源规格。

## 结果

- 初始Running09:52:06.159；第二路21.985复用group1、22.278 Running；第三路30.195新group2、30.503 Running；1/2/3路36.634、43.637、45.937正常Retired；第四路49.646新group3、49.939 Running。源结束后第四路无进展原因保留。
- CPU430样本，单核等效平均18.202644%、峰57.345972%；工作集122826752→204267520B，峰240599040B。最终payload0字节/0对象，40039次申请释放相等；workers/errors/workerErrors/drop/pressure均0，stalledIntervals1；纯视频A/V漂移不适用。
- 抓包181857 packets/drop0。输出RTP16096/11155/7211/40660 packets，loss0；TS TEI/AFC/连续性、expert error均0。聚合75162数据报、94541808 IP字节，与scope一致；50Mbps服务曲线超额1356B=最大包。sender全部deadline/pressure/partial/ambiguous失败0、最终backlog0。
- 四路均D3D11VA硬解。初始VLC实际丢90/50ms两帧及369→49ms、每步40ms的9帧；第三路丢28ms一帧。第二/恢复路只有11/12ms debug late。未见deadlock、corrupt、discontinuity。
- VLC完整日志显示上述丢帧发生于截图命令之前。父进程UTC时间是缓冲输出被读取时间，不能作为精确发生时间。初始缓冲1040ms媒体/528ms墙钟、等待首帧490ms；VLC等待结束后会重设时钟原点，不能直接把初始化耗时认定根因。依据：[VLC丢帧判定](https://raw.githubusercontent.com/videolan/vlc/3.0.23/src/video_output/video_output.c)、[缓冲完成时钟重置](https://raw.githubusercontent.com/videolan/vlc/3.0.23/src/input/es_out.c)。

## 抓包时钟

| 端口 | PCR样本 | PTS样本 | PCR/PTS步进 | PTS到包偏移跨度 |
|---|---:|---:|---|---:|
|61620|377|751|80/40ms|58.7611ms|
|61622|258|512|80/40ms|55.9704ms|
|61624|183|362|80/40ms|52.8474ms|
|61626|1002|1875|80/40ms|32.8159ms|

所有解析序列严格前进。PCR到包偏移跨度分别56.3067/54.1499/48.0149/101.3045ms，恢复路包含源结束后的PCR维护尾部。这些网络证据不足以证明VLC内部调度正确，也不能直接解释369ms积压。下一步以不变媒体规格增加CPU/GPU ETW定位，禁止调大cache、关闭硬解或随机重跑代替定位。

原始日志、抓包、SDP和PNG仅D盘；本报告提取后删除并核查精确进程无残留。分析命令曾遇tshark附加提示污染行及十六进制PCR解析错误，纠正为只解析数字记录/PCR按UInt64十六进制换算27MHz后重新完整执行成功；失败分析结果未用于上表。
