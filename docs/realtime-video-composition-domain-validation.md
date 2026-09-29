# 合屏域角色与时钟校验

## 设计范围

基线8099ca3a。validateRuntime、NTP需求解析、bootstrap、输入segment、group runtime、图形校验和视频controller均默认整条单源合同，纯源域缺少输出协议会失败，纯输出域被迫携带源clock。此次让planner产品显式携带内部domain role，缺失或与binding角色冲突直接失败；不新增CLI/API配置。

- SharedSourceOutput：旧单源planner明确产生此角色，原输入、输出、启动、servo和时序校验保留。
- SourceContribution：仅消费真实源clock、启动与音频校正，要求PreserveActivatedOutput，不允许输出协议；不需要本地输出NTP epoch。接收到的RTCP SR映射仍由原RTP输入时钟维护。
- ContinuousOutput：不携带源clock/lifecycle/输入协议/源启动队列/音频servo，只验证输出启动提前量、首帧策略、输出视频时序、观测合同及协议；NTP需求只由真实输出协议确定。视频controller错误信息可不含源clock，不伪造其身份。

binding角色与产品角色由构图及bootstrap双向核对；输入segment拒绝输出域，视频输出controller拒绝源域；group的initial-only/可恢复epoch service必须与角色一致。现有公共单源调用仍共用原路径，不新增线程、队列或平台分叉。角色选择全部在planner，runtime只验证，不fallback。

## 行业与适用边界

[GStreamer synchronisation](https://gstreamer.freedesktop.org/documentation/additional/design/synchronisation.html)区分共享单调时钟与各流timestamp/segment映射；[RFC3550 6.4.1](https://www.rfc-editor.org/rfc/rfc3550#section-6.4.1)描述发送方SR中的RTP/NTP对应。接收源的远端映射不能由本地输出sender epoch代替。本次保留原算法与所有权，仅拆职责校验；不声称已完成新的多源运行或完整状态机验证。

新source/output计划的公共生产形成入口、唯一输出资源准备、跨源总账、纯视频源及完整Windows→RKMPP门禁仍缺。冻结双审、Release及原规格真实链路结果按实际补充。
## 执行记录（2026-09-28）

13源码冻结后两位独立审查者均Standards/阶段Spec PASS，报告domain-validation-review-a/b.md；无新增阻断。Release全量session2536，configure/build exit0，647项构建图完成；两CLI12:33:34，实时CLI SHA256 7F9C2F6A04FC56963EF94F2A6539B7BA7784705857FEA1B6B2B8660EEFF20B6C。构建后仅规范CRLF，生产逻辑未改变。

### r35：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps、AAC CBR192kbps

分别直接执行以下命令，固定120秒源未降规：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-domain-validation-r35 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61000 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61002 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62000 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-validation-r35.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-validation-r35-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61000?rtcpport=61001&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61002?rtcpport=61003&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-validation-r35-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-domain-validation-r35-vlc.log --extraintf=rc --rc-host=127.0.0.1:63000 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-domain-validation-r35- --snapshot-format=png rtp://@127.0.0.1:62000
```

FFmpeg PID17044/session43628自然exit0，3600帧/120.00秒；CLI PID7948/session63481自然exit1，未强制结束。VLC PID19940启动命令exit0；实际查看12h37m59s816截图有1280×720游戏画面，但日志有picture迟到77/43ms和Failed to create video converter，不证明完整通过。

12:39:22.531 purge_ack=complete、7ack、old1/next2、acquiring；12:39:27.802 abort.begin，最终realtime runtime made no progress before timeout。edge46/47均10/10；最终queued/workers/payloadBytes=0，逻辑对象4，reservations71850/releases71846，payload高水12059339bytes/93objects，workerErrors/errors/pressureFailures=0、stall1。原无进展缺陷仍在，验收FAIL，不创建成功验收提交。

413个CPU采样/22逻辑核：整机均值1.250526%峰5.520170%，单核27.511561%/121.443737%；WS初始109744128、最终189747200、峰194375680bytes。后台5秒监控session10583自然exit0，12:37:48 CPU10.84375s/WS191995904/Private401305600，12:39:23 CPU34.5s/WS189747200/Private397393920；监控Private最高401993728。118条generation1 drift raw/filtered绝对最大均156ns，无恢复后媒体证据。sender提交86205datagrams/104674036payloadbytes，deadline/pressure/partial/ambiguous=0、pacing_cancelled=1、delivery_evidence=not_proven。

未运行local、其他输出协议、动态输出、RKMPP、多源矩阵或新域生产入口，不能宣称这些通过。完整合屏保持FAIL/42。

清理清单：CLI/source/VLC三日志、SDP、composition-domain-validation-r35-2026-09-28-12h37m59s816.png共5项；归档后核对VLC身份按PID19940清理并逐项删除。本轮无抓包及远程产物；固定源保留。主动清理VLC不计自然退出。

清理复查：5项文件、本轮前缀和媒体/编译进程残留均为0，固定源存在。
