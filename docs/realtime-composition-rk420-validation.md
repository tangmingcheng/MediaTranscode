# rk420：原规格四路realtime合屏复验

状态：已执行，进入runtime后启动envelope费用门禁失败；不是通过记录。源码ccab8fd6；probe消费typed planned timing修复已双审通过。保持原120秒四路媒体规格、不抓包、不修改网络或Windows FFmpeg。

## 目标机命令

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk420
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk420 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

启动方式：在acceptance/rk420执行nohup /bin/bash run.sh >runner.log 2>&1。实际PID、构建身份、指标及清理待运行后补充。

## 实际结果

构建PID2969461、6/6、exit0；CLI SHA256 35e399a13d7881a588325ca642782d73074cbafc67ae63d615a9d4a23bf0a72e。ldd六FFmpeg/MPP均隔离prefix，GCC12.4库正确。脚本已nohup执行：runner2980018，源2980022/2980023/2980024，CLI2980025，monitor2980027。

CLI自然exit1：`A/V startup envelope requires a trusted non-zero payload footprint`。已越过身份probe与runtime binder交接；16:55:05.141首失败node58=composition-rk420.source_2.av_sync.video.decoded_startup，输入edge83为Video/RawFrame/Frame，输出edge74为Metadata/Event/GraphEvent；node51/50随后Cancelled只是次生错误。不能将此源2错误归因于源3网络。

代码定位MediaAvStartupEnvelopeBuffer.cpp43；此前display-point强校验要求MediaVideoDecodeBuffer包裹FFmpegFrameBuffer及有效frame/lineage/sequence/presentation。因此动态类型可由已通过的校验推定，但日志未直接给出像素格式、费用null或0的区分，不假称已观测DRM具体对象。

监控16:55:00.316→04.567，CPU0/58/27.3/27.7/27%，RSS13180/39360/41716/45928/49884KiB。final report：peakQueued527、threads101/processThreads121、workerProgress852、processCalls1054、errors3；peakWorkingSet59297792B，totalPushed805/totalPopped431，encodedPacketsPushed/Popped均0；payload high12198487B/420objects，reservations/releases454/454，pressureFailures0，最终当前费用0。平衡计数不证明持续内存稳定；无完整输出/A-V漂移/画面验收。

source3 SSRC1800861453缺序：16:55:00.771 deadline_expired first_missing24294 resumed24323 pending496/window416666；00.788回放window64 pending65同缺口；01.734缺25526恢复25527、03.716缺27131恢复27154，均window_exceeded。UDPdelta InDatagrams19858、NoPorts791、InErrors0、OutDatagrams16703、RcvbufErrors0、SndbufErrors0、InCsumErrors0、IgnoredMulti3。61050 inode45309806五快照rx_queue/drops0，不证明无端到端缺失。

失败后核远端三源exe及120秒输入，SIGTERM后三源exit255；CLI_EXIT1、RUN_FINISHED1，六测试PID均不存在。Windows源41168/VLC45492由主线程escalated核路径清理，父71857exit0/sourceexit-1，最后1967帧65.46秒；属于失败后清理，不是完整120秒自然验收。

清理清单：run.sh、runner.log、pids.txt、result.txt、cli.log、cli-error.log、cli-maps.txt、monitor.log、socket-monitor.log、source0/1/2各.log和.out、udp-before.txt、udp-after.txt，共17项；另project-build-rk420.log。保留指定120秒源/回退CLI/隔离依赖。

远端清理完成：17项测试文件和1项构建日志均精确删除，目录为空，六测试PID无残留。

Windows源41168/VLC45492已提升权限核exe逐PID清理，父71857exit0/SOURCE_EXIT=-1，最后1967帧65.46秒；不记自然结束或120秒通过。未生成快照/抓包。

本机source3.out/source3.log/vlc.log三项临时文件已逐项删除，rk420前缀无残留。修复设计见realtime-composition-frame-footprint-plan.md。
