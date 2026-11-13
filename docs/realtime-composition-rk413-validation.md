# rk413：隔离身份依赖四路 realtime 合屏复验

状态：已执行，解码opaque合同options不一致；不是通过记录。项目源码d2b006f1（GCC启动存储适配）；FFmpeg 92aec19，MPP 560734e（安装库hash见隔离实施记录）。

保持三路H264＋一路HEVC RTP输入、源2 AAC；输出HEVC 1280×720/30、8000kbps CBR、AAC 192kbps/44100双声道、MPEG-TS/UDP，指定120秒源，不循环、不降规格；准备存储2GiB。Windows FFmpeg保持已验证版本。

## 目标机待执行命令

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk413
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk413 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

## Windows 待执行命令

```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk413-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk413-native- --snapshot-format=png udp://@:62024
```

实际PID、CPU/RSS趋势、A/V、退出原因和VLC原生快照结果待执行后记录。先记录后按本轮清单清理，不删指定源或回退产物。





准备记录：临时run.sh已上传目标out/dependency-work/rk413-run.sh，待移入本轮专属目录；本机out/rk413-run.sh传输副本记录后删除。尚未运行媒体。

## 实际结果

9文件同步前旧hash对29940fde、写后新hash对d2b006f1均通过；8核增量all PID2640840退出0，31/31。CLI SHA256 a5d7b1eec266b42cad1e4d92d52f0f7aab49e37e6bdedc665b244d678ac84f63；ldd六FFmpeg/MPP均新prefix，GCC12运行库正确。

远端runner2660011，源2660014/15/16，CLI2660017，monitor2660019；Windows源33484、VLC38644。四路decoder contract_bound及HEVC encoder 720p30 CBR8Mbps已准备，CLI自然退出1：InvalidArgument: Composition decoder opaque options differ from prepared facts。已越过rk412的GCC startup Unsupported，但无合屏输出、画面或A/V验收。

源3在16:03:54.167 gap29544→29556，window_exceeded pending65/window64。61050 socket inode44146352四次采样rx_queue0/drops0；UDP全局delta InDatagrams16065、NoPorts909、InErrors0、OutDatagrams13423、RcvbufErrors0、SndbufErrors0、InCsumErrors0、IgnoredMulti1，不能据此证明上游无丢包。本轮maps六新FFmpeg+GCC libstdc++，未捕获MPP条目，不扩大为maps证明。

CPU四点0/44/38.5/40%，RSS20112/29760/40532/45532KiB，只有启动阶段。CLI自然exit1后三源核身份SIGTERM各255，RUN_FINISHED1，远端六PID均不存在。Windows源读至3267帧/108.80秒后核路径与VLC清理，不记自然结束。未请求快照/抓包。

清理清单：远端rk413内cli-error.log、cli-maps.txt、cli.log、monitor.log、pids.txt、result.txt、run.sh、runner.log、socket-monitor.log、source0/1/2.log及.out、udp-after.txt、udp-before.txt；另project-build-rk413.log。本机composition-rk413-source3.out/source3.log/vlc.log；out/rk413-run.sh此前已删。保留指定源、隔离依赖、对象缓存与回退副本。

清理终态：远端17测试文件+1构建日志逐项删除，目录为空，所有列出的本轮PID均不存在；本机3日志删除并核prefix无残留，指定源保留124427809字节。源码根因已确认：共享MediaVideoPlanOptionApplier将完整解码options写给resolver，但VideoDecode白名单只收input_retention，漏掉opaque_retention；prepared事实和codec编码正确。修复在单链与合屏两入口复用同一合同分发，把已编码opaque合同交给真实消费节点，保持校验，不加RK特例。Windows同builder受影响，不能用历史Windows通过覆盖本轮变更。

修复审查：第一版helper对合法sharedDecode输出分支的无效本地decoder写options，独立审查判FAIL；已改为仅向存在的本地VideoDecode传播，source入口强制节点检查保留。修正后两位独立审查者重新明确SOURCE PASS，未用初次结论替代复审。尚未构建/运行验证本修复。
