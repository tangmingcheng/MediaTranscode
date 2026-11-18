# startup成员合同

## 实现与依据

基线e211bf89。按[GstAggregator实际pad集合与有序buffer/control模型](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)及既有成员计划，复用同一startup节点和状态机。该资料是成员/事件职责对照，不是本项目coverage窗口算法与GStreamer等价的证明；本轮保留原AV窗口选择、keyframe及连续preroll覆盖和trim，不引入另一个窗口算法。

- config冻结members，完整可选AudioConfig包含trim/skew、容量/字节/maxunit、identity及outputSampleRate；AV当且仅当音频产品存在，V不分配audio store。移除Preparation/Node独立sample-rate字段，release从同一audio子产品建立origin，V没有origin。
- WindowSelector复用视频候选与coverage索引；V同样满足keyframe和正preroll覆盖后选择video PTS，AV执行原音频交集/trim。coordinator的lock/EOF/purge/work统计只包含实际成员，未知stream与V音频unit/EOF失败。
- factory preparation核对members、必需端口和完整音频options，V拒绝音频端口/任一音频字段；node只读取实际成员，clock/terminal的有限snapshot barrier不等待不存在audio。start和release对照冻结members与注册group plan；等待新证据前也检查输入stream。
- 现有AV builder明确传members，common-core shape核对计划；最终资源账本按同一members统计startup batch容量，V不读取audio capacity，不以缺字段当零值fallback。未完成的prepared/planner/source图及多owner总准入限制仍保留。

线程、队列硬界、背压、代际退休、单次deadline、发布RAII和终态清理保持同链。Windows/Linux共享core与runtime，无平台专用链或新增公共参数。当前完整V源planner/builder入口仍关闭，不能把内部V形状能力当作可运行纯视频或合屏。

## 验证

独立设计与11源码双人Standards/阶段Spec审查PASS。Release全量重建session59478完成666项，configure/build exit0；CLI产物2026-09-29 12:00:22、5178880bytes。未测试local。

## r54：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30fps、8Mbps

输出AAC CBR192kbps、44100Hz双声道。CLI PID6436/session37825，源PID30276/session89551，VLC PID15400，外部5秒监控session67033。实际命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-startup-members-r54 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-startup-members-r54.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-startup-members-r54-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-startup-members-r54-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-startup-members-r54-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-startup-members-r54- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 结果：FAIL

指定源3600帧/120秒自然exit0；CLI自然exit1/no-progress。12:01:27.789首release video16/audio22、首commit audio22，走本轮统一AudioConfig和origin产品；12:03:26.787 purge7ack完成old1/next2 acquiring，12:03:32.063至32.145 abort。没有下一代媒体，也未运行VideoOnly或多源。

edge46/47各10/10，abort前20queued/24objects/281635bytes；最终queued/workers/payloadBytes为0、4逻辑对象（71844 reservations/71840 releases）。高水12254772bytes/91objects，workerErrors/errors/pressureFailures均0、stalledIntervals1；AAC关闭2帧残留，完整退出门禁FAIL。

400次CPU采样、22核，进程整机口径均值1.277437%、峰3.579418%，单核口径28.103610%/78.747204%。runtime WS初始111357952、最终188272640、峰193089536bytes。外部20次5秒样本12:01:56.493至12:03:31.802，CPU累计9.515625→35.375s；WS190935040→188272640、峰193089536；Private400089088→398106624、峰401014784bytes。短窗口不能证明长期无增长。

118条generation1漂移raw/filtered绝对最大156ns，不代表恢复或播放端同步。sender86215datagrams/104682972payloadBytes，deadline/pressure/partial/ambiguous和pacing取消均0，delivery_evidence=not_proven。

实际查看1280×720游戏截图composition-startup-members-r54-2026-09-29-12h02m18s012.png；VLC记录playback too late93500及Failed to create video converter，不能证明连续无丢帧。不创建成功验收提交，完整合屏FAIL42/100。

### 清理清单

先归档本页再核对VLC15400身份清理；CLI6436与源30276已自然退出。逐项删除本轮-cli.log、-source.log、-vlc.log、.sdp及上述PNG五文件；保留指定124427809bytes连续源及正式构建产物。本轮无远程运行、抓包、临时录制或测试脚本。VLC清理不计自然退出。

清理复核：上述五文件删除、三个本轮PID均无残留，固定源保留。
