# 真实源准备的所有权基础

## 范围与依据

2026-09-21，基于 `3a40b842`，继续 `feat/realtime-video-composition`。本步抽取共用 decoder open，并增加不消费正式回放的有界 RTP 快照。没有新增公共参数，没有修改外部 FFmpeg；不是完整合屏 prepare 或验收通过。

[FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html) 的状态机与引用所有权继续由既有组件承担。本机 FFmpeg `libavformat/demux.c` 在 `avformat_find_stream_info` 中保留 packet buffer 供后续回放，提供探测不丢正式输入的参照；本项目 snapshot 仍须接入既有 depacketize、canonical lineage 和启动事务。[GStreamer preroll](https://gstreamer.freedesktop.org/documentation/additional/design/preroll.html) 的保留、统一发布及取消清理边界与该分工一致。

## 已实现

- `CodecResolverDecoderContextBuilder` 是共用 decoder 创建入口。旧 `CodecResolverNode` 调用它一次，继续负责发布 context buffer、runtime facts 和 timestamp metadata。选择、COPY_OPAQUE、硬件设备与格式校验、线程 retention/readback 保持既有合同。
- `get_format` 的不可变 typed state 由 `CodecContextPtr` deleter 持有；移动和 `takeContext` 一并转移，释放 decoder 内部线程之后才允许释放 state。不再引用 resolver 成员或临时 builder。缺 state 返回 `AV_PIX_FMT_NONE`，分配失败返回错误。普通 `reset()` 会延长 state 到 wrapper 销毁，至多每个硬件 decoder 一份；共有指针对象体积随之增加。
- `MediaRawRtpProbeLease` 在 prepared 队列锁内复制当前数据报一次，返回只读视图，解码工作在锁外执行。只有原 capture 读取 socket；不调用 replay、不修改队列或原到达时间。
- 快照 payload 与固定描述符数组在分配前同时计入同一个 A/V prepared byte budget；不增加 wire observed bytes 或第二份容量。溢出、无输入、预算不足、分配失败均报错；move-only RAII 在数组释放后归还预算。lease 不借用 buffer，因此 buffer 取消或销毁不会使快照悬空。
- 一个 shared budget 同时最多一个 probe lease，持有期间禁止 seal；开始 replay 必须先核对 seal 再激活时钟。没有等待新数据、经验重试次数或隐藏扩容。
- 独立审查发现快照临时占额可能触发原 capture 的容量截止：已改为在同锁 snapshot 显示 probe 活动且容量不足时，记录 capture/shared budget 粘性错误并唤醒等待方；释放快照不清除失败，后续 seal 必须失败。没有 probe 时保留原采集截止语义。

## 证据与资源边界

decoder open 不等于实际解码首帧，snapshot 也不等于完整随机访问单元或当前有效 SSRC/参数集证明。此原语尚无生产 probe 消费者；后续须使用同一协议解析、源代次与 deadline、实际帧/device/格式/SAR 和生产回放验证。

不要求源 frames-pool 地址在 flush 后不变：CUDA adapter 核对实际 context/格式/几何；RKMPP 可重建池，RGA 核对当前 DRM 布局。唯一 canvas/encoder 仍须持有其实际 prepared 产品与证明。保持既有 `EngineManagedPayloadAndReservedStorage` 范围，device/driver 内部不可见分配为 observed-only，不伪称全物理内存硬上界。逐 owner 总账、packet/parser/header/候选元数据以及真实准备暂存仍需闭合。

## 尚未完成

真实输入预解码与正式回放交接、唯一输出/画布图前证明、按 owner 资源准入、等比内容矩形、完整 composition preflight 与已批准公共入口、Windows→RKMPP 原规格合屏矩阵。Shared 单源的源结束无进展与残留逻辑对象、AAC 重入错误尚未宣称修复。

## r25 Release 共享单源回归

本阶段两名独立审查者最终 Standards/阶段源码 Spec 均 PASS；完整合屏交付/合并仍 FAIL。首轮全量构建成功后修复审查发现的 capture 预算交互，最终再次全量重建成功：configure/build 均0，638项；13个源码文件冻结 SHA256 在最终构建前后完全一致。

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps，1280×720、30fps；AAC CBR192kbps、44100Hz双声道。使用指定连续120秒源，无循环、降规格或额外FFmpeg监控。CLI PID29476、FFmpeg PID15968、VLC PID30872。结果：完整验收 **FAIL**。

- 13:01:23.508选择cuda-nvenc、score3280；公共builder实际打开h264 CUDA decoder，scale_cuda=1280:720与hevc_nvenc成功。13:01:52实际查看VLC截图，1280×720游戏画面正常；仅证明该时刻播放。
- 源完整3600帧、120.00秒，自然exit0。13:03:22.808两路SenderLeft，.809七组purge ack完成，old1/next2进入acquiring；13:03:28.233 abort。CLI自然exit1：`NotInitialized: realtime runtime made no progress before timeout`，边46/47各10项满队列。没有恢复源，不能声称generation2媒体恢复。
- 最终queued/workers/payload bytes均0，payload objects仍4，71865次申请/71861次释放；高水12052236字节/76对象，pressure failures0，errors/workerErrors0，stalledIntervals1。逻辑对象残留不是进程退出后物理泄漏证明。
- 432个CPU样本、22逻辑核：整机平均1.082311%/峰3.225806%，单核等效平均23.810840%/峰70.967742%。工作集106536960→188628992字节，峰193433600，不能证明长期稳定。
- 后台另采45个进程样本：13:01:52.263 CPU7.328125秒、WS191000576、Private390291456；13:03:20.250 CPU28.765625、WS193433600、Private391794688；13:03:26.290 CPU29.671875、WS188628992、Private388886528。
- 118条generation1漂移，raw/filtered最大绝对值均156ns。sender提交86363数据报、104861900 payload字节，deadline/pressure/partial/ambiguous失败及pacing取消均0，最终backlog0；`delivery_evidence=not_proven`，不判网络服务曲线通过。

本轮只验证共用decoder创建在Shared单源真实路径可启动；新probe lease没有消费者，未运行Preserve合屏域。原无进展/4对象问题仍复现，未创建成功验收提交。无远程测试、抓包或临时测试脚本。

### 实际 CLI / FFmpeg / VLC 命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-prepared-source-r25 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60900 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60902 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61900 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-prepared-source-r25.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-prepared-source-r25-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60900?rtcpport=60901&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60902?rtcpport=60903&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-prepared-source-r25-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-prepared-source-r25-vlc.log --extraintf=rc --rc-host=127.0.0.1:62900 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-prepared-source-r25- --snapshot-format=png rtp://@127.0.0.1:61900
```

### 本轮清理清单

必要命令、结果及指标先归档于上文。随后按精确PID和命令身份清理VLC；该退出不记自然结束。逐项清理 `composition-prepared-source-r25-cli.log`、`composition-prepared-source-r25-source.log`、`composition-prepared-source-r25-vlc.log`、`composition-prepared-source-r25.sdp`、`composition-prepared-source-r25-2026-09-21-13h01m52s015.png`。执行后的残留核对另记；指定120秒源必须保留。

清理已执行：5项文件均删除，本轮前缀文件0、本轮3个PID残留0、指定120秒源存在。无本轮远程产物。VLC按已核对的PID30872强制结束，未计自然退出。
