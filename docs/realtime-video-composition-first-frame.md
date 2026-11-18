# RTP 真实首帧探测

## 范围与依据

2026-09-21，基线 `f148bdca`，继续合屏的图前准备。原始 RTP 快照已能保留正式回放，本步增加消费快照的实际预解码和独立准备暂存预算。没有改变外部 FFmpeg，没有新增公共配置；仍须接入完整 composition preflight，不能把内部证据产品等同合屏交付。

复用现有 RTP parser、reorder、NAL/depacketizer、参数集 observer、decoder codec API 与帧合同校验。[FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html) 要求 send EAGAIN 时推进 receive、保留尚未接受的 packet；探测不能用 null packet 强制 drain。[GStreamer RTP H.264 depayloader](https://gstreamer.freedesktop.org/documentation/rtp/rtph264depay.html) 的完整随机访问与丢失后等待关键帧语义对应既有 continuity/depacketizer 实现：快照首个未证明 timestamp 必须舍弃，marker 本身不能证明前缀完整。

## 准备预算与等待

- 从原 raw budget 抽取固定容量的 retain/release 计数核心，保留原 observed wire、首错、probe/seal 状态。新增准备 storage budget 使用同一计数操作，move-only lease 持有预算，实际媒体所有者应先销毁再归还额度。
- raw `probe-size` 仍约束原始预读与其快照副本。parser/reorder/AU/decoder 输入及帧的准备暂存由独立 typed plan 从协议上界、实际 decoder retention 和同时持有关系推导。不能给各源重复一份未经汇总的额度，也不能把 MaxCPB 强行裁成预读容量。
- 预算只描述明确的 engine logical payload/header 及准备存储合同，不等于进程 RSS、机器可用 RAM或全部 FFmpeg/driver 内存。现有 graph 总账也是推导的合同；尚无主机可用 RAM 的准入保证。跨源准备峰值与运行总账仍需在组合 preflight 汇总。
- prepared raw 输入现在保留原 open/analysis 截止时间的较早者。等待接口只在原 datagram 数量实际推进时返回成功，调用前必须释放旧快照；取消、采集结束、原截止时间到期与粘性预算错误均明确返回。采集线程结束会标记完成并唤醒，不把 joinable 当作线程仍在收包的证据。

## 首帧消费者

`MediaRtpSourceFirstFrameProbe` 接收只读快照、源规划事实与同一个 opened decoder。先核对 codec parameters/extradata 与预备参数集，再以原到达时间驱动两次相同 reorder 过程：首遍验证源身份和完整参数集，次遍通过既有 depacketizer 取完整随机访问 AU 并解码。它不创建生产 RTCP clock、gate 或 canonical token，不消耗原 replay。

send 接受后才释放 pending packet；通用 send/receive 双 EAGAIN 返回错误。已有 RKMPP typed receive interval 才允许可取消的节拍等待，始终受原 deadline 限制，不发送 null drain。真实首帧检查损坏标志、帧合同、SAR/color 和实际设备引用。暂未取得首帧时返回明确 `NeedMoreEvidence`，携带已提交 AU 数及 decoder 仍可能待输出的状态，不能盲目重提相同输入。

结果持有原 decoder、首帧和准备 reservation，销毁媒体对象后才归还预算；禁止默认 move assignment 提前释放旧额度。滤镜协商与真实 copy 仍由上层对该实际 frame 调用共用能力步骤，消费者本身不声称完成滤镜执行或完整 source ready。

## 保留边界

首帧探测结果不得发布生产 epoch/token。真实 frame 用于能力证据后，原始 datagrams 仍须沿原 clock/gate/canonical 路径回放；任何重复提交同一前缀前都必须释放探测首帧并明确 reset/flush 同一个 decoder。源池地址可变化，实际设备/格式/几何与 engine 持有上界须验证；唯一输出与画布仍需保留自己的实际准备产品。

完整准备事务、公共合屏入口、按 owner 全局资源准入、Windows→RKMPP 2～4源失活/恢复/持续黑场静音矩阵尚未完成。原 Shared 单源无进展超时、4逻辑对象残留及 AAC 重入失败未据此宣布修复。

## 2026-09-28 修复与复审

独立审查发现实际硬件 surface 布局遗漏；已从既有输出分支抽取共用硬件 frames 校验，由首帧与输出能力调用，核对 format/sw_format、device 引用及分配尺寸，不要求池地址不变。冻结18个源码文件后两位审查者 Standards/阶段 Spec 均 PASS；完整合屏仍 FAIL、42/100不变。

修复后的第一次 Release 全量重建触及120秒截止，脚本终止构建进程树，exit1；不能引用修复前的成功结果代替当前构建。同一全量流程复验结果见下文。

## r26 Release 原规格共享链路回归

2026-09-28。RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps，1280×720、30fps，AAC CBR192kbps、44100Hz双声道。指定连续120秒源，无循环/降规格/额外FFmpeg监控。CLI PID23468，FFmpeg PID3572，VLC PID29296。结果与清理记录见下文。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-first-frame-r26 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60910 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60912 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61910 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-first-frame-r26.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-first-frame-r26-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60910?rtcpport=60911&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60912?rtcpport=60913&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-first-frame-r26-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-first-frame-r26-vlc.log --extraintf=rc --rc-host=127.0.0.1:62910 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-first-frame-r26- --snapshot-format=png rtp://@127.0.0.1:61910
```

### 结果与诊断

完整验收 **FAIL**。修复后的第二次全量 Release 重建成功，configure/build exit0，18个源码文件的构建前后SHA256一致；首次120秒构建超时记录保留。

- 09:15:13.167选中cuda-nvenc（score3280），h264 CUDA decode、scale_cuda=1280:720、hevc_nvenc实际启动。09:16:04实际查看VLC截图1280×720游戏画面。VLC使用D3D11VA，但有19条too late记录（含音频迟到及flush、视频最大missing468ms），另有faad zero sample和D3D11 surface/slices警告，不能记播放门禁通过。
- FFmpeg完整3600帧、120.00秒自然exit0。09:17:12.329/.330两路source unavailable reason1；.332 purge ack_count7完成，old1/next2进入acquiring；09:17:17.673 abort。CLI自然exit1：NotInitialized: realtime runtime made no progress before timeout。边46/47各10项满队列。没有恢复源，不能声称generation2媒体恢复。
- 最终queued/workers/payload bytes均0；payload objects4，71884次reservation、71880次release；高水12028190字节/93对象，pressure failures0、errors/workerErrors0、stalledIntervals1。逻辑对象残留不等于进程退出后的物理泄漏。
- CPU425样本、22逻辑核，整机平均1.181514%/峰3.640777%，单核等效平均25.993303%/峰80.097087%。CLI报告工作集108195840→192708608字节、峰197738496。
- 后台另采27点：09:15:50.602 CPU9.234375秒、WS195686400、Private397860864；09:17:11.507 CPU31.65625、WS197746688、Private399331328；09:17:17.541 CPU32.5625、WS192708608、Private394625024。采样窗口及峰值与CLI略不同，不证明长期稳定。
- 118条generation1漂移，raw/filtered最大绝对值均20272ns。sender提交86240datagrams、104718936payload字节；deadline/pressure/partial/ambiguous失败0，pacing取消1、purge取消backlog21包/27536wire字节，最终backlog0。delivery_evidence=not_proven，不判网络服务曲线通过。

本轮只运行旧Shared单源，不覆盖新probe消费者、Preserve合屏、跨源总预算或RKMPP。旧无进展/4对象问题仍复现，AAC重入未验证。没有成功验收提交。

### 本轮清理

必要命令和结果先归档。将按已核对PID29296清理VLC；CLI23468和源3572已自然退出。清理退出不记自然结束。逐项移除composition-first-frame-r26-cli.log、composition-first-frame-r26-source.log、composition-first-frame-r26-vlc.log、composition-first-frame-r26.sdp、composition-first-frame-r26-2026-09-28-09h16m04s121.png；无本轮远程测试、抓包或临时脚本，保留指定120秒源。执行后残留核对另记。

清理已执行：5个文件删除，本轮前缀文件0、上述3个PID残留0，指定120秒源存在。VLC强制清理不记自然退出。
