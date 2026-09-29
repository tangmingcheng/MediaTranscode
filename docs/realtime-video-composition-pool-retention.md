# 合屏输出池驻留数量规划

基线662c4d25。先完成打开生产encoder前可获得的数量合同；不把它称为内存准备授权或最终资源准入。

## 算法与事实

沿用[GStreamer池协商](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)按队列和消费者持有量配置有限池、引用归还后复用的做法；[FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)允许内部保留引用及EAGAIN，因此必须纳入实际prepared encoder retention合同，不能以样例延迟估算。

本图要求aggregate直接连到唯一VideoEncode视频输入，无分叉和额外持帧节点。令q为该真实有界边容量，R为prepared encoder最大保留帧数。可写surface与独立发布lease上界为q+1个aggregate pending+1个encoder pending+R，生产pool再加1个固定black surface。三个1来自实际单对象成员和容器，不是经验余量；encoder克隆共享底层surface/lease，不重复计底层分配。AVFrame克隆头对象仍需另记，不得把lease上界称为所有头对象上界。逐源候选及滤镜池不属于输出pool。

原单链最终账本复用同一溢出检查算术，原输入边、pending及encoder-retained事实保持；新增persistent pool数量事实承载黑底。合屏拓扑在构图后规划数量，bind对同一图重新推导并核对实际canvas计数和pool，缺prepared事实、边不唯一或未有界均失败。

## 执行与边界

不新增运行线程或队列。准备仍须由同一deadline/cancellation/RAII事务统筹，规划结果自身不允许分配；源预算、实际allocation bytes与aggregate多源总账仍需接通，最终compiler继续拒绝不完整aggregate合同。硬件adapter及外部FFmpeg不改，Windows/RKMPP共用规划。

设计阶段独立审查认可数量上界，要求唯一直接有界边、prepared retention、全部溢出检查、bind重算以及lease/AVFrame对象区分。11源码冻结后双独立Standards/阶段Spec PASS。自查inspect调用既有capture所需非const AVCodecContext&已修正，发布前同owner线程读取，无const_cast或重开。Release重建成功，r45共享链复验失败，见下文；完整FAIL42。

MediaVideoEncoderPreparer的原readAfterOpen和randomAccess检查集中为inspect，prepare与bind共用；actual emission/retention超界仍直接失败。新数量产品从buildTopology就验证，最终binding再次推导并比较全部pool产品以及canvas surface/lease数量。当前最终账本仍不支持aggregate，不能只加白名单或让旧单源pool公式处理多源。

首次Release全量session73440仅取得configure成功，后续handle失效；复核无CMake/Ninja/CL进程且CLI产物缺失，不能判成功，也无证据指定为超时或编译错误。已按同一300秒clean-first/all-target入口重建：session98507成功658项，configure/build均exit0，CLI2026-09-28 16:02:05、5133824bytes。

## r45实际realtime链路

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps；AAC CBR192kbps、44100Hz双声道。固定120秒源，不运行local。CLI PID39896/session58288，源PID38936/session38975，VLC PID43632（启动exit0），5秒后台监控session86619。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-pool-retention-r45 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-pool-retention-r45.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-pool-retention-r45-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-pool-retention-r45-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-pool-retention-r45-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-pool-retention-r45- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 结果与清理

源自然exit0，完整3600帧/120.00秒；CLI自然exit1/no-progress。16:04:48.340 purge7ack complete、old1/next2 acquiring，53.530 abort.begin、53.563 abort.done。edge46/47均10/10；final queued/workers/payloadBytes=0，4逻辑对象（reservations71836/releases71832），高水12102872bytes/88objects。final workerErrors/errors/pressureFailures/stalledIntervals均0，但CLI明确报告no-progress，不能以该计数抹去失败。

401个CPU采样/22核：整机口径进程均值1.352033%、峰3.752759%；单核均值29.744720%、峰82.560706%。runtime WS初始113913856、最终189153280、峰194445312bytes。5秒后台监控17采样、exit0，WS峰193986560、Private峰402292736；16:03:28 CPU12.703125s/WS191201280/Private398024704，16:04:49 CPU36.640625s/WS189153280/Private397479936。

118条generation1 raw/filtered漂移绝对最大156ns、compensation_distance45346，不证明源恢复。sender提交86233datagrams/104710224payloadbytes，deadline/pressure/partial/ambiguous/pacing_cancelled均0，delivery_evidence=not_proven。VLC截图composition-pool-retention-r45-2026-09-28-16h03m47s480.png已观察到1280×720游戏画面；日志有buffer deadlock prevented、playback too late98430、Failed to create video converter。

本轮仅执行原共享路径的pool算术，不证明新canvas数量规划/多源bind已运行，完整FAIL42。旧no-progress/4逻辑对象和AAC重入未解决。先归档以上命令与证据，随后按清单清理：composition-pool-retention-r45-cli.log、composition-pool-retention-r45-source.log、composition-pool-retention-r45-vlc.log、composition-pool-retention-r45.sdp和上述PNG；VLC核验身份后精确清理PID43632，不计自然退出，CLI39896及源38936已自然退出。无远程、pcap或临时录制产物，指定源保留。

清理复核完成：5文件及3个精确PID均无残留，指定源124427809bytes保留。
