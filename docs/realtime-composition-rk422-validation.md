# rk422：原规格四路realtime合屏复验

状态：已执行，正常输出及失源画面已观测；源重入待验证。源码e9ba7fba，四文件已双审SOURCE PASS；decoded startup策略修复已在本轮正常输出链路观测，完整验收仍待重入。原120秒四路媒体规格不变，Windows源沿用localaddr192.168.96.122，无capture。

构建采用原rk-release、8核incremental all，每段300秒；首段PID40416，日志project-build-rk422.log，首段超时、续编成功，详见下文。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk422
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk422 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

在acceptance/rk422以nohup /bin/bash run.sh >runner.log 2>&1启动，实际启动及终态见下文。


构建终态：首段40416在300秒exit124，完成186/282；确认进程组无残留后保留objects续第二段70540，96/96、exit0。两日志project-build-rk422.log/project-build-rk422-2.log保留待归档清理。CLI SHA256 39090066146461dd9a4991d781e00acd2f927f552febddf3e2fb5f3fe1057869；ldd六FFmpeg/MPP/GCC12.4均来自既定隔离路径。原脚本bash -n通过，随后执行本轮真实媒体验证。

## 实际运行与观测边界

本轮正常输出和失源黑屏观测成立，未覆盖源重入，不能判定合屏完整验收通过。远端 runner=92639、source0/1/2=92642/92643/92644、CLI=92645、monitor=92647；Windows source3=9240、VLC=27056、父会话=29936。

三个远端源各完整输出3600帧/120.00秒并自然exit0；Windows源完整3600帧/119.90秒。全部远端源在CLI elapsed129秒时已不存在；此时encodedPacketsPushed/Popped=42436/42435，CLI仍运行。全失源后持续输出，清理前计数达到84461/84460，errors、workerErrors、droppedBuffers、stalledIntervals均为0。

主线程已查看三张VLC原生1280×720快照：11:42:10.528四格有画；11:43:45.275右下source3黑屏、其余三格继续画面；11:44:18.455全部黑屏。截图为VLC原生快照，不是系统截图。全失源后CLI持续输出符合合屏场景预期，不能以未自动退出判断死锁。

A/V最后有效输入观测11:43:15.353：generation=1、sequence=5021、raw_phase_ns=22675、filtered_phase_ns=22676，filtered_frequency_ppm/integral_ppm/stretch_ppm均0；此前持续观测约22675—22676ns，无增长漂移证据。全失源后的黑屏输出不等于已证明源重入同步。

进程工作集从46,436,352B增长至57,864,192B，后段保持该值；elapsed129秒RSS56,508KiB，平均单核CPU157.39%，全失源继续运行后平均降至98.64%，全程峰值276.14%（机器8逻辑核，峰值机器归一34.52%）。峰值队列1254、payload峰值9,871,626B/436对象，清理前112B/6对象、reservations245299/releases245293。

压力计数最终5：首次报告2紧随11:41:50.929 clock snapshot；4紧随11:43:09.763 snapshot；5紧随11:43:10.075 missing_audio_wait。主线程核对ledger唯一增点：容量不fits时登记waiter并返回wouldBlock，因此该值表示触及规划硬边界的背压等待次数，并非直接的丢包或worker失败。仍需后续核对源重入时释放及持续性。stderr存在MPP启动skip与filter range属性变化告警，不能隐去；本轮运行未出现process.failed。

全机UDP计数增量：InDatagrams412034、OutDatagrams446581、NoPorts73、InErrors/RcvbufErrors/SndbufErrors/InCsumErrors均0、IgnoredMulti34；全机计数不等同单一流归因。

## 结束与精确清理

快照观测完成后，核对/proc/92645/exe为本项目out/build/rk-release/media_transcode_realtime_video_cli，再发送SIGTERM。runner记录CLI_EXIT=1、三个SOURCE_EXIT=0、RUN_FINISHED=1。CLI的exit1来自本轮结束清理，不记自然结束或验收失败前自行退出；runner、三源、CLI及monitor六PID终态均不存在。Windows源已不存在，主线程核exe清理VLC，父会话exit0。

归档后按17项清单删除：cli-error.log、cli-maps.txt、cli.log、monitor.log、pids.txt、result.txt、run.sh、runner.log、socket-monitor.log、source0.log/source0.out、source1.log/source1.out、source2.log/source2.out、udp-after.txt、udp-before.txt；另删除project-build-rk422.log与project-build-rk422-2.log。不动复用120秒源和正式产物。本机source3.out/log、vlc.log及三张native PNG共6项由主线程随后清理。远端19项已逐项删除，acceptance/rk422目录为空，六个本轮PID均不存在。