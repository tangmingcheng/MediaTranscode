# 阶段一单源基础实施与验证

## 当前结论

本记录为 WIP，尚未完成合屏。新增显式单域运行时注册、逐输入 RTP ingress 规划与音频帧逻辑资源凭证。Release 全量重建通过；Windows 原规格音视频链路仍失败。FFmpeg 源码、DLL 与 master 未修改。

## 真实音视频链路

输入：120 秒连续 H.264/AAC，1280×720、30 fps、视频约 8 Mbps。输出目标：MPEG-TS/RTP、HEVC CBR 8 Mbps、1280×720、30 fps，AAC CBR 192 kbps、44100 Hz、双声道。各轮保持相同规格。

| 轮次 | CLI 结果 | 源流结果 |
|---|---|---|
| baseline | 构建前拒绝：音频缺少 planner-owned ingress | 日志 3600 帧/120 秒；未取得原生退出码 |
| r1 | 退出 1：共享 prepared byte budget 重复封存 | 退出 0，3600 帧/120 秒 |
| r2 | 退出 1：音频 frame producer 缺少 typed credit resolver | 退出 0，3600 帧/120 秒 |
| r3 | 退出 1：音频 RTP clock binder acquiring capacity 6/6 | 退出 0，3600 帧/120 秒 |

前三个失败点已通过后续轮次继续向下执行验证，但不能将其记为完整链路通过。r3 编码输出为 0，VLC 无有效输出；CPU 采样为 0，无法评估稳定内存或 A/V 漂移。末尾 payload reservation/release 为 14/14，仅证明该次失败清理的逻辑计量归零。r3 FFmpeg PID 16348，VLC PID 12992，CLI 过早退出未取得 PID；VLC 经 RC quit 关闭，进程残留检查为空。

### r3 实际命令

三个进程分别启动；前两条保留原生退出码。r1/r2 使用同一参数，仅 media-id 与日志前缀对应替换。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60740 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60742 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61740 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r3.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r3-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60740?rtcpport=60741&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60742?rtcpport=60743&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r3-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r3-vlc.log --extraintf=rc --rc-host=127.0.0.1:62740 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r3- --snapshot-format=png rtp://@127.0.0.1:61740
```

## 根因与未闭环项

- 原 preflight 只配置视频 ingress。现从音视频各自 socket 和探测事实规划，两个 arena 经 checked add 计入预算；两个捕获均结束后只封存一次共享预算。
- 音频 decode/trim/resample 复用 prepared 样本几何形成 frame credit。约束为逻辑样本字节，不包含全部对齐、元数据与 codec 内部物理分配。
- r3 的音频 binder 在收到 Locked snapshot 前取第七个包并失败。该节点与容量 planner 本轮未修改；当前源码显示输出驻留容量被用于输入时钟获取，尚无旧二进制同规格运行归责证据。
- 仅满缓存等待不能完成结构修复：RawRtpInput 的媒体 pending output 会阻止同节点后续 RTP/RTCP 接收，可能挡住解锁证据。控制进展、媒体准入及恢复顺序需在原阶段一范围内继续处理。

工业依据：[GStreamer queue](https://gstreamer.freedesktop.org/documentation/coreelements/queue.html) 的有界阻塞、[rtpbin](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpbin.html) 的 RTP/RTCP 会话职责、[FFmpeg AVFrame](https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html) 的引用计数与软件帧分配。当前实现不宣称完整等价。

## 审查与风险

两位未参与实现者对当前 37 个源码文件均给出 Standards PASS / 当前 WIP 源码 Spec PASS；完整交付验收 FAIL。多源时钟、合成贡献记录、黑屏静音与恢复尚未实现。合屏就绪度维持 42/100。VideoOnly 与 RKMPP 回归需独立记录，不能替代上述音视频失败。

## VideoOnly 独立回归：r2（FAIL）

RTP H.264 → MPEG-TS/RTP HEVC CBR 8 Mbps，1280×720、30 fps，固定 120 秒源。FFmpeg PID 512、CLI PID 28852、VLC PID 31408。源退出 0、3600 帧/120 秒；CLI 退出 1，最终原因 realtime runtime made no progress before timeout。VLC 截图可见正常画面，但日志存在 late picture 警告，不能判定完整回归通过。VideoOnly 无音频，A/V 漂移不适用。

最终遥测：平均整机口径进程 CPU 0.874204%，峰值 2.386635%；工作集初始 108285952、最终 185487360、峰值 185507840 字节；workerErrors/errors/droppedBuffers/graphPayloadPressureFailures 均 0，stalledIntervals=1。payload reservations/releases=43404/43404，当前计量归零。短时结果不能证明长期内存上界。源自然结束后 CLI 超时退出，VLC 经 RC quit 关闭，进程残留为空。

截图：out/acceptance/composition-domain-videoonly-r2-2026-09-15-19h37m18s667.png。r1 因执行者遗漏 --no-audio 在参数校验阶段退出；其源由执行者停止，属于无效测试，不计验收。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --no-audio --media-id composition-domain-videoonly-r2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60740 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61740 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-videoonly-r2.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-videoonly-r2-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60740?rtcpport=60741&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-videoonly-r2-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-videoonly-r2-vlc.log --extraintf=rc --rc-host=127.0.0.1:62740 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-videoonly-r2- --snapshot-format=png rtp://@127.0.0.1:61740
```

