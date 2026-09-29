# 合屏输出音频 FIFO 规划

## 设计与证据

基线c5878f36。现有MediaAudioEncoderFifoRetentionPlan同时推导源校正块上界和编码FIFO容量，要求源servo；连续输出域禁止携带源servo，因此不能直接形成独立输出产品。

复用原有推导：源校正的最大块仍取max(resamplerBlock, maximumCompensationDistance)，移入已负责校正时序的MediaAudioCorrectionReachabilityPlanner结果；编码FIFO只消费prepared输出格式和明确最大输入samples，保留maximumInput + frameSamples - 1及原字节/fragment检查。旧runtime planner和validator消费同一校正产品，不保留旧适配入口。aggregate每次buildAudio明确输出codecFrameSamples，合屏builder在构图前用这一真实块大小核对输出FIFO产品。

[FFmpeg Audio FIFO](https://ffmpeg.org/doxygen/trunk/group__lavu__audiofifo.html)说明write在空间不足时自动扩容。初始分配不等于硬上界；本项目AudioEncoderFrameQueue::push在write之前验证queued小于一帧、输入samples、总samples/bytes/fragments，因此保留该检查并沿用原上界公式。源校正使用原MediaAudioCorrectionQuantizer，不更改算法、经验值或补偿策略。

不新增线程、队列、公共参数或平台链；已有FIFO/帧RAII所有权、背压、失败传播不变，缺失/非正/溢出事实仍失败。此项仅解除输出容量对源servo的耦合，完整输出准备、资源总账和公共多源入口仍未完成。双审、构建与原规格媒体证据按实际补充，不以此项推导证明合屏验收。

## 执行记录（2026-09-28）

6源码冻结后双独立Standards/阶段Spec PASS，无新增阻断；报告audio-fifo-review-a/b.md。首次Release全量session38618达到120秒截止并终止进程树、exit1，确认无编译残留后同入口重试。第二次session8061 configure/build exit0，647项构建图，两CLI12:51:56；冻结后源码逻辑未改变。

### r36：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps、AAC CBR192kbps

固定120秒源，分别直接执行：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-audio-fifo-r36 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61010 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61012 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62010 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-audio-fifo-r36.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-audio-fifo-r36-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61010?rtcpport=61011&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61012?rtcpport=61013&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-audio-fifo-r36-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-audio-fifo-r36-vlc.log --extraintf=rc --rc-host=127.0.0.1:63010 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-audio-fifo-r36- --snapshot-format=png rtp://@127.0.0.1:62010
```

FFmpeg PID32072/session75133自然exit0，3600帧/120.00秒；CLI PID16236/session47578自然exit1。VLC PID34064启动命令exit0；实际查看12h53m12s685截图有1280×720游戏画面，日志有Failed to create video converter。未强制结束CLI，没有新增FIFO容量错误，但不能将画面或首段输出记作通过。

12:54:39.989 purge_ack=complete、7ack、old1/next2、acquiring；12:54:45.027 abort.begin，最终realtime runtime made no progress before timeout。edge46/47均10/10，最终queued/workers/payloadBytes=0，逻辑对象4，reservations71926/releases71922，高水12075066bytes/77objects，workerErrors/errors/pressureFailures=0；最终stalledIntervals=0，与明确失败及stalled edge记录一起保留，不据此否定无进展错误。完整验收FAIL，无成功验收提交。

421个CPU采样/22逻辑核：整机均值1.191376%峰2.970297%，单核26.210262%/65.346535%；WS初始104771584、最终191492096、峰196354048bytes。后台5秒监控session16112 exit0：12:53:01 CPU5.65625s/WS193781760/Private395374592，12:54:41 CPU32.328125s/WS191492096/Private393043968，监控Private峰396632064。118条generation1 drift raw/filtered绝对最大156ns，无恢复后持续媒体证据。sender提交86345datagrams/104842340payloadbytes，deadline/pressure/partial/ambiguous/pacing_cancelled均0、delivery_evidence=not_proven。

未运行local、其他输出协议、动态输出、RKMPP或多源生产矩阵；本轮共享单源回归不证明合屏入口。完整合屏仍FAIL/42，后续source/output计划形成和资源事务继续按原计划，不修改外部FFmpeg。

清理清单：CLI/source/VLC三日志、SDP、composition-audio-fifo-r36-2026-09-28-12h53m12s685.png共5项。归档后核对VLC身份按精确PID34064清理并逐项删除。本轮无抓包及远程产物，固定源保留；主动清理VLC不计自然退出。

清理复查：上述5项、本轮前缀和媒体/编译进程残留均为0，固定源存在。
