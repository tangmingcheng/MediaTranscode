# Windows28：H264 RTP 720p30 8Mbps → HEVC/H264 MPEGTS/RTP 1080p25 CBR6Mbps（WPR诊断，FAIL）

固定连续120秒源，GOP50，生产冻结98742fd6，CLI SHA256 EABCDBD7370A166A7A92DE06038CDC653ED3082A909895CB2C534C7FCE1B0053。本次是用户明确授权的单次管理员WPR CPU/GPU诊断；播放期间无RC唤醒。第三路实际丢1帧，正式门禁FAIL，不能进入RKMPP实流。

## 实际媒体命令

```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-28 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-28-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-vlc.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-28-add- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61622 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-vlc-add.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-28-third- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61624 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-vlc-third.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-28-readd- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61626 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-vlc-readd.log' -Encoding UTF8
```

标准输入按Running/Retired衔接：

```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-third.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
remove 1
remove 2
remove 3
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61626 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

源及CLI结束后才执行：

```powershell
$ErrorActionPreference='Stop'; foreach($port in @(62720,62722,62724,62726)){ $client=[Net.Sockets.TcpClient]::new('127.0.0.1',$port); try { $stream=$client.GetStream(); $bytes=[Text.Encoding]::ASCII.GetBytes("atrack -1`r`n"); $stream.Write($bytes,0,$bytes.Length); Start-Sleep -Milliseconds 500; foreach($sample in 1,2){$bytes=[Text.Encoding]::ASCII.GetBytes("stats`r`n");$stream.Write($bytes,0,$bytes.Length);Start-Sleep -Milliseconds 500;$buffer=[byte[]]::new(16384);$response='';while($stream.DataAvailable){$count=$stream.Read($buffer,0,$buffer.Length);$response+=[Text.Encoding]::UTF8.GetString($buffer,0,$count)};[pscustomobject]@{port=$port;sample=$sample;utc=[DateTime]::UtcNow.ToString('o');response=$response}|ConvertTo-Json -Compress|Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-final-stats.log' -Encoding UTF8}; $bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`r`n");$stream.Write($bytes,0,$bytes.Length);Start-Sleep -Milliseconds 500 } finally {$client.Dispose()} }; Get-Content -Encoding UTF8 -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-final-stats.log'
```

## 采集命令和边界

```powershell
$ErrorActionPreference='Stop'; $traceStart=Start-Process -FilePath 'C:/Windows/System32/wpr.exe' -ArgumentList @('-start','CPU','-start','GPU','-filemode','-recordtempto','D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-wpr-temp','-instancename','MediaTranscodeWin28') -Verb RunAs -WindowStyle Hidden -PassThru; $traceStart.WaitForExit(); [pscustomobject]@{PID=$traceStart.Id;ExitCode=$traceStart.ExitCode}; & 'C:/Windows/System32/wpr.exe' -status -instancename MediaTranscodeWin28
$code='& ''C:/Windows/System32/wpr.exe'' -status profiles collectors -details -instancename MediaTranscodeWin28 > ''D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-wpr-final-status.log'' 2>&1; & ''C:/Windows/System32/wpr.exe'' -stop ''D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-wpr.etl'' -instancename MediaTranscodeWin28 > ''D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-wpr-stop.log'' 2>&1; exit $LASTEXITCODE'; $encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($code)); $proc=Start-Process -FilePath 'C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe' -ArgumentList @('-NoProfile','-EncodedCommand',$encoded) -Verb RunAs -WindowStyle Hidden -PassThru; $proc.WaitForExit(); [pscustomobject]@{PID=$proc.Id;ExitCode=$proc.ExitCode}; Get-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-28-wpr-stop.log'
```

启动WPR PID17268退出0；另以管理员状态命令确认CPU.Verbose.File/GPU.Verbose.File、CSwitch/ReadyThread栈和D盘临时目录，状态助手PID26888退出0。非管理员status曾显示未记录，不能代替管理员查询。停止助手PID32632、实际WPR27476，保存最终退出0。停止时UAC等待延长了额外记录区间：ETL UTC03:51:17.5984932—03:57:02.3826109，344.7841177秒，其中真实源仍严格120秒；不能把整段称为120秒记录。13919846400字节ETL仅D盘；丢事件及丢缓冲均0，停止后无WPR进程残留。

分析使用本机xperf的tracestats、process/thread/image和限定区间dumper；先核实11389490个CSwitch、5012228个ReadyThread及10265对DXGI Present事件，再提取第三路。首个组合导出的输出参数未按预期生效，停止该分析进程；随后分别用全局-o指定D盘文件并完整执行退出0。未使用失败导出作结论。临时WPA profile未实际用于分析，未新增生产代码或测试体系。

## 媒体与资源结果

- PID：CLI23432、源7920、dumpcap29328、VLC30384/616/28884/32044。源3600帧/120秒自然退出0；CLI源结束NoProgress退出1。抓包184712/drop0，四张1920×1080游戏画面已查看，四路正常关闭，以上进程均无残留。
- 1/2/3/4路Running分别11:52:11.087、35.254、43.873、11:53:09.416；第二路复用group1、第三路新group2、第四路新group3；1/2/3路11:52:52.272、11:53:00.216、04.939正常退役，第四路11:54:15.249保留源结束原因。
- CPU421样本，单核等效平均28.549129%、峰90.410959%；工作集124723200→206741504B，峰248905728B。受ETW开销影响，不用于性能评分。最终workers/errors/workerErrors/drop/pressure均0，stalledIntervals1；payload0字节/0对象，40527次申请释放相等。纯视频A/V漂移不适用。
- RTP21982/13264/9975/32754包，到达顺序相邻序号全部严格+1；TS TEI/AFC/连续性及expert error无命中。scope78017包/98138632 IP字节与抓包相等，50Mbps曲线超额1356B。第三路509个PTS严格40ms步进，到包偏移跨度107.2104ms；这不直接等于播放器迟延。
- 四路均D3D11VA，终态lost0/0/1/0，完整日志第三路实际too-late20ms一帧；其余及第三路19ms为debug late。displayed1000/583/497/1491只能是VLC统计值，包含重显且存在下游统计归集时机，不能当唯一视频帧数或逐帧完整性证明。

## ETW结论与未定位项

第三路PID28884、显示线程TID16044：

| 证据范围 | 实测 | 可得结论 |
|---|---|---|
|83—110秒DXGI Present|555对，未配对0，Result均0；平均133.1315µs、最大2062µs|该区间未见Present API内部几十毫秒阻塞；不证明GPU最终扫描成功。|
|86—108秒CSwitch/Ready|显示线程最大唤醒后Ready1.778ms；被抢占/Ready离CPU最大259µs|所查显示线程没有20ms级就绪饥饿证据；不能把条件等待当调度饥饿。|
|91.123261→91.184556→91.184595秒|Waiting61.295ms、Ready39µs，WrAlertByThreadId；对应Present间隔76.573ms|确定长间隔主要在条件等待；缺picture日期与请求timeout，不能确定其应否提前醒来或对应哪一丢帧。|

首批57—59ms Present间隔与VLC初始重显期限约56ms相容；渲染预算下降后重显间隔可接近76ms。此为源码支持的解释边界，不能把所有活动期长等待都归为正常重显，也不能把Present次数当唯一帧数。[显示调度](https://raw.githubusercontent.com/videolan/vlc/3.0.23/src/video_output/video_output.c)、[渲染预算](https://raw.githubusercontent.com/videolan/vlc/3.0.23/src/video_output/chrono.h)。

实际平台路径已核实：ETL libvlccore基址0x596d0000、等待返回址0x59785de0，RVA0xB5DE0；本机PE checksum2C9396/timestamp3A1639E4与ETL一致。反汇编为地址等待仿真中的三参数条件等待调用；KernelBase返回址RVA0x24A480落在SleepConditionVariableCS+0x20，原生WaitOnAddress导出为0x24A4B0。VLC仅从kernel32查询地址等待API，本机32位kernel32没有对应导出，所以选32bucket条件变量仿真；不是Windows11不支持原生API，也不是5ms SleepEx轮询路径。同步代码有比较值和唤醒保护，尚无证据将其开销或通知竞态判为根因。[Windows等待adapter](https://raw.githubusercontent.com/videolan/vlc/3.0.23/src/win32/thread.c)、[通用条件等待](https://raw.githubusercontent.com/videolan/vlc/3.0.23/src/misc/threads.c)、[vout控制](https://raw.githubusercontent.com/videolan/vlc/3.0.23/src/video_output/control.c)。

独立审查复算Present及该Waiting/Ready区间一致。当前仍缺丢弃分支的picture.date、next日期、请求等待期限，不能以本轮证据修改生产核心或宣称播放器问题已定位。尚未排除Prepare阶段、fence/GPU等待、上游供帧迟到或期限计算问题。静态反汇编已确认首个丢弃分支可读取当前picture/date及chrono，但caller进入时next为空，无法恢复前次请求timeout或原始输入PTS。独立审查认为仅设该单次断点不足以新增根因证据，因此未启动新调试实流，也未再次使用WPR。

补充局部关联：用ETL起始UTC03:51:17.5984932与抓包epoch直接换算，第三路PTS36603.12/36603.16/36603.20/36603.24分别在相对91.0922572/91.1348213/91.1759283/91.2155382秒到达，间隔42.5641/41.1070/39.6099ms。该名义时间窗口没有同量级到包停顿；但没有独立校准两个采集时钟的误差，也没有PTS到实际显示picture的映射，因此不能用它证明对应丢帧已提前到达，或排除播放器内部供帧迟到。未据此修改生产策略。

所有原始ETL、抓包、日志、截图、导出表及临时profile均仅D盘；已删除104个文件共14917133302字节及3个临时目录（含WPR生成的NGENPDB），匹配本轮前缀的残留0，媒体、抓包、WPR及xperf进程残留0；保留固定源和构建结果。

最终文档由vlc_diagnostic_review与w28_report_review两位未参与生产实现的智能体独立审查，均明确PASS；仅表示证据与结论边界准确，不替代实流门禁。生产代码本轮未修改，冻结98742fd6，质量评分不变。
