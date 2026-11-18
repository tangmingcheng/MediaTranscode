# rk419：原规格四路realtime合屏复验

状态：已执行，源身份probe预检失败；不是通过记录。源码0477c4a0；prepared/runtime时序身份修复已双审通过，实流待验证。沿rk417原媒体规格，不含抓包、不改网络设置，Windows FFmpeg不变。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk419
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk419 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

启动方式：在acceptance/rk419执行nohup /bin/bash run.sh >runner.log 2>&1。实际PID、构建身份、运行指标和清理结果待执行后补充。

## 实际结果

增量all构建PID2935436，4/4、exit0；CLI SHA256 a6916a56de858f5612782c4e4f441aeb8080bbff628e41898d47e5d0b727a4e1。ldd六FFmpeg/MPP均为隔离prefix，GCC12.4运行库正确。本轮未变更依赖或网络、未抓包。

上述脚本实际nohup启动runner2944576，源2944579/2944580/2944581，CLI2944582，monitor2944584。CLI自然exit1：`[CLI] realtime video graph preflight failed: InvalidArgument: Source identity probe snapshot differs from the planned source`。16:48:55.912源0 decoder contract_bound后源0候选被拒绝；未进入runtime启动，因此不能称0477c4a0实流验证通过。

source3 SSRC2910325765另有缺序事件：55.618 deadline_expired缺22264恢复22297 pending185/window416666；55.877 deadline_expired缺22482恢复22484 pending382/window416666；55.892/55.896回放窗口64相应window_exceeded（pending65）。源0身份probe错误不能仅归因于source3网络。

监控16:48:54.824/55.872，CPU0/40%，RSS12960/0KiB；末值0处于退出竞争，不代表稳态回收。无可验收输出或A/V漂移。UDPdelta InDatagrams7056、NoPorts2673、InErrors0、OutDatagrams7982、RcvbufErrors0、SndbufErrors0、InCsumErrors0、IgnoredMulti0。61050 inode45185926唯一快照rx_queue10880字节、drops0，不证明无端到端缺失。

CLI失败后核三源exe和120秒输入，再SIGTERM；三源exit255，result为CLI_EXIT=1、SOURCE0/1/2_EXIT=255、RUN_FINISHED=1，六测试PID均不存在。远端精确清理清单为run.sh、runner.log、pids.txt、result.txt、cli.log、cli-error.log、cli-maps.txt、monitor.log、socket-monitor.log、source0/1/2各.log和.out、udp-before.txt、udp-after.txt，共17项，另project-build-rk419.log。保留指定源/回退CLI/隔离依赖。

远端清理完成：17项测试文件及1项构建日志均已精确删除，rk419目录为空，六测试PID无残留。

Windows源36732/VLC20820已由主线程核身份停止，父exec87164 exit0、SOURCE_EXIT=-1；源最后1627帧/54.13秒，属于失败后清理，不是自然源结束或完整120秒验收。

## Windows结果及下一修复设计

源36732、VLC20820、父会话87164；最后1627帧54.13秒，CLI失败后提升权限核路径逐PID清理，父exit0/SOURCE_EXIT=-1，不记自然结束。无快照/抓包。

rk419暴露同一身份合同的剩余消费者：SourcePreparer给ProbeGoalPlanner传facts.request.mediaId组身份。延续rk418中GStreamer STREAM_START权威流身份沿用原则，探测planner应接收prepared session已有的optional<MediaRtpVideoTimingPlan>，校验与源timing产品一致，SourcePresentation必须有非空identity并严格核snapshot；CanonicalInterval保留首帧目标且拒绝矛盾的身份产品。调用方只传已规划产品，不推导后缀、不从snapshot自证期望、不放宽错误。

只改变内部产品传递和一致性验证，无新公共参数/线程/队列/背压/所有权/平台分支。Windows共享影响仍待后续实流；双独立审查后RKMPP增量构建及原规格复验。

本机清理终态：source3.out/source3.log/vlc.log三项已逐项删除，rk419前缀无残留；提升权限核验36732/20820均不存在。

冻结三文件已经两名独立审查者明确SOURCE PASS；尚待目标构建与真实链路复验。
