# rk416：隔离身份依赖四路 realtime 合屏复验

状态：已执行但Windows后台进程提前消失，不是通过记录。项目源码064bbe71（source registry容量合同传递修复）；FFmpeg 92aec19，MPP 560734e（安装库hash见隔离实施记录）。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk416
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk416 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk416-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk416-native- --snapshot-format=png udp://@:62024
```

实际PID、CPU/RSS趋势、A/V、退出原因和VLC原生快照结果待执行后记录。先记录后按本轮清单清理，不删指定源或回退产物。







## Windows 运行异常

使用require_escalated运行Start-Process，源PID26876、VLC14432；后续Get-Process确认两者均不存在。源最后296帧、9.76秒（工具日志elapsed10.32秒），没有自然退出码，不能完成120秒验收；无快照/抓包。单独提升权限没有解决后台进程寿命，下轮以存活的工具父进程Wait-Process保留后台子进程，媒体参数不变。

## RKMPP 实际结果与清理

沿用064bbe71已构建CLI，SHA256为10d43c14302b6e632cb3b1ce08b4f4c9757eed2ec50ef0864360c05b1102826e，没有重复构建。上述run.sh实际以nohup /bin/bash run.sh >runner.log 2>&1启动：runner2799317，三源2799320/2799321/2799322，CLI2799325，monitor2799328。四源decoder contract_bound均完成；CLI自然exit1：`InvalidArgument: RTP control edge differs from its planned metadata contract`。该错误是新的明确预检合同失败；Windows源寿命不足使完整验收无效，但不能用环境异常解释该源码合同错误。

监控16:24:10.603至14.900，CPU五样本0/43/34.5/31.6/33.7%，RSS12968/36332/38164/42112/47684KiB。未有完整输出、A/V漂移或可验收画面。source3 SSRC306453487发生9次window_exceeded（pending65/window64）。61050 socket inode44651446，五样本drops均0，最后rx_queue=0x44240（279104字节），之前为0。UDPdelta InDatagrams18867、NoPorts2576、InErrors0、OutDatagrams16765、RcvbufErrors0、SndbufErrors0、InCsumErrors0、IgnoredMulti0；采样不能排除端到端丢失。启动maps采到prefix-ffmpeg六库和GCC12.4 libstdc++，未采到MPP，不将其视为实际加载MPP证明。

CLI失败后三个远端源核exe和指定120秒输入后SIGTERM，均exit255；result为CLI_EXIT=1、SOURCE0/1/2_EXIT=255、RUN_FINISHED=1，六PID均不存在。远端清理清单为run.sh、runner.log、pids.txt、result.txt、cli.log、cli-error.log、cli-maps.txt、monitor.log、socket-monitor.log、source0/1/2各.log和.out、udp-before.txt、udp-after.txt，共17项；本轮没有构建日志。预备rk417/run.sh只复制本轮脚本替换编号，未执行，因新预检错误暂停并一并清除。保留指定120秒源和隔离依赖。

清理完成：远端17项rk416临时文件和未运行的rk417/run.sh均已精确删除，两目录为空；六测试PID均不存在，指定源保留。

本机四项本轮临时产物已逐项删除，Windows源26876和VLC14432均无进程残留；未取得退出码，不能记为自然退出。