## PR 独立审核

新的独立智能体审查 PR #34 的 fd6109b4：当前基础源码 WIP PASS，合屏交付与合并门禁 FAIL；标题、正文与 Draft 状态的证据边界 PASS。要求先闭合输入控制进展与媒体准入，定位 VideoOnly 结束超时，再完成原规格 Windows→RKMPP 验证。本次仅文档追加审核记录，未改变已审源码。

## r4：时钟获取队列背压验证（已运行，FAIL）

r3 的直接失败是 binder 满队列后仍取包，并非已经复现 ingress credit 阻塞。先按 GStreamer queue 的有界阻塞语义修正消费端：未锁定且获取队列达到 planner 容量时停止取媒体包，保持 clock 优先消费并等待原 acquisition deadline；保留容量越界断言，不增加容量、不丢包、不改变超时。所有状态仍由该节点 worker 独占，沿用 RAII buffer/credit 与现有跨平台通知。

worker 每次调用前记录通知序号，等待仅由序号变化、取消或截止时间唤醒；队列非空本身不会忙轮询。EOF/flush 仍按媒体 FIFO 顺序等待，不能宣称立即结束。此项不解决 RawRtpInput 媒体等待对 RTCP 的潜在阻塞，也不代表合屏已实现；下一次原规格真实运行用于定位剩余阻塞，不能用本地队列修改替代完整控制进展设计。

### r4 实际结果：FAIL

Release 全量重建成功（configure/build 均 0）。本轮只修改 binder 满队列前的取包行为，未修改启动协调器或其容量。CLI 退出 1，执行越过 binder 后在 node 13 报 startup buffer capacity exceeded，状态 PrimingStreams；已收到视频关键帧 sequence=1/generation=1。不能据此证明输入控制隔离完成。

源 FFmpeg PID 10780，退出 0，3600 帧/120 秒；VLC PID 10204，无编码输出，结束后通过 RC quit 关闭。CLI 在首次进程采样前退出，未取得 PID，cpuSamples=0，无稳定 CPU/内存/A/V 漂移证据。最终 workingSet/peakWorkingSet=108498944 字节；encodedPackets=0，workerErrors/errors=2/2，payload reservations/releases=53/53、currentBytes/currentObjects=0、pressureFailures=0。prepared 视频 805 RTP/1 RTCP、音频 36 RTP/1 RTCP，观察跨度均 314 ms。

源码进一步确认 makePolicy 要求 500 ms preroll，却使用输出 maximumResidence=100 ms 的 ledger.media 容量（视频 4、音频 6）；尚未修复。下一步核对输入事实规划及预算，不能仅扩大数字或降低启动门槛。

本轮实际命令如下：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r4 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60740 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60742 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61740 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r4.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r4-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60740?rtcpport=60741&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60742?rtcpport=60743&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r4-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r4-vlc.log --extraintf=rc --rc-host=127.0.0.1:62740 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r4- --snapshot-format=png rtp://@127.0.0.1:61740
```

本轮临时产物清单：out/acceptance/composition-domain-r4-build.log、composition-domain-r4-cli.log、composition-domain-r4-source.log、composition-domain-r4-vlc.log。未生成 SDP、截图、录制或抓包；本轮未使用远程机。结果归档后已逐项清理，检查本轮文件和进程均无残留，保留复用源及历史产物。

两位未参与实现者对冻结修改均给出 Standards PASS / 局部 WIP Spec PASS，完整合屏交付 FAIL，建议维持就绪度 42/100。后续输入保留、字节/对象预算、凭证生命周期与启动整批发布修复及r5～r7命令/结果见[专项记录](realtime-video-composition-input-retention.md)。仍需处理控制推进、Locked后尚未排空即invalidation时的重获取边界，以及完整结束与跨平台验收。
