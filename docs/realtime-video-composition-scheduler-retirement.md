# 重采集期间 scheduler 输入退休调查与修复

基线 f2f0b690；当前完整合屏仍 FAIL42/100。

## r55：原规格失败复现

2026-09-29，现有Release产物12:00:22/5178880bytes；只测realtime，无local测试。CLI PID30476/session56743自然exit1；源PID23800/session18383自然exit0，3600帧120秒；VLC PID25532启动exit0，实际查看1280×720游戏画面。外部监控session46719自然exit0。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-drain-r55 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-drain-r55.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-drain-r55-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-drain-r55-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-drain-r55-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-drain-r55- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 证据

首release12:14:16.533，generation1 video22/audio22；12:16:15.619 purge7ack old1/next2进入acquiring；12:16:20.946开始abort，12:16:21.021完成。edge46/47各10/10，CLI无进展超时。最终queued/workers/payloadBytes0，4对象；71839 reservations/71835 releases，高水12212198bytes/84对象。workerErrors/errors/stalledIntervals均0（不否认CLI实际超时）；AAC关闭残留2帧。

CPU400样本/22核，整机口径均值1.375134%、峰3.425775%，单核30.252937%/75.367047%。runtime WS101203968→187944960、峰192249856bytes；外部19次5秒样本12:14:46.001→12:16:16.316，CPU9.5→37.1875s，WS191078400→187965440、峰192937984，Private395247616→393019392、峰396742656。不能据短窗口推断长期稳定。

118条gen1漂移raw/filtered最大156ns；无恢复证据。sender86288datagrams/104779072payloadBytes，deadline/pressure/partial/ambiguous均0，pacing取消1、backlog取消23datagrams/30060wireBytes，delivery not proven。VLC Failed to create video converter，截图只证明单时刻画面，不证明连续播放。

### 根因边界

MediaAvOutputSchedulerNode::process在重采集且controller被purge清除时，早于fillHead返回waiting；输入channel不属于被替换的session Data，存在退休媒体无法被消费的路径。MediaRtcpSenderReportTracker的BYE触发SenderLeft时钟失效，并非可信EOF；不能把本次退出改写成成功或把BYE当作永久结束。4对象归属尚未查明，不将其等同于scheduler队列对象。

采用[GStreamer flush事件](https://gstreamer.freedesktop.org/documentation/additional/design/events.html)解除旧数据背压的职责原则；复用本项目既有严格代际分类与有界head，不复制GStreamer线程模型，不放宽新代发布权限。具体实现待设计审查。

### 清理

先归档本节，随后按身份清理VLC25532；CLI30476和源23800自然结束。逐项清理本轮3日志、SDP、composition-drain-r55-2026-09-29-12h14m56s162.png，保留124427809bytes指定源与Release产物。无远程运行/抓包/临时录制/测试脚本。本次FAIL，不作成功验收提交。

清理复核：r55五文件及三个PID均无残留，指定源保留。

## 实现与审查边界

scheduler在Acquiring/ReadyForActivation期间取得同一代际仲裁的phase/epoch快照，再锁session mutation、刷新Data、读取最多两个既有head。统一preflight只释放严格退休区间的媒体，目标代保留，未来代报错；Purging不读取，避免后续Data交换遗失目标代。控制head保持有序，只有实际pop、首次closed→EOF状态变化或退休reset才返回progress；不新增线程、队列、容量、timeout或对外参数。所有锁在配置/发布输出前释放，输出许可不放宽。

ContinuousOutput由planner明确冻结为initial-only独立输出域，bootstrap不安装源重采集coordinator；它保持原激活路径，以initial-only epoch和明确Inactive语义调用同一分类循环。不能因缺coordinator而运行时fallback。共享source/output域使用成对仲裁。本修改共用Windows/Linux逻辑，未声称RKMPP运行通过。

设计初审的混合快照、Purging提前读取问题已修订；代码首审发现的ContinuousOutput无coordinator回归已修正；最终双审与r56结果见下文。第一次全量Release session2105 exit0；因之后上述源码修订，该产物未测试，必须重新全量构建。

与GStreamer只对照撤销旧数据后解除背压的职责；本轮没有实现完整FLUSH_STOP/EOS语义，不把来源退出、4逻辑对象或AAC重入视为已修复。完整V源prepared/planner/builder、多源总准入/公共入口、源恢复及双平台合屏门禁仍未完成，42/100。

## r56：退休队列修复证据，完整验收仍FAIL

修订源码冻结后双独立Standards/阶段Spec PASS。Release session40838全量665清理/666构建exit0，CLI产物12:25:40/5180928bytes。CLI PID9796/session55381自然exit1；源PID21640/session7472自然exit0、3600帧120秒；VLC PID31016启动exit0，实际查看1280×720游戏画面；monitor93962自然exit0。未测试local。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-drain-r56 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-drain-r56.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-drain-r56-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-drain-r56-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-drain-r56-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-drain-r56- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 对照结果

首release12:26:42.810，generation1 video16/audio33；12:28:41.876 purge7ack old1/next2 acquiring，12:28:41.877记录20条retired_input（video10/audio10，generation1）。没有stalled edge；encodedPacketsPushed/Popped均39971、totalPushed/Popped均171581。与r55两队列各10满对照，证明本次运行中的退休数据背压已解除；未证明新代恢复、目标代保留及ContinuousOutput完整实流。

12:28:47.307开始abort，47.341结束；CLI仍no-progress自然exit1。最终queued/workers/payloadBytes0、4对象，71879 reservations/71875 releases；payload高水12072728bytes/93对象，workerErrors/errors/pressure0、stalledIntervals1，AAC关闭2帧残留。不能把无满队列告警称作退出成功或合屏验收通过。

CPU395次/22核，整机均值1.435575%、峰3.719912%，单核31.582656%/81.838074%。runtime WS108519424→188661760、峰193605632bytes；外部22次5秒样本12:27:00.560→12:28:45.923，CPU5.984375→39.390625s，WS191344640→188661760、峰193605632，Private399503360→398004224、峰401121280。无长期内存结论。

118条gen1漂移raw/filtered绝对最大156ns。sender86262datagrams/104739504payloadBytes，deadline/pressure/partial/ambiguous0，pacing取消1、backlog取消53datagrams/70740wireBytes，delivery not proven。VLC Failed to create video converter；已观察的composition-drain-r56-2026-09-29-12h27m31s010.png仅证明单时刻画面。

### 清理与遗留

先归档本节，再按身份清理VLC31016；CLI9796和源21640已自然退出。逐项清理本轮3日志、SDP与上述PNG五文件；保留指定源及Release产物。本轮无远程运行、抓包、临时录制和测试脚本。CLI退出仍FAIL，不创建成功验收提交。

仍须定位4逻辑对象的精确持有者、完成可信终止/恢复与AAC重入，补齐V源prepared/planner/builder、多源入口与总准入、2–4源丢失/全丢失持续输出及Windows→RKMPP门禁。完整评分维持六维10/8/6/12/2/4=42/100。

清理复核：r56五文件与三个PID均无残留，指定124427809bytes源保留。
