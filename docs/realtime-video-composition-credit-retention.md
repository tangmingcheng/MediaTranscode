# 关闭阶段credit持有链诊断

基线3269cf46；完整合屏仍FAIL42/100。r57最终4objects/0bytes，resolver快照无credit，Reporter读取实时ledger；不能把残留等同metadata。

## 设计与边界

[FFmpeg AVBuffer](https://ffmpeg.org/doxygen/trunk/group__lavu__buffer.html)在最后引用释放时释放底层buffer；[GStreamer引用所有权](https://gstreamer.freedesktop.org/documentation/additional/design/MT-refcounting.html)强调各拥有者独立保留/释放。项目retainMediaFfmpegPayload将credit绑定AVBuffer，因此需要观察codec和节点的释放边界。

FFmpegCodecNodeRuntime在owner非空的reset前记录nodeId/kind/use_count，不复制owner；scheduler::abort在每节点调用前后读取同一ledger。日志构造与输出异常不跳过节点abort，不持ledger锁调用节点，不增加每帧registry、线程、队列或公开参数，不改flush/计数/退出策略。观测成本随节点和活跃accounts数量增长，仅关闭阶段逐节点采集。reset也由start调用，日志名仅描述释放行为。

shared_ptr use_count不等于AVBuffer底层引用数，ledger区间变化也不等于节点独占归属；需要结合join边界和具体拥有者。Windows/Linux共用代码，只先测Windows realtime，local与RKMPP未测。

## r58 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-credit-r58 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-credit-r58.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-credit-r58-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-credit-r58-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-credit-r58-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-credit-r58- --snapshot-format=png rtp://@127.0.0.1:62020
```

### r58结果

全量Release session80655成功，clean665/all666 exit0；产物13:01:34/5188096bytes。CLI31476/session7174自然1，源33984/session91375自然0、3600帧120秒；VLC26696启动0，实际查看1280×720游戏画面（PNG composition-credit-r58-2026-09-29-13h03m51s305.png），converter错误仍有。monitor12367自然0，21次5秒样本13:03:20.365→13:05:00.697，CPU6.40625→35.96875s，WS191528960→189386752、峰193327104，Private396390400→395530240、峰397668352。

397 CPU样本/22核，机器口径均值1.331183%、峰4.651163%，单核29.286035%/102.325581%，runtime WS114622464→188657664、峰193327104。118条gen1漂移最大156ns，20项退休输入，编码入出39971，总入出171751。workerErrors/errors/pressure0，stalled1；AAC关闭余2帧。仍no-progress，不是成功验收。

39个node_abort_payload区间中，VideoEncode node23前4后0，releases71856→71860、reservations恒71860，owner_references2；其后所有节点均0。其他codec引用数：AudioEncode29=3，AudioResample28=1，AudioDecode25=2，VideoDecode19=2。13:05:04.979 stop.begin，04.982 abort.begin，05.047 abort.done。CLI最终打印却为4objects/0bytes、71860/71856，高水12105624bytes/86objects。此不一致不能按打印顺序判断capture发生时点：后续源码追踪确认stop失败可早返，完成报告在作用域RAII reset前采集，而reset触发的abort日志先于外层CLI最终打印。r57只检查Reporter直接snapshot而排除陈旧报告的结论不完整，需按完整调用链纠正。

启动前一次操作失误执行了以下不完整命令，校验直接exit1（missing required argument: --rc），未进入媒体链路、无文件产物，不计为媒体测试：
```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-credit-r58 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.61020 --video-rtp-codec h264
```

已归档本轮实际命令及关键证据；清理清单为3日志、SDP、上述PNG共5文件。CLI及源自然退出，核对身份后清理VLC26696。保留指定120秒源和正式Release产物，无远程运行、抓包或临时测试脚本。

r58补充：首release13:03:00.511 video16/audio35；sender86262datagrams/104739504payloadBytes，deadline/pressure/partial/ambiguous0，pacing取消1、backlog取消53datagrams/70740wireBytes，delivery not proven。VLC还记录buffer deadlock prevented，不能声称连续播放通过。

清理复核：r58五文件及三个PID均无残留，固定源124427809bytes保留。

## stop失败收尾修复

真实调用链：MediaGraphScheduler::stop首错早返；ThreadedExecutor已join后仍返回失败而未完成节点stop；RuntimeLifecycleExecutor旧实现清理channel/group后保留ThreadedRunning状态返回；RealtimeRuntimeCompletion保留原wait失败；RunController采集finalReport；返回值构造后MediaGraphRuntimeReset析构调用reset，因runtime仍Running触发abort；外层CLI随后打印先前报告。因此Reporter直接snapshot虽为真，不能证明采集发生于回收之后。r58最后4项在VideoEncode abort区间真实释放，不能称进程退出后持久泄漏。

RuntimeLifecycleExecutor::stop现在在scheduler/executor错误后先调用既有abort再原样返回错误，使失败收尾在完成报告前完成；正常stop顺序不变，clear/close返回错误也完成abort后返回原错误。初始非法state仍直接拒绝。复用既有join/逆拓扑abort及组/activation/authority回收，无新策略、参数或队列；worker firstFailure和completion错误优先级保持。clear/close当前实现总是成功，不能称其错误分支经过真实测试。

采用[GStreamer下行关闭](https://gstreamer.freedesktop.org/documentation/additional/design/states.html)停止执行与资源回收的原则，不声称完整状态机等价。现有abort中的异常/平台行为仍需各平台验证；本轮只验证Windows realtime。

## r59 原规格复验命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-stop-r59 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-stop-r59.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-stop-r59-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-stop-r59-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-stop-r59-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-stop-r59- --snapshot-format=png rtp://@127.0.0.1:62020
```

### r59结果与清理

Release全量session98673 clean665/all666 exit0，产物13:11:57/5188096bytes。CLI21348/session90344自然exit1，源9000/session53747自然exit0、3600帧120秒；VLC29856启动0，已查看1280×720游戏画面（composition-stop-r59-2026-09-29-13h13m52s365.png）。VLC仍有buffer deadlock prevented及converter错误，截图不证明连续播放通过。

首release13:13:10.459 video16/audio34；13:15:09.511代际purge，20项退休输入被释放，编码包入出39971、总入出171626。13:15:14.674 stop.begin→14.678 abort.begin→14.762 abort.done。VideoEncode node23 owner_references2，区间ledger4→0，71871→71875 releases；随后所有节点为0。最终CLI报告queued/workers/payloadBytes/payloadObjects均0，reservations/releases均71875，高水11992599bytes/86objects。workerErrors/errors/pressure为0，stalledIntervals0不否认CLI实际no-progress自然exit1。AAC关闭余2帧。

401 CPU样本/22核，机器口径均值1.289230%、峰4.796163%，单核28.363059%/105.515588%，runtime WS115093504→188268544、峰192630784。monitor97162自然0，21次5秒样本13:13:30.594→13:15:10.929，CPU6.5625→34.84375s，WS191049728→188268544、峰192720896，Private398176256→396619776、峰399196160。不据短窗口推断长期稳定。

118条gen1漂移最大156ns，无恢复证据。sender86259datagrams/104737024payloadBytes，deadline/pressure/partial/ambiguous0，pacing取消1、backlog取消2datagrams/1772wireBytes，delivery not proven。

本项失败收尾修复已在同规格链路验证：报告采集前回收完成，账本归零且平衡，原错误仍保留。完整媒体/合屏验收仍FAIL，不作成功验收提交。后续仍须推进可信终止/恢复、AAC重入、V源完整prepared/planner/builder、多源总准入/入口及Windows→RKMPP原验收。只测realtime，没有local或外部FFmpeg变更。

先归档实际命令和结果，再按清单清理本轮3日志、SDP及上述PNG共5文件；CLI与源自然结束，核对身份清理VLC29856。保留指定120秒源124427809bytes与Release产物，无远程测试、抓包或临时Windows脚本。

清理复核：r59五文件及三个PID均无残留，固定源保留。
