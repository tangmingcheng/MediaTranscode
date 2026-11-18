# rk508：四源独立音频失源恢复与VLC录制（FAIL：静音/恢复有证据，后续BYE错误终止）

沿用rk498产物与H264 VBR8000/12000kbps 1280×720/30、AAC192kbps MPEGTS/RTP；所选音频独立FFmpeg进程，故障注入，不宣称独立源内容A/V同步。Windows源视频35852/6860、音频35212、VLC36476，远程runner1458088。VLC场景滤镜每900帧保存原生画面，接收录制供离线静音核查，不用系统截图。

## 实际命令

~~~bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk508
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk508 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk508/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
cat /proc/$cli/maps >cli-maps.txt 2>/dev/null
printf 'SOURCE0=%s SOURCE1=%s CLI=%s\n' "$s0" "$s1" "$cli" >pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; { date -Ins; ls -l /proc/$cli/fd; cat /proc/$cli/net/udp; } >>socket-monitor.log 2>&1; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" >>pids.txt
wait "$cli"; printf 'CLI_EXIT=%s\n' "$?" >result.txt
wait "$mon"
cat /proc/net/snmp >udp-after.txt
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" >>result.txt
wait "$s1"; printf 'SOURCE1_EXIT=%s\n' "$?" >>result.txt
printf 'RUN_FINISHED=1\n' >>result.txt


~~~

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk508-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk508-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk508-received.ts}}" --sout-keep --video-filter=scene --scene-format=png --scene-ratio=900 --scene-prefix=composition-rk508-frame- --scene-path=D:/Code/MyCode/MediaTranscode/out/acceptance rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk508-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk508-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk508-audio.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk508-audio.log' -PassThru; Write-Output "AUDIO_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk508-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk508-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode

~~~

## 结果与修复边界

远端源1458091/1458092、CLI1458093、monitor1458095。UTC06:02:04.8700049停止音频35212，首次重入35620时原视频已结束，不能称音频独立恢复；补四视频重入：Windows3384/36572，远端1470794/1470795，命令与初轮相同，仅日志增加-reentry。UTC06:03:55.3754918再次仅停音频35620，重启同命令audio-reentry2的4336；此时视频3384仍运行（time72.16），组随后发布generation3，内部drift4535ns。全部视频源每轮完整120秒；前两次音频是人工故障注入终止，最后音频完整120秒。

VLC原生scene画面已查看：02701所选左下黑而其他三格继续；04501所选格恢复；06301四格恢复；09001全黑。VLC quit刷新录制后，离线ffmpeg -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk508-received.ts -map 0:a:0 -af silencedetect=noise=-60dB:d=0.5 -f null - exit0。315.42秒录制的低于-60dB区间：75.259864–132.187392（56.927528秒）、182.391474–202.198095（19.806621秒）、250.120363–315.416463（65.2961秒）。中间恢复非静音支持音频恢复，但不等于内容A/V或绝对零静音证明。

14:04:29.766所选组SourceDrained从3→4并Acquiring；14:05:35.447收到后续audio BYE时node53首错Published RTP departure does not match its activated clock/member identity，CLI自然exit1，完整音频生命周期FAIL。最终errors4、payload413258匹配归零、pressure5，不能据此交付通过。

