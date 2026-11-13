# rk417：隔离身份依赖四路 realtime 合屏复验

状态：已执行，初始source3时钟证据丢失；不是通过记录。项目源码b55e19bf（每域边策略合同修复）；FFmpeg 92aec19，MPP 560734e（安装库hash见隔离实施记录）。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk417
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk417 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk417-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk417-native- --snapshot-format=png udp://@:62024
```

实际PID、CPU/RSS趋势、A/V、退出原因和VLC原生快照结果待执行后记录。先记录后按本轮清单清理，不删指定源或回退产物。







Windows进程必须由本轮持续存活的工具父会话Wait-Process保留；源仍为120秒不循环，失败后按精确PID清理VLC/源并等待父会话终态。

## 实际结果

源码b55e19bf，构建PID2838317，45/45、exit0；CLI SHA256 0f26a15a7e6fbd977ef1861a6442e0685dfc6beb00091ccc0a8c91580493afc6。ldd六FFmpeg库来自prefix-ffmpeg、MPP来自prefix-mpp、libstdc++/libgcc来自GCC12.4。旧064bbe71 CLI另存dependency-work/media_transcode_realtime_video_cli-064bbe71，SHA256 10d43c14302b6e632cb3b1ce08b4f4c9757eed2ec50ef0864360c05b1102826e，作为回退保留。

上述脚本已以nohup /bin/bash run.sh >runner.log 2>&1执行。runner2859439，源2859442/2859443/2859444，CLI2859445，monitor2859447。CLI自然exit1：`InvalidArgument: Initial composition source admission lost clock evidence`，未进入后置域策略验证，不能宣称b55e19bf真实链路通过。

source3 session548213069200于16:34:21.915安装。SSRC1316045005七次window_exceeded，均pending65/window64、maximum_delay250000000ns，generation0→7：22.738缺30099恢复30109；22.998缺30310恢复30312；23.138缺30502恢复30526；23.922缺31162恢复31192；23.941缺31266恢复31297；24.268缺31809恢复31847；25.795缺32924恢复32934。25.796 source3候选被clock证据门禁拒绝，25.804输出候选拒绝。这里只证明事件先后，不把接收端日志等同于线上丢包位置证明。

监控16:34:21.130→25.401，CPU五样本0/54/38.5/39.6/36%，RSS12960/37792/39888/43576/47004KiB。没有可验收的持续输出/A/V漂移/画面。UDPdelta InDatagrams18797、NoPorts2290、InErrors0、OutDatagrams16646、RcvbufErrors0、SndbufErrors0、InCsumErrors0、IgnoredMulti3；61050 inode44891223五快照rx_queue/drops均0。maps采到六个prefix-ffmpeg库及GCC12.4 libstdc++，未采到MPP；不能把采样零值解释为无端到端丢包。

失败后核远端三源exe及120秒输入，再SIGTERM；三源exit255，result为CLI_EXIT=1、SOURCE0/1/2_EXIT=255、RUN_FINISHED=1，六PID均不存在。Windows31040/33996通过escalated核exe后强制清理，父exec37918 exit0、SOURCE_EXIT=-1；清理退出不是源自然结束。普通sandbox看不到外部进程，不能据此推断其退出。

清理清单：远端rk417的run.sh、runner.log、pids.txt、result.txt、cli.log、cli-error.log、cli-maps.txt、monitor.log、socket-monitor.log、source0/1/2各.log和.out、udp-before.txt、udp-after.txt，共17项；另project-build-rk417.log。先记录后精确删除，不删除回退CLI/依赖或指定源。

清理完成：远端17项测试产物及1项构建日志均精确删除，目录为空，六测试PID无残留；回退CLI、依赖和指定源保留。

## Windows 执行与清理

源PID31040、VLC33996，使用require_escalated启动；父工具会话37918执行Wait-Process等待VLC。普通沙箱查询两PID不可见，但提升权限查询确认均存活，源最后2942帧/97.96秒，故不可把沙箱查询结果当进程退出证据。CLI失败后提升权限核对两个exe路径，逐PID强制清理；父会话exit0，源ExitCode=-1，不记自然结束。VLC日志仅UDP接收初始化，没有合屏快照。无抓包/录制文件。

本机source3.out、source3.log、vlc.log三项已逐项删除，rk417前缀无残留。
