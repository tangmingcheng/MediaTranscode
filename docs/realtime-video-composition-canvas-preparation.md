# 合屏 canvas 图像 payload 准备

基线874e4e83。本阶段把预算与原请求 deadline/cancellation 接入实际 canvas 准备函数；不是完整资源总账或公共合屏接通。

## 依据与合同

沿用[GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)先协商再分配、引用归还后复用的生命周期。拓扑根据同一最终图的数量合同及实际生产池 allocation readback，生成不可修改的准备产品。长期图像 payload 为全部可写 surface 加固定 black 的实际字节总和；临时 payload 为同时存在的 upload/download 两份 staging，几何字节通过 FFmpeg av_image_get_buffer_size 核对，乘加及 size_t/int 转换均检查。不得以宽高猜测硬件分配。

预算只覆盖 canvas 持有期间已公开的图像 payload；不含 AVFrame/AVBufferRef 头、metadata、adapter、控制块、pool 缓存、初始 readback、源帧与驱动私有分配。[FFmpeg AVHWFramesContext](https://ffmpeg.org/doxygen/trunk/structAVHWFramesContext.html)的池依赖硬件后端，不提供统一物理内存上限；不能把本预算称为完整准备授权、RSS 或设备硬上限。

## 生命周期与失败

prepare 先申请长期与临时两个租约，任一个失败都在平台分配前退出。长期租约由 Prepared、Producer 和发布帧的 HeaderLease 共同持有；编码器 clone 共享该 lease，producer 先销毁也不会过早归还。预算成员先声明、最后析构。临时租约在两份 staging 已销毁后释放。异常、取消、readback/实际池数量不匹配及平台错误均 RAII 回滚。

复用 MediaPreparationControl，在同步平台操作前后及发布准备结果前检查原 deadline/stop；源首帧探测改用同一检查，原错误类型和文本保持。没有新增线程、队列、后台操作或 fallback；预算不足立即失败，运行期池耗尽继续既有 wouldBlock 与引用释放唤醒。同步驱动调用不可由这些检查强制中断，取消只在调用边界生效；不能宣称硬实时终止。

Windows CUDA 与 RKMPP/RGA 共用数量、预算、所有权及取消逻辑，adapter 不变，外部 FFmpeg 不变。初始生产池/首个 readback 的预算事务、多源 allocation 总账及公共调用者仍缺；final compiler 继续拒绝不完整 aggregate 合同，完整合屏维持 FAIL、42/100。

## 验证

11源码冻结后双独立Standards/阶段Spec PASS，17文件UTF-8/CRLF及diff检查通过。Release全量session8865成功（660项构建图），configure/build均exit0；CLI2026-09-29 09:00:11、5134336bytes。只运行realtime，未运行local。


## r46实际realtime链路

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps；AAC CBR192kbps、44100Hz双声道。固定120秒源。CLI PID28288/session42807，源PID12468/session32642，VLC PID5596（启动exit0），5秒监控session98040。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-canvas-preparation-r46 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-canvas-preparation-r46.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-canvas-preparation-r46-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-canvas-preparation-r46-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-canvas-preparation-r46-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-canvas-preparation-r46- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 结果

源完整3600帧/120.00秒自然exit0；CLI自然exit1/no-progress。09:03:10.071 purge7ack complete、old1/next2 acquiring；15.403 abort.begin、15.438 abort.done，edge46/47均10/10。final queued/workers/payloadBytes=0，但4逻辑对象（reservations71873/releases71869），高水12069764bytes/104objects；workerErrors/errors/pressureFailures=0，stalledIntervals=1。旧退出问题未解决，不能记为通过。

403CPU采样/22核：整机口径进程均值1.279905%、峰3.773585%；单核均值28.157905%、峰83.018868%。runtime WS初始139366400、最终188334080、峰193220608bytes。5秒监控21采样/exit0，WS峰193507328、Private峰414048256bytes；首09:01:33 CPU7.828125s/WS191795200/Private413143040，末09:03:13 CPU35.03125s/WS188334080/Private409149440。

118条generation1漂移raw/filtered绝对最大156ns，不能证明恢复。sender提交86089datagrams/104533260payloadbytes，deadline/pressure/partial/ambiguous=0；pacing_cancelled=1，purge取消backlog21datagrams/27536wirebytes，delivery_evidence=not_proven。VLC截图composition-canvas-preparation-r46-2026-09-29-09h01m57s128.png已观察到1280×720游戏画面；有playback too late145881、way too late530664、画面迟到21–356ms及Failed to create video converter。

本轮仍为旧共享路径，不能证明尚无公共调用者的新canvas准备或多源合屏运行。完整FAIL42；原no-progress/4对象、AAC重入及全矩阵未解决。

### 清理清单

先归档上述命令与证据，再核验并清理VLC PID5596（不计自然退出）；CLI28288和源12468已自然结束。逐项清理composition-canvas-preparation-r46-cli.log、-source.log、-vlc.log、composition-canvas-preparation-r46.sdp及上述PNG。无远程、pcap或临时录制，本轮未生成测试脚本；指定复用源124427809bytes保留。清理复核完成：5文件及3个精确PID均无残留，指定源大小确认保留。
