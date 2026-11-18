# 代际转换合同的无效音频依赖清理

## 范围与依据

基线 c0eddffb。terminalDrainWindow 由 audioServo.maximumMeasurementGapNs 派生，但全 src 只有构造、传递和校验，没有等待、排空或终止消费者。本轮从计划、facts、两个 planner、调用方和 validator/coordinator 中完整删除该内部字段，保留 acknowledgementTimeout 和真实 servo 参数。

[RFC3550](https://www.rfc-editor.org/rfc/rfc3550.html)按 RTP session 定义媒体与 SR 映射；[GStreamer rtpbin](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpbin.html)按请求的 pad 启用相应会话。本项目应按真实成员和实际运行语义形成合同，不能为纯视频源伪造音频 servo 产品。本轮没有引入或替换算法，不声称已实现终止排空窗口。

线程、队列/内存上界、所有权、参与者、真实 drain/EOF/flush/abort、平台 adapter 均不变；无新增公共输入、fallback 或独立媒体链。Windows/Linux 共享修改，当前仅验证 Windows realtime。

## 审查与边界

两位未参与实现的独立审查者均明确 Standards/阶段 Spec PASS：全部引用与聚合初始化已核对，确认超时和实际终止路径不变。纯 V 的完整 prepared/planner/builder、可选音频整体产品、多源控制器与总准入仍未完成，不能据字段清理认定合屏可用。

## r60 原规格 realtime 复验

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-transition-r60 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-transition-r60.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-transition-r60-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-transition-r60-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-transition-r60-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-transition-r60- --snapshot-format=png rtp://@127.0.0.1:62020
```


Release全量session80504，clean665/all666，configure/build exit0；产物13:30:03/5187584bytes。仅运行realtime。

### 结果与清理

CLI31076/session67585自然exit1：NotInitialized: realtime runtime made no progress before timeout；源6588/session10565自然exit0，3600帧/120秒。VLC32200启动exit0，已查看1280×720游戏画面，截图composition-transition-r60-2026-09-29-13h31m41s219.png；VLC仍报buffer deadlock prevented及video converter错误，不能声称连续播放通过。

首release13:30:59.352 video20/audio22；13:32:58.448 purge后20项退休输入释放。stop13:33:03.604→abort03.607→03.674完成；VideoEncode23 owner引用2，账本4→0。最终queued/workers/payloadBytes/payloadObjects均0，reservations/releases均71871；高水12212198bytes/91objects；总入出171580、编码包入出39983。workerErrors/errors/pressure0，stalledIntervals1。118条gen1漂移最大156ns，无重入恢复证明。

398 CPU样本/22核，机器口径均值1.379762%、峰4.329004%，单核30.354757%/95.238095%；runtime WS114790400→188547072、峰193134592。后台monitor92769自然0，19次5秒样本13:31:31.072→13:33:01.415，CPU10.40625→37.734375s；WS191336448→188547072、峰193134592，Private400519168→397520896、峰401686528。短窗口不代表长期稳定。

sender86308datagrams/104801852payloadBytes，deadline/pressure/partial/ambiguous0，pacing取消1、backlog取消6datagrams/7008wireBytes，delivery not proven。完整媒体及合屏验收仍FAIL，不作成功验收提交；本轮仅证实现有AV路径可构建运行及终态账本仍平衡，不证明纯V或合屏完成。

先归档上述命令及证据，再核对身份清理VLC32200；CLI/源已自然结束。按清单删除composition-transition-r60-cli.log、composition-transition-r60-source.log、composition-transition-r60-vlc.log、composition-transition-r60.sdp及上述PNG共5文件。保留固定120秒源124427809bytes与Release产物；无远程运行、抓包或临时测试脚本。

清理复核：本轮5文件与3个PID均无残留，指定源完整保留。
