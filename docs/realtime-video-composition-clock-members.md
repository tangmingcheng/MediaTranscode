# RTP源时钟成员合同

## 实现与行业对照

基线7c13fe52。遵循[RFC3550的独立session与SR关联模型](https://www.rfc-editor.org/rfc/rfc3550.html)和[GStreamer rtpbin的请求session成员模型](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpbin.html)，沿已批准成员计划复用同一validator、mapper及节点，不建纯视频专用媒体链。

- builder把原plan.members显式传至RTP clock group及source state adapter；snapshot保留成员，统一校验成员/状态/音频calibration形状。binder对照注册group plan，projector拒绝不存在成员和非法stream，adapter在有损投影前验证成员。
- validator config把音频CNAME期限与跨流skew形成完整可选子产品，AV当且仅当产品存在。单成员仍执行自身SR、SSRC、时效及generation状态机；ActiveGeneration身份检查不再以音频存在为前提。仅实际AV执行跨成员比较，common epoch取实际成员最早SR源时间。
- node按计划成员轮询原有端口；V无audio mapper，拒绝audio options/channel。保留有限处理批次与invalidation优先级；图级校验核对成员一致、原始输入/binder数量及binder流种类覆盖。
- 现有PlannedStreamPair创建者写require_matching_cname=false，沿用该政策，不把它描述为CNAME匹配/新鲜度已验证。requireMatchingCname=true时单成员依然校验自身CNAME及期限，仅跨成员比较无需执行。后续完整V源规划不能借空音频或超时自行降级成员。

线程、队列上界、背压和RAII不变，无新平台实现；Windows/Linux共用protocol/runtime。固定大小snapshot增加成员标记和optional判别，不新增动态队列；既有CNAME vector存储保留，不能据此声称全局metadata分配准入完成。

当前生产builder入口仍要求AV，纯视频prepared→startup→builder/resource垂直链未完成，不提前放开V入口。该阶段移除源时钟层固定双calibration假设，完整纯视频或多源能力仍FAIL；不修改公共参数或外部FFmpeg。

## 验证

独立设计审查及11源码双审Standards/阶段Spec PASS。Release全量重建session76181成功，666项、configure/build exit0；CLI产物2026-09-29 11:40:48、5174784bytes。未测试local。

## r53：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30fps、8Mbps

输出AAC CBR192kbps、44100Hz双声道。CLI PID31700/session23654，源PID7724/session7343，VLC PID8244，外部5秒监控session52497。实际命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-clock-members-r53 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-clock-members-r53.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-clock-members-r53-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-clock-members-r53-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-clock-members-r53-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-clock-members-r53- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 结果：FAIL

固定源3600帧/120秒自然exit0，CLI自然exit1/no-progress。11:41:46.648先发布members=audio_video/acquiring，46.650 locked generation1；46.768首release video16/audio31、首commit audio31。生产路径已使用统一成员snapshot与消费者校验；未运行VideoOnly或2–4源。

11:43:45.930 old1→next2 purge7ack complete/acquiring；11:43:51.346至51.375 abort。edge46视频与edge47音频均为EncodedPacket/Packet、各10/10。abort前20queued/24objects/270826bytes，最终queued/workers/payloadBytes为0、4逻辑对象（71897 reservations/71893 releases）。高水11988210bytes/77objects；workerErrors/errors/pressureFailures均0、stalledIntervals1；AAC关闭2帧残留。没有第二代媒体恢复证据。

426次CPU采样、22核，进程整机口径均值1.131276%、峰3.589744%，单核口径24.888075%/78.974359%。runtime WS初始105455616、最终189595648、峰194203648bytes。外部19次5秒样本11:42:18.782至11:43:49.039，CPU累计8.796875→31s，WS191774720→189595648、峰194916352；Private399900672→397795328、峰402124800bytes。短窗口不能证明长期无增长。

118条generation1漂移raw/filtered绝对最大156ns，不证明重入或播放端同步。sender86345datagrams/104842340payloadBytes，deadline/pressure/partial/ambiguous及pacing/backlog取消均0，delivery_evidence=not_proven。

已实际查看1280×720游戏截图composition-clock-members-r53-2026-09-29-11h42m37s192.png；VLC记录playback too late88315、picture late41ms、Failed to create video converter，不证明连续无丢帧。完整退出验收FAIL，不创建成功验收提交；合屏保持FAIL42/100。

### 清理清单

先归档本页，再核实VLC8244身份并清理。CLI31700及源7724已自然退出。逐项删除本轮-cli.log、-source.log、-vlc.log、.sdp及上述PNG五文件；保留指定124427809bytes连续源和正式构建。本轮无远程运行、抓包、临时录制或测试脚本。VLC清理不计自然退出。

清理复核：上述五文件全部删除，三个本轮PID均无残留，固定源保留。
