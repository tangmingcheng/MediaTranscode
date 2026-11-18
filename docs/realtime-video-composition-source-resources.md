# 合屏逐源资源事实

基线5b56a035。最终不可变topology在构建和实际资源绑定时调用逐源资源规划；按sourceIndex保存各源输入及producer事实，不再借用唯一输出的整链ledger。

## 依据与实现

[GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)在激活前协商格式、分配大小与数量；[FFmpeg样本格式](https://ffmpeg.org/doxygen/trunk/group__lavu__sampfmts.html)提供样本字节几何。[libswresample](https://ffmpeg.org/doxygen/trunk/group__lswr.html)的下一次输出上界会随补偿改变，因此不能直接把未补偿block当实时校正输出界限。

- sourceMembers与唯一outputMembers对同一最终图逐节点精确覆盖，拒绝重复、遗漏、源索引错误；成员归属不是allocation alias证明。
- 每个Raw RTP视频/独立音频输入保留自己的access-unit envelope与typed ingress；核对实际节点stream、codec、上界、authority及全部ingress产品。其他输入缺demux/PES envelope时明确拒绝，不返回空事实冒充覆盖。
- 视频producer通过共享prefix resolver和既有frame validator取实际stage合同；software用pixelFormat、hardware用surfacePixelFormat计算logical图像字节。设备物理分配继续observed-only。
- 音频decode/trim复用decoder样本几何；resample复用同一校正规划器maximumOutputBlockSamples（包含补偿距离），核对runtime校正产品。原单源resolver共用字节算术并保留原有效值，不能拿其原常规块证明新校正上界。
- 每个真实源producer输出键恰好生成一份事实，同键fanout不重复。未知producer及无输出均拒绝。无新增线程、队列、公共参数或平台媒体链。

## 未完成与验证门禁

本产品只含逐源单位事实及输入ingress产品，不是完整驻留/准入；source startup、decoder/filter独立池、aggregate候选/贡献/metadata、输出与网络总账仍须统一。没有按相同几何去重，也没有相加N整链ledger。FinalCompiler仍拒绝aggregate，公共多源入口、纯视频源、首次pool/readback准备事务及Windows→RKMPP完整矩阵未完成。完整合屏FAIL、42/100。

12源码双独立审查Standards/阶段Spec PASS，完整FAIL42。18文件UTF-8无BOM/CRLF及diff检查通过；原样本字节算法、video prefix分支与frame-credit验证体逐字等价核对通过。Release全量session59978成功（665项构建图），configure/build均exit0；CLI2026-09-29 09:42:02、5140480bytes。原共享单源运行不能证明新多源产品实际执行。

## r48实际realtime复验

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps；AAC CBR192kbps、44100Hz双声道。固定120秒源，无local。CLI31580/session3747、FFmpeg30880/session61210、VLC12236（启动exit0），5秒监控session42021。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-source-resources-r48 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-resources-r48.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-resources-r48-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-resources-r48-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-resources-r48-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-source-resources-r48- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 结果与证据边界

源3600帧/120.00秒自然exit0；CLI自然exit1/no-progress。09:45:09.080 purge7ack complete、old1/next2 acquiring；14.684 abort.begin、14.718 abort.done。edge46/47均10/10；abort前20queued/24objects/263517bytes，final queued/workers/payloadBytes=0、4逻辑对象（reservations71868/releases71864）；高水12059051bytes/82objects，workerErrors/errors/pressureFailures=0、stalledIntervals=1，AAC关闭时2帧残留。

405CPU采样/22核：进程整机口径均值1.343710%、峰3.863636%；单核均值29.561625%、峰85%。runtime WS初始107401216、最终188870656、峰193474560bytes。5秒外部监控19样本exit0，09:43:42.740至09:45:13.093；CPU累计11.34375→36.890625s，WS191791104→188874752、峰193466368，Private401358848→398336000、峰402010112bytes。

118条generation1 raw/filtered漂移绝对最大156ns，不能证明恢复。sender86317datagrams/104810420payloadbytes，deadline/pressure/partial/ambiguous均0，pacingCancelled1/backlogCancelled12、delivery_evidence=not_proven。VLC截图composition-source-resources-r48-2026-09-29-09h43m54s851.png已观察1280×720游戏画面；日志有playback too late92386、picture late48ms，以及Failed to create video converter（仍成功生成已观察截图），不能把截图当持续无丢帧播放证明。

原共享路径成功开始并持续转码，但最终退出门禁未通过；本轮保值提取经过该路径，新多源产品未由此运行证明。完整合屏FAIL42，no-progress/4对象和AAC重入仍待解决。

### 清理清单

先归档上述实际命令/结果/指标，再核验并清理VLC12236（不计自然退出）；CLI31580与源30880已自然结束。精确5文件：composition-source-resources-r48-cli.log、-source.log、-vlc.log、composition-source-resources-r48.sdp及上述PNG。无远程、pcap、临时录制或测试脚本；指定复用源124427809bytes与正式构建产物保留。清理复核完成：5文件与3个精确PID均无残留，指定源大小确认保留。
