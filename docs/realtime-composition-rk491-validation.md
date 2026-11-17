# rk491：音频关闭修正后四屏（FAIL，控制代次待查）

源码820b33c6；CLI SHA256 efb1df948384516ea6846c242c79e29108845c092d588edd7dc146bc4310842c。launcher3580211；VLC15044、源2=38952、源3=42876。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk491
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk491 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk491/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk491-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk491-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk491-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk491-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk491-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk491-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
~~~

远端启动：bash -n acceptance/rk491/run.sh && bash acceptance/rk491/run.sh >acceptance/rk491/launcher.log 2>&1 &。



## 结果与证据

源0=3580215、源1=3580216、CLI3580217、monitor3580219。四源120秒exit0，CLI自然exit1。首错VideoPreparationNode25及44拒绝控制类型或代次；先有old1→next2 purge事件，仍需查明具体控制代次。未到达selected audio终止，不能宣称音频修复实测通过。全黑/重入未完成。外部118样本CPU最大150%单核口径，RSS最大88276KiB。

VLC TCP42120/UDP62024均核对PID15044；原生快照composition-rk491-native-2026-11-17-12h58m26s762.png已查看四格，左下frame889/pts29.633333。各源启动时刻不同，不把画面差异自行判为同源同步通过。

~~~text
[2026-11-17 12:57:24.135] [info] [graph][runtime.node] register node=25 name=composition-rk491.source_0.video.preparation kind=VideoPreparation
[2026-11-17 12:57:24.135] [info] [graph][runtime.node] register node=44 name=composition-rk491.source_1.video.preparation kind=VideoPreparation
[2026-11-17 12:59:12.497] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 12:59:12.497] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 12:59:17.502] [info] [graph][runtime.node] av_reacquisition group=composition-rk491.source.0 phase=purging old=1 next=2 transition=1
[2026-11-17 12:59:17.502] [info] [graph][runtime.node] av_reacquisition group=composition-rk491.source.1 phase=purging old=1 next=2 transition=1
[2026-11-17 12:59:17.503] [info] [graph][runtime.node] rtp_ingress_purge node=8 transition=1 cutoff=0
[2026-11-17 12:59:17.503] [info] [graph][runtime.node] rtp_ingress_purge node=27 transition=1 cutoff=0
[2026-11-17 12:59:17.503] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3626
[2026-11-17 12:59:17.503] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3626
[2026-11-17 12:59:17.503] [info] [graph][runtime.node] process.failed node=25 name=VideoPreparationNode kind=VideoPreparation inputs=[channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0] outputs=[channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=76] error=InvalidArgument: Video preparation control kind or generation is invalid
[2026-11-17 12:59:17.503] [info] [graph][runtime.node] worker.failed node=25 error=InvalidArgument: Video preparation control kind or generation is invalid
[2026-11-17 12:59:17.505] [info] [graph][runtime.node] process.failed node=102 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=134 edge=134 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=146 edge=146 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=Cancelled: scheduled datagram sender reservation wait was interrupted
[2026-11-17 12:59:17.504] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=478720412 process_calls=10666
[2026-11-17 12:59:17.504] [info] [graph][runtime.node] process.failed node=44 name=VideoPreparationNode kind=VideoPreparation inputs=[channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0] outputs=[channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=76] error=InvalidArgument: Video preparation control kind or generation is invalid
[2026-11-17 12:59:17.507] [info] [graph][runtime.node] worker.secondary_failure node=44 error=InvalidArgument: Video preparation control kind or generation is invalid
[2026-11-17 12:59:17.509] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=465436266 process_calls=10662
[2026-11-17 12:59:17.550] [info] [graph][runtime.node] process.failed node=6 name=MediaAvContinuousAggregateNode kind=AvContinuousAggregate inputs=[channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0; channel=29 edge=29 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0; channel=55 edge=55 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=96 edge=96 stream=Video edge_kind=RawFrame payload=Frame queued=72; channel=97 edge=97 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=123 edge=123 stream=Video edge_kind=RawFrame payload=Frame queued=0; channel=124 edge=124 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=125 edge=125 stream=Video edge_kind=Metadata payload=CodecContext queued=0] outputs=[channel=127 edge=127 stream=Video edge_kind=RawFrame payload=Frame queued=0; channel=129 edge=129 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=133 edge=133 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=135 edge=135 stream=Metadata edge_kind=Event payload=GraphEvent queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-11-17 12:59:17.560] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=243 after_objects=243 before_bytes=1408501 after_bytes=1408501 before_reservations=165431 after_reservations=165431 before_releases=165188 after_releases=165188
[2026-11-17 12:59:17.560] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=243 after_objects=243 before_bytes=1408501 after_bytes=1408501 before_reservations=165431 after_reservations=165431 before_releases=165188 after_releases=165188
[CLI] final runtime report: queued=0, peakQueued=2024, threads=102, processThreads=132, workers=0, workerProgress=328175, workerProcessCalls=1361215, workerWaits=1033029, workerWakeups=1021066, workerErrors=2, errors=2, stalledIntervals=0, cpuSamples=450, logicalProcessors=8, averageSystemMachineCpuPercent=41.718364, averageProcessMachineCpuPercent=21.596441, peakProcessMachineCpuPercent=55.500000, averageProcessSingleCoreCpuPercent=172.771531, peakProcessSingleCoreCpuPercent=444.000000, initialWorkingSetBytes=72556544, workingSetBytes=90394624, peakWorkingSetBytes=90394624, totalPushed=301193, totalPopped=300799, droppedBuffers=0, encodedPacketsPushed=37966, encodedPacketsPopped=37965, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=18025371, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=1246, graphPayloadReservations=165431, graphPayloadReleases=165431, graphPayloadPressureFailures=1, lastInputReceivedAtNs=92106185424388
[2026-11-17 12:59:15.890] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5041 source_master_ns=130411291199 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=234497 effective_output_sample_index=5393431
~~~

清理清单：本机composition-rk491-native-2026-11-17-12h58m26s762.png、composition-rk491-source2.log、composition-rk491-source2.out、composition-rk491-source3.log、composition-rk491-source3.out、composition-rk491-vlc.log、rk491-cli.log、rk491-run.sh；远端acceptance/rk491/{socket-monitor.log,udp-after.txt,output.sdp,source0.out,cli-error.log,monitor.log,result.txt,pids.txt,launcher.log,run.sh,cli.log,source1.out,source0.log,udp-before.txt,source1.log,cli-maps.txt}及out/acceptance/rk491-run.sh。VLC按命令行核对PID15044后清理，不记自然退出。

清理复核：两侧本轮临时清单为空、远端精确PID无残留；本机提权CIM无VLC/FFmpeg残留，复用源及回退保留。
