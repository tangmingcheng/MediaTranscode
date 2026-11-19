# rk519：候选取消接线后的四源异构合屏复验

状态：四源双代基础生命周期观测完成，完整交付门禁仍未通过。源码d4d2bae1，实时CLI SHA256=f011a0549227ddec06a31973a2c9f3b10cad7df13a41aea91b02db6b4c887061；沿用rk518已成功构建产物，没有重复编译。进程maps确认候选FFmpeg及MPP实际装载，未改Windows FFmpeg、未打包。

输入H264/H264/H264+AAC/HEVC，输出H264 VBR目标/最低8Mbps、最高12Mbps、1280×720/30fps、AAC192kbps，经MPEGTS/RTP到VLC。每源使用指定120秒连续文件。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk519
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk519 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk519/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk519-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk519-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk519-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk519-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode

$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk519-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk519-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk519-received.ts}}" --sout-keep --video-filter=scene --scene-format=png --scene-ratio=300 --scene-prefix=composition-rk519-frame- --scene-path=D:/Code/MyCode/MediaTranscode/out/acceptance rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
```

启动：`nohup bash acceptance/rk519/run.sh >acceptance/rk519/runner.log 2>&1 </dev/null &`；此前bash -n成功。runner3012097，源0/1=3012100/3012101，CLI3012102，monitor3012104；Windows源2/3=5088/19612，VLC10540。Windows初始两源数值exit0。原生快照00601确认四格，03301为依次失源过渡（仅右下HEVC尚有画面），不记全黑。最终结果见下节。


快照04201确认首轮全黑；同一CLI四路重入，复用上述命令和端口，远程日志source0/1改为reentry0/1，本机source2/3改为reentry2/3。重入PID：3026493、3026499、31316、10676；非同步起播。原生快照06001确认四格恢复。

## 最终结果

已查看快照09601再次全黑；核对/proc/3012102/exe后执行 `kill -TERM 3012102`，CLI_EXIT=0、end_reason=caller_stop。不是全失源自然结束。八次源进程均3600帧、数值exit0；复制/H264日志time120.00秒，HEVC末包time119.90秒（同一完整120秒输入，不截短）。

最终workerErrors/errors=0、workers=0；encodedPacketsPushed/Popped=116109；totalPushed=667220、totalPopped=666921（差299，未声称相等）。payload预约/释放401500，currentBytes/Objects=0，pressureFailures=8。平均进程整机CPU16.386739%、峰64.736842%；RSS初始114319360B、最终/峰136073216B。generation2内部phase9070ns、frequency0ppm；仅内部状态，不代表内容A/V同步。sender committed_datagrams=269985、deadline_misses=0、pressure_failures=0，最大提交迟到37018767ns、delivery_evidence=not_proven。

音频aggregate real_samples=9981632、silence_samples=5287232；这些总数不证明逐代完整连续覆盖。VLC专属RC端口127.0.0.1:42120发送quit；离线执行：

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk519-received.ts' -map 0:a:0 -af silencedetect=noise=-60dB:d=2 -f null NUL
```

exit0；静音108.603946–172.106168秒、289.780658–346.044240秒，中间恢复非静音。保留DTS120211<122999及null45056>=43243/44268告警；未采本轮原始TS，不能直接套用rk513归因。滤镜unknown→tv动态属性告警保留。首次出画与每源启动不是同步的，不将录制内容时长等同源覆盖时长。

本轮证明候选库正常四源异构生命周期可运行，不证明异常取消已触发、完整内容音频尾部、长期RSS或NIC出线。下一步首包前联合采输入61020–23及输出62024–25，覆盖两代完整源直到停止，沿r452/rk456方法验证内容A/V并结合当前SourceDrain日志验证尾部；不重开已实现关闭算法。

## 清理

精确PID核查：所列本机与远程媒体进程均已不存在。远程20项清单：socket-monitor.log、udp-after.txt、output.sdp、runner.log、source0.out、reentry1.out、reentry0.out、cli-error.log、monitor.log、result.txt、reentry0.log、pids.txt、run.sh、cli.log、source1.out、source0.log、reentry1.log、udp-before.txt、source1.log、cli-maps.txt。本机45项composition-rk519-前缀临时快照、录制、源/VLC日志。清理完成：远程20项及空目录删除，本机45项删除，复核本轮临时产物无残留。