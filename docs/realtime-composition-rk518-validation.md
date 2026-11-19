# rk518：候选取消接线后的三源合屏复验

状态：本轮观测已结束；支持三源正常生命周期的有限结论，不能据此关闭 rk516 取消失败路径。

源码 d4d2bae1；RKMPP 原构建目录 8 核增量构建 836/836、exit0。前序环境错误及缓存路径中止见取消计划，未覆盖原失败记录。实时 CLI SHA256 `f011a0549227ddec06a31973a2c9f3b10cad7df13a41aea91b02db6b4c887061`。实际装载候选 FFmpeg/MPP；Windows FFmpeg 未修改，未执行打包。

## 实际命令

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp-cancel-eos-20261118/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk518
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk518 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk518/output.sdp --video-codec hevc --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk518-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk518-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk518-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk518-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk518-received.ts}}" --sout-keep --video-filter=scene --scene-format=png --scene-ratio=300 --scene-prefix=composition-rk518-frame- --scene-path=D:/Code/MyCode/MediaTranscode/out/acceptance rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
```

重入使用上述源0命令，日志改为 acceptance/rk518/reentry0.out、reentry0.log；Windows 同一命令仅日志前缀 source2 改为 reentry2。未同步起播，均保持原120秒源与规格。

## 观测

runner2949188、初始源2949190/2949191、CLI2949192、monitor2949194；Windows 初始源32680、VLC24148；重入源2972741、Windows20244。初始Windows源日志3600帧/120秒；工具会话回收后未能重新读取数值退出码，不推断exit0。

已查看VLC原生快照00901（三格、右下黑；前段记录）、08101（全黑）、10201（源0/2恢复，右侧黑）。ABI注册日志7条仅证明注册，不证明取消回调已触发。generation2内部phase11338ns、frequency0ppm，不替代内容级A/V同步。最终结果及清理见下节。


## 最终结果与边界

快照13501再次全黑；核对/proc/2949192/exe后执行 `kill -TERM 2949192`，CLI_EXIT=0、end_reason=caller_stop，不是失源自然退出。初始远程两源、远程及Windows重入源exit0；初始Windows源缺数值退出码，完整门禁仍未通过。两次重入均完整120秒，未降规格。

workerErrors/errors=0、workers=0、totalPushed/Popped=635562、encodedPacketsPushed/Popped=159222；payload reservations/releases=393096，currentBytes/Objects=0，pressureFailures=4。平均进程整机CPU12.005796%、峰50.264550%；RSS初始58392576B、最终/峰71213056B。sender committed_datagrams=273059，deadline_misses=0、pressure_failures=0，最大提交迟到52025346ns，delivery_evidence=not_proven。

VLC通过专属RC端口127.0.0.1:42120发送quit正常关闭。录制离线检查实际命令：

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk518-received.ts' -map 0:a:0 -af silencedetect=noise=-60dB:d=2 -f null NUL
```

命令exit0，静音119.661293–313.861610秒及431.531474–474.471497秒，中间恢复非静音。保留DTS119761<122999和null输出45056>=43023/44047告警；本轮未采原始TS，不能借用rk513判定本轮根因，亦非内容级A/V通过。

未复现rk516首发超时，未证明取消callback实际返回；正常caller-stop不能替代异常abort证据。四源候选依赖回归、内容级音频与Windows共享回归仍待。后续优先合屏功能验证，不扩大打包或可靠性改造范围。

清理前所有本轮已列媒体PID均不存在。远程18项清单：socket-monitor.log、udp-after.txt、output.sdp、runner.log、source0.out、reentry0.out、cli-error.log、monitor.log、result.txt、reentry0.log、pids.txt、run.sh、cli.log、source1.out、source0.log、udp-before.txt、source1.log、cli-maps.txt。本机54项均为composition-rk518-前缀，包括VLC原生快照、录制及源/VLC日志。清理已执行：远程18项及空目录删除，本机54项删除，复查无本轮残留。两名独立审查者对本轮记录均明确限定PASS；不代表完整功能门禁通过。