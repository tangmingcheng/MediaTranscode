# rk415：隔离身份依赖四路 realtime 合屏复验

状态：已执行，但第四路源没有媒体；不是通过记录。项目源码064bbe71（source registry容量合同传递修复）；FFmpeg 92aec19，MPP 560734e（安装库hash见隔离实施记录）。

保持三路H264＋一路HEVC RTP输入、源2 AAC；输出HEVC 1280×720/30、8000kbps CBR、AAC 192kbps/44100双声道、MPEG-TS/UDP，指定120秒源，不循环、不降规格；准备存储2GiB。Windows FFmpeg保持已验证版本。

## 目标机实际执行命令

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk415
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk415 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk415-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk415-native- --snapshot-format=png udp://@:62024
```

实际PID、CPU/RSS趋势、A/V、退出原因和VLC原生快照结果待执行后记录。先记录后按本轮清单清理，不删指定源或回退产物。







## Windows 启动异常

本机Start-Process返回源PID10420、VLC40556，但随后Get-Process确认两者均不存在；源日志停在Stream mapping，未产生编码帧记录，VLC日志也停在启动阶段。没有取得退出码，不能记为自然结束；疑似工具后台子进程生命周期，尚未证实。本轮缺第四路有效输入及VLC，不能作为完整四路验收，也不能据此认定生产源码缺陷。未生成快照或抓包。下一轮采用允许持续后台运行的工具执行方式，媒体参数和Windows FFmpeg均不变。

## RKMPP 实际结果

项目源码064bbe71；增量all构建PID2754692，4/4、exit0。realtime CLI SHA256：10d43c14302b6e632cb3b1ce08b4f4c9757eed2ec50ef0864360c05b1102826e。ldd核六个FFmpeg库均来自prefix-ffmpeg、MPP来自prefix-mpp，libstdc++/libgcc来自GCC12.4，无not found。

上述run.sh已实际通过nohup /bin/bash run.sh >runner.log 2>&1启动。runner2775983；源2775986/2775987/2775988；CLI2775989；monitor2775991。CLI自然exit1：`NotInitialized: raw RTP video signaling probe reached open timeout before complete parameter sets; matching_rtp_packets=0 matching_datagram_bytes=0 parameter_sets=none`。source0/1/2协议安装，source3未安装；未进入完整四路输出，不能判定本轮registry修复真实链路通过。

监控16:20:39.076至16:21:09.028，约30秒，CPU最高38.0%、末值18.5%；RSS从13116KiB增长至峰值117744KiB，末值116156KiB。此为启动等待窗口，不能作为稳态内存结论；没有可验收的A/V漂移或VLC画面。UDP累计差值InDatagrams83584、NoPorts2424、InErrors0、OutDatagrams86529、RcvbufErrors0、SndbufErrors0、InCsumErrors0、IgnoredMulti3；全局计数不能证明端到端无丢包。

CLI失败后核对三源exe为隔离prefix-ffmpeg/bin/ffmpeg且输入为指定120秒源，再逐PID发SIGTERM；三个源exit255。result为CLI_EXIT=1、SOURCE0/1/2_EXIT=255、RUN_FINISHED=1，六个测试PID均不存在。源码回归验收待具备有效第四路源后重跑。

## 本轮清理清单

远端acceptance/rk415内17项：run.sh、runner.log、pids.txt、result.txt、cli.log、cli-error.log、cli-maps.txt、monitor.log、socket-monitor.log、source0.log、source0.out、source1.log、source1.out、source2.log、source2.out、udp-before.txt、udp-after.txt；另out/dependency-work/project-build-rk415.log。先归档上述结果，再逐项删除并核空目录；指定120秒源及隔离依赖保留。

清理已执行：远端17项测试产物及1项构建日志全部精确删除，测试目录为空；构建及六个测试PID均不存在，复用源仍124427809字节。

本机out/rk415-run.sh、source3.out、source3.log、VLC.log四项本轮临时文件已删除，rk415前缀无残留；Windows PID10420/40556不存在，未取得退出码。
