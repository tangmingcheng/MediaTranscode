# rk516：RKMPP三源HEVC VBR及纯视频/所选AV恢复

沿用21fc6bb8产物；三路H264，其中第三路含AAC。输出HEVC VBR8–12Mbps、1280×720/30、AAC192kbps MPEG-TS/RTP。验证三格＋第四格黑、全失源、纯视频与所选AV同端口恢复，各源保持完整120秒。尚未通过。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk516
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk516 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk516/output.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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


```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk516-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk516-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk516-received.ts}}" --sout-keep --video-filter=scene --scene-format=png --scene-ratio=300 --scene-prefix=composition-rk516-frame- --scene-path=D:/Code/MyCode/MediaTranscode/out/acceptance rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk516-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk516-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

```

## 失败取证
runner1766821、源1766827/1766828、CLI1766829、monitor1766831；本机AV39168、VLC33964。初始所选AV完整120秒exit0，未执行重入。source2 node60初始startup timeout，编码包始终0；超时后进程仍活超过146秒。初始gdb因sysroot仅??栈，修正set sysroot /及显式file后exit0；主线程abort→worker.join等待0x7f1d7e9b40，Thread19(mt-n23)在avcodec_send_packet内等待。
~~~text
[2026-11-18 15:09:33.903] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-18 15:09:33.904] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-18 15:09:33.904] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
runtime report: queued=152, peakQueued=185, threads=83, processThreads=98, workers=79, workerProgress=216, workerProcessCalls=368, workerWaits=141, workerWakeups=70, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=0, logicalProcessors=8, averageSystemMachineCpuPercent=0.000000, averageProcessMachineCpuPercent=0.000000, peakProcessMachineCpuPercent=0.000000, averageProcessSingleCoreCpuPercent=0.000000, peakProcessSingleCoreCpuPercent=0.000000, initialWorkingSetBytes=38809600, workingSetBytes=38809600, peakWorkingSetBytes=38809600, totalPushed=190, totalPopped=38, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=5595416, graphPayloadHighWaterBytes=5595416, graphPayloadCurrentObjects=184, graphPayloadHighWaterObjects=184, graphPayloadReservations=187, graphPayloadReleases=3, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186322587701399
[CLI] runtime report: queued=14, peakQueued=581, threads=83, processThreads=110, workers=77, workerProgress=1987, workerProcessCalls=4739, workerWaits=2741, workerWakeups=2668, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1, logicalProcessors=8, averageSystemMachineCpuPercent=35.294118, averageProcessMachineCpuPercent=31.550802, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=252.406417, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=43540480, peakWorkingSetBytes=43540480, totalPushed=1850, totalPopped=1836, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=406748, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=251, graphPayloadHighWaterObjects=254, graphPayloadReservations=1017, graphPayloadReleases=766, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186322845863437
[CLI] runtime report: queued=22, peakQueued=581, threads=83, processThreads=110, workers=77, workerProgress=2234, workerProcessCalls=5820, workerWaits=3579, workerWakeups=3500, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=2, logicalProcessors=8, averageSystemMachineCpuPercent=25.755167, averageProcessMachineCpuPercent=21.180806, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=169.446452, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=43540480, peakWorkingSetBytes=43540480, totalPushed=2086, totalPopped=2064, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=631025, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=284, graphPayloadHighWaterObjects=284, graphPayloadReservations=1171, graphPayloadReleases=887, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186323091842377
[CLI] runtime report: queued=32, peakQueued=581, threads=83, processThreads=110, workers=77, workerProgress=2500, workerProcessCalls=7258, workerWaits=4751, workerWakeups=4668, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=3, logicalProcessors=8, averageSystemMachineCpuPercent=27.564377, averageProcessMachineCpuPercent=17.883979, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=143.071828, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=44077056, peakWorkingSetBytes=44077056, totalPushed=2342, totalPopped=2310, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=1053726, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=318, graphPayloadHighWaterObjects=320, graphPayloadReservations=1347, graphPayloadReleases=1029, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186323341818057
[CLI] runtime report: queued=53, peakQueued=595, threads=83, processThreads=110, workers=77, workerProgress=2744, workerProcessCalls=8617, workerWaits=5866, workerWakeups=5777, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=4, logicalProcessors=8, averageSystemMachineCpuPercent=25.170637, averageProcessMachineCpuPercent=15.264836, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=122.118686, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=44077056, peakWorkingSetBytes=44077056, totalPushed=2575, totalPopped=2522, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=1270135, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=350, graphPayloadHighWaterObjects=353, graphPayloadReservations=1511, graphPayloadReleases=1161, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186323586801480
[CLI] runtime report: queued=77, peakQueued=615, threads=83, processThreads=110, workers=77, workerProgress=3015, workerProcessCalls=9945, workerWaits=6923, workerWakeups=6830, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=5, logicalProcessors=8, averageSystemMachineCpuPercent=24.498212, averageProcessMachineCpuPercent=15.190592, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=121.524736, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=44347392, peakWorkingSetBytes=44347392, totalPushed=2835, totalPopped=2758, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=1660663, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=385, graphPayloadHighWaterObjects=386, graphPayloadReservations=1689, graphPayloadReleases=1304, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186323849328180
[CLI] runtime report: queued=98, peakQueued=636, threads=83, processThreads=110, workers=77, workerProgress=3259, workerProcessCalls=10951, workerWaits=7683, workerWakeups=7584, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=6, logicalProcessors=8, averageSystemMachineCpuPercent=23.267226, averageProcessMachineCpuPercent=14.263105, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=114.104838, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=44347392, peakWorkingSetBytes=44347392, totalPushed=3068, totalPopped=2970, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=1761420, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=417, graphPayloadHighWaterObjects=418, graphPayloadReservations=1847, graphPayloadReleases=1430, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186324106397071
[CLI] runtime report: queued=122, peakQueued=660, threads=83, processThreads=110, workers=77, workerProgress=3525, workerProcessCalls=12080, workerWaits=8548, workerWakeups=8442, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=7, logicalProcessors=8, averageSystemMachineCpuPercent=23.572680, averageProcessMachineCpuPercent=14.001580, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=112.012641, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=44609536, peakWorkingSetBytes=44609536, totalPushed=3324, totalPopped=3202, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=1945653, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=451, graphPayloadHighWaterObjects=452, graphPayloadReservations=2023, graphPayloadReleases=1572, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186324359169635
[CLI] runtime report: queued=143, peakQueued=681, threads=83, processThreads=110, workers=77, workerProgress=3794, workerProcessCalls=13303, workerWaits=9498, workerWakeups=9393, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=8, logicalProcessors=8, averageSystemMachineCpuPercent=23.060306, averageProcessMachineCpuPercent=13.632962, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=109.063692, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=45150208, peakWorkingSetBytes=45150208, totalPushed=3581, totalPopped=3438, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=2224818, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=486, graphPayloadHighWaterObjects=488, graphPayloadReservations=2193, graphPayloadReleases=1707, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186324610608091
[CLI] runtime report: queued=166, peakQueued=704, threads=83, processThreads=110, workers=77, workerProgress=4042, workerProcessCalls=15263, workerWaits=11210, workerWakeups=11099, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=9, logicalProcessors=8, averageSystemMachineCpuPercent=22.696505, averageProcessMachineCpuPercent=13.722466, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=109.779729, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=45944832, peakWorkingSetBytes=45944832, totalPushed=3818, totalPopped=3652, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=2779268, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=518, graphPayloadHighWaterObjects=519, graphPayloadReservations=2360, graphPayloadReleases=1842, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186324858751694
[CLI] runtime report: queued=189, peakQueued=725, threads=83, processThreads=110, workers=77, workerProgress=4305, workerProcessCalls=16584, workerWaits=12270, workerWakeups=12154, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=10, logicalProcessors=8, averageSystemMachineCpuPercent=21.886314, averageProcessMachineCpuPercent=13.593463, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=108.747702, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=46215168, peakWorkingSetBytes=46215168, totalPushed=4071, totalPopped=3882, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=3301314, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=552, graphPayloadHighWaterObjects=556, graphPayloadReservations=2533, graphPayloadReleases=1981, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186325111051748
[CLI] runtime report: queued=211, peakQueued=739, threads=83, processThreads=110, workers=77, workerProgress=4544, workerProcessCalls=17799, workerWaits=13248, workerWakeups=13128, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=11, logicalProcessors=8, averageSystemMachineCpuPercent=21.665691, averageProcessMachineCpuPercent=13.340494, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=106.723955, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=46485504, peakWorkingSetBytes=46485504, totalPushed=4299, totalPopped=4088, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=3709327, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=585, graphPayloadHighWaterObjects=585, graphPayloadReservations=2687, graphPayloadReleases=2102, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186325353684668
[CLI] runtime report: queued=235, peakQueued=755, threads=83, processThreads=110, workers=77, workerProgress=4807, workerProcessCalls=19070, workerWaits=14256, workerWakeups=14126, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=12, logicalProcessors=8, averageSystemMachineCpuPercent=21.535941, averageProcessMachineCpuPercent=13.315743, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=106.525944, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=47026176, peakWorkingSetBytes=47026176, totalPushed=4551, totalPopped=4316, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=3865017, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=620, graphPayloadHighWaterObjects=622, graphPayloadReservations=2874, graphPayloadReleases=2254, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186325609001485
[CLI] runtime report: queued=256, peakQueued=774, threads=83, processThreads=110, workers=77, workerProgress=5044, workerProcessCalls=20172, workerWaits=15121, workerWakeups=14987, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=13, logicalProcessors=8, averageSystemMachineCpuPercent=21.326808, averageProcessMachineCpuPercent=13.201298, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=105.610384, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=47288320, peakWorkingSetBytes=47288320, totalPushed=4777, totalPopped=4521, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=4049567, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=652, graphPayloadHighWaterObjects=652, graphPayloadReservations=3031, graphPayloadReleases=2379, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186325864975147
[CLI] runtime report: queued=278, peakQueued=793, threads=83, processThreads=110, workers=77, workerProgress=5302, workerProcessCalls=21332, workerWaits=16023, workerWakeups=15884, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=14, logicalProcessors=8, averageSystemMachineCpuPercent=20.791306, averageProcessMachineCpuPercent=12.980233, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=103.841861, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=47554560, peakWorkingSetBytes=47554560, totalPushed=5023, totalPopped=4745, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=4244557, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=688, graphPayloadHighWaterObjects=688, graphPayloadReservations=3201, graphPayloadReleases=2513, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186326114904115
[2026-11-18 15:09:37.490] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-18 15:09:37.505] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[CLI] runtime report: queued=299, peakQueued=816, threads=83, processThreads=110, workers=77, workerProgress=5538, workerProcessCalls=22423, workerWaits=16875, workerWakeups=16733, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=15, logicalProcessors=8, averageSystemMachineCpuPercent=21.144349, averageProcessMachineCpuPercent=13.093145, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=104.745157, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=47824896, peakWorkingSetBytes=47824896, totalPushed=5246, totalPopped=4947, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=4424790, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=719, graphPayloadHighWaterObjects=719, graphPayloadReservations=3355, graphPayloadReleases=2636, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186326373437199
[CLI] runtime report: queued=323, peakQueued=840, threads=83, processThreads=110, workers=77, workerProgress=5787, workerProcessCalls=23702, workerWaits=17908, workerWakeups=17757, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=16, logicalProcessors=8, averageSystemMachineCpuPercent=21.451817, averageProcessMachineCpuPercent=12.839983, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=102.719861, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=47824896, peakWorkingSetBytes=47824896, totalPushed=5483, totalPopped=5160, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=4649808, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=754, graphPayloadHighWaterObjects=755, graphPayloadReservations=3527, graphPayloadReleases=2773, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186326624948154
[CLI] runtime report: queued=347, peakQueued=864, threads=83, processThreads=110, workers=77, workerProgress=6029, workerProcessCalls=24909, workerWaits=18873, workerWakeups=18716, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=17, logicalProcessors=8, averageSystemMachineCpuPercent=21.340840, averageProcessMachineCpuPercent=12.851953, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=102.815624, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=48365568, peakWorkingSetBytes=48365568, totalPushed=5715, totalPopped=5368, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=4884959, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=788, graphPayloadHighWaterObjects=789, graphPayloadReservations=3701, graphPayloadReleases=2913, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186326875976392
[2026-11-18 15:09:38.295] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[CLI] runtime report: queued=368, peakQueued=886, threads=83, processThreads=110, workers=77, workerProgress=6264, workerProcessCalls=25934, workerWaits=19663, workerWakeups=19503, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=18, logicalProcessors=8, averageSystemMachineCpuPercent=20.953111, averageProcessMachineCpuPercent=12.699421, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=101.595371, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=48627712, peakWorkingSetBytes=48627712, totalPushed=5937, totalPopped=5569, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=5059594, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=820, graphPayloadHighWaterObjects=820, graphPayloadReservations=3850, graphPayloadReleases=3030, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186327122450880
[2026-11-18 15:09:38.561] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[CLI] runtime report: queued=391, peakQueued=910, threads=83, processThreads=110, workers=77, workerProgress=6521, workerProcessCalls=27266, workerWaits=20735, workerWakeups=20574, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=19, logicalProcessors=8, averageSystemMachineCpuPercent=20.914451, averageProcessMachineCpuPercent=12.750041, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=102.000331, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=49438720, peakWorkingSetBytes=49438720, totalPushed=6181, totalPopped=5790, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=5464914, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=854, graphPayloadHighWaterObjects=856, graphPayloadReservations=4019, graphPayloadReleases=3165, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186327376479463
[CLI] runtime report: queued=413, peakQueued=932, threads=83, processThreads=110, workers=77, workerProgress=6746, workerProcessCalls=28282, workerWaits=21529, workerWakeups=21362, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=20, logicalProcessors=8, averageSystemMachineCpuPercent=20.765468, averageProcessMachineCpuPercent=12.628844, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=101.030749, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=49438720, peakWorkingSetBytes=49438720, totalPushed=6395, totalPopped=5982, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=5678341, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=887, graphPayloadHighWaterObjects=888, graphPayloadReservations=4171, graphPayloadReleases=3284, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186327633474435
[CLI] runtime report: queued=436, peakQueued=948, threads=83, processThreads=110, workers=77, workerProgress=6990, workerProcessCalls=29583, workerWaits=22586, workerWakeups=22413, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=21, logicalProcessors=8, averageSystemMachineCpuPercent=20.570287, averageProcessMachineCpuPercent=12.565105, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=100.520836, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=49979392, peakWorkingSetBytes=49979392, totalPushed=6627, totalPopped=6191, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=5909644, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=923, graphPayloadHighWaterObjects=923, graphPayloadReservations=4345, graphPayloadReleases=3422, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186327885825487
[CLI] runtime report: queued=457, peakQueued=966, threads=83, processThreads=110, workers=77, workerProgress=7201, workerProcessCalls=30582, workerWaits=23374, workerWakeups=23195, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=22, logicalProcessors=8, averageSystemMachineCpuPercent=20.231399, averageProcessMachineCpuPercent=12.465896, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=99.727167, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=49979392, peakWorkingSetBytes=49979392, totalPushed=6828, totalPopped=6371, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=6097495, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=954, graphPayloadHighWaterObjects=954, graphPayloadReservations=4497, graphPayloadReleases=3543, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186328131976218
[CLI] runtime report: queued=481, peakQueued=990, threads=83, processThreads=110, workers=77, workerProgress=7435, workerProcessCalls=31694, workerWaits=24252, workerWakeups=24070, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=23, logicalProcessors=8, averageSystemMachineCpuPercent=20.159566, averageProcessMachineCpuPercent=12.446590, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=99.572719, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=50245632, peakWorkingSetBytes=50245632, totalPushed=7052, totalPopped=6571, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=6343057, graphPayloadHighWaterBytes=6399045, graphPayloadCurrentObjects=988, graphPayloadHighWaterObjects=989, graphPayloadReservations=4667, graphPayloadReleases=3679, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186328381536303
[CLI] runtime report: queued=502, peakQueued=1011, threads=83, processThreads=110, workers=77, workerProgress=7671, workerProcessCalls=33384, workerWaits=25705, workerWakeups=25520, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=24, logicalProcessors=8, averageSystemMachineCpuPercent=20.986250, averageProcessMachineCpuPercent=12.291230, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=98.329839, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=50786304, peakWorkingSetBytes=50786304, totalPushed=7277, totalPopped=6775, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=6765811, graphPayloadHighWaterBytes=6786593, graphPayloadCurrentObjects=1022, graphPayloadHighWaterObjects=1024, graphPayloadReservations=4829, graphPayloadReleases=3807, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186328642489596
[CLI] runtime report: queued=524, peakQueued=1019, threads=83, processThreads=110, workers=77, workerProgress=7874, workerProcessCalls=34998, workerWaits=27117, workerWakeups=26928, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=25, logicalProcessors=8, averageSystemMachineCpuPercent=21.222069, averageProcessMachineCpuPercent=12.057645, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=96.461161, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=51056640, peakWorkingSetBytes=51056640, totalPushed=7469, totalPopped=6945, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=7069146, graphPayloadHighWaterBytes=7151817, graphPayloadCurrentObjects=1055, graphPayloadHighWaterObjects=1056, graphPayloadReservations=4912, graphPayloadReleases=3857, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186328876718317
[CLI] runtime report: queued=548, peakQueued=1027, threads=83, processThreads=110, workers=77, workerProgress=8097, workerProcessCalls=36326, workerWaits=28222, workerWakeups=28029, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=26, logicalProcessors=8, averageSystemMachineCpuPercent=21.492792, averageProcessMachineCpuPercent=11.928338, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=95.426702, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=51326976, peakWorkingSetBytes=51326976, totalPushed=7681, totalPopped=7133, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=7319691, graphPayloadHighWaterBytes=7527588, graphPayloadCurrentObjects=1090, graphPayloadHighWaterObjects=1093, graphPayloadReservations=5003, graphPayloadReleases=3913, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186329140726873
[CLI] runtime report: queued=569, peakQueued=1034, threads=83, processThreads=110, workers=77, workerProgress=8299, workerProcessCalls=38005, workerWaits=29699, workerWakeups=29503, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=27, logicalProcessors=8, averageSystemMachineCpuPercent=21.777010, averageProcessMachineCpuPercent=11.718029, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=93.744231, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=51867648, peakWorkingSetBytes=51867648, totalPushed=7872, totalPopped=7303, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=7677210, graphPayloadHighWaterBytes=7677210, graphPayloadCurrentObjects=1122, graphPayloadHighWaterObjects=1122, graphPayloadReservations=5081, graphPayloadReleases=3959, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186329385266282
[CLI] runtime report: queued=591, peakQueued=1042, threads=83, processThreads=110, workers=77, workerProgress=8505, workerProcessCalls=39249, workerWaits=30737, workerWakeups=30537, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=28, logicalProcessors=8, averageSystemMachineCpuPercent=22.535358, averageProcessMachineCpuPercent=11.568345, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=92.546761, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=52133888, peakWorkingSetBytes=52133888, totalPushed=8066, totalPopped=7475, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=7954639, graphPayloadHighWaterBytes=7954639, graphPayloadCurrentObjects=1154, graphPayloadHighWaterObjects=1156, graphPayloadReservations=5167, graphPayloadReleases=4013, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186329642653300
[CLI] runtime report: queued=606, peakQueued=1049, threads=83, processThreads=110, workers=77, workerProgress=8614, workerProcessCalls=40844, workerWaits=32220, workerWakeups=32019, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=29, logicalProcessors=8, averageSystemMachineCpuPercent=22.937950, averageProcessMachineCpuPercent=11.423520, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=91.388161, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=52404224, peakWorkingSetBytes=52404224, totalPushed=8160, totalPopped=7554, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=9037020, graphPayloadHighWaterBytes=9037020, graphPayloadCurrentObjects=1183, graphPayloadHighWaterObjects=1183, graphPayloadReservations=5212, graphPayloadReleases=4029, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186329899637686
[CLI] runtime report: queued=630, peakQueued=1057, threads=83, processThreads=110, workers=77, workerProgress=8724, workerProcessCalls=41815, workerWaits=33082, workerWakeups=32880, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=30, logicalProcessors=8, averageSystemMachineCpuPercent=23.020198, averageProcessMachineCpuPercent=11.313006, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=90.504051, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=52674560, peakWorkingSetBytes=52674560, totalPushed=8259, totalPopped=7629, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=9835204, graphPayloadHighWaterBytes=9835204, graphPayloadCurrentObjects=1217, graphPayloadHighWaterObjects=1217, graphPayloadReservations=5246, graphPayloadReleases=4029, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186330138807538
[CLI] runtime report: queued=647, peakQueued=1060, threads=83, processThreads=110, workers=77, workerProgress=8824, workerProcessCalls=42644, workerWaits=33811, workerWakeups=33609, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=31, logicalProcessors=8, averageSystemMachineCpuPercent=23.300851, averageProcessMachineCpuPercent=11.138844, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=89.110754, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=52944896, peakWorkingSetBytes=52944896, totalPushed=8348, totalPopped=7701, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=10588489, graphPayloadHighWaterBytes=10588489, graphPayloadCurrentObjects=1246, graphPayloadHighWaterObjects=1246, graphPayloadReservations=5275, graphPayloadReleases=4029, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186330397150656
[CLI] runtime report: queued=663, peakQueued=1060, threads=83, processThreads=110, workers=77, workerProgress=8927, workerProcessCalls=43461, workerWaits=34525, workerWakeups=34323, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=32, logicalProcessors=8, averageSystemMachineCpuPercent=23.341142, averageProcessMachineCpuPercent=10.961520, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=87.692163, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=53215232, peakWorkingSetBytes=53215232, totalPushed=8440, totalPopped=7777, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=10935039, graphPayloadHighWaterBytes=10935039, graphPayloadCurrentObjects=1273, graphPayloadHighWaterObjects=1273, graphPayloadReservations=5302, graphPayloadReleases=4029, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186330645986685
[CLI] runtime report: queued=677, peakQueued=1060, threads=83, processThreads=110, workers=77, workerProgress=9019, workerProcessCalls=44379, workerWaits=35349, workerWakeups=35149, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=33, logicalProcessors=8, averageSystemMachineCpuPercent=23.520359, averageProcessMachineCpuPercent=10.774421, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=86.195366, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=53485568, peakWorkingSetBytes=53485568, totalPushed=8522, totalPopped=7845, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=11223109, graphPayloadHighWaterBytes=11223109, graphPayloadCurrentObjects=1297, graphPayloadHighWaterObjects=1297, graphPayloadReservations=5326, graphPayloadReleases=4029, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186330905390113
[CLI] runtime report: queued=693, peakQueued=1060, threads=83, processThreads=110, workers=77, workerProgress=9127, workerProcessCalls=45412, workerWaits=36276, workerWakeups=36074, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=34, logicalProcessors=8, averageSystemMachineCpuPercent=23.598526, averageProcessMachineCpuPercent=10.611514, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=84.892115, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=53485568, peakWorkingSetBytes=53485568, totalPushed=8618, totalPopped=7925, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=11597280, graphPayloadHighWaterBytes=11597280, graphPayloadCurrentObjects=1325, graphPayloadHighWaterObjects=1325, graphPayloadReservations=5354, graphPayloadReleases=4029, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186331153344333
[2026-11-18 15:09:42.518] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-18 15:09:42.519] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[CLI] runtime report: queued=709, peakQueued=1060, threads=83, processThreads=110, workers=77, workerProgress=9245, workerProcessCalls=47138, workerWaits=37884, workerWakeups=37682, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=35, logicalProcessors=8, averageSystemMachineCpuPercent=23.738414, averageProcessMachineCpuPercent=10.461938, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=83.695504, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=54837248, peakWorkingSetBytes=54837248, totalPushed=8722, totalPopped=8013, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=11842013, graphPayloadHighWaterBytes=11842013, graphPayloadCurrentObjects=1351, graphPayloadHighWaterObjects=1351, graphPayloadReservations=5380, graphPayloadReleases=4029, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186331405852261
[CLI] runtime report: queued=723, peakQueued=1062, threads=83, processThreads=110, workers=77, workerProgress=9342, workerProcessCalls=48078, workerWaits=38727, workerWakeups=38525, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=36, logicalProcessors=8, averageSystemMachineCpuPercent=23.780925, averageProcessMachineCpuPercent=10.305737, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=82.445898, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=54837248, peakWorkingSetBytes=54837248, totalPushed=8808, totalPopped=8085, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=12108827, graphPayloadHighWaterBytes=12108827, graphPayloadCurrentObjects=1376, graphPayloadHighWaterObjects=1376, graphPayloadReservations=5405, graphPayloadReleases=4029, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186331649282565
[CLI] runtime report: queued=739, peakQueued=1077, threads=83, processThreads=110, workers=77, workerProgress=9445, workerProcessCalls=49155, workerWaits=39701, workerWakeups=39499, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=37, logicalProcessors=8, averageSystemMachineCpuPercent=23.910398, averageProcessMachineCpuPercent=10.198804, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=81.590432, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=55377920, peakWorkingSetBytes=55377920, totalPushed=8900, totalPopped=8161, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=13005254, graphPayloadHighWaterBytes=13005254, graphPayloadCurrentObjects=1403, graphPayloadHighWaterObjects=1403, graphPayloadReservations=5432, graphPayloadReleases=4029, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186331906075236
[2026-11-18 15:09:43.306] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[CLI] runtime report: queued=753, peakQueued=1091, threads=83, processThreads=110, workers=77, workerProgress=9553, workerProcessCalls=50709, workerWaits=41146, workerWakeups=40945, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=38, logicalProcessors=8, averageSystemMachineCpuPercent=23.984808, averageProcessMachineCpuPercent=10.141504, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=81.132029, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=56184832, peakWorkingSetBytes=56184832, totalPushed=8995, totalPopped=8242, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=13317748, graphPayloadHighWaterBytes=13317748, graphPayloadCurrentObjects=1428, graphPayloadHighWaterObjects=1428, graphPayloadReservations=5457, graphPayloadReleases=4029, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186332162834074
[2026-11-18 15:09:43.586] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[CLI] runtime report: queued=767, peakQueued=1105, threads=83, processThreads=110, workers=77, workerProgress=9667, workerProcessCalls=51583, workerWaits=41907, workerWakeups=41705, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=39, logicalProcessors=8, averageSystemMachineCpuPercent=24.283616, averageProcessMachineCpuPercent=10.045131, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=80.361050, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=38809600, workingSetBytes=56717312, peakWorkingSetBytes=56717312, totalPushed=9096, totalPopped=8329, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=121, graphPayloadCurrentBytes=13818748, graphPayloadHighWaterBytes=13818748, graphPayloadCurrentObjects=1455, graphPayloadHighWaterObjects=1455, graphPayloadReservations=5484, graphPayloadReleases=4029, graphPayloadPressureFailures=0, lastInputReceivedAtNs=186332412505533
[2026-11-18 15:09:43.906] [info] [graph][runtime.node] av_startup_deadline deadline_ns=11456451170 observed_ns=11456528992
[2026-11-18 15:09:43.914] [info] [graph][runtime.node] process.failed node=60 name=MediaAvStartupCoordinatorNode kind=AvStartupCoordinator inputs=[channel=73 edge=73 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=74 edge=74 stream=Metadata edge_kind=Event payload=GraphEvent queued=5; channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=1] outputs=[channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0] error=InvalidArgument: A/V sync code=7 operation=poll source_clock_mode=0 state=3 expected_stream= observed_stream= expected_generation=1 source_epoch_ns=0 running_epoch_ns=0 running_bounds_ns=[0,10000000000] detail=startup timeout
[2026-11-18 15:09:43.914] [info] [graph][runtime.node] worker.failed node=60 error=InvalidArgument: A/V sync code=7 operation=poll source_clock_mode=0 state=3 expected_stream= observed_stream= expected_generation=1 source_epoch_ns=0 running_epoch_ns=0 running_bounds_ns=[0,10000000000] detail=startup timeout
~~~
完整有符号栈：
~~~text
[New LWP 1767134]
[New LWP 1767133]
[New LWP 1767132]
[New LWP 1767131]
[New LWP 1767130]
[New LWP 1767129]
[New LWP 1767128]
[New LWP 1767127]
[New LWP 1767126]
[New LWP 1767125]
[New LWP 1767124]
[New LWP 1767123]
[New LWP 1767122]
[New LWP 1767121]
[New LWP 1767120]
[New LWP 1767119]
[New LWP 1767074]
[New LWP 1767073]
[New LWP 1767035]
[New LWP 1767034]
[New LWP 1767033]
[New LWP 1767032]
[New LWP 1767031]
[New LWP 1767030]
[New LWP 1767029]
[New LWP 1766834]
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/aarch64-linux-gnu/libthread_db.so.1".
__pthread_clockjoin_ex (threadid=545955683136, thread_return=0x0, clockid=0, abstime=<optimized out>, block=<optimized out>) at pthread_join_common.c:145
warning: 145	pthread_join_common.c: 没有那个文件或目录

