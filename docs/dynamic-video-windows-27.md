# Windows27：H264 RTP 720p30 8Mbps → HEVC/H264 MPEGTS/RTP 1080p25 CBR6Mbps（FAIL）

固定连续120秒源，GOP50，冻结生产98742fd6、文档HEAD3be217ec，CLI SHA256 EABCDBD7370A166A7A92DE06038CDC653ED3082A909895CB2C534C7FCE1B0053。预先限定一次撤除周期RC唤醒的对照；源及CLI退出前没有RC控制。恢复路实际丢1帧，因此正式门禁FAIL，不能进入RKMPP实流，不进行相同条件重复选绿。

## 实际命令

```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-27 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-27-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27-vlc.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-27-add- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61622 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27-vlc-add.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-27-third- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61624 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27-vlc-third.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-27-readd- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61626 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27-vlc-readd.log' -Encoding UTF8
```

动态标准输入依Running/Retired衔接：

```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27-third.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
remove 1
remove 2
remove 3
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61626 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

全部源和CLI结束、抓包停止后，才执行以下单次音轨唤醒、分离统计读取和截图命令：

```powershell
$ErrorActionPreference='Stop'; foreach($port in @(62720,62722,62724,62726)){ $client=[Net.Sockets.TcpClient]::new('127.0.0.1',$port); try { $stream=$client.GetStream(); $bytes=[Text.Encoding]::ASCII.GetBytes("atrack -1`r`n"); $stream.Write($bytes,0,$bytes.Length); Start-Sleep -Milliseconds 500; foreach($sample in 1,2){$bytes=[Text.Encoding]::ASCII.GetBytes("stats`r`n");$stream.Write($bytes,0,$bytes.Length);Start-Sleep -Milliseconds 500;$buffer=[byte[]]::new(16384);$response='';while($stream.DataAvailable){$count=$stream.Read($buffer,0,$buffer.Length);$response+=[Text.Encoding]::UTF8.GetString($buffer,0,$count)};[pscustomobject]@{port=$port;sample=$sample;utc=[DateTime]::UtcNow.ToString('o');response=$response}|ConvertTo-Json -Compress|Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27-final-stats.log' -Encoding UTF8}; $bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`r`n");$stream.Write($bytes,0,$bytes.Length);Start-Sleep -Milliseconds 500 } finally {$client.Dispose()} }; Get-Content -Encoding UTF8 -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-27-final-stats.log'
```

## 结果

- PID：CLI31412，源30248，dumpcap16780，VLC28304/28968/9732/11124。源3600帧/120秒自然退出0；CLI于11:46:07按源结束后的NoProgress退出1。抓包189569包/drop0；四张1920×1080游戏画面已查看，随后正常关闭四个VLC，全部上述PID无残留。
- 初始11:44:02.653运行；第二路11:44:57.029复用group1、58.747运行；第三路11:45:21.074创建group2；1/2/3路27.461/34.422/38.187正常退役；第四路41.377创建group3、41.672运行。首次状态查询ReadAllText遇文件共享限制，改用rg后完成全矩阵；该控制延迟如实保留，不降低源时长或规格。动态操作时刻与26不同，只能证明无周期RC仍晚帧，不能将跨轮差异归因于RC。
- CPU429样本，单核等效平均21.649618%、峰58.510638%；工作集117313536→205807616B，峰240406528B。最终workers/errors/workerErrors/drop/pressure0，stalledIntervals1，payload0字节/0对象，40316次申请释放相等。纯视频不适用A/V漂移。
- RTP45428/18775/7749/10881包，最终丢失0，按到达顺序相邻序号模65536全部+1。TS TEI/AFC/连续性及expert error无命中。scope82874数据报/104281176 IP字节与抓包完全相符，50Mbps服务曲线超额1356B=最大包；sender deadline/pressure/partial/ambiguous失败0，最终backlog0。
- 四路均D3D11VA。终态统计已从初始冻结值刷新，两次分离读取一致；displayed2092/851/377/476，lost0/0/0/1。完整日志与统计一致：恢复路实际too-late22ms一帧；初始11ms、第三路20ms为debug late，没有计入实际丢帧。
- 统计读取发生在源和CLI结束约20秒后，截图在统计之后，不能归咎于周期唤醒或截图。仍不能从父进程缓冲日志的UTC确定丢帧精确时刻，也不能将网络零丢失等同于播放器调度无误。

## 下一步及清理

用户已明确授权本次管理员WPR CPU/GPU采集及UAC。必要性：无周期观测仍复现真实丢帧，而现有日志和抓包不能区分VLC线程未获调度、驱动/GPU等待或上游时间安排。只进行本次限定120秒链路采集，临时记录和ETL均在D盘，分析后删除；不改变缓存、硬解或源规格。Windows27不是成功验收提交。

本轮日志、抓包、统计、SDP及截图均仅D盘；独立审查提取后已删除16份原始文件，匹配残留0、上述进程残留0；源和构建产物保留。
