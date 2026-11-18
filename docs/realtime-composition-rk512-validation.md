# rk512：单播地址调用随包四源 C 示例

修正rk511的通配绑定错误；使用实际单播192.168.130.229。原120秒源，输出H264 VBR8–12Mbps/1280×720/30，AAC目标192kbps，MPEG-TS/RTP；所选音视频同进程发送。C示例四源/全失源持续输出/调用停止有限PASS；完整音频时序未通过。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk512
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://192.168.130.229:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://192.168.130.229:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/delivery/media-transcode-composition-rkmpp-21fc6bb8-20261118/bin/rkmpp_composition 192.168.130.229 192.168.96.122 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk512-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk512-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk512-received.ts}}" --sout-keep --video-filter=scene --scene-format=png --scene-ratio=900 --scene-prefix=composition-rk512-frame- --scene-path=D:/Code/MyCode/MediaTranscode/out/acceptance rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk512-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk512-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk512-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk512-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode

```

## 结果
Windows源20144/10004完整120秒自然exit0，VLC38560。远端runner1596090、源1596092/1596093、示例1596094、monitor1596096；两源exit0。全失源后输出继续；核实exe后SIGTERM触发API request_stop，示例exit0，state4/completion2，worker_errors0，encoded48259，RSS72585216，平均整机CPU18.528%。实际运行映射FFmpeg62.28.102/62.12.102、MPP prefix-mpp/lib/librockchip_mpp.so.0、RGA2.2.0、libstdc++6.0.30。四格VLC原生快照已查看。
离线FFmpeg silencedetect -60dB/0.5秒：103.008821–143.635556秒静音，之前为非静音。命令与rk510相同仅文件rk512，exit0但有TS DTS119401<122999及null DTS45056>=42847/43871/44894、361530>=361465警告。因此C示例启动/运行/停止成功，完整音频时序未通过；下一轮直接捕获接收网卡RTP/TS与VLC录制对照，先定位，不修改外部依赖。

独立审查B限定PASS；sender提交121721 datagrams，deadline_misses0、pressure_failures0、delivery_evidence=not_proven/coverage0。系统UDP InErrors/RcvbufErrors增量0，NoPorts增26（系统全局不可直接归因）。C示例未暴露完整payload账本，不借用rk510数据。
实际停止：核实 /proc/1596094/exe 等于随包示例后执行 kill -TERM 1596094。离线命令：
~~~powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk512-received.ts -map 0:a:0 -af silencedetect=noise=-60dB:d=0.5 -f null -
~~~

清理清单：
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-frame-00001.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-frame-00901.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-frame-01801.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-frame-02701.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-frame-03601.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-received.ts
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-silence.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-source2.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-source3.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-source3.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-vlc.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\cli-maps.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\pids.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\result.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\runner.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\socket-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\source0.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\source1.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\udp-after.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk512-remote\udp-before.txt

清理：录制文件首次删除被占用，确认VLC38560已退出后重试。远端与rk511同名14项日志逐项删除，临时脚本和空目录删除，五个远端PID无残留；用户120秒源及库包保留。