Thread 27 (Thread 0x7f83578b40 (LWP 1766834) "media_transcode"):
#0  futex_abstimed_wait_cancelable (private=0, abstime=0x7f83578280, clockid=<optimized out>, expected=0, futex_word=0x130d3560) at ../sysdeps/nptl/futex-internal.h:320
#1  __pthread_cond_wait_common (abstime=0x7f83578280, clockid=<optimized out>, mutex=0x130c6120, cond=0x130d3538) at pthread_cond_wait.c:520
#2  __pthread_cond_clockwait (abstime=0x7f83578280, clockid=<optimized out>, mutex=0x130c6120, cond=0x130d3538) at pthread_cond_wait.c:686
#3  __pthread_cond_clockwait (cond=0x130d3538, mutex=0x130c6120, clockid=<optimized out>, abstime=0x7f83578280) at pthread_cond_wait.c:674
#4  0x000000000041ecec in media::ffmpeg::graph::cli::RealtimeStopSignal::State::State(media::ffmpeg::graph::MediaRealtimeVideoRunControl&, std::chrono::duration<long, std::ratio<1l, 1000l> >)::{lambda(std::stop_token)#1}::operator()(std::stop_token) const ()
#5  0x000000000041f1d8 in std::thread::_State_impl<std::thread::_Invoker<std::tuple<media::ffmpeg::graph::cli::RealtimeStopSignal::State::State(media::ffmpeg::graph::MediaRealtimeVideoRunControl&, std::chrono::duration<long, std::ratio<1l, 1000l> >)::{lambda(std::stop_token)#1}, std::stop_token> > >::_M_run() ()
#6  0x0000007f841ebb0c in std::execute_native_thread_routine (__p=0x130ae6b0) at /opt/mt-tools/src/gcc-12.4.0/libstdc++-v3/src/c++11/thread.cc:82
#7  0x0000007f8433f51c in start_thread (arg=0x7fff7609ff) at pthread_create.c:477
#8  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 26 (Thread 0x7f33ffeb40 (LWP 1767029) "mpp_h265e_1766"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x131802c4) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x13180268, cond=0x13180298) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x13180298, mutex=0x13180268) at pthread_cond_wait.c:647
#3  0x0000007f83cccb14 in mpp_thread_wait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f83cecdc8 in mpp_enc_async_thread () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f8433f51c in start_thread (arg=0x7fff734c9f) at pthread_create.c:477
#6  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 25 (Thread 0x7f82d77b40 (LWP 1767030) "mpp_dec_parser"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x131857a4) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x13185748, cond=0x13185778) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x13185778, mutex=0x13185748) at pthread_cond_wait.c:647
#3  0x0000007f83cccb14 in mpp_thread_wait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f83cf2844 in mpp_dec_parser_thread () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f8433f51c in start_thread (arg=0x7fff73599f) at pthread_create.c:477
#6  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 24 (Thread 0x7f82576b40 (LWP 1767031) "mpp_dec_hal"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x130fd460) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x130fd408, cond=0x130fd438) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x130fd438, mutex=0x130fd408) at pthread_cond_wait.c:647
#3  0x0000007f83cccb14 in mpp_thread_wait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f83cf0408 in mpp_dec_hal_thread () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f8433f51c in start_thread (arg=0x7fff73599f) at pthread_create.c:477
#6  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 23 (Thread 0x7f50ff8b40 (LWP 1767032) "mpp_dec_parser"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x131012c4) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x13101268, cond=0x13101298) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x13101298, mutex=0x13101268) at pthread_cond_wait.c:647
#3  0x0000007f83cccb14 in mpp_thread_wait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f83cf2844 in mpp_dec_parser_thread () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f8433f51c in start_thread (arg=0x7fff73599f) at pthread_create.c:477
#6  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 22 (Thread 0x7f4bffeb40 (LWP 1767033) "mpp_dec_hal"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x1311fc84) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x1311fc28, cond=0x1311fc58) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x1311fc58, mutex=0x1311fc28) at pthread_cond_wait.c:647
#3  0x0000007f83cccb14 in mpp_thread_wait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f83cf0408 in mpp_dec_hal_thread () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f8433f51c in start_thread (arg=0x7fff73599f) at pthread_create.c:477
#6  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 21 (Thread 0x7f4affcb40 (LWP 1767034) "mpp_dec_parser"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x13116624) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x131165c8, cond=0x131165f8) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x131165f8, mutex=0x131165c8) at pthread_cond_wait.c:647
#3  0x0000007f83cccb14 in mpp_thread_wait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f83cf2844 in mpp_dec_parser_thread () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f8433f51c in start_thread (arg=0x7fff73599f) at pthread_create.c:477
#6  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 20 (Thread 0x7f4a7fbb40 (LWP 1767035) "mpp_dec_hal"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x13116864) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x13116808, cond=0x13116838) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x13116838, mutex=0x13116808) at pthread_cond_wait.c:647
#3  0x0000007f83cccb14 in mpp_thread_wait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f83cf0408 in mpp_dec_hal_thread () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f8433f51c in start_thread (arg=0x7fff73599f) at pthread_create.c:477
#6  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 19 (Thread 0x7f1d7e9b40 (LWP 1767073) "mt-n23"):
#0  futex_abstimed_wait_cancelable (private=0, abstime=0x7f1d7e8570, clockid=<optimized out>, expected=0, futex_word=0x1310a950) at ../sysdeps/nptl/futex-internal.h:320
#1  __pthread_cond_wait_common (abstime=0x7f1d7e8570, clockid=<optimized out>, mutex=0x1310a8f8, cond=0x1310a928) at pthread_cond_wait.c:520
#2  __pthread_cond_timedwait (cond=0x1310a928, mutex=0x1310a8f8, abstime=0x7f1d7e8570) at pthread_cond_wait.c:665
#3  0x0000007f83ccc7ec in mpp_cond_timedwait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f83cd0880 in mpp_list_wait_timed () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f83c6917c in mpp_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#6  0x0000007f83c6c6fc in mpi_decode_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#7  0x0000007f84f441e8 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#8  0x0000007f84f4538c in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#9  0x0000007f84bcbc24 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#10 0x0000007f84bcc280 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#11 0x0000007f84bcc4cc in avcodec_send_packet () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#12 0x0000000000b3d7a4 in media::ffmpeg::graph::VideoDecodeNode::submitPendingPacket(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#13 0x0000000000b4058c in media::ffmpeg::graph::VideoDecodeNode::onProcess(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#14 0x00000000009d6860 in media::ffmpeg::graph::MediaNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#15 0x00000000009d2b88 in media::ffmpeg::graph::FFmpegNodeRuntime::processImpl(media::ffmpeg::graph::MediaGraphExecutionContext&) ()