设计依据：[RFC3550 §6.3.4/6.6](https://www.rfc-editor.org/rfc/rfc3550)将BYE作用于对应SSRC成员；不能因保留的旧播放epoch而丢弃当前协议成员的BYE。独立源码调查确认consumeDeparture误将旧published epoch的Retired分类视作身份损坏。最小修复：Retired返回nullopt，复用consume中既有validate/credit/prepareRtcp(InvalidateMember)/commitControl/ordinal路径；不是直接丢包，也不新建状态机。Target身份检查、Future失败、锁顺序、内存额度与错误传播不变。共享Windows/RKMPP逻辑均须验证视频先结束、音频稍后BYE。

~~~text
[2026-11-18 14:01:34.916] [info] [graph][runtime.node] av_reacquisition group=composition-rk508.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-18 14:02:03.236] [info] [graph][runtime.node] av_reacquisition group=composition-rk508.source.3 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 14:02:18.588] [info] [graph][runtime.node] av_reacquisition group=composition-rk508.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 14:02:18.589] [info] [graph][runtime.node] av_reacquisition group=composition-rk508.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 14:03:25.063] [info] [graph][runtime.node] av_reacquisition group=composition-rk508.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-18 14:04:29.766] [info] [graph][runtime.node] av_reacquisition group=composition-rk508.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=3 next=4 transition=3
[2026-11-18 14:04:31.137] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:04:36.144] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:04:41.170] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:04:45.061] [info] [graph][runtime.node] av_reacquisition group=composition-rk508.source.3 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=2 next=3 transition=2
[2026-11-18 14:04:46.174] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:04:51.208] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:04:56.223] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:04:59.489] [info] [graph][runtime.node] av_reacquisition group=composition-rk508.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=2 next=3 transition=2
[2026-11-18 14:05:00.985] [info] [graph][runtime.node] av_reacquisition group=composition-rk508.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=2 next=3 transition=2
[2026-11-18 14:05:01.239] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:05:06.265] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:05:11.278] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:05:16.292] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:05:21.307] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:05:26.328] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:05:31.343] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=4 invalidated_generation=3 locked=0
[2026-11-18 14:05:35.447] [info] [graph][runtime.node] worker.failed node=53 error=InvalidArgument: Published RTP departure does not match its activated clock/member identity
[CLI] final runtime report: queued=0, peakQueued=2090, threads=102, processThreads=116, workers=0, workerProgress=683853, workerProcessCalls=2716540, workerWaits=2032676, workerWakeups=1987906, workerErrors=4, errors=4, stalledIntervals=0, cpuSamples=1259, logicalProcessors=8, averageSystemMachineCpuPercent=33.211050, averageProcessMachineCpuPercent=17.411419, peakProcessMachineCpuPercent=43.386243, averageProcessSingleCoreCpuPercent=139.291352, peakProcessSingleCoreCpuPercent=347.089947, initialWorkingSetBytes=60563456, workingSetBytes=79679488, peakWorkingSetBytes=79679488, totalPushed=610977, totalPopped=610956, droppedBuffers=0, encodedPacketsPushed=105911, encodedPacketsPopped=105910, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=20294696, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=991, graphPayloadReservations=413258, graphPayloadReleases=413258, graphPayloadPressureFailures=5, lastInputReceivedAtNs=182484131904123
~~~

清理清单本机：composition-rk508-audio-reentry.log、composition-rk508-audio-reentry.out、composition-rk508-audio-reentry2.log、composition-rk508-audio-reentry2.out、composition-rk508-audio.log、composition-rk508-audio.out、composition-rk508-frame-00001.png、composition-rk508-frame-00901.png、composition-rk508-frame-01801.png、composition-rk508-frame-02701.png、composition-rk508-frame-03601.png、composition-rk508-frame-04501.png、composition-rk508-frame-05401.png、composition-rk508-frame-06301.png、composition-rk508-frame-07201.png、composition-rk508-frame-08101.png、composition-rk508-frame-09001.png、composition-rk508-received.ts、composition-rk508-run.sh、composition-rk508-silence.log、composition-rk508-source2-reentry.log、composition-rk508-source2-reentry.out、composition-rk508-source2.log、composition-rk508-source2.out、composition-rk508-source3-reentry.log、composition-rk508-source3-reentry.out、composition-rk508-source3.log、composition-rk508-source3.out、composition-rk508-vlc.log；远端rk508及本地副本：cli-error.log、cli-maps.txt、cli.log、monitor.log、output.sdp、pids.txt、result.txt、runner.log、socket-monitor.log、source0-reentry.log、source0-reentry.out、source0.log、source0.out、source1-reentry.log、source1-reentry.out、source1.log、source1.out、udp-after.txt、udp-before.txt；远端另有acceptance/composition-rk508-run.sh。

清理完成：本机全部清单项和远程副本、远端19项及脚本逐项删除；本轮本机/目标机记录PID无残留。指定源、示例正式产物和rollback保留。

核心修复冻结后独立A/B均SOURCE PASS；当前仅源码通过，待RKMPP增量构建及Windows同规格复验。目标机回退out/rollback/rk509保存旧源码9473db0cea8775d8a4275e80992fc73c83d8183f2b34c26a11f458a2d59a84fd及旧CLI b4c9d8b39118505b079ef1445b37341e8ef7cf26fc1ec7d64a8318f3e4e44d4a。
