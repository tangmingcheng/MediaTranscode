# 独立输出 runtime 装配

## 设计

基线d86523ac。现有runtime planner把音频FIFO和协议装配放在源校正之后，完整facts resolver又要求源clock/servo。提取纯输出timing事实及独立MediaRealtimeAvOutputRuntimePlanner，消费解析音频、输入音频块上界、已规划队列/边界、输出同步政策、prepared emission及deployment，复用原FIFO和协议planner形成拥有型输出runtime。输出activation lead保留协议planner真实结果。

生产caller只生成一次纯输出同步政策；源共享规划消费其独立值副本并加入真实源政策，原输出政策移交给输出runtime装配。旧共享runtime从该结果领取原协议/FIFO产品，源assembly/correction/transition保留；不从混合共享计划删除字段伪装纯输出。纯输出timing resolver只读取编码/packetization/TS事实，原完整facts resolver共用它；copy/transcode原校验继续保留。

沿用[GStreamer时钟与逐流映射职责](https://gstreamer.freedesktop.org/documentation/additional/design/synchronisation.html)及[FFmpeg音频FIFO API](https://ffmpeg.org/doxygen/trunk/group__lavu__audiofifo.html)边界，算法、样本容量公式和写入前硬界不变。此项不新增线程、批次、策略常量、默认值、公开参数或平台链。请求借用只存在于同步规划栈，返回产品拥有所需元数据；缺失或冲突产品在构图前失败。源与输出政策的独立副本表达不同域，不建立第二编码器或发送权威。

后续仍需源专用在途界、真实全源准备与唯一encoder资源事务、总账、纯视频源和多源公共协调器。原共享链回归不替代独立多源运行及Windows→RKMPP门禁。

## 验证

11源码冻结后双独立首审均指出输出timing缺少与真实协议产品的交叉校验，复用resolveOutput重算并对四字段完整比较后，两者Standards/阶段Spec均PASS。所有检查在协议资源move前完成。Release全量session68562及修复后51368均configure/build exit0，649项构建图，最终实时CLI更新于2026-09-28 14:00:39、5115904bytes。本轮没有运行local。

### r40 结果：完整验收FAIL

FFmpeg PID38068/session93104自然exit0，3600帧/120.00秒；CLI PID32636/session51378自然exit1，无进展超时，未强制结束。VLC PID32484启动命令exit0，已实际查看14h02m25s404截图为1280×720游戏画面；日志有playback迟到69193、picture迟到22ms及Failed to create video converter。

14:03:50.827 purge_ack=complete、7ack、old1/next2进入acquiring；14:03:56.116 abort.begin，56.155 abort.done。edge46/47均10/10；最终queued/workers/payloadBytes=0，逻辑对象4，reservations71793/releases71789，高水12242289bytes/90objects；workerErrors/errors/pressureFailures=0。stalledIntervals最终为0，不覆盖CLI明确报告的无进展失败。

416个CPU采样/22核：整机均值1.335249%、峰4.697987%；单核29.375476%/103.355705%。WS初始115167232、最终189816832、峰194433024bytes。后台5秒监控session4958 exit0：14:02:13 CPU8.078125s/WS192806912/Private400465920；14:03:53 CPU36.71875s/WS189816832/Private396828672，Private采样峰401469440。118条generation1 drift raw/filtered绝对最大156ns，不证明恢复。sender提交86229datagrams/104704084payloadbytes，deadline/pressure/partial/ambiguous=0，pacing_cancelled=1，delivery_evidence=not_proven。

该单源生产路径消费新输出runtime装配；没有运行独立多源协调器、其他协议或RKMPP。旧无进展及AAC重入未修复，完整合屏仍FAIL/42。四个逻辑对象不等于进程退出后的物理泄漏证明。

已将实际命令、结果和必要诊断归档。随后按精确清单清理3日志、SDP及composition-output-assembly-r40-2026-09-28-14h02m25s404.png共5项，VLC核对身份后按PID32484清理；主动清理不计自然退出。固定120秒源保留，无抓包、临时录制或远程产物。


### r40 实际执行命令

输入RTP H.264/AAC，输出MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps，AAC CBR192kbps、44100Hz双声道。固定120秒源，无循环、无local测试。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-output-assembly-r40 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-assembly-r40.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-assembly-r40-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-assembly-r40-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-assembly-r40-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-output-assembly-r40- --snapshot-format=png rtp://@127.0.0.1:62020
```
