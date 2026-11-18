# rk511：随包 C 示例真实四源验证

使用合并静态库重新链接的 rkmpp_composition，所选视频和音频由同一个 Windows FFmpeg 进程发送完整120秒。H264/H264/H264/HEVC → H264 VBR8–12Mbps 1280×720/30，AAC目标192kbps，MPEG-TS/RTP。验证进行中，尚未通过。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk511
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/delivery/media-transcode-composition-rkmpp-21fc6bb8-20261118/bin/rkmpp_composition 0.0.0.0 192.168.96.122 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk511-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk511-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk511-received.ts}}" --sout-keep --video-filter=scene --scene-format=png --scene-ratio=900 --scene-prefix=composition-rk511-frame- --scene-path=D:/Code/MyCode/MediaTranscode/out/acceptance rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk511-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk511-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk511-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk511-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode

```

结果FAIL：runner1587754、源1587756/1587757、C示例1587758、monitor1587760；Windows源37340/4100、VLC20172。C API准备立即失败：Raw RTP video probe requires numeric unicast address（error1/stage3）。实际传入0.0.0.0不满足已有单播地址约束，不能作为合屏或音频实现失败证据。测试中止后按精确PID停止源并退出VLC；不会记录为完整120秒成功。下一轮改用实际目标机单播地址，远端两个源同样发送到该地址，不改变媒体规格。

CLI_EXIT=1，远端两源中止exit255；未通过。清理本轮以下本机文件：
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk511-received.ts
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk511-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk511-source2.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk511-source3.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk511-source3.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk511-vlc.log

远端14个文件：socket-monitor.log、udp-after.txt、runner.log、source0.out、cli-error.log、monitor.log、result.txt、pids.txt、cli.log、source1.out、source0.log、udp-before.txt、source1.log、cli-maps.txt；已逐项清理，并移除临时run.sh和空目录，远端五PID已无残留。
