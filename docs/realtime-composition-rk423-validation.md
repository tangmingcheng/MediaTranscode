# rk423：原规格四路合屏失源后重入验收

状态：已执行原规格初次输出、全失源、同CLI重入和再次全失源观测。沿用源码e9ba7fba及CLI SHA256 39090066146461dd9a4991d781e00acd2f927f552febddf3e2fb5f3fe1057869，不重建、不修改依赖。

四路完整120秒源自然结束后，先确认全黑和连续输出；保持同一CLI，单独重启source2的H264＋AAC完整120秒，主线程同时控制Windows source3 HEVC重入。禁止loop或降低规格。记录新PID、SSRC、generation、首恢复画面、选中音频恢复、A/V漂移及CPU/RSS；恢复后的再次失源也需观察。只有真实链路证据满足原门禁才判定通过。

## 初始运行脚本

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk423
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk423 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
cat /proc/$cli/maps >cli-maps.txt 2>/dev/null
printf 'SOURCE0=%s SOURCE1=%s SOURCE2=%s CLI=%s\n' "$s0" "$s1" "$s2" "$cli" >pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; { date -Ins; ls -l /proc/$cli/fd; cat /proc/$cli/net/udp; } >>socket-monitor.log 2>&1; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" >>pids.txt
wait "$cli"; printf 'CLI_EXIT=%s\n' "$?" >result.txt
wait "$mon"
cat /proc/net/snmp >udp-after.txt
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" >>result.txt
wait "$s1"; printf 'SOURCE1_EXIT=%s\n' "$?" >>result.txt
wait "$s2"; printf 'SOURCE2_EXIT=%s\n' "$?" >>result.txt
printf 'RUN_FINISHED=1\n' >>result.txt
```

## source2独立重入命令（等待主线程REENTER）

```bash
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2-reentry.out 2>source2-reentry.log &
echo REENTRY_SOURCE2=$!
```

脚本准备完成后等待START，不自动启动媒体或重入。结束时先归档，再精确删除本轮17个基础文件及source2-reentry.out/log、重入PID/退出记录；保留120秒源和CLI产物。

## 真实运行结果

本轮未重建。目标CLI哈希与计划一致；120秒原源SHA256为6e0de760672a6b8386b59b22f824de442e2a5ef7594d7b872c6851b64cabca9b。启动前rk422目录为空、旧PID无残留；rk423空目录写入run.sh并bash -n通过。实际在该目录执行nohup /bin/bash run.sh >runner.log 2>&1 &。runner136007、初次source0/1/2=136009/136010/136011、CLI136012、monitor136014。Windows初次source28568、VLC30672、父会话81162。

初次三个远端源均3600帧/120.00秒自然exit0。11:51:47.537目标机观测三源已不存在，同CLI elapsed141秒仍输出45410包，errors0、pressure0。VLC主线程原生快照11:50:28.880确认四格有画，11:52:32.439确认全黑。

随后保持CLI136012，执行上文source2相同完整命令，重入日志改source2-reentry.out/log；由独立bash后台回收器wait源并写reentry-result.txt。重入runner153230、FFmpeg153231。原source2视频/音频SSRC为1318129888/3543068647，新SSRC为2029937675/639023213，聚合generation由1进入2。11:52:31.304目标机已记录source2 aggregate_contribution generation2，11:52:31.306首条generation2 A/V drift为2268ns。Windows source3重入23416、父96389，新SSRC2414256498；11:52:40.339目标机记录source3 generation2贡献。各机器墙钟不可直接用于跨机延迟计算。

VLC原生11:53:36.850确认上两格保持黑、下两格source2/3恢复真实画面，1280×720。source2重入完整3600帧/120秒自然exit0；Windows重入完整3600帧/119.90秒且父96389 exit0。VLC11:55:27.309再次全黑已查看。持续保持同CLI直到观测结束，没有重启CLI规避重入。

generation1漂移约22675—22676ns；generation2从sequence1到4943持续约2267—2268ns，末条11:54:25.493 raw/filtered2268ns，frequency/integral/stretch均0ppm。音频恢复有generation2内部时间线证据；VLC快照本身不证明可听音频品质。

最后encodedPacketsPushed/Popped=118368/118367，errors/workerErrors/droppedBuffers/stalledIntervals均0，没有process.failed。平均单核CPU113.29%，峰值309.28%（8核机器归一均14.16%、峰38.66%）。工作集初始62,414,848B、峰值及最后77,688,832B；重入前已到77,688,832B，重入至结束保持不变。peakQueued=2004；payload峰20,250,079B/942对象，最后112B/6对象；reservations399698/releases399692。graphPayloadPressureFailures=6，按ledger语义为硬容量wouldBlock等待计数，并非worker失败或丢包；本轮队列继续推进且最终回落，但不由单轮证明无限期稳定性。

全机UDP增量InDatagrams628898、OutDatagrams659718、NoPorts6、IgnoredMulti51；InErrors/RcvbufErrors/SndbufErrors/InCsumErrors均0。该全机统计不用于单流精确归因。

## 结束与清理

主线程确认第二次全黑后，核/proc/136012/exe为项目rk-release/media_transcode_realtime_video_cli并SIGTERM。结果CLI_EXIT=1（观测结束清理，不记自然结束）、SOURCE0/1/2_EXIT=0、REENTRY_SOURCE2_EXIT=0、RUN_FINISHED=1。八个本轮远端PID均不存在。

归档后逐项清理21文件：cli-error.log、cli-maps.txt、cli.log、monitor.log、pids.txt、reentry-pid.txt、reentry-result.txt、result.txt、run.sh、runner.log、socket-monitor.log、source0.log/out、source1.log/out、source2.log/out、source2-reentry.log/out、udp-after.txt、udp-before.txt。保留复用源、CLI及依赖。远端21项已精确删除，目录为空，八个本轮PID无残留。Windows完整命令与VLC统计见realtime-composition-rk423-windows-observation.md；本机9项临时产物已逐项清理，无残留，VLC30672核exe清理，28568/23416无残留，父81162及96389均exit0。