Thread 18 (Thread 0x7f1cfe8b40 (LWP 1767074) "mt-n42"):
#0  futex_abstimed_wait_cancelable (private=0, abstime=0x7f1cfe7570, clockid=<optimized out>, expected=0, futex_word=0x1311ae74) at ../sysdeps/nptl/futex-internal.h:320
#1  __pthread_cond_wait_common (abstime=0x7f1cfe7570, clockid=<optimized out>, mutex=0x1311ae18, cond=0x1311ae48) at pthread_cond_wait.c:520
#2  __pthread_cond_timedwait (cond=0x1311ae48, mutex=0x1311ae18, abstime=0x7f1cfe7570) at pthread_cond_wait.c:665
#3  0x0000007f83ccc7ec in mpp_cond_timedwait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f83cd0880 in mpp_list_wait_timed () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f83c6917c in mpp_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#6  0x0000007f83c6c6fc in mpi_decode_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#7  0x0000007f84f441e8 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#8  0x0000007f84f4538c in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#9  0x0000007f84bcbc24 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#10 0x0000007f84bcc280 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#11 0x0000007f84bcc4cc in avcodec_send_packet () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#12 0x0000000000b3d7a4 in media::ffmpeg::graph::VideoDecodeNode::submitPendingPacket(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#13 0x0000000000b4058c in media::ffmpeg::graph::VideoDecodeNode::onProcess(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#14 0x00000000009d6860 in media::ffmpeg::graph::MediaNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#15 0x00000000009d2b88 in media::ffmpeg::graph::FFmpegNodeRuntime::processImpl(media::ffmpeg::graph::MediaGraphExecutionContext&) ()

