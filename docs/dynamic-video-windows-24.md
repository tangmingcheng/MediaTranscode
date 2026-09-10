# Windows 第24轮：原事务期限不足时提前拒绝通过

结论：负向合同完整PASS，新增输出被预期拒绝，原流完整通过。生产源码41bb17a1；二进制SHA256：72BCDF64B3BB406F70FDCB0B78D9CB565750D3909C2782D642CEE378228065E5。连续120秒H.264 RTP 1280×720、30fps、8,013,434bps → HEVC MPEG-TS/RTP 1920×1080、25fps、CBR6Mbps、GOP750。

## 实际命令
```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-24 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-24.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 750 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-24-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-24-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-24-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-24-vlc.log' -Encoding UTF8
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61623' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-24.pcapng'
```
原输出Running后发送：
```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-24-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 750
```
源及CLI结束后向62720发送RC `snapshot\r\n`，查看PNG再关闭VLC。

## 结果
- PID：CLI14244、FFmpeg19464、dumpcap1948、VLC18628。
- 原输出09:25:36.962 Running；新增53.168 Preparing，53.457 Failed并Retired，阶段Preparation，错误为“Dynamic video join IDR interval and activation lead do not fit the remaining first-output transaction budget”。未发布encoding group、未WaitingForRandomAccess，拒绝端口61622/23抓包0包。30秒自然IDR间隔加激活开销无法装入原30秒期限，未放宽或重置期限。
- 原流持续3600帧/120秒，FFmpeg退出0；CLI源结束后09:27:40.923按裸RTP无进展策略退出1。VLC D3D11VA、实际后续游戏画面正常，无too-late/deadlock/corrupt，仅14ms debug late。
- 捕获170632包/drop0；原输出63906 RTP包零丢失，TS TEI/AFC/连续性和expert error无命中。63937数据报/80461152 IP字节与scope相等；50Mbps服务曲线超额1356B、最大包1356B。sender deadline/pressure/partial/ambiguous/WouldBlock0，最终backlog0。
- CPU439样本，单核等效均值15.170966%、峰值41.706161%；工作集121692160→201904128B、峰值相同。payload最终0字节/对象，38231次申请释放相等；workerErrors/errors/pressure/drop0，stalledIntervals1保留；纯视频A/V漂移不适用。
- PNG转换候选失败后仍成功导出，RC及窗口关闭错误不冒充媒体解码失败。原始日志、pcap、SDP、PNG只在D盘，整理后删除并核查精确PID退出。单独提交此项负向合同验收。
