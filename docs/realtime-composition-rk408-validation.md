# rk408：四路合屏信用机制 RKMPP 重试

源码75a83c7e，RKMPP all-target已构建通过。仅修正临时脚本环境加载顺序；媒体规格与rk407一致，四路H.264/HEVC、所选AAC、2 GiB准备预算，输出HEVC1280×720/30、8 Mbps CBR、AAC192kbps。状态：已执行，CLI地址校验失败，未进入预算准入。

## 远程临时脚本

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk408
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk408 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://0.0.0.0:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
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

Windows第四源与VLC沿rk407命令，日志及快照前缀改为rk408；实际PID、结果、指标和清理待记录。


## 结果与清理

远程runner1886771；源1886772/1886773/1886774；CLI1886775，monitor1886776。本机源3992、VLC27948。CLI退出1，原始错误 `InvalidArgument: Raw RTP video probe requires numeric unicast address`；第四输入使用0.0.0.0不符合现有契约，下一轮改为实际RKMPP地址192.168.130.229，不修改核心、不降低规格。另保留MPP `client 12 driver is not ready` 提示。CLI即刻退出，monitor仅有2026-11-13T13:46:39.660956929+08:00时间戳，未取得RSS/CPU/A/V或VLC画面，不宣称预算通过。

远程三源各完成3600帧/120秒，exit0，均已自然结束；本机源日志末次3251帧/108.26秒，核验exe后强制停止，VLC同样清理，不记自然退出或验收成功。本机命令与rk407逐项一致，源/播放器日志和快照前缀为composition-rk408。

文件清单：远端run.sh、runner.log、runner-pid.txt、pids.txt、result.txt、monitor.log、cli.log、cli-error.log、source0/1/2.out和source0/1/2.log；本机source3.out/source3.log/VLC.log共3项。结果记录后逐项删除；源文件保留。未产生快照、抓包或录制。

远端14项文件已逐项删除，remaining=[]，六个精确PID均不存在；本机3项日志已删除。
