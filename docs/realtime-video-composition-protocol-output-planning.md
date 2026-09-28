# 合屏协议输出规划

## 范围与依据

基线e9aa5fed。唯一输出资源准备仍受最终拓扑总账约束：现有总账不接受aggregate节点，合屏builder又要求prepared canvas，不能借经验池或重复临时DAG绕过。先按原协调器计划拆出已有A/V协议输出规划，解除其对单源decoder、输入clock及完整runtime的接口依赖。

[RFC3550第7节](https://www.rfc-editor.org/rfc/rfc3550#section-7)区分输入贡献源和混合器输出身份；输出会话必须有自己的协议产品。[GStreamer allocation](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)先协商pool与容量再激活；[FFmpeg AVHWFramesContext](https://ffmpeg.org/doxygen/trunk/structAVHWFramesContext.html)固定池必须在初始化前确定大小，动态池的initial_pool_size不等于内存硬上界。因此本轮不伪造生产池。

提取MediaRealtimeAvProtocolOutputPlanner，沿用原RTP/RTCP、SDP、MPEG-TS cadence/batching、datagram规划算法；输入仅输出group、布局/传输、已规划协议、cadence、队列字节边界、prepared emission和deployment。单源保留assembly/correction/transition/FIFO与原groupKey，并消费同一协议输出产品。所有权仍由输出draft移动交接，错误原样传播；不新增线程、队列、后台任务、平台链或公共参数。独立入口须明确拒绝缺少A/V emission或group身份，不能依靠原完整源调用者间接保证。

## 验证边界

冻结后双独立审查、Release全量与原规格真实CLI回归按实际结果记录。该提取不等于合屏生产入口完成，唯一资源事务、源/输出runtime、跨源总账、纯视频源和Windows→RKMPP矩阵仍未通过。

## 执行记录（2026-09-28）

三源码冻结后，两名未参与实现者均Standards/阶段Spec PASS、无新增阻断。首次Release全量configure/build exit0，647项构建图完成，实时CLI12:04:32；3源码hash与冻结一致。构建不代表媒体验收。

### r33：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps、AAC CBR192kbps

以下为实际分别执行命令，固定120秒源未降规：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-protocol-output-r33 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60980 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60982 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61980 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-protocol-output-r33.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-protocol-output-r33-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60980?rtcpport=60981&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60982?rtcpport=60983&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-protocol-output-r33-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-protocol-output-r33-vlc.log --extraintf=rc --rc-host=127.0.0.1:62980 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-protocol-output-r33- --snapshot-format=png rtp://@127.0.0.1:61980
```

源PID33020/session18341自然exit0，3600帧/120.00秒；CLI PID11244/session62106自然exit1，未强制结束。VLC PID388启动命令exit0，截图composition-protocol-output-r33-2026-09-28-12h05m54s496.png实际检查有1280×720画面；存在playback too late(111460)、picture迟到69/35ms和Failed to create video converter。画面不能替代完整门禁。

12:07:19.126 purge_ack=complete、7ack、old1/next2、acquiring；12:07:24.420 abort.begin，最终realtime runtime made no progress before timeout，edge46/47均10/10。最终queued/workers/payloadBytes=0，逻辑对象4，reservations71979/releases71975，高水12138594bytes/78objects；workerErrors/errors/pressureFailures=0，stall1。完整链路FAIL，无成功验收提交。

425个CPU采样/22逻辑核：整机口径平均1.227165%峰4.301075%，单核26.997636%/94.623656%；WS初始108572672、最终188596224、峰193155072bytes。后台5秒监测session78514自然exit0：12:05:43 CPU7.203125s/WS190570496/Private395259904，12:07:23 CPU33.75s/WS189276160/Private393543680。118条generation1 drift的raw/filtered绝对最大均156ns；无恢复后连续媒体证据。

sender提交86346datagrams/104842408payloadbytes，deadline/pressure/partial/ambiguous/pacing_cancelled均0，delivery_evidence=not_proven。未证明端到端送达。本轮仅改变realtime协议planner，未重复local；Separate RTP、UDP、动态输出、RKMPP及多源矩阵均未运行，不得标记通过。

清理清单：CLI/source/VLC三日志、SDP、上述PNG共5项；先归档本节，再核对VLC身份按PID清理并逐项删除。本轮没有抓包或远程产物，固定120秒源保留。

清理复查：上述5项文件已删除，r33前缀文件和本轮媒体/编译进程均无残留，固定源存在。VLC为结果记录后的主动清理，不计自然退出。
