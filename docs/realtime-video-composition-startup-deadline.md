# 合屏源startup独立截止时间调度

## 依据与实现

基线fbebb63e。源startup此前以audioServo.minimumUpdateIntervalNs（10 ms）驱动周期tick，纯视频源无法提供真实音频servo。依据[GStreamer clocks单次可取消时钟通知](https://gstreamer.freedesktop.org/documentation/additional/design/clocks.html)，改用既有worker的输入或绝对deadline等待；不是另加视频专用线程或经验周期。

- coordinator把真实acquisition/keyframe-start时间与planner原有maximumWait/keyFrameWait形成checked绝对期限，deadline查询与poll共用。唤醒选最早期限，若两者均过期仍优先StartupTimeout；溢出失败。Running、WaitingForEvidence、Idle及终态无期限。
- keyframe存在性由原store无分配遍历，不复制presentation快照；保留不要求keyframe时的非空候选语义。
- StartupClockNode继续校验源代次/失效/证据并接收恢复唤醒，只在处理证据后发时间通知；删除periodic interval及其assembly、node option和servo相等校验。音频servo自身政策未改。
- CoordinatorNode每轮检查期限，未到期且无事件时用waitingUntilInputOrDeadline；到期样本占用既有单个pending clock槽，沿原有限snapshot barrier排空已排队media再poll。外部clock FIFO与本地timer是不同生产者，分别识别，外部回退仍失败，processed watermark不回退。
- terminal head/barrier禁止注入本地样本；进入terminal时清理尚未处理的本地样本。purge/stop/abort复用resetClockBarrier清buffer、来源和计数；不让WaitingForEvidence反复触发旧期限。

线程、发布RAII和输出背压不变，不新增队列；到期样本是既有有界槽中的metadata对象。没有证明全局metadata分配前准入，该门禁仍缺。MediaGraphWorker使用同组master clock计算剩余时间；Windows的可取消waitable timer与Linux condition-variable均由既有MediaNodeWakeup适配，序列号防丢唤醒，不修改平台实现。关键到期观测输出av_startup_deadline的deadline_ns/observed_ns；原尝试过期、purge与runtime telemetry沿用。

本轮解除一个共同前置依赖，纯视频成员的clock/startup/builder及2–4源入口/准入仍未完成。源码与时序设计通过不等于无输入到期或多源实流验收通过。

## 验证记录

独立设计审查及两名非实现者源码审查均PASS。Release全量重建session23658完成666项，configure/build exit0；CLI产物2026-09-29 11:19:03、5170688bytes。仅运行realtime，没有测试local。

## r52：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30 fps、8 Mbps

输出AAC CBR192kbps、44100Hz双声道。CLI PID18096/session41032；源PID25964/session98579；VLC PID24796；5秒监控session87237。实际命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-startup-deadline-r52 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-startup-deadline-r52.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-startup-deadline-r52-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-startup-deadline-r52-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-startup-deadline-r52-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-startup-deadline-r52- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 结果：FAIL

指定源3600帧/120秒自然exit0；CLI自然exit1，退出原因为realtime runtime made no progress before timeout。11:20:43.234首release video16/audio31；11:22:42.397 purge7ack完成old1/next2 acquiring；11:22:47.777至47.803 abort。没有下一代媒体或av_startup_deadline日志，到期分支尚无实流覆盖，不能证明纯视频或恢复。

edge46/47各10/10；abort前20queued/24objects/270826bytes；最终queued/workers/payloadBytes为0，仍有4逻辑对象（71950 reservations、71946 releases）。高水12047348bytes/78objects，workerErrors/errors/pressureFailures均0，stalledIntervals1；AAC关闭时2帧残留。

425次CPU采样、22核：进程整机口径均值1.181610%、峰5.089059%；单核口径25.995429%/111.959288%。runtime WS初始108519424、最终190513152、峰195481600bytes。外部19次5秒样本11:21:15.726至11:22:45.991，CPU累计9.03125→32.453125s，WS193105920→190513152、峰195702784；Private389132288→386359296、峰391168000bytes。短窗口不证明长期无增长。

118条generation1漂移，raw/filtered最大绝对值均156ns；不代表重入或播放端同步。sender86343datagrams/104842204payloadBytes，deadline/pressure/partial/ambiguous均0，delivery_evidence=not_proven。

实际查看VLC截图composition-startup-deadline-r52-2026-09-29-11h21m27s002.png，有1280×720游戏画面；VLC记录playback too late89487、picture late42ms和Failed to create video converter，不作为连续无丢帧证明。完整退出门禁FAIL，不创建成功验收提交；合屏保持FAIL42。

### 清理清单

归档后按精确身份清理VLC24796；CLI18096与源25964已自然退出。删除本轮-cli.log、-source.log、-vlc.log、.sdp和上述PNG五文件；保留指定120秒源及正式构建产物。本轮无远程测试、抓包、临时录制或测试脚本。

清理复核：上述五文件全部删除，三个精确PID均无残留，指定源124427809bytes保留；VLC清理不计自然退出。
