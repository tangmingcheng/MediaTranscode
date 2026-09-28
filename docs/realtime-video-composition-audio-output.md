# 合屏音频编码段独立输入合同

### local-r37：文件 H.264/AAC → MP4 HEVC CBR8Mbps、1280×720/30fps、AAC CBR192kbps

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_local_video_cli.exe --input D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 --output D:/Code/MyCode/MediaTranscode/out/acceptance/composition-audio-output-local-r37.mp4 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-audio-output-local-r37-cli.log 2>&1
exit $LASTEXITCODE
```

2026-09-28 13:15:32–13:15:33，命令自然exit1（1.346秒）。CUDA实际打开encoder后未取得有效retained-frame bound；QSV纹理80070057、帧池初始化失败，最终HardwareUnavailable，未进入生产DAG或输出媒体播放。进程检查时已退出，未捕获PID、CPU、内存采样，不能声称完成运行时监控或A/V验证；本次只提供构建前失败证据。未启动VLC。完整合屏验收仍FAIL，42/100不变。

r37实时5项产物及VLC33716已按清单清理，检查无该轮文件或媒体进程残留，指定源保留。local轮在记录后按清单清理CLI日志及可能生成的同名MP4，清理结果另核对；无远程测试产物。


基线5ae7ea9a。实际构图链发现：CompositionGraphBuilder对输出也调用mapSynchronizedAudioSourceOptions，强制源audioCorrection/servo；AudioEncodeBranchBuilder和AudioPlanOptionApplier又无条件要求sourceStreamIndex。连续输出域禁止这些字段，现有组合不能形成纯输出音频编码段。

按[FFmpeg send/receive API](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)，编码器消费符合已打开context的原始AVFrame，解码源索引不是编码输入事实。采用独立内部MediaAudioOutputEncoderOptions，只含已解析输出格式、边策略、lineage容量、FIFO和输出group。旧单源/源分支与独立输出继续共用同一个节点创建、端口连接和编码配置实现；内部只借用真实源选项，输出不创建虚假源pipeline或servo。源校正校验只作用于真实源段，输出保留规范lineage/FIFO/group校验。

不修改编解码算法、send/receive循环、时间线、线程、背压、队列界限和RAII，不新增公共参数或平台分支。源索引缺失仍在源构图前失败；输出格式及lineage合同缺失同样失败。此项先解除真实构图矛盾，完整runtime产品形成、资源总账、多源准备和Windows→RKMPP门禁仍未完成。

## 执行记录（2026-09-28）

5源码冻结后双独立Standards/阶段Spec PASS，无新增阻断，报告audio-output-review-a/b.md。首次Release全量session13608达到120秒截止、exit1，进程树终止且确认无残留；同入口第二次session9665 configure/build exit0，647项构建图，两CLI13:08:39，冻结后源码逻辑未改。

### r37：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps、AAC CBR192kbps

固定120秒源，分别直接执行：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-audio-output-r37 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-audio-output-r37.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-audio-output-r37-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-audio-output-r37-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-audio-output-r37-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-audio-output-r37- --snapshot-format=png rtp://@127.0.0.1:62020
```

FFmpeg PID37284/session4508自然exit0，3600帧/120.00秒；CLI PID36988/session67665自然exit1，未强制结束。VLC PID33716启动命令exit0；实际查看13h09m54s454截图有1280×720游戏画面，日志有picture迟到66/32ms及Failed to create video converter，不能证明完整通过。

13:11:21.909 purge_ack=complete、7ack、old1/next2、acquiring；13:11:27.180 abort.begin，最终realtime runtime made no progress before timeout。edge46/47均10/10，最终queued/workers/payloadBytes=0，逻辑对象4，reservations71912/releases71908，高水12236928bytes/82objects，workerErrors/errors/pressureFailures=0。最终stalledIntervals=0与明确timeout及stalled edges一并保留，完整验收FAIL，无成功验收提交。

422个CPU采样/22逻辑核：整机均值1.182460%峰4.269663%，单核26.014122%/93.932584%；WS初始108408832、最终188043264、峰192901120bytes。后台5秒监控session83686 exit0，13:09:43 CPU5.8125s/WS190664704/Private397266944，13:11:23 CPU32.234375s/WS188043264/Private394498048；Private监控峰398721024。118条generation1 drift raw/filtered绝对最大0ns，不证明源恢复。sender提交86358datagrams/104859040payloadbytes，deadline/pressure/partial/ambiguous=0、pacing_cancelled=1、delivery_evidence=not_proven。

先归档后清理CLI/source/VLC三日志、SDP、composition-audio-output-r37-2026-09-28-13h09m54s454.png共5项，并核对VLC身份按PID33716清理。本轮无抓包及远程产物，固定源保留；主动清理不计VLC自然退出。其他输出协议、动态输出、RKMPP、多源矩阵及新纯输出入口未运行，不能用共享单源回归代替。
