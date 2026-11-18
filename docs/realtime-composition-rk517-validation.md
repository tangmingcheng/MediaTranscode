# rk517：三源 HEVC VBR 启动超时诊断复验

新增超时前缓存诊断，经两名未参与实现者源码审查 PASS。RKMPP 8 核增量构建成功（exit0）；实时 CLI SHA256 `c858b89a497d917fc5d3c788b47de56d9a0227ead99c9500339898b355c7a758`。仅诊断修改，原启动超时与失败回收问题尚未判修复。完整验证前不打包。

诊断复用定容 snapshot，记录各源缓存数、首末 PTS、候选 frontier 极值及实际启动配置；极值不是连续覆盖证明。保持原错误/状态转换/清理语义，诊断分配失败保留原超时。行业对照：[FFmpeg 日志接口](https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavutil/log.h)。

## 实际验证命令

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk517
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk517 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk517/output.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
cat /proc/$cli/maps >cli-maps.txt 2>/dev/null
printf 'SOURCE0=%s SOURCE1=%s CLI=%s\n' "$s0" "$s1" "$cli" >pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; { date -Ins; ls -l /proc/$cli/fd; cat /proc/$cli/net/udp; } >>socket-monitor.log 2>&1; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" >>pids.txt
wait "$cli"; printf 'CLI_EXIT=%s\n' "$?" >result.txt
wait "$mon"
cat /proc/net/snmp >udp-after.txt
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" >>result.txt
wait "$s1"; printf 'SOURCE1_EXIT=%s\n' "$?" >>result.txt
printf 'RUN_FINISHED=1\n' >>result.txt


```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk517-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk517-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk517-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk517-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk517-received.ts}}" --sout-keep --video-filter=scene --scene-format=png --scene-ratio=300 --scene-prefix=composition-rk517-frame- --scene-path=D:/Code/MyCode/MediaTranscode/out/acceptance rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
```

## 结果

验证已结束；未修改本机或目标机 FFmpeg，未生成新包。

## 本轮结果与边界

源码 a2c3a15e。远程 runner=1861795、初始源=1861799/1861800、CLI=1861801、monitor=1861803、纯视频重入=1879914；Windows 初始AV=40776、重入AV=21644、VLC=38108。三个初始源及两个重入源均3600帧/120秒、exit0。源0、源2重入并非同步起播。

已实际查看VLC原生快照00901（三格+右下黑）、03901（全黑）、07501（左上/左下恢复、右侧黑）、11101（再次全黑）。观测全失源持续输出后，核对CLI exe并执行 `kill -TERM 1861801`，最终 `end_reason=caller_stop`、CLI_EXIT=0；不是全失源自然结束。

最终errors/workerErrors=0；encodedPacketsPushed/Popped=131552，totalPushed/Popped=584021；payload预约/释放=366596，currentBytes/Objects=0，pressureFailures=8。平均进程整机CPU=13.414028%、峰57.219251%；RSS初始56991744B、最终/峰70516736B。generation2内部filtered_phase=11338ns、frequency=0ppm，不能替代内容级A/V。sender committed_datagrams=263709，deadline_misses=0、pressure_failures=0，最大提交迟到63198444ns，delivery_evidence=not_proven。

VLC录制音频离线检查命令：

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk517-received.ts' -map 0:a:0 -af silencedetect=noise=-60dB:d=2 -f null NUL
```

检测到静音119.680454–239.861406秒、357.521769–392.114444秒，中间恢复非静音。保留录制告警：DTS119941<122999；null输出45056>=43111/44135，未在本轮抓原始TS，不能借用rk513判断本轮根因。VLC另有23ms迟到、退出read error及SetThumbNailClip 0x800706F4；滤镜unknown→tv范围变化告警保留。

本轮仅支持三源HEVC VBR生命周期、纯视频/所选AV恢复、黑屏/静音及正常caller-stop的有限观察。未复现rk516超时，新增诊断错误分支未被触发，不能关闭首次启动/abort阻塞，也不能判完整音频时序、长期RSS、NIC出线或Windows回归通过。不打包。

重入实际命令：

```bash
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >acceptance/rk517/reentry0.out 2>acceptance/rk517/reentry0.log &
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk517-reentry2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk517-reentry2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

```

清理前精确PID核验：上述远程与Windows媒体进程均已不存在。远程清理清单：socket-monitor.log、udp-after.txt、output.sdp、runner.log、source0.out、reentry0.out、cli-error.log、monitor.log、result.txt、reentry0.log、pids.txt、run.sh、cli.log、source1.out、source0.log、reentry-result.txt、udp-before.txt、source1.log、cli-maps.txt。本机清理仅本轮composition-rk517-前缀的快照、录制和源/VLC日志。清理完成状态见末尾。



清理完成：本机46项、远程19项临时产物及远程空目录已删除；指定复用源和既有开发包保留。PR#35新智能体审查：本轮诊断源码PASS，整体合并FAIL（完整门禁未完成）。
