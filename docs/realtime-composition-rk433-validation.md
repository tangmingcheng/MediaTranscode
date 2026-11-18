# rk433：旧头C消费者链接当前库真实单源验收

状态：旧头消费者真实媒体输出已观察，源结束后自然终态运行失败，不能标完整通过。旧头从本机git show038f069c重新提取，未复用已清理rk432文件。examples/realtime_beta_c.c原样复制并以旧头C11编译、链接当前5b5565d4 beta/application/core。原example仅注册SIGINT，停止必须核exe后SIGINT，不能SIGTERM。

原120秒H264720p30源视频→HEVC1280×720/30fps/8Mbps CBR/GOP60，输出MPEGTS_RTP至62024，VLC使用rtp://@:62024。单源旧API本来无音频，不代替合屏矩阵。源自然结束后观察旧消费者终态；若仍存活再caller-stop，保留全部错误，不把提前退出当通过。

实际运行脚本（待主线程START）：
```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk433
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/acceptance/rk433/consumer composition-rk433-old-api 127.0.0.1 61020 h264 96 90000 192.168.96.122 62024 hevc 1280 720 30 1 60 cbr 8000000 50000000 100 >cli.log 2>cli-error.log &
cli=$!
cat /proc/$cli/maps >cli-maps.txt 2>/dev/null
printf 'SOURCE0=%s CLI=%s\n' "$s0" "$cli" >pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; { date -Ins; ls -l /proc/$cli/fd; cat /proc/$cli/net/udp; } >>socket-monitor.log 2>&1; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" >>pids.txt
wait "$cli"; printf 'CLI_EXIT=%s\n' "$?" >result.txt
wait "$mon"
cat /proc/net/snmp >udp-after.txt
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" >>result.txt
printf 'RUN_FINISHED=1\n' >>result.txt
```

## 编译及实流结果

C11旧头编译、当前静态库链接均exit0，ldd六FFmpeg/MPP/GCC12.4均既定隔离路径。consumer SHA256 8ac3698559843d7f6e73153587a59761fe99cf60b498f9d2e5f9892b0a328c15；原example源码SHA256 a273d3a8e4f24166a085342f6eec85c429a561c3d0bde94e94bc8b0d9f6dd89a；旧头SHA256 5ba348ea3b8227b10416267529296325ce4d6d7801aced6a7f2241eb00e65431。源码原样来自examples/realtime_beta_c.c，无临时修正掩盖旧消费者行为。

在acceptance/rk433执行nohup /bin/bash run.sh >runner.log 2>&1 &；runner546976、源546978、consumer546979、monitor546987，VLC13808、父17370。主线程VLC原生快照13:05:03确认全幅1280×720真实画面。elapsed83秒源2507帧/83.56秒，consumer state2/backend6、dropped0、RSS29,949,952B、平均单核49.66%。

源完整3600帧/120秒自然exit0。尚未发送SIGINT或SIGTERM，consumer约elapsed130008ms自行FAILED5、exit1；event error10/stage7/detail realtime runtime made no progress before timeout，completion5 RUNTIME_FAILURE。不是SOURCE_LOSS，也不能把旧头实流有画面等同整个终态通过。最终queued0、dropped0、平均单核CPU45.46%、RSS29,949,952B。本轮没有主动stop，保留自然失败待共享单源策略审查。
编译命令
```bash
/opt/mt-tools/gcc-12.4-native/bin/gcc -std=c11 -O2 -Wall -Wextra -Werror -I/home/tang/MediaTranscode-composition-09093919/acceptance/rk433/old -c /home/tang/MediaTranscode-composition-09093919/acceptance/rk433/realtime_beta_c.c -o /home/tang/MediaTranscode-composition-09093919/acceptance/rk433/consumer.o
/opt/mt-tools/gcc-12.4-native/bin/g++ /home/tang/MediaTranscode-composition-09093919/acceptance/rk433/consumer.o -o /home/tang/MediaTranscode-composition-09093919/acceptance/rk433/consumer /home/tang/MediaTranscode-composition-09093919/out/build/rk-release/libmedia_transcode_beta.a libmedia_transcode_realtime_application.a libmedia_transcode_core.a /usr/local/lib/aarch64-linux-gnu/librga.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavfilter.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavformat.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavutil.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libswscale.so /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libswresample.so -lpthread
```

终态原文
```text
state=2 backend=6 elapsed_ms=118977 queued=1 dropped=0 cpu_single=48.74 rss=29949952
state=2 backend=6 elapsed_ms=119986 queued=0 dropped=0 cpu_single=48.55 rss=29949952
state=2 backend=6 elapsed_ms=120993 queued=1 dropped=0 cpu_single=48.17 rss=29949952
state=2 backend=6 elapsed_ms=122002 queued=0 dropped=0 cpu_single=47.82 rss=29949952
state=2 backend=6 elapsed_ms=123018 queued=0 dropped=0 cpu_single=47.50 rss=29949952
state=2 backend=6 elapsed_ms=124032 queued=0 dropped=0 cpu_single=47.18 rss=29949952
state=2 backend=6 elapsed_ms=124944 queued=0 dropped=0 cpu_single=46.90 rss=29949952
state=2 backend=6 elapsed_ms=125960 queued=0 dropped=0 cpu_single=46.58 rss=29949952
state=2 backend=6 elapsed_ms=126971 queued=0 dropped=0 cpu_single=46.27 rss=29949952
state=2 backend=6 elapsed_ms=127983 queued=0 dropped=0 cpu_single=45.96 rss=29949952
state=2 backend=6 elapsed_ms=128994 queued=0 dropped=0 cpu_single=45.64 rss=29949952
event=5 state=0 error=10 stage=7 completion=0 native=0 detail=realtime runtime made no progress before timeout
output_description_path=/tmp/media-transcode-beta-oWu00a
event=5 state=0 error=10 stage=7 completion=0 native=0 detail=realtime runtime made no progress before timeout
output_description_path=/tmp/media-transcode-beta-oWu00a
event=3 state=0 error=10 stage=7 completion=0 native=0 detail=realtime runtime made no progress before timeout
event=1 state=5 error=0 stage=0 completion=0 native=0 detail=
event=4 state=0 error=0 stage=0 completion=5 native=0 detail=
state=5 backend=6 elapsed_ms=130008 queued=0 dropped=0 cpu_single=45.46 rss=29949952
mpp[546979]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group
CLI_EXIT=1
SOURCE0_EXIT=0
RUN_FINISHED=1
```

清理清单：旧头、realtime_beta_c.c、consumer.o、consumer、build-commands.txt、run.sh及本轮基础日志；归档后精确删除。远端18项文件已精确删除，两个空头目录移除，rk433目录为空、四PID无残留；会话SDP临时路径/tmp/media-transcode-beta-oWu00a已由release删除，核验不存在。

## 独立源码复核

对照038f069c基线，RunController中无输入活跃且无输出进展触发ProgressTimeout、Beta将运行阶段失败映射RUNTIME_FAILURE的行为均已存在，当前没有ABI破坏证据。SOURCE_LOSS枚举当前没有Beta生产路径；缺少权威EOF/BYE/失源分类证据时不能将所有进度超时改为SOURCE_LOSS。合屏复用同一watchdog，但全失源黑屏/静音持续输出会维持进展，因此单源超时不能否定已观察的合屏全失源持续输出。自然终态仍保留失败，旧头实流兼容和失败分类分开评价。
