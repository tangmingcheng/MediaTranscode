# 合屏阶段一：源失活与时钟代次

## 失败与工业依据

r7原规格120秒RTP H.264/AAC→HEVC/AAC MPEG-TS/RTP在源结束附近失败：gate锁定1，收到Discontinuity generation2；随后clock group又变3。实际命令及指标见[输入保留记录](realtime-video-composition-input-retention.md)。源码根因是validator仍处Active时每次clear都递增，adapter没有失效旧代次产品。

[GStreamer事件](https://gstreamer.freedesktop.org/documentation/additional/design/events.html)将旧数据flush与新segment建立分开；[同步模型](https://gstreamer.freedesktop.org/documentation/additional/design/synchronisation.html)由segment映射源时间到公共running-time。本项复用该失效/重新建域原则及仓库既有typed purge状态机，不宣称RTP协议定义了本项目的generation算法。[RFC3550](https://www.rfc-editor.org/rfc/rfc3550.html)的BYE只说明所列源退出，不能证明整个会话永久完成。

## 设计与范围

- validator显式区分Initial、Active、Reacquiring：首次Active失效保留old、仅分配old+1；同轮重复失效重置候选但不递增。首次未锁定失效保持Acquiring0，不产生非法0代次purge。
- snapshot携带invalidatedGeneration；adapter失效事件投影old，Acquiring/Locked投影next。新Locked提交后清除old，恢复后的真实新失效才开始下一轮。generation溢出明确失败。
- Initial与Reacquiring都执行原有SR/CNAME新鲜度校验；锁定仅由双方完整且有效的证据产生。无默认代次、到达时刻合成时钟或放宽gate。
- 保持现有每节点worker、有限批处理、FIFO控制顺序和背压；新状态仅常数个标量，不新增队列、线程、payload分配或公共参数。平台共享同一validator/adapter，Windows先验证，RKMPP随后覆盖。
- 增加失效旧代次与RTCP失效原因诊断，区分收到BYE与仅凭源命令推测。BYE不映射EOF；黑场/静音、持续输出及多源恢复仍按原计划继续实现。

## 验证进度

设计独立审查、两位未参与实现者的源码交叉审查均通过；Release全量重建成功。r8原120秒真实CLI/FFmpeg/VLC确认代次修复，完整验收仍FAIL，结果如下。

独立审查另发现既有恢复竞态待验：一侧gate同步purge时，另一侧可能先收到Acquiring(next)，而gate仅接受Acquiring/Ready阶段并拒绝Purging阶段。当前只确认代次生产及投影修复，不宣称完整重锁、尾部排空或多源恢复通过。评分维持42/100。

## r8 实际运行：局部代次修复成立，完整验收 FAIL

- Release全量重建成功，两个独立源码审查Standards/局部Spec均PASS。CLI PID520退出1，源FFmpeg PID20148退出0、3600帧/120秒；VLC PID18712经RC quit结束。未停止CLI，源自然结束。
- 链路：RTP H.264/AAC→CUDA解码/scale_cuda→HEVC CBR 8000kbps、1280×720、30fps、AAC CBR192kbps/44100Hz双声道→MPEG-TS/RTP→VLC。截图观察到有效1280×720游戏画面，无明显破图；仅代表观察时刻，不证明全程无迟帧或唇同步。
- 10:17:31.782两条runtime诊断均为`rtcp_clock_invalidation generation=1 reason=RTCP BYE ended active source`，对应两次`state=3 generation=2 invalidated_generation=1 locked=0`。本轮由生产解析日志确认匹配当前源的BYE，不是抓包证据；没有generation3或malformed discontinuity错误。
- 内部漂移118条，source master约0.311～119.616秒，raw phase最小/最大0ns，记录中的filtered phase/frequency/compensation均0；不等于播放器端声画偏差测量。
- CPU430次采样，进程整机平均1.050336%、峰值3.508772%（22逻辑核）。workingSet初始109617152、约10秒190222336、峰值193204224、退出报告188604416字节。热阶段仍缓慢增长约3MB，不宣称长期无增长。
- payload高水位11983691字节/84对象，最终报告27018字节/8对象，reservations/releases=71857/71849，pressureFailures=0。报告时仍有节点引用，不宣称销毁后归零或泄漏。encodedPacketsPushed/Popped=39971/39923为各边累计，非唯一编码包数。
- sender已提交86314 datagrams/104808692 payload bytes，deadline/pressure/partial failures=0，backlog最终0；无wire capture，delivery_evidence=not_proven，不能由sender尾部为空推断整个编码尾部已完整排空。
- 源结束约5.5秒后CLI因`realtime runtime made no progress before timeout`退出；workerErrors/errors=0/0，stalledIntervals=1，满边33/34为视频frame，46/47为encoded输入。VLC日志有audio late、buffer deadlock prevented及硬解surface警告；截图转换链一次失败后生成PNG，不隐去这些警告。

### 剩余生命周期缺口

独立源码调查确认：reacquisition全部purge ack后进入Acquiring并清除purge开始时间，pollTimeout仅处理Purging；gate初次锁定后清除的acquisition deadline未在恢复时重建。startup逻辑仍Running，只有新AU到来才advance generation；完全停源不会触发。scheduler等待新epoch，没有新输出；CLI按既有无输入/无编码进展策略超时。本轮没有完整purge/ack遥测，不把该结果断言为worker死锁。

下一实施应将失活源等待、purge屏障、新epoch获取和planner期限纳入同一类型化生命周期及转换遥测，再接独立输出时间轴的黑场/静音产品；不能仅调大CLI watchdog或把BYE映射EOF。完整恢复、VideoOnly/动态多输出回归、RKMPP、多源合屏仍未完成。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r8 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60740 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60742 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61740 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r8.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r8-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60740?rtcpport=60741&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60742?rtcpport=60743&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r8-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r8-vlc.log --extraintf=rc --rc-host=127.0.0.1:62740 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r8- --snapshot-format=png rtp://@127.0.0.1:61740
```

VLC RC 127.0.0.1:62740执行`snapshot`观察画面，结束后执行`quit`。本轮产物清单（out/acceptance）：composition-domain-r8-build.log、composition-domain-r8-cli.log、composition-domain-r8-source.log、composition-domain-r8-vlc.log、composition-domain-r8.sdp、composition-domain-r8-2026-09-16-10h15m59s850.png。没有抓包/录制/远程脚本，结果归档后已按清单逐项删除，确认本轮文件和PID520/20148/18712无残留，保留复用120秒源与历史文件。
