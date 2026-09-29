# 编码提交事务：Release r21 验证

## 实施与审查

发送前准备贡献候选，成功发送后 noexcept 提交；EAGAIN 先销毁候选再接收，保留 pending 输入并重新准备。map/priming/reset、编码器支持范围不变。原 submit 已复制 accumulator，本次移动提交边界，未新增 PCM 拷贝。工业状态机与驻留缺口见[调查记录](realtime-video-composition-encoder-retention.md)。

Release 全量 clean-first/all-target 构建成功，configure/build exit 0，两个 CLI 产物生成。两名未参与实现的审查者 composition_transaction_review_a/b 均明确 Standards/局部事务 Spec PASS；完整合屏 FAIL，就绪度维持42/100。它们核对 FFmpeg send/receive、锁、EAGAIN 重试和全部 accumulator 状态交换。局部源码结论不覆盖恢复或分配故障实测。

## r21：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30 fps、8 Mbps

完整恢复 **FAIL**。原120秒源，AAC输出CBR192 kbps、44100 Hz双声道。CLI PID7628自然exit1；首源PID33388自然exit0、3600帧/120秒；首源退出事件立即启动同源重入PID32356。VLC PID10996，实际查看10:06:12的1280×720截图，证明首段画面，不证明恢复。

10:07:33.043七组purge全部ack；重入generation2出现后，10:07:38.327 AudioEncode报 Audio encoder packet timeline is discontinuous。新代sender仅2数据报/400 payload字节，MPEG-TS access_units=0，未恢复媒体。严格校验保留，测试到此失败结束；随后按身份清理重入源与VLC，清理退出不算自然完成。

最终workers/queued/payload字节及对象均0，70506次申请/释放相等；errors/workerErrors=1/1、stalledIntervals=1。CPU421样本、22逻辑核，整机平均1.128166%/峰4.491726%，单核等效24.819645%/98.817967%。工作集70868992→195473408字节，峰198672384；运行中仍有增长，不证明长期稳定。payload高水14715197字节/398对象，pressure failures=0。

generation1漂移116条，raw/filtered绝对最大4408ns；generation2仅2条，最大6996ns，不能证明恢复持续同步。sender累计82972数据报/100760580 payload字节；deadline/pressure/partial/ambiguous均0，pacing取消1；源代次清理backlog取消17数据报/22488wire字节。delivery_evidence=not_proven。

额外10轮后台进程采样：CPU累计18.34375→29.484375秒，WorkingSet196800512→198615040字节，PrivateMemory429244416→430764032字节；另有早期样本CPU10.640625秒、WorkingSet195526656、PrivateMemory428306432。时间点覆盖有限，不能替代长期内存证明。持续CPU/内存/漂移以上述CLI telemetry为准。

未运行远程测试，未创建Windows测试脚本、抓包或额外FFmpeg监控。首源、重入、CLI命令末尾均由执行shell透传原生退出码。

## 实际执行命令

### CLI

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-transaction-r21 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60780 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60782 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61780 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-transaction-r21.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-transaction-r21-cli.log 2>&1
exit $LASTEXITCODE
```

### 首源

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60780?rtcpport=60781&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60782?rtcpport=60783&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-transaction-r21-source.log 2>&1
exit $LASTEXITCODE
```

### 重入源

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60780?rtcpport=60781&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60782?rtcpport=60783&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-transaction-r21-source-rejoin.log 2>&1
exit $LASTEXITCODE
```

### VLC

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-transaction-r21-vlc.log --extraintf=rc --rc-host=127.0.0.1:62780 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-transaction-r21- --snapshot-format=png rtp://@127.0.0.1:61780
```

## 清理与剩余风险

归档本节后，按本轮七项文件清单删除四份媒体日志、SDP、截图和构建日志。重入源清理后执行会话返回-1，不算自然结束；VLC通过本轮RC端口退出。PID7628/33388/32356/10996均已不存在，composition-transaction前缀产物检查为空，指定120秒源仍存在；未启动远程测试，无本轮远程产物。

事务仍有候选分配，EAGAIN 会重新准备；mapper驻留硬界及元数据物理预算未完成。错误音频delay事实、独立output origin/generation、持续聚合、黑帧/静音、多源与Windows→RKMPP验收仍待完成。本次不宣称AAC恢复修复，不创建成功验收提交，不提高评分。
