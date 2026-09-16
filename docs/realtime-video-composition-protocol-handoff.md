# 合屏协议代次交接

## 设计与边界

依据 [GStreamer flush/segment](https://gstreamer.freedesktop.org/documentation/additional/design/events.html)，先撤销旧发布权限，再等待持有状态的线程清理，最后允许新计划先于新数据生效。上轮 r11 的真实失败为构造器保留旧计划；完整修复须同时覆盖 MPEG-TS/RTP 构造器、发送器、未提交预约与协议输入队列。

- coordinator/group 保存每个 child 的完成状态；`WouldBlock` 表示同一事务仍在执行，不生成 ack。成功 child 不重复清理。回调期间不持 coordinator state/activation 锁，单独串行化推进。
- 构造器及发送器使用单槽清理请求，均在自己的 worker 清理本代状态并确认。完成通知既有 domain wakeups；gate 在 base pending transfer 之前推进，使用 planner 原有 acknowledgement deadline，不新增轮询周期。
- native submit 与前缀记账持有发布权限，等待不持锁；关闭旧 session 保留已提交证据。取消未提交预约必须有明确计数，不伪造发送或吞掉部分提交错误。
- 持续复用物理 service scope 与已发送限速债务。各端口独立到达，旧数据仅按已授权清理丢弃，新数据等待精确匹配计划，缓冲有界。
- Windows/RKMPP 共用这些生产 DAG 组件；不修改外部 FFmpeg，不新增公共输入。源域/输出域分离、多源聚合、黑场静音仍按原计划验收。

## 执行记录

r12之后按[RFC3550](https://www.rfc-editor.org/rfc/rfc3550.html)的SSRC/SR身份关系与GStreamer事件串行语义修复输入顺序：移除`processRtp`重排序前的证据发布，在有序packet入口验证当前证据；有待发事件时保留packet并让出执行，事件不等待payload credit。pending重试入口与expire/批接收/普通接收统一先事件后媒体。discontinuity已作废的首份缓存SR不保留，恢复等待下一份真实SR，不放宽任何超时。两名独立审查者对该细化及全部源码增量Standards/局部Spec均PASS，完整合屏仍未完成，评分维持42/100。

异步交接实现已完成两名独立源码审查；真实验证结果见下文，完整合屏尚未完成。

## r12：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30 fps、8 Mbps

完整恢复 **FAIL**。两次 VS2026 Release 全量重建成功（最终624目标），两个独立源码审查均通过本轮局部交接实现。CLI PID30592退出1；首源PID27544和重入源PID31080均自然退出0、各3600帧/120秒；VLC PID23320经RC quit退出。

13:39:26.375开始old1→next2；26.376发送owner完成旧会话关闭与归档，全部7组ack。26.899新SSRC先发布generation1的时钟证据，随后才发布RTP重排序的SSRC discontinuity generation2；组时钟瞬间Locked2又失效为3。gate明确报`Locked packet gate rejects incompatible discontinuity evidence`，未得到第二代输出，尚不能证明materializer实际恢复通过。源码链为RawRtpInputNode::processRtp先observeMedia/queueClockEvidence，再reorder.push/processReordered；BYE此前已清tracker media identity，新SR缓存可在此次observeMedia被绑定并提前发布，之后重排序器仍识别旧SSRC变化，invalidate又清掉新证据。该次失败不是已证明的网络乱序。

首源截图有效1280×720，仅证明该时刻画面。内部漂移118条、仅generation1、源时间0.319～119.623秒，raw/filtered均0，不代表播放端同步。内部CPU407采样，22核整机均值1.239889%、峰值4.207921%；额外进程采样16次。工作集初始104030208、峰值196468736、结束191877120字节，未证明长期稳定。payload高水位11936886字节/77对象，最终0/0，reservation/release71903/71903，pressure0；workerErrors/errors2/2，stalledIntervals0。

旧代发送86348datagrams/104842544payload bytes；pacing_cancelled与backlog_cancelled均0，deadline/pressure/partial/ambiguous均0，shared_scope_failed=0；delivery_evidence=not_proven。本场景没有未提交尾部，不能据此声称取消路径已被压力覆盖。AAC不支持flush警告仍在；VLC有late/buffer deadlock prevented及截图转换警告，最终PNG已检查，无抓包。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r12 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60780 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60782 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61780 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r12.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r12-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60780?rtcpport=60781&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60782?rtcpport=60783&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r12-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r12-vlc.log --extraintf=rc --rc-host=127.0.0.1:62780 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r12- --snapshot-format=png rtp://@127.0.0.1:61780
```

首源自然结束后立即执行相同FFmpeg命令，仅日志改为`composition-domain-r12-source-rejoin.log`；不使用循环源或max-duration。

### 临时产物清单

结果归档后逐项删除out/acceptance中的`composition-domain-r12-build.log`、`composition-domain-r12-build-final.log`、`composition-domain-r12-cli.log`、`composition-domain-r12-source.log`、`composition-domain-r12-source-rejoin.log`、`composition-domain-r12-vlc.log`、`composition-domain-r12.sdp`、`composition-domain-r12-2026-09-16-13h38m16s015.png`。四个PID均无残留；保留固定120秒源，无临时录制或远程脚本。

## r13：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30 fps、8 Mbps

完整恢复 **FAIL**。Release全量重建624目标成功。首源PID34416及重入源PID10276均自然退出0，各3600帧/120秒。CLI PID16540在音频错误后进入退出锁等待，未取得自然退出码；VLC PID24224首段截图有效1280×720。

13:54:20.936七组purge全部ack；21.206有序SSRC失效；26.210收到下一份真实SR并锁generation2；26.212生成generation2的MPEG-TS计划；26.218 AudioEncodeNode失败`Audio encoder packet timeline is discontinuous`。此前输入事件顺序错误已越过，但没有持续第二代输出证据。

根因：AudioEncodeLineageState无条件flush并清mapper/FIFO，而当前AAC未声明AV_CODEC_CAP_ENCODER_FLUSH，实流明确警告忽略flush，codec延迟状态仍在。依据[FFmpeg官方契约](https://ffmpeg.org/doxygen/trunk/group__lavc__misc.html)，不能将该调用当重置成功，也不能将null-frame drain当可重用reset。严格时间轴校验保持；需按原计划源/输出域隔离，不通过挪PTS、丢包或单删purge注册绕过。日志未打印具体packet PTS，不能断言具体比较值。

末次运行报告在错误前：CPU424采样、22核，整机占用均值1.191097%、峰3.092784%，单核等效均值26.204136%、峰68.041237%；初始工作集101613568、末次194523136字节。外部17采样最高195362816字节，错误后CPU累计32.84375秒不再增长。payload高水13879301字节/395对象，末次5169305/395，reservations/releases72264/71869；不能声称退出归零。末次error=0早于随后真实worker.failed，不能据此报告无错。generation1漂移118条、raw/filtered绝对最大156ns；generation2仅2条、最大11531ns，不证明恢复同步。sender旧代86345datagrams/104842340payload bytes，取消/压力/部分提交/歧义/期限失败均0，delivery_evidence=not_proven。

退出诊断：非侵入CDB快照后均qd detach，未修改目标状态。主线程等待worker join，多线程等epoch mutex。当前Release没有匹配可用PDB，弃用错误符号名；当前exe RVA与本轮COFF .obj的12字节匹配定位progressPurge+0x8e、classifyReleaseLocked+0x7a→epoch snapshot、startup prepareOutput→reserveCommit及gate reserveCommit。源码中AudioDrift PendingTransaction跨背压持epoch授权，worker退出不清pending，而stop/reset在join之后，符合锁等待链；未直接采样持锁对象地址。另stage持epoch时请求reacquisition存在二次取锁风险。将pending改为仅候选数据，每次原子提交取短授权，统一epoch→state→channel，锁外请求恢复。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r13 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60780 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60782 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61780 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r13.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r13-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60780?rtcpport=60781&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60782?rtcpport=60783&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r13-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r13-vlc.log --extraintf=rc --rc-host=127.0.0.1:62780 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r13- --snapshot-format=png rtp://@127.0.0.1:61780
```

首源自然结束后再次执行同一FFmpeg命令，仅日志改为`composition-domain-r13-source-rejoin.log`。

诊断命令：

```powershell
& 'C:/Program Files/Windows Kits/10/Debuggers/x64/cdb.exe' -pv -p 16540 -c '~* kb; qd'
& 'C:/Program Files/Windows Kits/10/Debuggers/x64/cdb.exe' -y D:/Code/MyCode/MediaTranscode/out/build/x64-release -pv -p 16540 -c '~* kb; qd'
```

### 清理状态

两个源、VLC及诊断器已退出。用户明确授权结束旧死锁CLI PID16540后，已核对可执行文件路径及media-id=composition-domain-r13，再定点强制结束；父执行会话返回-1，属于授权清理，不是自然退出成功。父进程释放文件后删除composition-domain-r13-cli.log。此前已逐项删除其余r13产物，当前r13无进程或临时文件残留，固定120秒源保留。

### 退出锁修复

AudioDrift PendingTransaction现仅保存候选数据，stage和commit分别使用局部授权；commit统一epoch→state→channel锁序，复核generation/origin后原子发布audio/correction并推进servo。输出满即释放全部锁。恢复请求在锁外执行并传播错误；owner finishExecution仅清pending，不重置generation。两位独立审查者均Standards/局部Spec PASS，完整Spec FAIL，评分42/100。既有hard-discontinuity成功请求后返回Cancelled仍是后续域隔离范围，不能声称自动恢复已完成。

## r14：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30 fps、8 Mbps

完整恢复 **FAIL**；短授权修复后的失败退出路径本次正常完成。旧r13占用Release文件且停止例外未获回复，使用既有Debug目录全量重建624目标成功，媒体规格不变。CLI PID35596自然退出1，最终worker=0、queue=0、payload=0/0，74342次reservation/release相等；没有停止CLI。首源PID32340和重入源PID32296均自然退出0，各3600帧/120秒。VLC PID34340由RC quit关闭，首段截图有效1280×720。

14:09:28.793七组purge ack；34.093新MPEG-TS计划generation2；34.111保留AAC时间轴不连续的真实失败；sender完成generation2清理。第二代只提交3个datagram/600payload bytes，不能解释为视频或音频恢复通过。累计86192datagrams/104655080payload bytes，deadline/pressure/partial/ambiguous/cancelled均0、delivery_evidence=not_proven。

CPU419采样、22核，进程整机均值3.863181%、峰22.857143%，单核等效84.989972%/502.857143%；Debug性能不能与Release直接对比。工作集136654848→215486464字节，payload高水13968970字节/400对象，errors/workerErrors=1/1、stalledIntervals=1，pressure=0。generation1漂移118条、raw/filtered最大156ns；generation2仅2条、raw最大8816ns、filtered最大8815ns，缺少持续恢复证据。VLC画面不证明播放端A/V同步；完整输出域、静音/黑场及跨平台验证仍待实施。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-debug/media_transcode_realtime_video_cli.exe --media-id composition-domain-r14 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60790 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60792 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61790 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r14.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r14-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60790?rtcpport=60791&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60792?rtcpport=60793&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r14-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r14-vlc.log --extraintf=rc --rc-host=127.0.0.1:62790 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r14- --snapshot-format=png rtp://@127.0.0.1:61790
```

首源自然结束后再次执行同一FFmpeg命令，仅日志改为`composition-domain-r14-source-rejoin.log`。两源退出码均由执行会话取得0。结果归档后逐项删除r14 build/cli/source/source-rejoin/vlc日志、SDP及截图composition-domain-r14-2026-09-16-14h08m19s937.png；CLI/两源/VLC四个PID均确认不存在，固定120秒源保留。本轮无抓包或临时测试脚本。

## r15：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30 fps、8 Mbps

用户已澄清：停止源流观察CLI属于测试阶段；测试已结束时直接核对精确PID清理残留进程，无需再次确认，清理退出不能算自然退出或通过。已写入AGENTS.md；r13旧进程及日志已清理。

生产源码仍为52c014c0，Release全量重建624目标成功。完整恢复 **FAIL**，但短授权失败退出路径在Release实流得到验证：CLI PID31496自然退出1，worker/queue/payload均归零，72362次申请/释放相等，errors/workerErrors=1/1、stalledIntervals=1。未强制结束本轮CLI。首源PID30260退出0、3600帧/120秒；重入源PID34220亦自然退出0、3600帧/120秒。VLC PID34980由RC quit关闭，首段截图有效1280×720。

14:47:14.540七组purge全部ack，19.839生成generation2的MPEG-TS计划，19.844再次明确报AAC packet timeline discontinuous。第二代仅3datagrams/600payload bytes，不是持续媒体恢复。旧代materialized/scheduled=86316、submitted/committed=86263，差53与backlog_cancelled_datagrams=53一致，取消wire bytes=70740；pacing_reserved=86264、cancelled=1、submitted=86263。该日志覆盖一次非空未提交尾部取消，不能外推全部并发/压力矩阵。新代账本独立归零，累计86266datagrams/104740172payload bytes；deadline/pressure/partial/ambiguous均0、delivery_evidence=not_proven。

CPU399采样、22核，进程整机均值1.475016%、峰4.966140%，单核等效32.450346%/109.255079%；工作集112369664→193716224字节，未证明长期稳定。payload高水13900834字节/398对象，pressure=0。第一代漂移118条、raw/filtered绝对最大156ns，第二代仅2条、最大11531ns；不代表播放端同步或恢复通过。AAC生命周期、独立输出域、黑场/静音、多源及Windows→RKMPP验收仍待完成。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r15 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60780 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60782 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61780 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r15.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r15-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60780?rtcpport=60781&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60782?rtcpport=60783&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r15-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r15-vlc.log --extraintf=rc --rc-host=127.0.0.1:62780 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r15- --snapshot-format=png rtp://@127.0.0.1:61780
```

首源自然结束后再次执行同一FFmpeg命令，仅日志改为`composition-domain-r15-source-rejoin.log`。两次源执行会话均取得exit0，各3600帧/120秒。外部进程监控18轮、其中9轮取得CLI存活采样。两名独立审查者核对原始日志，局部证据PASS、完整恢复FAIL、评分42/100。结果归档后逐项删除r15 build/cli/source/source-rejoin/vlc日志、SDP及截图composition-domain-r15-2026-09-16-14h46m15s049.png；本轮CLI/两源/VLC和旧r13 PID16540均确认不存在。固定120秒源保留，无本轮临时脚本或抓包。
