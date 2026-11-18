# rk418：原规格四路合屏与双端RTP序号诊断

状态：已执行，runtime启动合同校验失败，不是通过记录。沿用b55e19bf构建CLI，SHA256 0f26a15a7e6fbd977ef1861a6442e0685dfc6beb00091ccc0a8c91580493afc6。媒体参数保持rk417完整规格；只增加抓包，不修改代码或网络设置。

目标ip route get 192.168.96.122实查：via 192.168.130.254 dev eth0 src 192.168.130.229。tcpdump4.9.3/libpcap1.9.1可用。本机路由由主线程核实经Mihomo接口14/IP198.18.0.1、下一跳198.18.0.2，因此同时抓dumpcap接口9（Mihomo）和5（以太网）。[Mihomo官方TUN文档](https://wiki.metacubex.one/config/inbound/tun/)说明可配置不同网络栈；该背景不能证明当前序号缺口由TUN造成。

## 目标机脚本

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk418
/usr/sbin/tcpdump -i eth0 -n -U -s 128 -w source3-wire.pcap 'udp and dst host 192.168.130.229 and (dst port 61050 or dst port 61051)' >capture.out 2>capture.log &
printf 'CAPTURE=%s\n' "$!" >capture-pid.txt
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk418 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

实际启动命令：在acceptance/rk418执行nohup /bin/bash run.sh >runner.log 2>&1。capture-pid.txt记录抓包PID，pids.txt记录三源/CLI/monitor，runner PID由启动工具返回。失败后先记录并核身份停止源；tcpdump以SIGINT正常落盘获取抓包丢弃统计。抓包保留至跨端序号/SSRC/时序比对完成，再按本轮清单清理。

## Windows抓包命令

```powershell
D:\Wireshark\dumpcap.exe -i 9 -f "udp and dst host 192.168.130.229 and (dst port 61050 or dst port 61051)" -i 5 -f "udp and dst host 192.168.130.229 and (dst port 61050 or dst port 61051)" -s 128 -a duration:60 -w D:\Code\MyCode\MediaTranscode\outcceptance\composition-rk418-source3.pcapng
```

Windows源和VLC沿原规格，由主线程记录实际命令/PID；父exec会话等待VLC及dumpcap，所有外部进程状态在同等执行权限下核验。截取前128字节用于RTP/RTCP头序号诊断，不作为媒体完整性录像。待执行后补充结果及精确清理清单。

## 远端实际结果

runner2896214、tcpdump2896216、三源2896223/2896224/2896225、CLI2896226、monitor2896228。CLI自然exit1：`[CLI] start realtime video runtime failed: InvalidArgument: Prepared RTP binder differs from runtime clock policy`。本轮已越过graph preflight，但尚未完成runtime启动，无可验收输出、A/V漂移或画面。cli.log没有rtp_reorder_gap记录，本轮不能复现此前七次缺序号的因果链。

监控16:40:40.945→43.089，CPU三样本0/48/38%，RSS13228/36476/39888KiB。UDPdelta InDatagrams11685、NoPorts2056、InErrors0、OutDatagrams10894、RcvbufErrors0、SndbufErrors0、InCsumErrors0、IgnoredMulti1；61050 socket inode45010757三样本rx_queue/drops均0。

失败后核tcpdump真实exe发SIGINT，30868 packets captured、31017 received by filter、0 dropped by kernel；source3-wire.pcap为4374685字节。不能将received/captured差额直接视为线上丢包。三个源核exe及指定120秒输入后SIGTERM，exit255；result为CLI_EXIT=1、SOURCE0/1/2_EXIT=255、RUN_FINISHED=1。七个本轮PID均不存在。

待跨端比对的清理清单：原17项run.sh/runner.log/pids.txt/result.txt/cli.log/cli-error.log/cli-maps.txt/monitor.log/socket-monitor.log/source0、1、2各.log和.out/udp-before.txt/udp-after.txt，新增capture-pid.txt、capture.log、capture.out、source3-wire.pcap，共21项。本轮未编译，无新构建日志；抓包比对记录前暂留。

远端清理完成：跨端比对结论取得后，21项本轮文件已逐项精确删除，rk418目录为空，七个测试PID均不存在；指定源及回退产物保留。

## 双端序号对照与本机终态

Windows PID：dumpcap8952、源34588、VLC30632；父工具会话91068。dumpcap60秒自然exit0，105923包；Mihomo报告53277/27、以太网52672/1 received/dropped，损失分类为结束flush，不能将抓包本身当绝对无丢。CLI失败后提升权限核exe清源/VLC，父exit0，SOURCE_EXIT=-1；源最后2695帧89.73秒。不记完整120秒通过，无VLC快照。

分别使用tshark -r文件 -d udp.port==61050,rtp -Y rtp -T fields -E separator=, -e frame.interface_id（接收端省略） -e frame.time_epoch -e rtp.ssrc -e rtp.seq导出临时CSV，以SSRC和无回绕序号集合比较，剔除tshark非CSV提示行。SSRC0x61c31188，公共闭区间26024..54135：Mihomo28112个完整连续序号，以太网27905、RK27905；后两者集合完全相等，Mihomo独有207个序号。例29547..29562、30512..30518。该窗口证据将缺口定位至本机虚拟→物理出口之间，不证明具体代理实现缺陷；本轮CLI无gap，说明抓包窗口中的后续损失不能伪称导致本轮binder失败。接收抓包共30861 RTP包，区间26024..57091；末端超出发送抓包窗口的部分未纳入比较。

本机临时清单：composition-rk418-source3.pcapng、receiver.pcap、sender.csv、receiver.csv、dumpcap.log、source3.out、source3.log、vlc.log，均位于out/acceptance；归档后逐项清理，保留指定源。

## binder身份修复设计

[GStreamer STREAM_START](https://gstreamer.freedesktop.org/documentation/gstreamer/gstevent.html)区分组身份与具体流身份，既有流的下游应沿用身份而非逐节点重新生成。本项目等价采用planner统一生成、prepare/runtime复用同一流身份，不移植随机ID或重新拼接策略。

源码：PreparedRtpSourceSessionPlan.cpp直接copy(request.mediaId)，而已有planRtpInputClock产品videoInput.identity是组下视频流身份；runtime沿startup.videoIdentity→assembly→node options使用后者。SourcePresentation合同严格比较因此失败。修复仅让prepared复制policy.value().videoInput.identity，保留非空/分配失败及handoff严格检查。无新公共参数、线程、队列、背压、所有权或平台adapter变化；共享Windows路径同样受影响。两名独立源码审查后目标增量构建及原规格realtime，未完成前不标通过。

清理终态：本机8项临时产物已逐项删除，rk418前缀无残留；提升权限核验34588/30632/8952均不存在。初审发现videoInput.identity为optional，已补缺失报错并解引用，保留原空值/分配失败检查，正在双方复审。

最终复审：两名独立审查者重新读取含optional存在检查及解引用的最终版本，均明确SOURCE PASS；初版直接传optional不可编译，旧PASS撤回。尚待目标构建与runtime复验。
