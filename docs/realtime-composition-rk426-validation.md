# rk426：四路合屏接收音频内容验证

状态：实时运行已完成；接收录制音频内容由主线程离线分析。复用e9ba7fba产物与rk423原四路完整120秒规格、source2选中H264+AAC，CLI SHA256 39090066146461dd9a4991d781e00acd2f927f552febddf3e2fb5f3fe1057869。不修改代码或依赖。

初次四源自然结束，保持同CLI；确认全失源后仅source2重入完整120秒，Windows source3不重入。主线程VLC同时显示并TS录制，结束后离线silencedetect核音频有声/失源静音/重入恢复，不增加实时FFmpeg监控。记录目标单机事件时间、generation与canonical媒体时间，录制侧以自身媒体PTS建立区间，禁止跨机墙钟直接相减。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk426
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk426 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

等待START及独立REENTER指令。源重入复用上文source2 FFmpeg命令，仅日志改source2-reentry.out/log并记录PID/退出码。结束后归档命令、媒体时间映射、CPU/RSS及A/V漂移，按本轮清单删除临时脚本/日志/录制TS与快照，保留原120秒源及正式产物。

## 目标机实际结果

CLI哈希精确匹配，脚本在新空目录bash -n通过。执行nohup /bin/bash run.sh >runner.log 2>&1 &：runner280431、sources280433/280434/280435、CLI280436、monitor280438。Windows source36092、VLC15836、父2348；VLC duplicate显示并录制composition-rk426-received.ts，由主线程记录完整命令。

初始三远端源各完整3600帧/120秒自然exit0，Windows完整3600帧/119.90秒。目标机12:18:30.698观测三远端源均已消失，同CLI elapsed137秒持续44174包、errors0。收到REENTER后，仅选中source2复用上文完整FFmpeg命令，日志改source2-reentry.out/log，独立bash回收器记录PID/exit：runner295543、FFmpeg295544；启动调用monotonic_ns=3288561377366。该源亦完整3600帧/120秒自然exit0，没有loop，没有重启CLI。

旧source2视频/音频SSRC2858695919/3260600492，新4074600443/3976223979；恢复进入generation2。generation1漂移22675—22676ns，末source_master_ns=119801205822；generation2 sequence1至4943漂移约2268ns，末source_master_ns=281831870251，frequency/integral/stretch均0ppm。

## 同机时间映射证据

输出first_video_head与first_audio_head均canonical_target_ns=0、target_master_ns=5039542752。目标机日志分别12:16:18.903/12:16:18.936；首video canonical_created_steady_ns=3127590470524。

重入ActivationReleaseSequencer在目标机12:19:00.970记录generation2、epoch_source_start_ns=999000000、epoch_master_release_ns=167057174106、steady_ns=3289657454954；首generation2音频drift在12:19:00.972、source_master_ns=167078854378。同master域重入release相对输出canonical起点为162017631354ns。该值只支持引擎时间线相对定位，不直接等于VLC TS绝对PTS；离线录制需按自己的首音频PTS/重封装时间偏移对齐。跨机墙钟不直接相减。generation1最后采样和generation2最后采样也只是已观测点，不充当精确静音边缘。

## 运行指标与结束

最终encodedPackets110038/110037，errors/workerErrors/droppedBuffers/stalledIntervals均0，无process.failed。平均单核CPU110.87%、峰440.64%（8核机器归一均13.86%、峰55.08%）。工作集初始60,583,936B、峰及最后76,201,984B；重入前已到该值，重入后保持。peakQueued1878，payload峰18,807,191B/883对象、最后112B/6对象，reservations364273/releases364267。pressureFailures4为ledger硬容量wouldBlock等待，非丢包或worker失败。

主线程VLC通过RC quit正常封口、父2348exit0后，本轮实时观测结束。核/proc/280436/exe为项目CLI后SIGTERM，CLI_EXIT=1为清理退出；初始三源与重入源exit0，RUN_FINISHED=1，八个本轮远端PID均不存在。接收音频内容是否满足静音/恢复仍以离线分析为准，本记录不提前宣称通过。

归档后精确删除21项：cli-error.log、cli-maps.txt、cli.log、monitor.log、pids.txt、reentry-pid.txt、reentry-result.txt、result.txt、run.sh、runner.log、socket-monitor.log、source0.log/out、source1.log/out、source2.log/out、source2-reentry.log/out、udp-after.txt、udp-before.txt；保留120秒源及正式产物。远端21项已删除，目录为空，八PID无残留。全机UDP增量InDatagrams522163、OutDatagrams653638、NoPorts7、IgnoredMulti47，InErrors/RcvbufErrors/SndbufErrors/InCsumErrors均0。