Thread 17 (Thread 0x7f067bbb40 (LWP 1767119) "mt-n45"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ea8001734) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ea80016d8, cond=0x7ea8001708) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ea8001708, mutex=0x7ea80016d8) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f117d012f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 16 (Thread 0x7f05fbab40 (LWP 1767120) "mt-n45"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ea80017ac) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ea8001750, cond=0x7ea8001780) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ea8001780, mutex=0x7ea8001750) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f117d012f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 15 (Thread 0x7f057b9b40 (LWP 1767121) "mt-n45"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ea8001824) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ea80017c8, cond=0x7ea80017f8) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ea80017f8, mutex=0x7ea80017c8) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f117d012f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 14 (Thread 0x7f04fb8b40 (LWP 1767122) "mt-n45"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ea800189c) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ea8001840, cond=0x7ea8001870) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ea8001870, mutex=0x7ea8001840) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f117d012f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 13 (Thread 0x7e67ffeb40 (LWP 1767123) "mt-n26"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ef8001804) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ef80017a8, cond=0x7ef80017d8) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ef80017d8, mutex=0x7ef80017a8) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f11fd112f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 12 (Thread 0x7e677fdb40 (LWP 1767124) "mt-n45"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ea8001914) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ea80018b8, cond=0x7ea80018e8) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ea80018e8, mutex=0x7ea80018b8) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f117d012f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 11 (Thread 0x7e66ffcb40 (LWP 1767125) "mt-n45"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ea800198c) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ea8001930, cond=0x7ea8001960) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ea8001960, mutex=0x7ea8001930) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f117d012f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 10 (Thread 0x7e667fbb40 (LWP 1767126) "mt-n26"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ef800187c) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ef8001820, cond=0x7ef8001850) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ef8001850, mutex=0x7ef8001820) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f11fd112f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 9 (Thread 0x7e657f9b40 (LWP 1767127) "mt-n26"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ef80018f4) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ef8001898, cond=0x7ef80018c8) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ef80018c8, mutex=0x7ef8001898) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f11fd112f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 8 (Thread 0x7e65ffab40 (LWP 1767128) "mt-n45"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ea8001a04) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ea80019a8, cond=0x7ea80019d8) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ea80019d8, mutex=0x7ea80019a8) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f117d012f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 7 (Thread 0x7e64ff8b40 (LWP 1767129) "mt-n26"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ef800196c) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ef8001910, cond=0x7ef8001940) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ef8001940, mutex=0x7ef8001910) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f11fd112f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 6 (Thread 0x7e37ffeb40 (LWP 1767130) "mt-n45"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ea8001a7c) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ea8001a20, cond=0x7ea8001a50) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ea8001a50, mutex=0x7ea8001a20) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f117d012f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 5 (Thread 0x7e377fdb40 (LWP 1767131) "mt-n26"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ef80019e4) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ef8001988, cond=0x7ef80019b8) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ef80019b8, mutex=0x7ef8001988) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f11fd112f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 4 (Thread 0x7e36ffcb40 (LWP 1767132) "mt-n26"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ef8001a5c) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ef8001a00, cond=0x7ef8001a30) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ef8001a30, mutex=0x7ef8001a00) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f11fd112f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 3 (Thread 0x7e367fbb40 (LWP 1767133) "mt-n26"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ef8001ad4) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ef8001a78, cond=0x7ef8001aa8) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ef8001aa8, mutex=0x7ef8001a78) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f11fd112f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 2 (Thread 0x7e35ffab40 (LWP 1767134) "mt-n26"):
#0  futex_wait_cancelable (private=0, expected=0, futex_word=0x7ef8001b4c) at ../sysdeps/nptl/futex-internal.h:183
#1  __pthread_cond_wait_common (abstime=0x0, clockid=0, mutex=0x7ef8001af0, cond=0x7ef8001b20) at pthread_cond_wait.c:508
#2  __pthread_cond_wait (cond=0x7ef8001b20, mutex=0x7ef8001af0) at pthread_cond_wait.c:647
#3  0x0000007f844ebecc in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so.60
#4  0x0000007f8433f51c in start_thread (arg=0x7f11fd112f) at pthread_create.c:477
#5  0x0000007f83fa4a2c in thread_start () at ../sysdeps/unix/sysv/linux/aarch64/clone.S:78

