# 持续聚合接线与 Release r22 回归

## 当前实现与边界

2026-09-21，工作分支 `feat/realtime-video-composition`。源域和独立输出域已进入 binding、编译、注册及形状校验；视频逐格来源与音频真实贡献/生成静音进入规范化载荷。聚合节点独占候选、整数帧/样本轴和单 pending 输出，逐源 purge 不清共享输出编码器。图构建器复用现有输入、解码、缩放、重采样、编码、调度和协议段；共享实际编码器设备供各源解码。

画布预分配黑模板和有限可写 surface，释放帧的 header lease 时通知聚合 owner，避免全源缺流时依赖输入消息唤醒。FFmpeg `av_frame_unref` 先释放 image buffers 再释放 extended buffers；本机 NVENC 持有完整 `AVFrame` 引用。RKMPP 的实际保留/释放时序尚未验证，不能据此声称跨平台背压完成。

采用 [GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html) 的单 owner、有界输入和按时钟处理缺口职责；其成熟机制不等于本项目已经实现等价恢复。新图构建器尚无 composition preflight、全局资源准入及 CLI/C API 调用入口。目前源域仅支持完整 A/V 源，纯视频非音源仍待接入；源失锁策略仍可能终止全图。画布能力检查仍在节点准备期，尚未前移到 DAG 构建前。以上均为未完成项。

Release 全量构建：输入 helper 抽取后两次因 `branchEnabled` 不可见失败，修正后成功（633 项）；加入组合图后因 move-only 协议产品被复制失败，改为消费选项并移动协议所有权后成功（634 项）。成功轮 configure/build 均为 0。构建结果不是媒体验收。

冻结后两名未参与实现者 `composition_integration_review_a/b` 分别复审：RGA逐平面布局及修复中RGB24/BGR24字节序问题已关闭，**当前WIP源码安全保留PASS、完整合屏FAIL**。该结论不批准合并或生产交付，也不替代原单源、动态多输出和双平台合屏运行门禁。RGA adapter 在Windows构建中被排除，仍缺RKMPP编译/运行证据。

## r22：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30 fps、8 Mbps

本轮是共享实现的原规格单源回归，**完整验收 FAIL**，不是多源合屏通过。AAC 输出 CBR192 kbps、44100 Hz、双声道。使用指定连续120秒源，无循环、降规格或额外 FFmpeg 监控。

- FFmpeg PID9696 自然退出0，3600帧、120.00秒；CLI PID18836 自然退出1；VLC PID30620。
- 11:10:00 的1280×720 VLC截图已实际查看，首段有画面。11:11:08.157源结束后七组purge全部ack、进入acquiring；11:11:13.383 CLI因 `realtime runtime made no progress before timeout` 退出。编码包边46/47各满10项。这证明原单源路径仍不能在全源缺流时持续输出，不证明恢复成功。
- 最终 queued/workers/payload bytes为0，但payload objects为4，71836次申请/71832次释放；不得宣称资源完全归零，也不能只凭退出前遥测认定进程退出后的物理泄漏。errors/workerErrors为0、stalledIntervals为1。
- 433个CPU样本、22逻辑核：整机平均1.110764%/峰3.712871%，单核等效平均24.436810%/峰81.683168%。工作集106831872→190447616字节、峰195579904；热阶段193949696→194568192字节，未证明长期稳定。
- 118条generation1漂移记录的raw/filtered最大绝对值均为0。没有generation2恢复媒体证据。
- payload高水12081009字节/78对象、pressure failures为0。sender累计86344数据报/104842272 payload字节，deadline/pressure/partial/ambiguous失败均0；`delivery_evidence=not_proven`。MPEG-TS处理8702 AU，终止原因aborted。

后台额外进程采样：CPU13.3125→25.078125→30.125秒；工作集193949696→194568192→190464000字节；PrivateMemory404873216→405250048→402411520字节。采样只覆盖本轮，不能替代长期内存验收。

## 实际命令

以下执行 shell 在 CLI 和源命令后使用 `exit $LASTEXITCODE` 透传原生退出码。VLC启动命令返回0仅代表启动，不算播放进程自然退出证据。

### CLI

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-aggregate-r22 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60790 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60792 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61790 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-aggregate-r22.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-aggregate-r22-cli.log 2>&1
exit $LASTEXITCODE
```

### 首源

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60790?rtcpport=60791&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60792?rtcpport=60793&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-aggregate-r22-source.log 2>&1
exit $LASTEXITCODE
```

### VLC

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-aggregate-r22-vlc.log --extraintf=rc --rc-host=127.0.0.1:62790 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-aggregate-r22- --snapshot-format=png rtp://@127.0.0.1:61790
```

## 清理与风险

本轮文件清单：`composition-aggregate-r22-cli.log`、`composition-aggregate-r22-source.log`、`composition-aggregate-r22-vlc.log`、`composition-aggregate-r22.sdp`、`composition-aggregate-r22-2026-09-21-11h10m00s331.png`，均在 `out/acceptance/`。归档后已逐项删除；VLC通过本轮RC端口quit清理，不算自然结束。PID18836/9696/30620均已不存在，本轮前缀文件检查为空，指定120秒源仍存在。没有启动远程测试或生成抓包/临时录制媒体；远程只有adapter依赖只读调查，无本轮远程测试产物。

剩余门禁：逐源缺流/恢复完整状态机、未发布恢复代再次失锁、迟到输出的有界工作策略、贡献元数据物理预算、全局payload/canvas资源准入、实际生产帧池提前验证、公共入口、纯视频源、Windows→RKMPP原规格合屏矩阵。完整合屏仍FAIL，评分不因构建或首段画面上调。
