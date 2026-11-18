# rk424：双路异构合屏及重入验收

状态：已执行双路原规格、全失源、选中音视频源同CLI重入及再次全失源观测。复用e9ba7fba产物，不重建。source0为H264+AAC（61020/61022，latency200ms），source1为Windows HEVC（61050，250ms），audio-source0。其余完整120秒源、1280×720/30fps/HEVC 8Mbps CBR、AAC192kbps、出口和存储事实不变。本轮补双路矩阵，不替代四路证据。

先等双源自然结束并确认全黑/持续输出；保持同CLI重启H264+AAC完整120秒，核generation、画面及音频时间线恢复、CPU/RSS、A/V漂移，最后再次失源。等待START和REENTER分别启动，不自动重入。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk424
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk424 --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 0 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
cat /proc/$cli/maps >cli-maps.txt 2>/dev/null
printf 'SOURCE0=%s CLI=%s\n' "$s0" "$cli" >pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; { date -Ins; ls -l /proc/$cli/fd; cat /proc/$cli/net/udp; } >>socket-monitor.log 2>&1; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" >>pids.txt
wait "$cli"; printf 'CLI_EXIT=%s\n' "$?" >result.txt
wait "$mon"
cat /proc/net/snmp >udp-after.txt
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" >>result.txt
printf 'RUN_FINISHED=1\n' >>result.txt
```

## 实际执行结果

CLI SHA256核验为39090066146461dd9a4991d781e00acd2f927f552febddf3e2fb5f3fe1057869。新目录原为空，脚本bash -n通过后，在acceptance/rk424执行nohup /bin/bash run.sh >runner.log 2>&1 &。runner189546、source0=189548、CLI189549、monitor189551；Windows source38512、VLC38016、父62571。

初次远端源完整3600帧/120秒自然exit0，Windows源3600帧/119.90秒。VLC原生快照12:00:33.183确认左右两源有画、上下留黑，1280×720；12:02:21.267确认全黑。同CLI全失源后仍输出，未重启CLI。

收到REENTER后，复用上文FFmpeg完整source0命令，仅日志改为source0-reentry.out/log，独立bash回收器记录PID并wait退出。重入runner205279、FFmpeg205280。旧视频/音频SSRC2002158669/3502954179，新SSRC4175683610/1044167115。目标机12:02:15.292记录source0 generation2聚合贡献；VLC12:03:06.208确认仅左源恢复，右侧和上下保持黑。重入完整3600帧/120秒自然exit0；VLC12:05:05.330再次全黑。跨机墙钟不用于计算恢复延迟。

generation1漂移约22675—22676ns；generation2持续约4535—4536ns，末条sequence5097（目标机12:04:13.055）raw4535ns、filtered4536ns，frequency/integral/stretch均0ppm。音频恢复有内部generation2时间线证据，快照本身不证明可听品质。

最终encodedPackets109565/109564，errors/workerErrors/droppedBuffers/stalledIntervals均0，无process.failed。工作集初始40,468,480B、峰及最后49,688,576B，重入前后稳定；平均单核CPU96.15%、峰200%（8核机器归一均12.02%、峰25%）。peakQueued956，payload峰7,691,332B/371对象、最后8304B/5对象，reservations257665/releases257660。pressureFailures1为ledger硬容量wouldBlock等待，并非丢包；重入阶段未增加该计数。

全机UDP增量InDatagrams298311、OutDatagrams440878、NoPorts55、IgnoredMulti42；InErrors/RcvbufErrors/SndbufErrors/InCsumErrors均0，不作为单流精确归因。

## 结束清理

确认第二次全黑后核/proc/189549/exe为项目CLI并SIGTERM。CLI_EXIT=1是观测结束清理退出，不记自然退出；SOURCE0_EXIT=0、REENTRY_SOURCE0_EXIT=0、RUN_FINISHED=1。六个本轮远端PID均不存在。

归档后精确清理17文件：cli-error.log、cli-maps.txt、cli.log、monitor.log、pids.txt、reentry-pid.txt、reentry-result.txt、result.txt、run.sh、runner.log、socket-monitor.log、source0.log/out、source0-reentry.log/out、udp-after.txt、udp-before.txt。保留120秒源和正式产物。远端17项已删除，目录为空、六PID无残留。Windows独立观察文档已由主线程记录；本机7项临时产物已删除无残留，VLC38016核exe清理，源38512自然结束、父62571exit0。