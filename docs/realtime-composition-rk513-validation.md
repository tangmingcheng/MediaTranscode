# rk513：同轮原始RTP与VLC录制音频时序对照

复用r76/r144离线对照，真实流规格与rk512相同，只有新增接收端抓包；不增加FFmpeg实时监控，不修改依赖。进行中。

```powershell
& D:/Wireshark/dumpcap.exe -i '\Device\NPF_{9F7430E9-3A7B-4738-9F92-9B18AA0D18EC}' -f 'udp dst port 62024' -a duration:200 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-output.pcapng
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk513-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-received.ts}}" --sout-keep --video-filter=scene --scene-format=png --scene-ratio=900 --scene-prefix=composition-rk513-frame- --scene-path=D:/Code/MyCode/MediaTranscode/out/acceptance rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode

```

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk513
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

## 已执行结果

Windows源5612/572均完整120秒exit0，VLC15008；远端runner1620440、源1620443/1620444、示例1620445、monitor1620452。核对exe后kill -TERM 1620445，例程exit0，state4/completion2，encoded78494、worker_errors0、RSS73728000、整机平均CPU13.621%。远端两源exit0，完整120秒。capture会话9026自然exit0，122838包，pcap/dumpcap/flushed/ifdrop均0；1SSRC序号不连续0，TS cc.drop/analysis.skips/analysis.drops无匹配。

接收原始RTP payload直接十六进制解码成TS147565336B，未重封装。原始TS视频4549包、音频6531包，逐流DTS非递增0；完整音视频解码exit0，warning日志0字节。抓包200秒包含约151.6秒输出；VLC录制233.7秒，不将抓包扩为覆盖停止尾部。录制音频10068包hash均在wire出现，4470唯一；静音重复hash不能证明尾部包身份。

VLC第44→45音频包DTS127950→123532回退；相同AAC payload唯一匹配wire DTS91951→94041连续。index43..48逐个唯一匹配：wire89861/91951/94041/96131/98220/100310；record125860/127950/123532/125622/127711/129801。由此将本轮该回退限定在VLC录制/重封装路径，不归生产音频；不宣称定位VLC内部根因，不修改外部依赖。录制仍有null45056>=42891/43915/44938告警，失源低于-60dB区间103.705102–233.704195；不称数学零静音或内容同步。

## 实际离线命令

~~~powershell
& D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-output.pcapng -d udp.port==62024,rtp -Y rtp -T fields -e rtp.payload | Where-Object { $_ -match '^[0-9a-fA-F]+$' } | Out-File -LiteralPath D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-payload.hex -Encoding ascii
& C:/Windows/System32/certutil.exe -decodehex D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-payload.hex D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-wire.ts 12
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -v warning -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-wire.ts -map 0:v:0 -map 0:a:0 -f null -
& D:/mabs/local64/bin-video/ffprobe.exe -v error -show_packets -show_data_hash sha256 -show_entries packet=stream_index,pts,dts,pts_time,data_hash -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-wire.ts
& D:/mabs/local64/bin-video/ffprobe.exe -v error -select_streams a:0 -show_packets -show_data_hash sha256 -show_entries packet=pts,dts,pts_time,data_hash -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-received.ts
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk513-received.ts -map 0:a:0 -af silencedetect=noise=-60dB:d=0.5 -f null -
~~~

独立审查A/B均明确限定PASS，复算原始与录制AU hash/时间戳结论一致；可支持明确标注范围的开发验证版库打包，完整生产验收未通过。

## 本轮清理清单
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-capture.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-frame-00001.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-frame-00901.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-frame-01801.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-frame-02701.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-frame-03601.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-frame-04501.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-frame-05401.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-frame-06301.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-output.pcapng
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-payload.hex
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-received.ts
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-record-packets.json
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-silence.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-source2.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-source3.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-source3.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-vlc.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-wire-decode.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-wire-packets.json
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-wire.ts
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\cli-maps.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\pids.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\result.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\runner.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\socket-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\source0.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\source1.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\udp-after.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk513-remote\udp-before.txt

本机清单全部清理；远端14项日志/SDP同rk512清单逐项删除，临时脚本与空目录移除。两端精确PID均无残留，120秒用户源、正式库包及rollback保留。独立双审通过后才清除原始证据。
