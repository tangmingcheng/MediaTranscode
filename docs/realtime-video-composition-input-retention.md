# 合屏阶段一：输入启动保留契约

## 问题与依据

r4 已越过 binder 的满队列取包错误，但启动协调器在 PrimingStreams 报容量不足。当前策略要求 500 ms 共同窗口，Raw RTP 却复用按 100 ms 输出驻留算出的 4/6 个 AU。协调器选窗前保留全部 Locked 前缀，最长等待 10 秒，不能仅按 500 ms 扩容。

[GStreamer queue](https://gstreamer.freedesktop.org/documentation/coreelements/queue.html) 分离启动最低阈值与时间/字节/对象最大容量；[延迟设计](https://gstreamer.freedesktop.org/documentation/additional/design/latency.html) 要求缓冲能力覆盖等待要求，否则拒绝运行。本修改复用这种契约分离，沿用原共同窗口选择器、时间策略和超限失败语义，不创造新的选帧算法。

## 本轮实现

- 输入保留产品与输出 `ledger.media` 分离。有限 AU 接纳数为 `ceil(maximumWait × 源 cadence) + sealed replay AU 上界`；字节容量由原输入 AU envelope 相乘并检查溢出。
- 视频 cadence 是既有探测的 marker timestamp 差，音频 cadence 来自输入 codec 的 clock rate/AU duration；不是输出编码速率，也不是任意 UDP 最大到达率。超过接纳上限仍明确失败，不增容、不静默丢包。
- 封存后在原队列内统计回放：H.264/HEVC marker 数加一个跨回放边界 AU，AAC 复用 AU header planner，Opus 按每 RTP 一个 AU。逐包临时解析，不跨线程借用接收 span；捕获结束和共享预算封存是读取前提。
- startup 消费同一输入产品，全局 ledger 另计保留 payload；原单次 completion/partial AU 余量保留。同一 AU 在节点间移动依靠已有 RAII lease，不重复记一份实际分配。
- 最终 DAG 对象凭证显式包含 binder acquiring 队列和 startup 批次容量，按选中节点记账；批次经 GraphEvent/release/extractor 转移时仍使用原 AU lease。不能仅增加字节额度而遗漏对象硬界。多代批次同时保留仍受共享信用上限约束，不承诺所有本地容量同时满载。
- runtime 不再使用统一 256 项常量，按 planner 显式容量和序列化范围验证；generic preflight 的既有 256 项观察预算保持原行为并明确命名。
- 接收、解包、启动 worker 与平台 adapter 不变；本项不构成完整控制隔离。恢复、超 cadence 突发和长期内存仍需后续真实链路证据。

## 验证进度

全量重建与双独立源码审查已覆盖输入保留、对象账及凭证生命周期修复。r5/r6仍为真实链路FAIL；r6揭示原子启动发布边容量未覆盖输入批次，修复后继续原规格验证。构建或局部代码通过不代表真实链路或合屏通过。

VideoOnly 的 InitialOutputResourcePartition 由 `MediaRealtimeVideoRunController` 在 streamSet=VideoOnly 时创建，A/V 不进入该独立 quota 路径。本项共用保留类型及最终对象编译器，不改变平台 adapter；共享影响仍需 Windows 后再覆盖 RKMPP。

## r5 实际运行：FAIL

两次 Release 全量重建均成功，第二次包含对象账补齐。输入契约：acquisitionWindow=10000000000 ns，视频 replay AU 上界22、总322、2817500000字节；音频 replay AU上界30、总461、3776051字节。这是逻辑接纳额度，并非实际常驻分配，也不证明未来到达率上界。

CLI PID30432，退出1；执行越过startup选窗，进入VideoDecode后报 input packet lacks payload credit ownership。源FFmpeg PID26916，退出0、3600帧/120秒；VLC PID20252，无编码输出，经RC quit结束。encodedPackets=0，cpuSamples=0，无稳定CPU/内存/A/V漂移证据；workingSet/peakWorkingSet=111935488字节，workerErrors/errors=1/1，payload reservations/releases=59/59、currentBytes/currentObjects=0、pressureFailures=0。

RTP binder与demux binder均在takePacket/wrapPacket时遗漏资源凭证转移。已复用既有takePayloadCredit/attachPayloadCredit补齐，失败仍传播；不重新分配payload、不重新申请信用、不绕过解码校验。原则参照[FFmpeg AVPacket引用与移动所有权](https://ffmpeg.org/doxygen/trunk/group__lavc__packet.html)。此后续修复待r6验证，不能追认r5通过。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r5 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60740 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60742 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61740 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r5.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r5-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60740?rtcpport=60741&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60742?rtcpport=60743&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r5-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r5-vlc.log --extraintf=rc --rc-host=127.0.0.1:62740 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r5- --snapshot-format=png rtp://@127.0.0.1:61740
```

本轮临时清单：composition-domain-r5-build.log、composition-domain-r5-build2.log、composition-domain-r5-cli.log、composition-domain-r5-source.log、composition-domain-r5-vlc.log（均位于out/acceptance）。无SDP、截图、录制或抓包；未使用远程目标。命令与结果归档后已逐项清理，本轮文件和进程均无残留，保留复用源和历史文件。

## r6 前所有权补齐

两类 binder 凭证转移修复经双独立复审 PASS、全量重建成功。沿同一生产链继续核对发现 `MediaScheduledPayloadClone` 为 displayed/repeated video 克隆 AVPacket 引用，却不共享原 credit；原 wrapper 释放后，副本仍持有的数据会提前退出计量。已复用 `sharePayloadCreditFrom`，使原凭证跟随副本寿命，不重复申请主 payload 信用，未改变既有 `av_packet_clone` 分配行为。此项来自源码审计，不是 r5 运行复现。两位独立复审者对本轮完整源码均 PASS，r6 仍按原规格执行。

物理计量边界：`av_packet_clone` 会复制 side data，非引用计数 packet 还可能复制主数据；共享 lease 不代表这些额外分配已逐一准确计账，不宣称整个 clone 零分配或完整物理内存预算通过。

## r6 实际运行：FAIL

包含两类 binder 与 scheduled clone 凭证修复的两次 Release 全量重建均成功。CLI PID6088、退出1；FFmpeg PID28448、退出0，3600帧/120秒；VLC PID30644，经RC quit结束。视频decoder CUDA及scale_cuda=1280:720初始化成功，首帧已进入滤镜，无编码输出、无播放画面和持续A/V漂移证据。此次失败不能归因于CUDA滤镜启动失败。

首批release含video16、audio31；运行约6.4秒后无进展超时，workerErrors/errors=0/0。cpuSamples=22，进程平均/峰值整机CPU=0.594265%/3.225806%；workingSet由104357888升至186675200字节，短时平台期不足以证明长期稳定。payload高水位11279505字节/139对象，最终报告966169字节/62对象，reservations/releases=156/94，pressureFailures=0；这是abort报告时仍有节点持有引用，尚未取得对象销毁后计量，不能断言泄漏或完全释放。

独立审查确认等待闭环：输出驻留100ms推导queues.packet=4+6=10，原atomicAudioPacket复用10；extractor需要整批预留31音频AU，count>capacity永远不能成功。Filter等ReleaseCommitted，sequencer等extractorOutputsReserved。满载事件边18/19/21/22/23是反压结果。修复由planner仅为原子video/audio发布边采用startup对应上界，并由runtime产品validator重算核对；最终DAG账本从实际边容量计入。永久不可能的reservation明确InvalidArgument，临时占用仍返回等待。不改变线程模型、发布原子性或平台adapter。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r6 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60740 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60742 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61740 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r6.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r6-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60740?rtcpport=60741&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60742?rtcpport=60743&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r6-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r6-vlc.log --extraintf=rc --rc-host=127.0.0.1:62740 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r6- --snapshot-format=png rtp://@127.0.0.1:61740
```

本轮临时清单：composition-domain-r6-build.log、composition-domain-r6-build2.log、composition-domain-r6-cli.log、composition-domain-r6-source.log、composition-domain-r6-vlc.log（out/acceptance）。无SDP、截图、录制、抓包，未使用远程目标。归档后已按清单删除并确认本轮文件、进程均无残留。

## r7 前发布策略隔离

两位复审者发现通用atomicVideoPacket/atomicAudioPacket还用于scheduled RTP输出与demux入口，不能直接扩大。现增加仅用于startup release的内部边策略，覆盖video decode入口、audio decode入口及audio CopyPacket到scheduler入口；原通用atomic策略及VideoOnly产品保持原契约。RuntimePlanner与Validator共同生成并核对完整边产品，builder只传递产品。首轮构建因旧VideoOnly调用受接口改动影响失败，原接口已恢复，重新执行全量构建；不将失败构建计为通过。

## r7 实际运行：持续输出，完整验收 FAIL

最终Release全量重建成功（此前一次因VideoOnly旧调用接口不匹配失败，已修正并重建）。冻结源码由两个未参与实现的智能体独立复审，Standards/本轮源码Spec均PASS。

- CLI PID20884退出1；源FFmpeg PID12972退出0、3600帧/120秒；VLC PID27728播放后经RC quit退出。未停止CLI，源自然结束。链路为RTP H.264/AAC→生产DAG CUDA解码/scale_cuda→HEVC CBR 8000 kbps、1280×720、30fps、AAC CBR192 kbps/44100Hz双声道→MPEG-TS/RTP。
- 已观察VLC 1280×720游戏画面截图，无黑屏或明显破图；截图只证明观察时画面，不证明全程无丢帧或端到端唇同步。VLC日志存在迟帧和audio late/flushing警告；Failed to create video converter发生在截图转换链尝试，之后d3d11_filters+swscale成功生成PNG，不能当作生产CUDA滤镜启动失败。
- 内部av_drift_trace共118条，source master区间约0.336～119.640秒，raw/filtered phase记录为0，频率/补偿记录为0；这是引擎内部时间关系，不是播放器声画偏移测量。
- CPU409次采样，进程整机平均1.134210%、峰值3.291139%（22逻辑核）；工作集初始109891584、热启动约193544192、末尾196972544字节。热阶段仍缓慢增长约3.4MB，不能据120秒断言多小时稳定。最终payload current bytes/objects=0/0，高水位12064274字节/83对象，reservations/releases=71844/71844，pressureFailures=0。
- encodedPacketsPushed/Popped=39983/39937，是多条边计数累计，不等同唯一编码包数。发送端已提交86318 datagrams、104814268 payload bytes，deadline/pressure/partial failures均0；无wire capture，delivery_evidence=not_proven。
- 源结束附近时钟状态从Locked generation1转为ReacquireRequired generation2/3，gate诊断locked_generation=1、state_generation=2、readiness=2，报Locked packet gate rejects malformed discontinuity evidence，workerErrors/errors=1/1。AAC关闭时还有2帧，发送尾部尚有9 datagrams，未证明完整尾部排空，不能标记成功或创建成功验收提交。

### 剩余源失活边界

源码中clock group invalidate增加groupGeneration，adapter直接将新generation投影为带Discontinuity的SourceClockState，而Locked gate要求该事件标识当前失效的旧generation。该生产路径已由上述日志暴露；应继续按源失活/代次转换契约修复，不放宽gate的相等检查。当前日志未标识RTCP报文原因，未抓包，不能宣称已抓到BYE。

独立设计审查对照[RFC3550 §6.2.1/§6.6/§8.2](https://www.rfc-editor.org/rfc/rfc3550.html)：BYE可用于旧SSRC退出后换SSRC继续，且可能有迟到RTP，不证明业务会话永久结束。原合屏计划已授权内部源离开→黑屏/静音及恢复；不能为让此测试exit0新增未经批准的BYE即EOF策略。多源/恢复仍未实施，当前交付保持WIP。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r7 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60740 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60742 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61740 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r7.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r7-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60740?rtcpport=60741&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60742?rtcpport=60743&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r7-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r7-vlc.log --extraintf=rc --rc-host=127.0.0.1:62740 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r7- --snapshot-format=png rtp://@127.0.0.1:61740
```

截图通过VLC RC `snapshot`取得，关闭通过同一RC `quit`，地址127.0.0.1:62740。临时产物清单（out/acceptance）：composition-domain-r7-build.log、composition-domain-r7-build2.log、composition-domain-r7-cli.log、composition-domain-r7-source.log、composition-domain-r7-vlc.log、composition-domain-r7.sdp、composition-domain-r7-2026-09-16-09h54m36s385.png。无抓包/录制/远程脚本；归档后已逐项删除，确认本轮文件及PID20884/12972/27728均无残留，保留120秒复用源与历史文件。
