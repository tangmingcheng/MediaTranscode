# rk496：四路失源控制身份复验（FAIL，启动边界失效）

源码 466557ed；目标 CLI SHA256 94b3d203c81cba2e0855196f25f90414155fcbda8ff7687bdad2d1117fb783f7。只运行 realtime，未修改 Windows FFmpeg。

输入三路 H264、一路 HEVC，所选源2 AAC；输出 MPEGTS/RTP H264 1280×720 30fps VBR 目标/最低8000kbps、最高12000kbps，AAC 192kbps。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk496
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk496 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk496/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk496-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk496-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk496-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk496-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk496-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk496-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

远端启动：bash -n acceptance/rk496/run.sh && bash acceptance/rk496/run.sh >acceptance/rk496/launcher.log 2>&1 &。



## 现场结果

launcher1149760；源0=1149764、源1=1149765、CLI=1149766、monitor=1149768；本机源2=38900、源3=3508、VLC=39456。VLC原生快照四格正常，左下frame1504/pts50.133333。CLI自然exit1，远端两源exit0；本机两源完成3600帧。首错node36 startup generation boundary，发生在source1/0 HardDiscontinuity purge之后；没有reservation_failed，也没有video_preparation_control_rejected。未证明全黑或重入成功。

~~~text
[2026-11-18 12:51:48.837] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-18 12:51:48.840] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-18 12:51:53.839] [info] [graph][runtime.node] av_reacquisition group=composition-rk496.source.1 phase=purging old=1 next=2 transition=1 reason=1
[2026-11-18 12:51:53.845] [info] [graph][runtime.node] av_reacquisition group=composition-rk496.source.0 phase=purging old=1 next=2 transition=1 reason=1
[2026-11-18 12:51:53.851] [info] [graph][runtime.node] worker.failed node=36 error=InvalidArgument: Startup generation boundary requires sealed input and its fully released frozen epoch
[CLI] final runtime report: queued=0, peakQueued=2270, threads=102, processThreads=148, workers=0, workerProgress=324708, workerProcessCalls=1336856, workerWaits=1012135, workerWakeups=1000329, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=442, logicalProcessors=8, averageSystemMachineCpuPercent=36.907936, averageProcessMachineCpuPercent=23.010336, peakProcessMachineCpuPercent=59.016393, averageProcessSingleCoreCpuPercent=184.082685, peakProcessSingleCoreCpuPercent=472.131148, initialWorkingSetBytes=80515072, workingSetBytes=99495936, peakWorkingSetBytes=99495936, totalPushed=298122, totalPopped=297489, droppedBuffers=0, encodedPacketsPushed=37244, encodedPacketsPopped=37243, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=21482823, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=1567, graphPayloadReservations=145380, graphPayloadReleases=145380, graphPayloadPressureFailures=4, lastInputReceivedAtNs=178062507849123
[2026-11-18 12:51:52.912] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4975 source_master_ns=131021950068 raw_phase_ns=11338 filtered_phase_ns=11337 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=231425 effective_output_sample_index=5322775
~~~

后续审查启动协调节点purge后输入FIFO旧EndGeneration与frozen epoch约束；当前日志未打印该控制代次，不能仅凭时间先后宣称唯一根因。

清理清单：本机composition-rk496-native-2026-11-18-12h51m28s595.png、composition-rk496-source2.log/out、composition-rk496-source3.log/out、composition-rk496-vlc.log、rk496-cli.log；目标acceptance/rk496内run.sh、launcher.log、pids.txt、result.txt、cli.log、cli-error.log、cli-maps.txt、monitor.log、socket-monitor.log、udp-before.txt、udp-after.txt、output.sdp、source0.out/log、source1.out/log。VLC39456核对身份后清理，不记自然退出。源和CLI已结束，保留复用源及rollback。
