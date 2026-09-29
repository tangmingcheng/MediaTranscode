# 合屏源域 purge 屏障推进

## 最新结果：r11 完整恢复仍 FAIL

最新冻结源码已通过两名独立审查，Release全量重建成功；本轮三个真实重入场景r9/r10/r11均未通过完整恢复，更未完成合屏。

r11 CLI PID29008退出1；首源PID30392及重启源PID32452均自然退出0，各3600帧/120秒；VLC PID9776经RC quit退出。10:56:13.889进入old1→next2 purge，10:56:13.891全部5组ack；10:56:19.191恢复源Locked2，随后MPEG-TS materializer报`MPEG-TS RTP protocol batch generation differs`。旧startup错误、transport plan激活错误均未再出现。

源码确认materializer持有旧protocol/transport plan后不再消费新计划，故generation2批次进入generation1构造器；不是已证明网络乱序。sender也有交接缺口：先处理pending再读新plan，bindPlan要求pending和queued为空，跨端口旧批次可能晚到；发送session必须在其owner线程关闭，不能由coordinator线程直接reset。下一步须按已批准源/输出域解耦设计处理完整协议生命周期，不逐个放宽generation检查。

r11观察到有效1280×720画面，仅代表首源时刻；内部漂移118条覆盖0.338～119.642秒，raw/filtered/frequency/compensation均0，不是播放端同步证据。CPU413采样（后台额外140次），22核整机口径平均1.317485%、峰值4.587156%；WS初始113180672、最高/结束报告197292032字节，热段仍增长。payload高水位13912951字节/398对象、最终0/0，reservations/releases72299/72299，pressureFailures0；workerErrors/errors1/1，stalledIntervals1。最终退出原因是协议代次错误，不把stalledIntervals计数等同退出原因。

sender generation1提交86327datagrams/104825656payload bytes，deadline/pressure/partial失败0，最终backlog0；没有wire证据，delivery_evidence=not_proven，不代表codec尾部完整排空。VLC存在迟帧/音频迟到，以及截图转换失败后PNG成功的警告；AAC不支持flush的警告仍保留为后续恢复风险。

### r11 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r11 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60770 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60772 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61770 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r11.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r11-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60770?rtcpport=60771&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60772?rtcpport=60773&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r11-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r11-vlc.log --extraintf=rc --rc-host=127.0.0.1:62770 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r11- --snapshot-format=png rtp://@127.0.0.1:61770
```

第二源实际命令与上列FFmpeg完全相同，仅日志改为`composition-domain-r11-source-rejoin.log`。本轮产物（out/acceptance）：`composition-domain-r11-build.log`、`composition-domain-r11-cli.log`、`composition-domain-r11-source.log`、`composition-domain-r11-source-rejoin.log`、`composition-domain-r11-vlc.log`、`composition-domain-r11.sdp`、`composition-domain-r11-2026-09-16-10h55m25s207.png`。归档后逐项删除，检查四PID无残留；保留固定源，无抓包、录制或远程脚本。


### r10 指标与实际命令

完整恢复验收FAIL。最终版本Release全量重建成功。CLI PID18372退出1；首源PID33144、重启源PID6436均自然退出0、各3600帧/120秒；VLC PID29332经RC quit结束。10:49:22.042 BYE触发old1/next2，1毫秒内4组purge ack；10:49:22.287新SSRC再入，10:49:27.290源时钟锁定generation2；下一毫秒传输计划节点因遗漏purge参与而拒绝激活。无原startup malformed失效，也非CLI无进展watchdog触发。

画面截图有效1280×720；内部漂移118条覆盖0.366～119.670秒，raw20627～20628ns，filtered20628ns，frequency/compensation0；未取得generation2输出，不证明播放端同步。CPU411次采样（后台额外141次），22核整机口径进程平均1.343241%、峰值3.800475%；WS初始124456960、峰值/结束报告195960832字节，热段仍增长。payload高水位13927788字节/397对象，最终0/0，reservations/releases72214/72214，pressureFailures0；workerErrors/errors1/1，stalledIntervals0。VLC仍有迟帧、迟到音频与buffer deadlock prevented；AAC purge不支持flush警告保留，未证明恢复编码正确。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r10 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60760 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60762 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61760 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r10.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r10-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60760?rtcpport=60761&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60762?rtcpport=60763&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r10-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r10-vlc.log --extraintf=rc --rc-host=127.0.0.1:62760 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r10- --snapshot-format=png rtp://@127.0.0.1:61760
```

第二源实际执行同上FFmpeg命令，唯一差异为日志`composition-domain-r10-source-rejoin.log`。产物清单（out/acceptance）：`composition-domain-r10-build.log`、`composition-domain-r10-build-final.log`、`composition-domain-r10-cli.log`、`composition-domain-r10-source.log`、`composition-domain-r10-source-rejoin.log`、`composition-domain-r10-vlc.log`、`composition-domain-r10.sdp`、`composition-domain-r10-2026-09-16-10h48m23s895.png`。归档后逐项清理，确认四个PID无残留，保留固定源；无抓包、录制或远程脚本。


## 设计与适用边界

2026-09-16，基线 `beaac5ee`。r8 已证明匹配源 BYE 后 old=1/next=2 正确，但 CLI 无进展退出；没有证明 purge 并发竞态实测发生。本轮先修源码确认的并发控制顺序和唤醒缺口，并补足生产转换日志。

