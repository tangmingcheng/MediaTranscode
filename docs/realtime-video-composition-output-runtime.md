# 合屏输出 runtime 与发送段边界

## 设计与证据

基线3a539650。CompositionGraphOptions原输出持有继承源合同的MediaRealtimeAvSyncRuntimePlan，包括源assembly、input、transition及correction；实际输出调度/发送仅消费group、边策略、音频分支模式和协议产品。独立输出不应需要伪造源字段。

参考[GStreamer clocks](https://gstreamer.freedesktop.org/documentation/additional/design/clocks.html)由pipeline选择并分发时钟，以及[Aggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)集中输出并保持事件顺序。当前改动只落实合同边界：独立MediaRealtimeAvOutputRuntimePlan拥有已解析输出音频、输出同步域、队列及协议；旧单源与合屏共用借用型MediaRealtimeAvOutputSegmentPlan及原三个调度/发送segment。view仅在同步构图栈内存在，节点不保存引用；protocol产品移动进runtime binding发生于最后一次使用之后。

不修改线程、批次、背压、队列硬界、时间线算法、RAII或平台adapter；缺少合同继续在构图校验失败，无fallback。ContinuousOutput域仍通过既有validateDomain拒绝源时钟/校正字段，音频输出仍与aggregate格式/FIFO精确匹配。此拆分不是独立输出planner或多源生产入口完成；下一步仍需真实输出政策/资源产品形成与协调器，不能用单源回归代替多源门禁。

## 验证状态

11源码冻结后双独立Standards/阶段Spec均PASS（output-runtime-review-a/b）。Release全量三次session63705、86252、12946均configure成功后达到120秒截止、exit1，构建入口终止进程树；最后确认cmake/ninja/cl无残留。实时CLI未生成，不能宣称本轮构建通过或运行测试。第三次末段仍在编译；未取得完整构建输出，不推断不存在编译错误。

用户随后明确授权超时上限改为300秒，脚本及技能文档同步更新，双审追加复审PASS。第四次session17102全量configure/build exit0，647项构建图、最终646链接，实时CLI更新于13:29:44。前三次失败记录保留。遵照用户最新要求，不运行local CLI。

### r38实际命令

FFmpeg PID35544/session41581自然exit0、3600帧/120.00秒；CLI PID38600/session31340自然exit1，最终无进展超时，未强制结束。VLC PID32816启动命令exit0，实际查看13h31m10s849截图显示1280×720游戏画面；日志有playback迟到68214、picture迟到21ms及Failed to create video converter，完整验收FAIL。

13:32:29.839 purge_ack=complete、7ack、old1/next2进入acquiring；13:32:35.268 abort.begin，13:32:35.301 abort.done。edge46/47均10/10；最终queued/workers/payloadBytes=0，逻辑对象4，reservations71863/releases71859，高水11994742bytes/93objects，workerErrors/errors/pressureFailures=0、stalledIntervals=1。387个CPU样本/22核：整机均值1.628505%、峰5.617978%；单核均值35.827103%、峰123.595506%；WS初始101330944、最终189222912、峰193966080bytes。后台5秒采样session82571 exit0：13:30:49 CPU7.656250s/WS192753664/Private405053440，13:32:34 CPU45.015625s/WS189222912/Private401383424；Private采样峰405053440。118条generation1 drift raw/filtered绝对最大156ns，不证明源恢复。

sender提交86256datagrams/104738080payloadbytes；deadline/pressure/partial/ambiguous=0，pacing_cancelled=1，delivery_evidence=not_proven。没有新多源入口/纯输出runtime产品形成的运行证据；此回归只覆盖旧共享生产链，不等于合屏验收。

先记录后按清单清理3日志、SDP和composition-output-runtime-r38-2026-09-28-13h31m10s849.png共5项；核对VLC命令行后按PID32816清理，主动清理不计自然退出。固定源保留；本轮无抓包、临时录制或远程产物。完整合屏仍FAIL/42。

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps、AAC CBR192kbps/44100Hz双声道，固定120秒源。分别直接执行：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-output-runtime-r38 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-runtime-r38.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-runtime-r38-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-runtime-r38-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-runtime-r38-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-output-runtime-r38- --snapshot-format=png rtp://@127.0.0.1:62020
```

清理复核：本轮5项文件已删除，指定源保留；CLI/源均自然退出，VLC按核对身份的PID清理。
