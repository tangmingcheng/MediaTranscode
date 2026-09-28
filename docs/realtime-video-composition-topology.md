# 合屏最终逻辑拓扑与资源绑定

基线2b3e7b24。本轮将原builder“实际encoder/canvas/decoder齐备才构图”拆为一次最终逻辑拓扑和最终资源绑定。尚未完成生产池准备授权、多源总账及公共入口，完整FAIL、42/100。

## 实现与边界

- buildTopology按原segment构建同一MediaGraph，move-only owner私有保存图和source/output registration；仅公开const图与aggregate拓扑，不产生含空prepared指针的runtime binding。
- canvas geometry与storage分型，aggregate拓扑只持几何和候选合同。实际aggregate plan在bind时加入真实storage；画布生产、readback、claim仍核验全部字段及同一生产pool。
- bind消费拓扑，保留各源独立decoder、实际设备、owned encoder、canvas与完整shape校验；内部从同图编译最终ledger，不接受调用方给一份账本就声称准入。绑定失败不发布可启动产品，资源依原RAII引用清理。
- 原共享单源和合屏共用资源选项写入：payload credit、reserved bytes、external allocation标识、encoder pool容量和authority。没有新增线程、队列或媒体拷贝；原节点线程、背压、owner模型保持。

逻辑图不是内存准备授权。实际canvas字节需要生产pool readback，生产pool打开又需要事先的驻留容量及有界准备预算，因此后续必须区分准备授权与实际资源最终准入，不能提前宣称最终bytes已准入。当前FinalLedgerCompiler仍不支持aggregate，bind继续明确失败；不能以此阶段代码声称合屏可以运行。Windows/RKMPP共享上述代码，未修改外部FFmpeg。

## 工业依据

[GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)在激活前完成格式和allocation协商，池引用回收与有限容量形成背压；本轮沿用规划与物理激活分离。具体驻留数仍必须来自本图的真实合同。[FFmpeg AVHWFramesContext](https://ffmpeg.org/doxygen/trunk/structAVHWFramesContext.html)的initial_pool_size有后端差异，不能宣称它是通用设备内存硬界。bind核对请求池合同与实际值，设备/驱动额外分配仍须单独界定。

## 审查与验证

首审A发现逻辑拓扑未提前复用已有geometry/chroma及aggregate容量检查，B撤回仅搬移阶段的PASS。已提为共用GeometryValidator和AggregatePlanValidator，buildTopology、CanvasProducer和AggregateNode消费同一检查；修后16源码双独立Standards/阶段Spec PASS。首次全量session53484成功654项，修后全量session73891成功656项，configure/build均exit0，实时CLI2026-09-28 15:25:45、5129728bytes。realtime结果见下文。不运行local CLI。原r43 no-progress/4逻辑对象和AAC重入问题仍未修复。

## r44真实realtime链路

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps；AAC CBR192kbps、44100Hz双声道。固定120秒源；无local测试。CLI PID36092/session68159，源PID39844/session71201，VLC PID29248（启动exit0），后台监控session28173。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-topology-r44 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-topology-r44.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-topology-r44-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-topology-r44-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-topology-r44-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-topology-r44- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 结果与清理清单

r44源自然exit0，3600帧/120.00秒；CLI自然exit1/no-progress。15:28:31.620 purge完成7ack、old1/next2 acquiring；37.092 abort.begin、37.158 abort.done。最后edge46/47均10/10，queued/workers/payloadBytes=0、4逻辑对象（reservations71844/releases71840），高水11962055bytes/85objects。workerErrors/errors/pressureFailures均0，stalledIntervals=1。

394个CPU采样/22核：整机口径进程CPU均值1.472967%、峰4.042553%；单核均值32.405279%、峰88.936170%。runtime WS初始104878080、最终191352832、峰196231168bytes。后台5秒监控exit0：15:27:04 CPU11.671875s/WS194064384/Private399097856；15:28:35 CPU40.34375s/WS191361024/Private396378112，已读样本Private达400629760bytes。118条generation1 raw/filtered漂移绝对最大22475ns、compensation_distance45346；不证明恢复。

sender提交86261datagrams/104746580payloadbytes，deadline/pressure/partial/ambiguous=0、pacing_cancelled1，delivery_evidence=not_proven。VLC截图composition-topology-r44-2026-09-28-15h27m25s025.png已实际观察1280×720游戏画面；日志有buffer deadlock prevented、playback too late103261、Failed to create video converter。不能据画面或小漂移判通过。

本轮仅验证共用资源选项写入的旧共享生产链，未调用新多源拓扑/bind；完整FAIL42，旧no-progress与AAC重入未解决。先归档上述命令与失败证据，随后清理精确清单：composition-topology-r44-cli.log、composition-topology-r44-source.log、composition-topology-r44-vlc.log、composition-topology-r44.sdp及上述PNG。VLC核对身份后按PID29248清理，不能计自然退出；CLI36092及源39844已自然结束。无远程、抓包或临时录制产物；指定源保留。

清理已执行并复核：5文件和3个精确PID均无残留，指定源124427809bytes保留。
