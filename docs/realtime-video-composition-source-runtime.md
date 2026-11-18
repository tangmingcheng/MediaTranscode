# 合屏源同步与运行时装配

## 本轮范围与依据

基于c930cf66继续：源时钟和源校正已存在，但同步政策只能从含输出协议的计划开始，不能直接生成SourceContribution运行时。新增明确源域标记，由同一同步planner产生源专用政策；共用原有时钟、视频恢复、metrics政策，单源共享调用保留输出协议。生命周期由域决定：源贡献PreserveActivatedOutput，共享链FailSessionOnSourceLoss。不通过删除输出字段伪装源域。

[GStreamer同步](https://gstreamer.freedesktop.org/documentation/additional/design/synchronisation.html)区分公共管线时钟与各源buffer/segment到running-time的映射；本轮沿该职责边界复用既有源clock assembly，逐源purge不包含输出encoder/scheduler/mux/sender。[GStreamer延迟](https://gstreamer.freedesktop.org/documentation/additional/design/latency.html)要求按真实排队路径规划容量与延迟；源校正沿上一轮实际frame命令边，startup批次与字节界复用原edge planner。引用不证明既有servo经验阈值正确，未新增或调节阈值。

## 实施

- MediaAvSyncPlanner通过显式variant接收源贡献域或已校验的连续输出产品。源域只支持既有RTP/RTCP A/V和frame audio；缺事实、错域、copy audio或非RTP保活均失败。原共享调用实际使用该入口。
- MediaAvSynchronizationPolicyPlanner集中原公共政策，没有新增默认值。原输出planner与源域共用。
- MediaRealtimeAvSourceRuntimePlanner消费实际输入、音频decoder/resampler、队列、线程政策和输出激活提前量，依次规划源时钟、校正、边界和源专用transition，返回既有composition builder所需的完整A/V源runtime。参数为内部planner产品，不新增CLI/API/profile字段。
- 既有单源runtime与新源runtime共用planSynchronizedSource，保留原startup字节加法溢出检查和有界批次。composition builder校验各源activationOutputLead与唯一输出一致。

规划发生于构图前，不新增运行线程、队列或复制媒体；运行仍使用既有每节点执行、SPSC/原子事务、BlockProducer与startup批次。输入与音频计划按值move进入运行时，prepared资源继续由原RAII owner持有；失败返回原错误，不fallback。Windows/RKMPP复用同一planner，未修改外部FFmpeg或新增平台链路。

## 首审修正

首审A指出新入口漏掉旧整链的实际RTP transport一致性、音频源索引和线程合同校验；B首审未发现该缺口，未按双通过处理。现将原RuntimeInputValidator改为借用源输入request、同步政策、源facts和assembly，新旧入口共用原RTP/TS/demux校验；源入口拒绝负音频索引。原严格PerNodeWorker/High/非pinning/收集metrics政策提为同一validateThreading，新旧入口共用，不另设平台线程模式。首次全量session18976成功（652项），修正后须重新全量复验。

## 验证边界

本轮新源runtime尚无公共多源协调器调用，不能计为多源真实运行。纯视频源、唯一资源准备/总准入、2–4路公共入口及Windows→RKMPP原矩阵仍缺。r42旧无进展/4逻辑对象和AAC重入未修复。完整验收仍FAIL、42/100；本轮双审通过，Release全量成功，r43仅realtime复验失败，见下文。

## Release与r43真实链路

双独立修复后Standards/阶段Spec PASS。首次全量session18976、修后session72215均configure/build exit0、652项；修后CLI2026-09-28 14:51:39、5125632bytes。没有运行local CLI。

r43：CLI PID23252/session99642自然exit1；FFmpeg PID3488/session14557自然exit0，完整3600帧120.00秒；VLC PID37700启动命令exit0，实际截图composition-source-runtime-r43-2026-09-28-14h53m02s151.png观察到1280×720游戏画面。VLC有playback too late112228、picture late65/31ms和Failed to create video converter，不能据有画面判为无故障。

14:54:27.891 purge_ack=complete、7ack、old1/next2进入acquiring；33.021 abort.begin，33.051 abort.done。edge46/47均10/10；最后queued/workers/payloadBytes=0、4逻辑对象、reservations71867/releases71863，高水12056561bytes/77objects；workerErrors/errors/pressureFailures=0、stalledIntervals=1，CLI明确报告no progress。

417个CPU采样/22核：整机均值1.128995%、峰3.241895%；单核24.837893%/71.321696%。runtime WS初始105021440、最终188342272、峰192790528bytes。后台5秒监控session49623 exit0：14:52:50 CPU6.09375s/WS191045632/Private393900032；14:54:31 CPU30.953125s/WS188350464/Private391065600，监控WS峰193372160、Private峰395776000。118条generation1漂移raw/filtered绝对最大156ns，观察到compensation_distance45346；不证明恢复。sender提交86347datagrams/104843172payloadbytes，deadline/pressure/partial/ambiguous=0、pacing_cancelled1，delivery_evidence=not_proven。

旧共享单源运行执行共用政策、源边规划和借用输入验证，没有调用新SourceContribution runtime、纯视频、多源或RKMPP。完整FAIL42，旧退出无进展/4逻辑对象及AAC重入未解决。

实际命令及证据先归档；随后按清单清理本轮3日志、SDP和上述PNG共5项，VLC核对身份后按PID37700清理，不计自然退出或通过。指定120秒源124427809bytes保留；无抓包、临时录制或远程产物。

### r43实际执行命令

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps；AAC CBR192kbps/44100Hz双声道，固定120秒源：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-source-runtime-r43 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-runtime-r43.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-runtime-r43-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-runtime-r43-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-runtime-r43-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-source-runtime-r43- --snapshot-format=png rtp://@127.0.0.1:62020
```
