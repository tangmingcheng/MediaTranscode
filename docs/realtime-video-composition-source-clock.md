# 独立源时钟与装配规划

## 设计

基线400ffc4a。现有源时钟解析被完整输出facts包围，canonical装配藏在共享runtime中，导致逐源准备仍需整条输出draft。提取MediaRealtimeAvSourceClockPlanner，输入明确的源协议、stream/codec、输入合同、音频处理事实及同步政策，返回源时间事实和canonical装配；旧生产facts resolver立即调用它，runtime直接领取同一次规划的装配。源产品不含输出packetization、encoder延迟或协议batch，不新增算法或政策值。

依据[GStreamer同步职责](https://gstreamer.freedesktop.org/documentation/additional/design/synchronisation.html)，逐流buffer到running-time映射与公共时钟不同；[延迟设计](https://gstreamer.freedesktop.org/documentation/additional/design/latency.html)要求按实际链路累计延迟。因此这一步只复用原源时钟/样本持续时间算法，不把原整链encoder/scheduler/protocol在途界冒充源贡献界。源校正容量与完整多源协调器仍须后续闭环。

无新线程、批次、队列容量、公共参数或平台adapter；同步借用仅在调用栈内，返回值拥有元数据与已有prepared引用。原RTP/TS/demux及copy/transcode校验保留，缺失事实在构图前失败。纯视频非音源尚未适配，不伪造音频来通过约束。冻结后双独立审查、300秒Release全量重建及仅realtime原规格验证；不改外部FFmpeg。

首审发现独立入口缺少旧外层的源时钟完整校验，已把原validateInputClock提为共用validateSourceClock：先保护必要optional，再复用原RTP/TS/demux校验。它不需要输出政策或尚未形成的servo commandLead；拒绝非法demux代次、阈值和prepared证据。

## 验证

10源码双独立首审指出源时钟校验缺口，复用原专项校验后两者Standards/阶段Spec均PASS。有效RTP/TS/demux、copy/transcode映射保持；源assembly提前生成不依赖随后补全的commandLead/compensation/frequency。两次Release全量session61980/52468均configure/build exit0，650项构建图；最终CLI于2026-09-28 14:16:53更新，5120512bytes。没有运行local。

### r41：完整验收FAIL

FFmpeg PID8516/session44974自然exit0，3600帧/120.00秒；CLI PID28668/session31076自然exit1，无进展超时，未强制结束。VLC PID38900启动命令exit0，实际查看14h18m07s220截图为1280×720游戏画面；日志含playback迟到71922、picture迟到25ms及Failed to create video converter。

14:19:34.426 purge_ack=complete、7ack、old1/next2进入acquiring；39.880 abort.begin、39.919 abort.done。edge46/47均10/10；最终queued/workers/payloadBytes=0，4逻辑对象，reservations71891/releases71887，高水12097030bytes/83objects；workerErrors/errors/pressureFailures=0，stalledIntervals=1。逻辑对象残留不能直接证明进程退出后的物理泄漏。

399个CPU采样/22核：整机均值1.459903%、峰3.628118%；单核32.117867%/79.818594%。runtime WS初始106061824、最终187990016、峰192839680bytes；独立5秒监控session45390 exit0，WS采样峰192901120，Private峰398622720。14:17:55 CPU6.546875s/WS190644224/Private397078528；14:19:35 CPU39.78125s/WS188010496/Private393887744。118条generation1漂移raw/filtered绝对最大156ns，不证明恢复。sender提交86297datagrams/104785984payloadbytes，deadline/pressure/partial/ambiguous=0，pacing_cancelled=1，delivery_evidence=not_proven。

新源clock planner经过实际共享生产caller和runtime validator；没有运行多源入口、其他协议或RKMPP。旧无进展和AAC重入未宣布修复，完整FAIL/42。下一步仍需源专用校正容量、完整source runtime、唯一资源事务、纯视频源及多源公共协调器。

归档命令与以上证据后，按精确清单清理3日志、SDP及composition-source-clock-r41-2026-09-28-14h18m07s220.png共5项，VLC核对身份后按PID38900清理；主动清理不计自然退出。指定120秒源保留，本轮没有抓包、临时录制或远程产物。


### r41 实际命令

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps，AAC CBR192kbps、44100Hz双声道。分别直接执行：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-source-clock-r41 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-clock-r41.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-clock-r41-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-clock-r41-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-clock-r41-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-source-clock-r41- --snapshot-format=png rtp://@127.0.0.1:62020
```
