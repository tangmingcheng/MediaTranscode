# 合屏逐源输入准备事务

基线311f8d5f。原raw RTP preflight的输入采集、封存、ingress规划/配置和owner交付独立为输入事务，现有single realtime入口已改用它；事务不接收输出配置，不创建编码器、mux或发送器。完整多源准备、全局准入和合屏尚未完成，保持FAIL、42/100。

## 工业依据与适用边界

- [GStreamer状态设计](https://gstreamer.freedesktop.org/documentation/additional/design/states.html)区分资源准备、可处理与运行阶段；失败不发布成功状态，退出时先解除等待再释放。本次用两个内部move-only类型表达本项目已经存在的capture/sealed阶段，不另造运行时恢复算法。
- [GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)协商后配置、激活/停用和最后引用归还，支持分离输入准备owner与输出能力规划。它不是N源全局预算的证明。
- 本项目是主动捕获真实RTP证据，不能照搬live source在PAUSED不产数据的行为；继续原jthread和有界byteBudget，能力规划期间持续采集，超预算保留原失败。capture停止顺序复用request_stop→transport.stop→join→reset；无平台分叉。

## 实现与生产调用

MediaRtpSourcePreflight::begin仅接input、streamSet和原absolute deadline，复用现有input planner/preparer。observation返回独立事实快照，不泄漏mutable probe。调用方照旧规划能力，seal&&消费probe并停止采集、封存同一预算、规划同owner的video/audio ingress；失败局部RAII回收，不能重试半封存状态。

MediaPreparedRtpSource只借出const资源view供planWithInput，release&&消费owner，将内部sealed ingress配置到原输入，全部成功且deadline仍有效才交付video/optional audio。move清空旧probe；第二次seal/release或已消费对象resources返回错误。借用view不能跨owner移动/释放；现有调用者仅在规划期间使用。

原remaining deadline毫秒取整/范围检查整体复用；begin、seal前后和release前后核对同一deadline，不重置总open timeout。底层同步调用仍可能直到返回才检查期限，没有新增公共stop参数，也不宣称强制打断驱动。probe自身preparationDeadline/analyze限制继续由旧底层保留。

startup继续由planWithInput使用MediaPreparedRtpStartupPlanner，从同一sealed输入replay AU bound、已规划raw cadence/AU和原acquisitionWindow推导；seal尚无这些全部事实，不能提前造默认产品。composition后续可为各源持事务再共用该planner，不需要N次whole transcode preflight。

## 未完成与验证

本轮未将N个输入事务接到公共composition控制器，SourcePlan尚无完整prepared retention/并发owner产品；metadata分配前配额、统一总账、纯视频源runtime及Windows→RKMPP矩阵仍待闭合。旧EOF/no-progress与AAC重入问题不在本次准备职责迁移中解决。

3源码双独立阶段审查PASS，9文件UTF-8无BOM/CRLF与diff检查通过；原deadline算术逐字等价。Release全量session92180（666项构建图）configure/build exit0；realtime CLI 2026-09-29 10:31:01、5166592bytes。未运行local CLI。

## r50真实realtime复验

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR 8Mbps、1280×720/30fps；AAC CBR192kbps、44100Hz双声道。原连续120秒源，无循环/降规格/额外FFmpeg监控。CLI PID31848/session27205，源PID2512/session69652，VLC PID32648/启动exit0，外部5秒监控session84939。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-source-preflight-r50 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-preflight-r50.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-preflight-r50-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-preflight-r50-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-preflight-r50-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-source-preflight-r50- --snapshot-format=png rtp://@127.0.0.1:62020
```

准备证据：10:32:03.654 startup planner记录window_ns=10000000000、video_replay_au_bound=24、audio_replay_au_bound=33、video_units=324/audio_units=464、video_bytes=2835000000/audio_bytes=3800624，future_arrival_rate_guarantee=not_proven。新事务由原单源生产调用实际运行，startup仍消费同sealed owner；不是多源准备证据。

### 结果：FAIL

源3600帧/120.00秒自然exit0；CLI自然exit1/no-progress。10:34:02.883 purge7ack complete、old1/next2 acquiring；10:34:08.235 abort.begin、08.263 abort.done。edge46/47均10/10；abort前20queued/24objects/250464bytes，final queued/workers/payloadBytes=0、4逻辑对象（reservations71839/releases71835）；高水12211488bytes/85objects，workerErrors/errors/pressureFailures=0、stalledIntervals=1，AAC关闭时2帧残留。

424CPU采样/22核：进程整机口径均值1.112540%、峰4.025424%；单核均值24.475878%、峰88.559322%。runtime WS初始114434048、最终190484480、峰195354624bytes。外部5秒监控19样本exit0，10:32:35.080至10:34:05.335；CPU累计8.75→30.5s，WS192479232→190504960、峰195354624，Private397549568→396144640、峰399540224bytes。

118条generation1 raw/filtered漂移绝对最大156ns，不能证明恢复。sender86261datagrams/104739436payloadbytes，deadline/pressure/partial/ambiguous均0，pacingCancelled1/backlogCancelled53、delivery_evidence=not_proven。已实际查看VLC截图composition-source-preflight-r50-2026-09-29-10h32m47s081.png中的1280×720游戏画面；VLC有playback too late115438、picture late67/33ms及Failed to create video converter，截图不证明持续无丢帧。

新输入事务已经过真实单源生产调用，准备/封存/ingress配置/既有startup规划和转码启动未报错；未直接验证多源并发、VideoOnly变体或注入部分配置失败。本次完整退出门禁FAIL，不计成功验收提交；旧no-progress/4对象及AAC重入仍待解决。

### 清理清单

先记录上述命令、结果与指标，再精确核验并清理VLC32648，不记自然退出。CLI31848、源2512已自然结束。删除本轮5文件：composition-source-preflight-r50-cli.log、-source.log、-vlc.log、composition-source-preflight-r50.sdp及上述PNG。没有远程、抓包、临时录制或测试脚本；保留指定120秒源及正式构建产物。清理复核完成：5文件与3个媒体PID均无残留，指定源124427809bytes保留。
