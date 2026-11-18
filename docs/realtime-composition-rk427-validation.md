# rk427：四路合屏停止门禁复验

状态：真实运行提前IoFailure，停止门禁未执行，FAIL。复用e9ba7fba及原CLI SHA256 39090066146461dd9a4991d781e00acd2f927f552febddf3e2fb5f3fe1057869；四路原规格完整120秒，无重入、不录制。

SIGTERM由RealtimeStopSignal转换为requestStop，不能仅以人工清理解释exit1。初次源全部自然结束后保持CLI，等待STOP；核exe后发送SIGTERM并记录单机monotonic停止起止、完整cli/error尾部、end_reason、final report、failStatus、ledger归零与worker errors。保留原始失败，完成归档并经主线程确认后精确清理，不提前删日志。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk427
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk427 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

## 实际失败与边界

原CLI哈希与脚本语法核验通过，未改源码/参数。在acceptance/rk427执行nohup /bin/bash run.sh >runner.log 2>&1 &：runner330608、source0/1/2=330610/330612/330613、CLI330614、monitor330616。Windows source10628、VLC36336、父7663。

尚未发送SIGTERM、源仍运行时，目标机12:26:25.515首错发生于node101 MediaScheduledDatagramSenderNode，随后CLI自行exit1。本轮停止门禁未执行，不能归因停止处理，也不能算完整120秒通过。Windows失败后中止于3251帧/108.26秒；远端三源核exe后SIGTERM停止，均exit255，非自然结束。

首错原文：IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=38511 now_ns=45832603319 release_ns=45799601283 deadline_ns=45832933735 not_before_ns=45832806109 not_after_ns=45832718695 duration_ns=215040 rate=6250000 queue_wire_bytes=5000 average_queue_residence_ns=13370735。

reservation_failed trace：generation1、endpoint1；original_protocol_deadline与prefix_completion_limit均45832933735；activation_started45819811840、activation_completed45819817091；permit_started45832601278、permit_completed45832602153；snapshot_started45832602736、snapshot_sampled45832603319、snapshot_completed45832603903；reservation_completed45832636570。wait_started/wait_completed/submit_observed均unavailable，wait_attempts及所有wait细分计数均0。失败发生于reservation形成前，不能归因实际wait API超时。可行起点not_before比not_after晚87414ns；activation_completed至permit_started相隔12784187ns，具体调度或调用原因尚待源码审查，不凭该差值归因OS。

首错前最后report errors0、workers94、queued33、encoded14794/14786、payload856150B/327对象；最后有效A/V generation1 source_master45925185583ns，raw/filtered22675ns、0ppm。随后四个process.failed为graph payload credit waiters were cancelled，包括VideoFilterNode、VideoDecodeNode和AudioEncodeNode，属于首错后的取消传播，不能替代首错。

12:26:25.872 output_id1状态failed随后retired，error6/native0/stage3，detail/reason完整保留上述同一IoFailure。stderr末尾：AAC Qavg555.452；2 frames left in the queue on closing；CLI realtime video runtime failed为同一IoFailure；随后MPP buffer service cleaning misc group。此前另有H265d current stream is no right skip it及filter range变化告警。没有观察到独立end_reason/failStatus字段输出，不能虚构其值。

final runtime report：queued0、peakQueued1124、threads101、processThreads147、workers0、workerProgress128311、workerProcessCalls553719、workerWaits425394、workerWakeups420024、workerErrors/errors5、stalledIntervals0、cpuSamples176、logicalProcessors8；平均单核CPU173.745159%、峰242.931937%（机器归一均21.718145%、峰30.366492%）；工作集初始46616576B、最后/峰55762944B；totalPushed117766、totalPopped117730、droppedBuffers0、encoded14794/14786、backpressureItems146；payloadCurrentBytes/Objects均0、highBytes9522008/highObjects334、reservations=releases=78238、pressureFailures1。最终ledger归零不抵消媒体运行失败。

## 结束清理

CLI失败后才核远端源exe并SIGTERM；CLI_EXIT1、三个SOURCE_EXIT255、RUN_FINISHED1，六个本轮PID均不存在。主线程核Windows源/VLC路径清理，父7663exit0。保留失败证据后按17项删除cli-error.log、cli-maps.txt、cli.log、monitor.log、pids.txt、result.txt、run.sh、runner.log、socket-monitor.log、source0.log/out、source1.log/out、source2.log/out、udp-after.txt、udp-before.txt，保留120秒源与正式产物。远端17项已精确删除，目录为空、六PID无残留。