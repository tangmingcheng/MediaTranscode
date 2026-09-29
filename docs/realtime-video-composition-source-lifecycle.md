# 源缺流生命周期与准备步骤复用

## 范围与依据

2026-09-21，继续在 `feat/realtime-video-composition` 实施，基线 `81e30778`。目标是让已激活的合屏输出保留自身时钟与编码状态，缺流只使相应源退出当前贡献；首次启动、真实 I/O 错误和 purge 超时仍须失败。原单源入口明确选择原失败模式。外部 FFmpeg 不修改。

职责依据是 [GStreamer udpsrc](https://gstreamer.freedesktop.org/documentation/udp/udpsrc.html) 的接收超时通知、[rtpsession](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpsession.html) 的 SSRC/BYE 生命周期，以及 [GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html) 的逐输入聚合与持续输出边界。BYE 不等于整个合屏会话的可信 EOF。现有共享 DAG、时钟、启动发布和真实参与者 purge 是实现基础；不能通过伪造新代激活来跳过清理。

## 编码器与输入准备

视频编码器实际 open、emission envelope 校验、readback 和 context 保留抽为共用 `MediaVideoEncoderPreparer`，动态输出继续消费同一个已打开编码器。格式/open 选项统一由 `MediaVideoEncoderPlanOptionCodec` 编码，码率选项由现有 adapter 编码；原图内重复序列化已移除。必要产品缺失在 open 前失败。图外调用仍须提供链级 lineage 与真实硬件池契约；这一步不代表 composition preflight 已完成。

RTP 输入探测函数只接收输入配置与流集合，单源调用继续传相同事实及剩余启动时限，不再为输入探测复制完整输出请求。探测、后台 capture、seal 和实际 ingress 算法保持原实现；后续多源预检查可直接复用输入步骤。

局部编码器抽取经独立审查：语义和 UTF-8/CRLF 格式 PASS。

## 构建与审查经过（历史状态）

第一次 Release 全量构建 configure 成功、build 失败（外层退出1，Ninja退出2）；工具输出截断，未保存具体编译诊断，不计通过。第二次已完整捕获诊断。

首轮冻结双审发现两项源码阻断，当时阶段审查 FAIL：完成 purge 后，已排队的新失锁请求与旧恢复代发布存在竞争，并可解引用被另一 worker 清空的开始时间；另外 Degraded 可能恢复为同代 Locked，与下游预先推进的代次冲突。已分别统一发布仲裁事务与 clock-group 权威失效代次，并经双方重新审查关闭。

第二次全量构建同样退出1，完整诊断为输入探测调用的 C2664：`optional<MediaTranscodeStreamSet>` 不能直接传给必填枚举。已在原请求校验确保该字段存在后解引用传入，未增加默认值。复审又发现双路 gate 消费进度不同时，迟到的已退休 Acquiring 代次可能被误判为致命错误；已由公共权威代次分类修复，未来媒体与非法证据仍拒绝。

第三次 Release 全量构建完成，configure/build 均0、636项；期间两份 startup clock 文件仍发生所有权签名调整，且复审发现新的分类与提交间竞争，因此只记录为中间结果。同一目标可能先后由 `HardDiscontinuity G2` 与 `Future G3` 表示，现已在协调器仲裁锁内按当前权威状态重新分类，避免把已经建立或发布的同一目标判为致命冲突。最终冻结后的第四次全量构建及运行结果见下。

## 已实现与最终源码审查

源生命周期已接入内部 planner、输入事件、clock-group、启动协调器和运行时域注册：Preserve 模式只在唯一输出域真实激活后等待源恢复，初始准入及真实错误仍失败。未发布代退休复用真实参与者的 owner-thread purge/ack；恢复到期释放候选后等待新 SR 与新 AU，单 pending 槽保留原输入引用。权威代次分类由 gate 与启动时钟共用，排队失锁、迟到状态和同目标事件表示均在现有仲裁边界处理。

最终源码冻结后，两名未参与实现者分别复审，Standards 与本阶段源码 Spec 均明确 PASS；审查范围包含各轮修复、编码器抽取和输入接口调整。该结论不表示运行通过。

## 尚未闭合

完整 composition preflight、唯一输出编码器与画布的预 DAG 物理验证、按域资源总账、公共入口、纯视频非音源和 Windows→RKMPP 合屏验收仍在实施。当前公共入口仍为单源，没有运行到 Preserve 模式的完整合屏链路。r22 的无进展失败及逻辑对象残留证据继续有效，不能以本轮源码修改宣布修复。评分保持 42/100。

## r23 实际运行命令

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR 8 Mbps，1280×720、30 fps；AAC CBR192 kbps、44100 Hz、双声道。源为指定连续120秒文件，未循环或降规格。本轮为共享单源路径回归，不是合屏验收。CLI PID7736，FFmpeg PID33060，VLC PID11808。

最终第四次 Release 全量构建 configure/build 均0、636项；构建前后61个本轮生产源码文件的SHA256集合完全一致，验证了最终冻结版本。没有修改外部依赖。

### 结果：完整验收 FAIL

- FFmpeg 自然退出0，3600帧、120.00秒；CLI自然退出1。VLC启动命令返回0仅代表启动，不能作为播放进程自然结束证据。12:03:23的VLC截图已实际查看，1280×720游戏画面正常。
- 12:04:48.991两路SenderLeft事件，12:04:48.993七组purge全部ack并进入acquiring；12:04:54.229开始abort，最终错误 `realtime runtime made no progress before timeout`。编码包边46/47各满10项。未启动恢复源，没有generation2媒体证据。
- 最终queued/workers/payload bytes为0，但payload objects为4；70498次申请、70494次释放。errors/workerErrors均0，stalledIntervals为1。不能宣称资源完全归零，也不能仅凭退出前逻辑计数认定进程退出后的物理泄漏。
- 427个CPU样本、22逻辑核：整机平均1.118988%/峰5.191874%，单核等效平均24.617736%/峰114.221219%。工作集108244992→190943232字节，峰200507392；不能据此证明长期内存稳定。
- 后台进程采样：12:03:23 CPU9.203125秒、WS199155712、Private429903872字节；12:03:44 CPU14.8125秒、WS199364608、Private429830144；12:04:49 CPU30.25秒、WS190939136、Private422338560。
- 117条generation1漂移记录，raw/filtered最大绝对值均4793 ns（约4.8微秒）。payload高水15667464字节/362对象，pressure failures为0。
- sender提交82929数据报、100719512 payload字节，deadline/pressure/partial/ambiguous失败均0；取消1个未提交pacing预约、22个backlog数据报，末尾backlog为0。`delivery_evidence=not_proven`。MPEG-TS处理8358 AU、终止原因aborted。

本轮仍走旧SharedSourceOutput模式，未运行新Preserve源域，不能据此声称反复未发布代purge或持续黑场/静音已经通过。r22的无进展和4个逻辑对象残留仍可复现；后续必须完成独立输出预检查和资源准入，进入真正合屏链路验证。

### 本轮清理清单

以上证据归档后，已逐项删除 `out/acceptance/` 中的 `composition-source-lifecycle-r23-cli.log`、`composition-source-lifecycle-r23-source.log`、`composition-source-lifecycle-r23-vlc.log`、`composition-source-lifecycle-r23.sdp`、`composition-source-lifecycle-r23-2026-09-21-12h03m23s728.png`。本轮VLC通过RC quit关闭，不能记作自然结束；三个精确PID均已不存在，本轮前缀文件检查为空。指定120秒源仍存在；没有抓包、临时录制、测试脚本或远程测试产物。

### CLI

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-source-lifecycle-r23 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60790 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60792 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61790 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-lifecycle-r23.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-lifecycle-r23-cli.log 2>&1
exit $LASTEXITCODE
```

### FFmpeg 源

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60790?rtcpport=60791&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60792?rtcpport=60793&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-lifecycle-r23-source.log 2>&1
exit $LASTEXITCODE
```

### VLC

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-lifecycle-r23-vlc.log --extraintf=rc --rc-host=127.0.0.1:62790 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-source-lifecycle-r23- --snapshot-format=png rtp://@127.0.0.1:61790
```
