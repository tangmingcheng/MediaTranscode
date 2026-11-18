# 独立输出同步政策规划

## 实施设计

基线087baf55。MediaAvSyncPlanner把源startup/clock/servo与输出master/video/metrics/RTP identity/MPEG-TS timing混合生成，独立输出runtime没有可调用的政策生产者。将现有输出规划提取到MediaAvOutputSynchronizationPlanner，输入仅为媒体身份、输出layout/transport、已准备视频cadence、已解析音频rate、deployment及TS编码事实。输出直接满足ContinuousOutput域校验，不需要虚构源clock、startup容量或servo。旧单源planner立即调用该生产者，然后加入真实源政策并形成SharedSourceOutput，避免新旧实现分叉。

依据[GStreamer同步设计](https://gstreamer.freedesktop.org/documentation/additional/design/synchronisation.html)和[时钟设计](https://gstreamer.freedesktop.org/documentation/additional/design/clocks.html)，共同单调时钟和buffer映射是分离的职责。此次复用原政策赋值及协议时序算法，不把GStreamer当作既有经验阈值的权威证明：原20/40/80ms等阈值原值搬移，后续仍需独立政策依据收口，不宣称已经达到工业等价。纯输出按内部带代次控制使用RequiredExact；旧源根据真实协议覆盖为原来的控制模式。

线程、批次、背压、队列硬界、RAII和平台adapter不变。缺少输出事实或政策校验失败直接返回错误；无fallback、无新增公共参数。独立源runtime、输出runtime完整装配、资源事务和多源协调器仍待接通；单源回归不能替代原规格2–4源和Windows→RKMPP门禁。

## 验证

3源码冻结后双独立Standards/阶段Spec均PASS（output-sync-review-a/b）。已核对旧三种源clock、两种audio模式、所有政策赋值和控制代次覆盖；无新增阻断。Release全量session34890 configure/build exit0，648项构建图，实时CLI更新于2026-09-28 13:42:29。本轮不运行local。

### r39：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps、AAC CBR192kbps

FFmpeg PID29916/session84374自然exit0，3600帧/120.00秒；CLI PID33992/session78267自然exit1，无进展超时，未强制结束。VLC PID36136启动命令exit0，已实际查看13h43m57s283截图为1280×720游戏画面；日志有playback迟到115021、picture迟到72/38ms及Failed to create video converter，完整验收FAIL。

13:45:15.133 purge_ack=complete、7ack、old1/next2进入acquiring；13:45:20.493 abort.begin，13:45:20.527 abort.done。edge46/47均10/10，最终queued/workers/payloadBytes=0，逻辑对象4，reservations71882/releases71878；高水12146701bytes/79objects，workerErrors/errors/pressureFailures=0、stalledIntervals=1。

423个CPU采样/22核：整机均值1.139529%、峰3.773585%；单核25.069644%/83.018868%。WS初始106209280、最终189415424、峰194322432bytes。后台5秒监控session99400 exit0：13:43:36 CPU5.421875s/WS191434752/Private397590528；13:45:16 CPU31.031250s/WS189415424/Private394526720；Private采样峰399466496。118条generation1 drift raw/filtered绝对最大156ns，不证明恢复。sender提交86342datagrams/104842136payloadbytes，deadline/pressure/partial/ambiguous/pacing_cancelled=0、delivery_evidence=not_proven。

本次单源生产路径调用新输出同步producer，但没有运行独立输出域的多源协调器、其他输出协议或RKMPP。旧无进展及AAC重入问题未宣布修复；完整合屏仍FAIL/42。

先归档后按清单清理3日志、SDP及composition-output-sync-r39-2026-09-28-13h43m57s283.png共5项，核对命令行后按VLC PID36136清理；主动清理不计自然退出。指定120秒源保留，本轮无抓包、临时录制或远程产物。

固定120秒源、44100Hz双声道，分别直接执行：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-output-sync-r39 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-sync-r39.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-sync-r39-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-sync-r39-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-sync-r39-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-output-sync-r39- --snapshot-format=png rtp://@127.0.0.1:62020
```

清理复核：本轮5文件已删除，CLI/FFmpeg/VLC及编译进程均无残留，指定源保留。