依据 [GStreamer flush/segment](https://gstreamer.freedesktop.org/documentation/additional/design/events.html)，先撤销旧数据发布、完成清理，再允许新时基数据。依据 [GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)，缺失输入与输出时钟应分离。有限 purge 事务、有界恢复候选、可无限源缺失是三种不同生命周期；不能将既有 startup 10 秒常量当作网络源必然返回的事实，也不能增加致命 absence deadline 代替黑场/静音。

- gate 在同一次 generation arbitration 下验证 Acquiring(next) 与旧 epoch；Purging 期间只接受控制证据，媒体仍 Withhold。Locked(next) 可以记录本地源锁事实，任何媒体发布继续经过共享分类器。单 clock 边 FIFO 保持失效先于新锁。
- registrar 从类型化源域成员绑定既有 wakeup；全部 participant ack 后先释放状态锁，再通知域节点。worker 在 process 前取通知序号，覆盖检查与等待之间的通知，复用 Windows/Linux 同一执行器契约。
- 保留单 pending 媒体包，受阻返回 Waiting，避免无效 Progress 忙转；不扩大队列、线程、媒体预算，不新增对外参数和平台链路。
- 独立审查发现基类背压 pending transfer 曾绕过 gate 代次校验；校验现放到每次 output commit reservation，旧包取消，新代屏障未完成则等待，允许态持仲裁直到提交。首次受阻未进入基类 pending 时由 gate 保留单包；基类 pending 重试遇到 reservation WouldBlock 继续等待，不将正常背压当作 worker 错误。
- purge 保持在 coordinator 状态锁外执行；通知对象由 shared_ptr 持有，无回指环；失败保持原有终止语义。日志记录 group、old/next、transition、阶段及 ack 数。

独立设计审查 PASS。此增量尚未完成独立输出时间轴、逐源黑场/静音、有界恢复候选及完整合屏。现有 purge timeout 仍缺生产调用，不能声明已覆盖空输入期限。

## 验证记录

r10成功恢复RTP源锁generation=2，随后`MediaDatagramTransportPlanSourceNode`拒绝新输出激活。根因是该节点已有purge接口，但planner参与者名单与factory注册都遗漏它，局部状态仍停留旧代次。现将共用datagram transport plan作为独立参与组（内部枚举追加6，既有值不变），两种输出适配器复用；assembler仍要求声明名单的完整注册。r11越过该错误，但尚未覆盖materializer和sender完整生命周期。后续多源设计仍需把输出组从单源purge名单分离，当前增量不代表单源整链恢复已完成。

r9 全量构建成功，120秒首源正常输出后在10:40:31.663开始purge，10:40:31.664全部4组ack；重启源262毫秒后带来新SSRC，恢复候选Acquiring/失效交替，startup clock因首次失效已经清除active generation，误拒同一old=1的重复失效，CLI退出1。该结果不是purge未完成，也不是无进展watchdog触发。

按同一失效事务保持幂等原则，startup clock增加失效代次事实，仅接受精确同代重复失效，恢复Locked必须比失效代次更新；重复通知不恢复tick，不开放输出，不新增期限或队列。成功消费失效/Acquiring后报告Progress，下一次无输入才Waiting，防止已排队Locked没有新通知而滞留。全量构建、双复审完成，r10/r11已越过该错误；完整恢复和合屏仍FAIL。

### r9 指标与实际命令

完整恢复验收FAIL。CLI PID9248退出1；首源PID15368退出0、3600帧/120秒；重启源PID27800自然退出0、3600帧/120秒；VLC PID8116经RC quit退出。首源画面截图为正常1280×720游戏画面，仅代表观察时刻。内部漂移118条（0.319～119.623秒），raw phase155～156ns；非播放端声画同步证明。

运行时CPU411次采样、22逻辑核，整机口径进程平均1.177665%、峰值4.255319%；工作集初始109326336、峰值193007616、退出报告188346368字节，热段仍慢增长。后台额外采样135次，覆盖约101秒。payload高水位12060467字节/78对象，退出报告0字节/0对象，reservations/releases=71879/71879，pressureFailures0；不外推长期无泄漏。workerErrors/errors=1/1，stalledIntervals0。sender committed86345datagrams/104842340payload bytes，deadline/pressure/partial failures0、尾队列0，无抓包，不证明端到端无丢包或完整编解码尾部排空。

保留失败证据：VLC有迟帧、音频迟到与buffer deadlock prevented警告；purge时AAC打印不支持flush的警告，其恢复影响尚未验证，不把ack成功等同codec状态已正确恢复。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r9 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60750 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60752 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61750 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r9.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r9-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60750?rtcpport=60751&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60752?rtcpport=60753&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r9-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r9-vlc.log --extraintf=rc --rc-host=127.0.0.1:62750 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r9- --snapshot-format=png rtp://@127.0.0.1:61750
```

重启源实际命令同上FFmpeg命令，唯一差异是日志路径为`composition-domain-r9-source-rejoin.log`；未循环播放，未降低源参数，未停止CLI。


r9临时产物清单：`composition-domain-r9-build.log`、`composition-domain-r9-build-final.log`、`composition-domain-r9-cli.log`、`composition-domain-r9-source.log`、`composition-domain-r9-source-rejoin.log`、`composition-domain-r9-vlc.log`、`composition-domain-r9.sdp`、`composition-domain-r9-2026-09-16-10h39m26s106.png`，均位于out/acceptance。结果归档后逐项清理，四个进程均无残留；保留固定120秒源。无抓包、临时录制及远程脚本。
