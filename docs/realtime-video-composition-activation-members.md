# 合屏成员型epoch/release激活产品

## 实现与边界

基线8a744dd5，复用[成员合同调查](realtime-video-composition-source-membership.md)的RFC3550与GStreamer成员/有序激活模型。本轮落实其首个完整激活产品切片，不新增公共参数，不修改外部FFmpeg。

- `MediaPlaybackActivation`只由校验工厂创建：VideoOnly不得有audio origin，AudioVideo必须有同generation/sourceStart/masterRelease的有效origin；非零generation、非负sample index、正sample rate仍强制。成员、epoch和origin作为同一值保存/传递。
- planner明确写入现有A/V成员，经plan validator、binding、bootstrap进入service；service冻结成员并拒绝initial/next改变它。ContinuousOutput仍强制AudioVideo，group注册核对plan与service一致。
- startup release、transaction、activated event、preparation anchor、sequencer、capability、service snapshot与permit统一消费新产品。重锚只改变masterRelease，transaction/extractor要求其余字段完全相等；不会改变成员、源坐标、代次或音频sample坐标。
- generation evidence分类消费同一完整activation，保留retired区间/target/future/readiness/poison/permit校验；drift消费无音频origin明确失败。没有假音频origin或从超时推断成员的fallback。
- 不新增线程、队列、平台链或动态分配；产品为有界值对象，原RAII发布预约、worker、原子提交和背压沿用。Windows与RKMPP共享这些组件，后者运行证据尚缺。

当前clock/startup/builder的纯视频装配仍未实施，plan validator明确拒绝VideoOnly同步域；内部产品支持无音频形状不等于纯视频输入可运行。独立startup cadence、完整多源准备/总准入、公共入口、既有no-progress/AAC重入及双平台合屏门禁仍未完成，完整FAIL42。

## 审查与构建

32源码由两名未参与实现的审查者分别给出Standards/首切片源码Spec PASS。Release全量重建session14034成功（666项，configure/build exit0）；CLI产物2026-09-29 11:00:14、5168640bytes。没有运行local CLI。

## r51：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30 fps、8 Mbps

输出音频AAC CBR192kbps、44100Hz双声道，输入沿用指定连续120秒源。CLI PID29616/session49463，FFmpeg PID15264/session88999，VLC PID19492（启动exit0），外部5秒监控session43686。下面是实际命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-activation-members-r51 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-activation-members-r51.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-activation-members-r51-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-activation-members-r51-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-activation-members-r51-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-activation-members-r51- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 结果：FAIL

源3600帧/120.00秒自然exit0，CLI自然exit1/no-progress。11:01:28.338首release为video16/audio32，首commit发布audio32；当前生产二进制的startup/reanchor/activation/permit均走本轮统一产品。11:03:27.379 purge7ack complete、old1/next2 acquiring，11:03:32.822 abort.begin、32.851 abort.done。未得到下一代媒体，不能证明恢复或纯视频产品运行。

edge46/47均10/10；abort前20queued/24objects/238665bytes；final queued/workers/payloadBytes=0、4逻辑对象（reservations71958/releases71954）。高水12007844bytes/88objects，workerErrors/errors/pressureFailures均0，stalledIntervals1；AAC关闭时仍有2帧残留。

407次CPU采样、22核：整机口径进程均值1.328969%、峰3.679654%；单核口径均值29.237327%、峰80.952381%。runtime WS初始110256128、最终190455808、峰194793472bytes。外部5秒监控18样本exit0，11:02:06.576至11:03:31.820；CPU累计13.484375→36.796875s；WS193302528→190455808、峰195088384，Private397529088→393748480、峰398626816bytes。有限窗口不能证明长期无增长。

118条generation1漂移，raw/filtered绝对最大均156ns；源master范围0.387～119.692秒，不证明重入或播放端同步。sender86246datagrams/104723540payloadBytes，deadline/pressure/partial/ambiguous均0，pacingCancelled1、backlogCancelled12，delivery_evidence=not_proven。

实际查看VLC截图`composition-activation-members-r51-2026-09-29-11h02m17s824.png`，有1280×720游戏画面。VLC仍有playback too late89578、picture late46/13ms与Failed to create video converter，不能当作连续无丢帧证明。本次是现有A/V共同激活合同回归，未运行VideoOnly或2–4源合屏；完整退出门禁FAIL，不创建成功验收提交。

### 清理清单

先归档本页命令、结果和指标，再核验本轮VLC19492身份并清理；CLI29616及源15264已自然退出。需删除本轮5文件：上述PNG、composition-activation-members-r51-cli.log、-source.log、-vlc.log及composition-activation-members-r51.sdp。无远程测试、pcap、临时录制或测试脚本；保留指定120秒源与正式构建产物。清理复核：5文件全部删除，本轮媒体进程无残留；源PID15264已被backgroundTaskHost.exe复用，未对复用进程操作。VLC清理不记自然退出。指定源124427809bytes保留。
