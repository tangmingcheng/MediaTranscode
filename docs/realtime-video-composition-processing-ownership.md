# 源处理与输出处理归属：实施与 r16 复验

## 实施范围

各输入、启动、转码、调度和协议 segment 显式返回节点归属，realtime builder 不再通过末尾扫描全图填充成员。validator 检查 ID、互斥、完整覆盖和节点角色，未知节点类型直接失败。registrar 按明确的 release extractor、准备 owner 和 sequencer 角色注入准备状态，并使用同一归属产品注册 purge 与 wakeup。

当前仍执行两侧整体 generation transition；codec resolver 仍共同准备 decoder/encoder context。该步骤仅建立已被实际消费的结构边界，没有解决 AAC flush、独立输出 origin、黑帧/静音或多源合成。共享解码、源帧 fanout 和编码后 fanout 的归属元数据同步维护，不新增公开输入或平台专用链路。

两名未参与实现的独立审查者均 Standards/局部 Spec PASS，未发现新增 P1/P2；完整合屏 Spec FAIL，维持就绪度 42/100（10/8/6/12/2/4）。工业对照：[GStreamer 同步设计](https://gstreamer.freedesktop.org/documentation/additional/design/synchronisation.html)区分公共 running-time 与源映射；[GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)区分每输入队列与聚合生命周期。本次尚未实现后者的逐输入恢复隔离、GAP 与连续聚合，不能据局部 PASS 判定等价完成。

## 构建

第一次 Release 全量构建触发固定 120 秒截止，退出 1；终止命令报告子进程无法终止，随后确认无 cl/link/ninja/cmake 残留。相同入口 clean-first/all-target 重试成功，配置和构建均退出 0，624 项流程完成；Ninja 最终链接耗时位置为 109.629 秒。没有修改超时门禁，也没有使用旧二进制验收。

## r16 实际命令

链路：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30 fps、8 Mbps，AAC CBR 192 kbps、44100 Hz、双声道 → VLC。固定源经 ffprobe 确认 120 秒、H.264 1280×720 30 fps、8013434 bps，AAC 44100 Hz 双声道。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r16 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60780 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60782 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61780 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r16.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r16-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60780?rtcpport=60781&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60782?rtcpport=60783&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r16-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r16-vlc.log --extraintf=rc --rc-host=127.0.0.1:62780 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r16- --snapshot-format=png rtp://@127.0.0.1:61780
```

## r16 结果：源结束后无进展退出，完整恢复 FAIL

CLI PID30256自然退出1；源PID38796自然退出0，3600帧/120秒；VLC PID13248经RC quit关闭，已实际查看其1280×720画面。16:22:51.876七组purge全部ack；第二源未在退出窗口内启动，16:22:57.197开始退出，错误为realtime runtime made no progress before timeout。本轮没有重入媒体证据，不能当作AAC重入复现。下一轮以首源进程结束事件紧接启动第二源，仍使用完整120秒源。

最终workers/queue=0、payload bytes=0，但对象=4，reservations/releases=71898/71894，不能报告全部资源归零或断流持续输出通过。errors/workerErrors=0不覆盖应用层无进展失败，stalledIntervals=1。CPU421采样、22核：整机均值1.233498%、峰4.328018%，单核等效27.136961%/95.216401%；工作集101629952→189800448，高水194703360字节，不证明长期稳定。payload高水11972048字节/80对象，pressure=0。generation1漂移118条，raw/filtered最大均0ns；不证明播放端A/V同步。

sender提交86346datagrams/104842408payload bytes，cancelled/deadline/pressure/partial/ambiguous均0，delivery_evidence=not_proven。无抓包或临时测试脚本。CPU/内存结论取CLI telemetry，截图由VLC RC snapshot取得。结果归档后按本轮清单删除日志、SDP和截图，检查三个PID已结束，保留固定120秒源。


## r17：启动调度失败，不计媒体验收

r16命令中的文件前缀/media-id全部替换为composition-domain-r17。CLI先启动，源启动晚于30秒open timeout；CLI preflight报告matching_rtp_packets=0、matching_datagram_bytes=0、parameter_sets=none并自然退出1。源PID30716、VLC PID2956随后启动，未建立CLI生产DAG。本轮不能用于判断代码回归、AAC恢复或播放通过；中止源自动重入调度，并按精确PID清理源/VLC，随后删除本轮日志。改进下一轮工具调用时序，不修改媒体参数或超时标准。
## r18：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR，1280×720、30 fps、8 Mbps

完整恢复 **FAIL**。同一源码和Release构建，启动CLI后紧接启动源；首源结束事件立即启动第二源，未新增Windows测试脚本。CLI PID8756自然退出1；首源PID32908已自然退出0、3600帧/120秒。第二源PID27764亦自然退出0、3600帧/120秒。VLC PID31260首段截图已实际查看1280×720，日志记录HEVC D3D11VA，不能证明恢复后播放。

16:30:02.396七组purge全部ack，07.666锁定generation2，07.668生成新MPEG-TS计划，07.674 AudioEncode报Audio encoder packet timeline is discontinuous。AAC同时明确忽略不支持的encoder flush；随后解码及输入取消属于失败后的清理。final errors/workerErrors=2/2、stalledIntervals=0，不把计数理解为两个独立根因。新代仅3datagrams/600payload bytes、access_units=0，未恢复媒体。

最终worker/queue/payload字节和对象均0，72485次申请/释放相等，仅证明该计量边界。CPU422采样、22核：整机均值1.157630%、峰4.028436%，单核等效25.467869%/88.625592%；工作集105680896→194969600字节，不证明长期稳定。payload高水13919067字节/398对象，pressure=0。generation1漂移118条，raw/filtered绝对最大156ns；generation2仅2条，raw最大9200ns、filtered最大9199ns，不证明持续恢复或播放端同步。

旧代materialized/scheduled=86363，submitted/committed=86354，差9等于backlog取消9datagrams/12204wire bytes；pacing取消1次，与积压包取消数量不同。累计86357datagrams/104850548payload bytes，deadline/pressure/partial/ambiguous均0，delivery_evidence=not_proven。两位独立审查者已核对原始日志，局部源码与证据PASS、完整合屏FAIL，不创建通过验收的提交。

### r18 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-r18 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60780 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60782 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61780 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r18.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r18-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60780?rtcpport=60781&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60782?rtcpport=60783&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r18-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-r18-vlc.log --extraintf=rc --rc-host=127.0.0.1:62780 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-r18- --snapshot-format=png rtp://@127.0.0.1:61780
```

重入FFmpeg命令与首源相同，仅日志替换为composition-domain-r18-source-rejoin.log。两次源退出码由各自执行会话取得，CLI退出码由其独立执行会话取得。


后台进程采样17轮，7轮取得CLI存活记录，外部工作集采样最高194424832字节；CPU与A/V结论仍取完整CLI telemetry。VLC通过RC quit关闭，结果归档后逐项清除r18 CLI/两源/VLC日志、SDP及截图composition-domain-r18-2026-09-16-16h28m50s779.png；确认本轮四个PID均不存在。r16/r17也无本轮临时产物残留，固定120秒源保留，无抓包或临时测试脚本。
