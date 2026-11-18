# 逐源视频规划与真实准备边界

## 本轮范围

2026-09-21，继续在 `feat/realtime-video-composition`，基线 `ffbf31a7`。从既有整链 planner 抽取 decoder/filter 源产品、公共候选、帧域匹配、评分和执行契约，使合屏逐源规划不再要求输出编码器意图。单源完整链继续使用相同公共逻辑。没有增加公共参数或修改外部 FFmpeg。

已实现独立源产品与单份后端候选声明；旧整链在相同源候选上追加编码器。相邻帧域匹配、阶段评分、decoder lineage/receive cadence 和 filter implementation 共用。新源候选不创建编码器，源目标必须与完整帧域描述匹配；帧率与 SAR 必须为正。

filter graph 配置与 readback 共用一个核心。新源协商要求显式首帧、时基、帧率、SAR 和源输出目标；旧整链 synthetic probe 保留自身证据边界，再核对编码器输入格式与尺寸。旧整链中的 synthetic pool/SAR 不是新源准备产品。两名未参与实现者均明确 Standards/阶段源码 Spec PASS，完整合屏交付仍 FAIL。Release 全量重建 configure/build 均0、636项，10个源码文件构建前后 SHA256 一致。

## 真实输入准备的源码约束

- prepared RTP 保留同一个 transport 和数据报队列；当前 replay 要求先 seal，读取会消费数据报、释放预算并映射回放时间。不能直接借用 replay 进行探测后再声称正式输入完整。
- 当前 decoder 的 `get_format` 回调引用 `CodecResolverNode` 成员。图外准备需一起保留 callback owner 与 context，不能返回引用临时对象的解码器。
- 生产首帧依赖 canonical lineage 与启动时钟。预解码得到 `AVFrame` 不等于可以直接发布；输入包、解码器待输出状态和首帧必须与同一启动事务衔接。
- 多源准备、解码器池、缩放池和画布必须按实际 owner 准入资源。现有单源总账不能通过给所有池套用同一个最大帧大小来替代。

[FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html) 要求按输入/输出状态机推进并保留引用所有权；本机 `libavformat/demux.c` 的 `avformat_find_stream_info` 保留 packet buffer 供后续读取，是探测后保留输入的成熟依据。[GStreamer preroll](https://gstreamer.freedesktop.org/documentation/additional/design/preroll.html) 的保留 buffer、统一发布和 flush 取消边界与既有启动事务职责一致。真实设备、输入所有权和预算仍须由本项目证明，不能由这些参考直接推定支持。

## 完成门禁

本步的 source plan 只表示候选规划；filter negotiation 只证明所提供帧的 graph 配置及 sink readback。没有真实解码、传输、同设备身份和物理预算证据时，不返回“源准备完成”。完整合屏仍须继续完成真实准备交接、唯一输出与画布预检查、逐域资源总账、公共入口以及 Windows→RKMPP 原规格验收。

## r24 原规格 Release 单源回归

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps，1280×720、30fps；AAC CBR192kbps、44100Hz双声道。指定连续120秒源，无循环或降规格；CLI PID31524，FFmpeg PID33240，VLC PID33284。结果：完整验收 **FAIL**。源自然退出0、3600帧、120.00秒，CLI自然退出1。VLC启动命令返回0仅表示启动；12:33:24截图已实际查看，1280×720游戏画面正常，不能代表完整播放或恢复验收。

- 12:32:51选择 `cuda-nvenc`、score3280，实际 `h264` CUDA解码、`scale_cuda=1280:720` 和 `hevc_nvenc` 打开成功。
- 12:34:50.008两路SenderLeft，12:34:50.013七组purge ack完成，old1/next2进入acquiring；12:34:55.292开始abort。最终错误 `realtime runtime made no progress before timeout`；编码包边46/47各10项满队列。未发送恢复源，无generation2恢复媒体证据。
- 末尾queued/workers/payload bytes均0，但payload objects为4；70444次申请、70440次释放。errors/workerErrors为0，stalledIntervals为1，payload高水15617370字节/361对象，pressure failures为0。不能声称资源完全归零或将逻辑对象数直接解释为进程退出后物理泄漏。
- 433个CPU样本、22逻辑核：整机平均1.049801%/峰5.191874%，单核等效平均23.095616%/峰114.221219%。工作集108015616→192434176字节，峰202149888；本轮不能证明长期稳定。
- 后台另采38个进程样本：12:33:24.399 CPU7.5625秒、WS199372800、Private427048960字节；12:34:48.604 CPU27.9375、WS202149888、Private428937216；12:34:54.634 CPU28.734375、WS192434176、Private421228544。
- 117条generation1漂移，raw最大绝对4793ns、filtered4792ns。sender提交82939数据报、100731944 payload字节，deadline/pressure/partial/ambiguous失败均0；取消1个pacing预约、18个backlog数据报，最终backlog0。`delivery_evidence=not_proven`，不得据此宣布网络服务曲线验收通过。

本轮仍运行Shared单源入口，没有运行新source-only入口或Preserve合屏域。源码双审和全量构建通过不替代合屏运行门禁；r22/r23的无进展与4对象问题继续有效。没有外部依赖修改、远程测试、抓包或临时测试脚本。

证据归档后，已逐项删除 `composition-source-planning-r24-cli.log`、`composition-source-planning-r24-source.log`、`composition-source-planning-r24-vlc.log`、`composition-source-planning-r24.sdp`、`composition-source-planning-r24-2026-09-21-12h33m24s101.png`，共5文件。VLC按精确PID及命令身份清理，清理退出不记自然结束；三个本轮PID均已不存在，前缀文件残留为0，指定120秒源保留。无本轮远程产物。

### CLI

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-source-planning-r24 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60800 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60802 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61800 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-planning-r24.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-planning-r24-cli.log 2>&1
exit $LASTEXITCODE
```

### FFmpeg 源

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60800?rtcpport=60801&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60802?rtcpport=60803&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-planning-r24-source.log 2>&1
exit $LASTEXITCODE
```

### VLC

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-planning-r24-vlc.log --extraintf=rc --rc-host=127.0.0.1:62800 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-source-planning-r24- --snapshot-format=png rtp://@127.0.0.1:61800
```
