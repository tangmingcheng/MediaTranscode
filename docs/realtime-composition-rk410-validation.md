# rk410：隔离身份依赖四路 realtime 合屏复验

状态：已执行，预检失败；不是通过记录。项目源码26449d1e（含2cf6a6b2依赖身份与只读query编译修复）；FFmpeg 92aec19，MPP 560734e（安装库hash见隔离实施记录）。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk410
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk410 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
cat /proc/$cli/maps >cli-maps.txt 2>/dev/null
printf 'SOURCE0=%s SOURCE1=%s SOURCE2=%s CLI=%s\n' "$s0" "$s1" "$s2" "$cli" >pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" >>pids.txt
wait "$cli"; printf 'CLI_EXIT=%s\n' "$?" >result.txt
wait "$mon"
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" >>result.txt
wait "$s1"; printf 'SOURCE1_EXIT=%s\n' "$?" >>result.txt
wait "$s2"; printf 'SOURCE2_EXIT=%s\n' "$?" >>result.txt
printf 'RUN_FINISHED=1\n' >>result.txt
```

## Windows 待执行命令

```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk410-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk410-native- --snapshot-format=png udp://@:62024
```

实际PID、CPU/RSS趋势、A/V、退出原因和VLC原生快照结果待执行后记录。先记录后按本轮清单清理，不删指定源或回退产物。


准备记录：临时脚本已上传并放置目标acceptance/rk410/run.sh，尚未执行；本机传输副本out/rk410-run.sh已记录后删除。项目增量构建PID2353126的本轮现场采样：elapsed76秒，70/820项，进程仍存活。

## 实际结果与进程清理

项目all-target最终构建通过：首段2353126编译错误退出1，修正后第二段2389813超时124，保留对象第三段2418478完成214/214退出0。CLI SHA256：084f095373016fa5b4618daf79efe82600191764354422bdede583452e218cf1；显式环境ldd中六个FFmpeg库指向prefix-ffmpeg，MPP指向prefix-mpp，GCC运行库指向gcc-12.4-native，无缺失；无RPATH/RUNPATH，测试脚本保留显式LD_LIBRARY_PATH。

远端runner2448255，源2448256/2448257/2448258，CLI2448259，monitor2448261；Windows HEVC源7028、VLC38164。CLI在约5秒预检中自然退出1：InvalidArgument: Initial composition source admission lost clock evidence。未再出现rk409的opaque-retention拒绝，但没有合屏输出，不能判定身份重排/释放/失源恢复通过。保留mpp client12 driver not ready与filter颜色范围unknown→tv提示。

六次采样elapsed0–5秒：CPU为0/30.0/30.5/28.6/27.2/29.8%，RSS为12744/30452/40840/45320/48216/52692KiB，峰51.46MiB；窗口仅启动阶段，不能推导持续内存趋势。无输出阶段A/V漂移证据，未请求快照或抓包。

远端三源核exe、输入及端口后SIGTERM清理，各退出255；CLI不是人为终止，RUN_FINISHED=1，runner/三源/CLI/monitor均核验不存在。本机源读至1982帧、65.96秒时核路径并与VLC强制清理，两个PID已不存在；不记源自然结束。临时日志待关键clock结论记录后逐项清理。

关键时间线：15:28:58.107/.116/.126源0/1/2协议安装，15:28:59.751源3安装；.756/.774/.785前三路h264_rkmpp contract_bound，.796第四路hevc_rkmpp contract_bound。15:29:00.866、01.443、01.770出现video reason=0的RTP discontinuity，序号缺口15092→15144、15632→15633、15987→16005，generation0→1→2→3；日志未标源，不能直接归属源3。15:29:03.605明确源3候选以clock evidence错误拒绝，.618输出候选传播同因。所有ingress truncations与pressure_failures为0，不等于证明网络无丢包。

cli-maps核实FFmpeg/MPP确实为新prefix，GCC运行库正确。MPP仍打印c087是提交前编译的内嵌metadata，不能据此认定加载原库。源码与日志只支持预检时clock投影降级/不连续被拒，尚未证明SR过期或opaque丢失。

记录后精确清理清单：远端rk410内runner.log、source0/1/2.out、source0/1/2.log、cli-error.log、monitor.log、result.txt、pids.txt、run.sh、cli.log、cli-maps.txt；依赖目录project-configure-lineage.log、project-build-lineage.log、project-build-lineage-2.log、project-build-lineage-3.log。本机composition-rk410-source3.out、composition-rk410-source3.log、composition-rk410-vlc.log。指定源、隔离库、对象缓存及回退产物保留。

清理终态：远端14项测试文件及4项构建日志已逐项删除，rk410目录为空；本机3项日志已删除，本轮前缀无残留。精确测试/构建PID均不存在，指定120秒源仍为124427809字节。首次远端清理因Python无Path.is_relative_to在删除前失败，改用commonpath核界后成功。reason=0源码确认为SequenceGap；现有日志无法区分window超限/等待期限到期及归属源，下一步只补可关联诊断后原规格复验，不放宽门禁。