Thread 1 (Thread 0x7f8357c610 (LWP 1766829) "media_transcode"):
#0  __pthread_clockjoin_ex (threadid=545955683136, thread_return=0x0, clockid=0, abstime=<optimized out>, block=<optimized out>) at pthread_join_common.c:145
#1  0x0000007f841ebd80 in __gthread_join (__value_ptr=0x0, __threadid=<optimized out>) at /opt/mt-tools/build/gcc-12.4.0-native/aarch64-unknown-linux-gnu/libstdc++-v3/include/aarch64-unknown-linux-gnu/bits/gthr-default.h:669
#2  std::thread::join (this=0x144cdd98) at /opt/mt-tools/src/gcc-12.4.0/libstdc++-v3/src/c++11/thread.cc:112
#3  0x00000000008db874 in media::ffmpeg::graph::MediaGraphWorker::join() ()
#4  0x00000000008da684 in media::ffmpeg::graph::MediaGraphThreadedExecutor::abort(media::ffmpeg::graph::MediaGraphExecutionContext&, media::ffmpeg::graph::MediaGraphScheduler&) ()
#5  0x000000000087b948 in media::ffmpeg::graph::MediaGraphRuntimeLifecycleExecutor::synchronizeThreadedState(media::ffmpeg::graph::MediaGraphRuntime&) ()
#6  0x00000000007d30b8 in media::ffmpeg::graph::MediaGraphRuntime::synchronizeThreadedState() ()
#7  0x000000000042502c in media::ffmpeg::graph::MediaRealtimeVideoRunController::run(media::ffmpeg::graph::MediaRealtimeRtpTranscodeRequest const&, media::ffmpeg::graph::MediaRealtimeVideoRunPolicy const&, media::ffmpeg::graph::MediaRealtimeVideoRunControl&, media::ffmpeg::graph::MediaRealtimeVideoRunObserver const&)::{lambda()#1}::operator()() const ()
#8  0x0000000000427284 in media::ffmpeg::graph::MediaRealtimeVideoRunController::run(media::ffmpeg::graph::MediaRealtimeRtpTranscodeRequest const&, media::ffmpeg::graph::MediaRealtimeVideoRunPolicy const&, media::ffmpeg::graph::MediaRealtimeVideoRunControl&, media::ffmpeg::graph::MediaRealtimeVideoRunObserver const&) ()
#9  0x000000000041078c in (anonymous namespace)::runRealtimeVideoCli(int, char**) ()
#10 0x000000000040ef0c in main ()
[Inferior 1 (process 1766829) detached]

