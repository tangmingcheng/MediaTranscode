# rk421：原规格四路realtime合屏复验

状态：已执行，source0等待关键帧超时，失败后CLI未自动退出；不是通过记录。源码6a03ad4f；frame footprint修复已双审SOURCE PASS。媒体规格与120秒源不变，远端无capture。

Windows发送URL仅增加localaddr=192.168.96.122，packet size1200不变，不修改系统路由。主线程只读Find-NetRoute指定LocalIPAddress192.168.96.122显示以太网15/gateway192.168.96.254，默认走Mihomo；本机ffmpeg协议帮助确认localaddr，[FFmpeg官方RTP文档](https://ffmpeg.org/ffmpeg-protocols.html#rtp)说明该发送源地址选项。尚未以实际包证实绕过TUN，不能提前归因修复。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk421
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk421 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

在acceptance/rk421以nohup /bin/bash run.sh >runner.log 2>&1启动；实际结果/PID/清理待补。

Windows附加诊断（仅包头）：D:/Wireshark/dumpcap.exe -i 5 -f "udp and dst host 192.168.130.229 and (dst port 61050 or dst port 61051)" -i 9 -f "udp and dst host 192.168.130.229 and (dst port 61050 or dst port 61051)" -s 128 -a duration:60 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk421-source3.pcapng 。实际PID及命令启动证据由主线程补充；本轮pcapng、dumpcap日志、source3日志及VLC日志均须比对归档后按精确文件清单清理。
## Windows执行、路径验证与清理清单

Windows源29600、VLC8472、dumpcap26136，父会话54547；源最后2709帧90.20秒。CLI失败后核exe清源/VLC，父exit0/SOURCE_EXIT=-1；dumpcap60秒自然exit0。没有合屏输出或VLC快照。

抓包52976包；Mihomo0/0 received/dropped，物理53100/125（结束flush125，不能声称抓包绝对不丢）。tshark按接口/ip.src/rtp.ssrc/rtp.seq导出CSV：RTP52964包均在物理接口1，源192.168.96.122，SSRC0x300b2061，序号2148..55111无回绕、唯一数52964、区间缺失0。本轮localaddr实测绕过Mihomo且该窗口连续；不泛化为任何网络永不丢包。

命令：D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk421-source3.pcapng -d udp.port==61050,rtp -Y rtp -T fields -E separator=, -e frame.interface_id -e ip.src -e rtp.ssrc -e rtp.seq。过滤非CSV提示后按SSRC及无回绕序号集合核对。

本机清理清单：out/acceptance/composition-rk421-source3.pcapng、sender.csv、dumpcap.log、source3.out、source3.log、vlc.log。证据归档后逐项删除，保留指定120秒源。

## 实际结果

构建PID3329876，50/50、exit0，CLI SHA256 9ce1733c0afb8216275004ce088d482c689a57e8badf7da1764c5a424fe8c6ae；ldd六FFmpeg/MPP/GCC12.4来自既定隔离路径。runner3344862，源3344865/3344866/3344867，CLI3344868，monitor3344870。

09:35:10.873首失败node17=composition-rk421.source_0.av_sync.startup.coordinator：A/V sync code8 operation=poll state3 expected_generation1 running_bounds_ns=[0,10000000000]，video key frame wait timeout。本轮无rtp_reorder_gap；四条first_frame trace均drm_prime1280x720，未打印KEY flags，不能从日志断言帧是否标记关键帧。encodedPackets为0，无有效输出/A-V漂移/快照验收。

失败后核exe与指定源后SIGTERM三源，CLI65秒仍未退，14线程均futex_wait_queue_me；RSS峰/末74708KiB，CPU采样峰67%（单核口径）。gdb路径/opt/mt-tools/arm-gnu-toolchain-15.2.rel1-aarch64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gdb，以timeout15s -batch -ex 'set pagination off' -ex 'thread apply all bt' -p3344868获取threads.txt，exit0；未加载exe符号，无法可靠展开用户栈，不将corrupt stack提示当作内存损坏证据。残留mt-n42/66/90是source1/2/3 VideoDecode，另有MPP parser/hal线程。随后核CLI exe，SIGKILL清理，result为CLI_EXIT137、三源255、RUN_FINISHED1；137是清理导致，不能记自然结束。

主线程Windows证据：源29600/VLC8472核身份清理，最后2709帧90.20s，父54547exit0/sourceexit-1，capture26136 exit0。60秒抓包Mihomo0、物理52976包；RTP52964包，SSRC0x300b2061，seq2148..55111完整无洞，源192.168.96.122，支持本轮localaddr确实走物理路径。详细抓包统计及本机清理由主线程补充。

远端待清理18项：原17项run.sh/runner.log/pids.txt/result.txt/cli.log/cli-error.log/cli-maps.txt/monitor.log/socket-monitor.log/source0、1、2各.log和.out/udp-before.txt/udp-after.txt，另threads.txt；再加project-build-rk421.log。当前SSH暂时无响应，尚未删除，不宣称清理完成。
本机6项临时产物已逐项删除，rk421前缀无残留。目标SSH后续默认连接被关闭；ssh -b192.168.96.122 -tt -oConnectTimeout=8到192.168.130.229:22亦8.41秒超时，未进入认证。远端18项测试产物（原17+threads.txt）及project-build-rk421.log尚待恢复连接后逐项清理；未宣称已完成该清理。
