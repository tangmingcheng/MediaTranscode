# 合屏源恢复参与者规划

## 设计与证据

基线958b1a68。单源generation planner把源处理、encoder和sender放在同一恢复集合；合屏builder原校验仅检查参与者类别及aggregate_source存在，不能拒绝CanonicalLineage中混入video_encode、遗漏decode、重复子项或遗漏AudioCorrection。这些错误直到runtime assembler seal才会暴露。

按原源/输出域设计增加planner的源贡献合同：复用单源相同的源处理子集合，额外纳入aggregate_source，仅允许CanonicalLineage和AudioCorrection；不含输出encoder、scheduler、协议和sender。原单源使用同一公共子集合加原输出成员，保持顺序。合屏builder在添加任何节点前用planner产品逐项验证完整集合及正的超时事实，拒绝错配。

[GStreamer事件设计](https://gstreamer.freedesktop.org/documentation/additional/design/events.html)要求flush解除等待并清除旧数据后才能恢复；[GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)分别管理sink pad的队列和事件。这支持按输入贡献划分清理责任，不能据此声称本项目purge/ack状态机已与其运行验证等价。本轮不修改现有purge算法，仅将已有节点责任形成精确planner合同；逐源清理aggregate通过已有generationPurgeRegistrationForSource注册。

不新增线程、队列、分配池、默认容量、平台实现或公共参数；旧参与者顺序、超时及失败传播保持。源合同明确针对当前双轨frame-transcode分支，纯视频源仍待原计划共同组件支持，不能伪造音轨。完整事务、跨源总账和多源公共入口仍未完成。

## 验证

冻结后双独立源码审查、Release全量和原规格实时回归按实际结果记录；旧单源回归不能证明尚未接通的合屏入口或恢复功能。

## 执行记录（2026-09-28）

4源码冻结后双独立Standards/阶段Spec PASS，无新增阻断。首次Release全量configure/build exit0，647项构建图完成，实时CLI12:16:25，4源码hash保持。源码审查不代表新合屏恢复已运行。

### r34：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps、AAC CBR192kbps

分别实际执行，固定120秒源未降规：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-source-transition-r34 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60990 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60992 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61990 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-transition-r34.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-transition-r34-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60990?rtcpport=60991&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60992?rtcpport=60993&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-transition-r34-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-transition-r34-vlc.log --extraintf=rc --rc-host=127.0.0.1:62990 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-source-transition-r34- --snapshot-format=png rtp://@127.0.0.1:61990
```

FFmpeg PID35808/session19849自然exit0，3600帧/120.00秒；CLI PID24368/session98662自然exit1，未强制结束。VLC PID2600启动命令exit0，实际查看composition-source-transition-r34-2026-09-28-12h17m42s973.png有1280×720游戏画面；VLC有picture迟到68/34ms、Failed to create video converter。画面不能证明完整通过。

12:19:10.922 purge_ack=complete，7ack，old1/next2，acquiring；12:19:16.216 abort.begin，edge46/47均10/10，最终realtime runtime made no progress before timeout。最终queued/workers/payloadBytes=0，逻辑对象4，reservations71890/releases71886，高水12197646bytes/76objects，workerErrors/errors/pressureFailures=0、stall1。原无进展缺陷未修复，验收FAIL，无成功验收提交。

426个CPU采样/22逻辑核：整机均值1.193069%峰3.274559%，单核26.247521%/72.040302%；WS初始107524096、最终188489728、峰193015808bytes。后台5秒监测session53415自然exit0：12:17:30 CPU5.546875s/WS190111744/Private395526144，12:19:16 CPU32.953125s/WS160690176/Private348176384（终止过程瞬时值）。118条generation1 drift raw/filtered绝对最大均156ns，没有恢复后持续媒体证据。

sender提交86345datagrams/104843036payloadbytes，deadline/pressure/partial/ambiguous=0，pacing_cancelled=1，delivery_evidence=not_proven。未运行local、Separate RTP/UDP、动态输出、RKMPP、多源矩阵或新source transition生产入口，不得宣称这些通过。

清理清单：CLI/source/VLC三日志、SDP、上述PNG共5项。先归档本节再核对VLC身份、按精确PID清理并逐项删除；本轮无抓包或远程产物，固定120秒源保留。

清理完成复查：上述5项文件、r34前缀残留及本轮媒体/编译进程均为0，固定源存在。VLC为测试结束后主动清理，不计自然退出。
