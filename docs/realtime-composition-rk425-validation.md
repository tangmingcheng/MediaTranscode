# rk425：三路异构合屏验收

状态：已执行三路初次输出、全失源、选中音视频源同CLI重入及再次全失源观测。复用e9ba7fba及CLI SHA256 39090066146461dd9a4991d781e00acd2f927f552febddf3e2fb5f3fe1057869。source0 H264视频61030/latency100ms；source1 H264+AAC61020/61022/200ms，audio-source1；source2 WindowsHEVC61050/250ms。完整120秒源、1280×720/30fps/HEVC8Mbps CBR/AAC192kbps、存储和出口事实均保持。补三路矩阵，不替代四路证据。

等待START；先三路正常画面和全失源持续输出，再由主线程协调同CLI选中音视频源重入完整120秒，观察generation、CPU/RSS和持续A/V漂移。结束后先归档再精确清理。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk425
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk425 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

## 实际结果

新目录原为空，CLI哈希与计划一致、run.sh经bash -n通过。在acceptance/rk425执行nohup /bin/bash run.sh >runner.log 2>&1 &。runner232436、source0/1=232442/232443、CLI232444、monitor232448；Windows source32148、VLC39296、父28991。

初次两个远端源各3600帧/120秒自然exit0；Windows完整3600帧/119.90秒。VLC原生12:08:15.385确认左上/右上/左下有画、右下黑；12:10:32.923确认全黑，均1280×720。源结束后同CLI持续输出，elapsed141秒encoded46097、errors0。

REENTER后复用上文source1完整FFmpeg命令，仅日志改source1-reentry.out/log，独立bash回收器wait并记录退出。runner249493、FFmpeg249494，同CLI232444保持。原source1视频/音频SSRC1535333619/2797283949，新1262626568/1265655649；目标机12:10:26.257记录generation2聚合贡献。VLC12:11:21.534确认仅右上source1恢复真实画面，其余黑。重入源完整3600帧/120秒自然exit0；12:13:24.593快照确认再次全黑。跨机墙钟不用于计算恢复延迟。

generation1漂移22675—22676ns；generation2持续4535—4536ns，末条sequence5107、raw4535ns/filtered4536ns，frequency/integral/stretch均0ppm。内部音频时间线恢复已观察；快照不证明可听品质。

最终encodedPackets117753/117752，errors/workerErrors/droppedBuffers/stalledIntervals均0，无process.failed。平均单核CPU100.96%、峰226.74%（8核机器归一均12.62%、峰28.34%）。工作集初始44,650,496B，峰及最后57,995,264B；重入前达到该值，重入后保持。peakQueued1364；payload峰9,289,205B/475对象、最后112B/5对象，reservations310246/releases310241。pressureFailures最终2，为ledger硬边界wouldBlock等待次数，不直接等同丢包或worker失败。

全机UDP增量InDatagrams417803、OutDatagrams550586、NoPorts14、IgnoredMulti48，InErrors/RcvbufErrors/SndbufErrors/InCsumErrors均0。全机统计不作单流精确归因。

## 结束与清理

第二次全黑后核/proc/232444/exe为项目CLI并SIGTERM；CLI_EXIT=1为观测结束清理退出，不记自然退出；两个初始SOURCE_EXIT与REENTRY_SOURCE1_EXIT均0、RUN_FINISHED=1。七个本轮远端PID均不存在。

归档后精确删除19项：cli-error.log、cli-maps.txt、cli.log、monitor.log、pids.txt、reentry-pid.txt、reentry-result.txt、result.txt、run.sh、runner.log、socket-monitor.log、source0.log/out、source1.log/out、source1-reentry.log/out、udp-after.txt、udp-before.txt。保留复用源及正式产物。远端19项已删除、目录为空，七PID无残留；Windows完整命令与VLC统计见主线程独立观察文档。本机7文件已精确删除无残留，VLC39296核身份清理，32148自然结束无残留，父28991exit0。