~~~

## 失败收尾结论与清理
第一次有效栈15:12:38.502，第二次15:16:23.739（间隔约225秒），同Thread19/18仍是mpi_decode_get_frame→avcodec_send_packet，主线程仍MediaGraphWorker::join。不是单份futex即断言永久死锁；至少证明该区间未完成失败收尾。独立审查B依据符号栈将阻塞限定在RKMPP解码调用未返回，排除本次是finishExecution/capture join的早期假设；启动超时仍需独立定位。A确认clock locked不等于形成公共启动窗口；error state3为Startup错误类别，不是Armed状态。
实际取证命令（第一次仅-p产生匿名栈，第二次显式sysroot/file有效，两次有效gdb exit0）：
~~~bash
timeout 25s /opt/mt-tools/arm-gnu-toolchain-15.2.rel1-aarch64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gdb -q -batch -ex 'set sysroot /' -ex 'file out/build/rk-release/media_transcode_realtime_video_cli' -ex 'attach 1766829' -ex 'set pagination off' -ex 'thread apply all bt 16' -ex detach
~~~
源码映射与MPP timeout/重试条件是下一步；不得并发free/reset codec或跳过join掩盖所有权问题。需同步审查共享VideoDecodeNode中断合同和Windows影响。
确认/proc/1766829/exe匹配后kill -KILL 1766829，runner记录CLI_EXIT137（清理退出，非自然停止），两远端源exit0，本机AV也完整120秒exit0；没有执行重入。精确PID两端已无残留，未新增包或修改外部依赖。

本机清理清单：
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-received.ts
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-source2.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-vlc.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\cli-maps.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\fds.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\pids.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\process-status.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\runner.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\socket-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\source0.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\source1.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\thread-waits.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\threads-symbols.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\threads.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk516-remote\udp-before.txt

本机所列文件全部清理。远端逐项删除20项：socket-monitor.log、udp-after.txt、thread-waits.txt、runner.log、process-status.txt、source0.out、threads-second.txt、cli-error.log、monitor.log、result.txt、pids.txt、fds.txt、threads.txt、cli.log、source1.out、source0.log、udp-before.txt、threads-symbols.txt、source1.log、cli-maps.txt，并删除临时脚本/空目录；用户120秒源保留。
